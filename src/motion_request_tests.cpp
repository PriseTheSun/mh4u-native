#include "motion_request.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
namespace {
using namespace mh4u::motion_request;
std::size_t checks=0;
void check(bool v,const char* message) {++checks;if(!v)throw std::runtime_error(message);}
std::uint32_t read(std::istream& in) {
    std::array<unsigned char,4>b{};in.read(reinterpret_cast<char*>(b.data()),4);
    if(!in)throw std::runtime_error("oracle truncated");
    return std::uint32_t(b[0])|(std::uint32_t(b[1])<<8)|(std::uint32_t(b[2])<<16)|(std::uint32_t(b[3])<<24);
}
using Bytes=std::array<unsigned char,0x130>;
Bytes read_layer(std::istream& in) {Bytes b{};in.read(reinterpret_cast<char*>(b.data()),b.size());if(!in)throw std::runtime_error("oracle layer truncated");return b;}
std::uint32_t word(const Bytes& b,std::size_t o,unsigned size=4) {std::uint32_t v=0;for(unsigned j=0;j<size;++j)v|=std::uint32_t(b[o+j])<<(8*j);return v;}
float real(const Bytes& b,std::size_t o) {return std::bit_cast<float>(word(b,o));}
Layer decode(const Bytes& b,const Request& r) {
    Layer l;l.selector=word(b,4,2);l.flags=word(b,6,2);l.field08=word(b,8,2);l.attributes=word(b,12);
    l.requested_frame=real(b,0x10);l.offset=real(b,0x14);l.interpolation=real(b,0x40);
    l.remaining_interpolation=real(b,0x44);l.weight=real(b,0x48);l.frame=real(b,0x4c);
    l.previous_frame=real(b,0x50);l.end_frame=real(b,0x54);l.field58=real(b,0x58);l.speed=real(b,0x5c);l.field60=real(b,0x60);
    for(unsigned g=0;g<4;++g) {
        const auto p=word(b,0xf8+g*4);
        if(p==0)l.bindings[g]=std::nullopt;
        else if(p==0x2600000+g*72)l.bindings[g]=Binding{2,11,19,g};
        else if(p==0x2400000+g*72)l.bindings[g]=Binding{1,(r.selector>>8)&15U,r.selector&255U,g};
        else throw std::runtime_error("oracle binding identity");
        l.masks[g]=word(b,0x108+g*4);
    }
    return l;
}
void equal(const Layer& a,const Layer& b) {
    check(a.selector==b.selector,"selector");check(a.flags==b.flags,"flags");check(a.field08==b.field08,"field08");check(a.attributes==b.attributes,"attributes");
    const std::array x{a.requested_frame,a.offset,a.interpolation,a.remaining_interpolation,a.weight,a.frame,a.previous_frame,a.end_frame,a.field58,a.speed,a.field60};
    const std::array y{b.requested_frame,b.offset,b.interpolation,b.remaining_interpolation,b.weight,b.frame,b.previous_frame,b.end_frame,b.field58,b.speed,b.field60};
    for(std::size_t j=0;j<x.size();++j)check(std::bit_cast<std::uint32_t>(x[j])==std::bit_cast<std::uint32_t>(y[j]),"float bits");
    check(a.bindings==b.bindings,"bindings");check(a.masks==b.masks,"masks");
}
void error(const Result& r,ErrorCode expected) {const auto* e=std::get_if<Error>(&r);check(e&&e->code==expected,"expected profile rejection");}
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
struct FloatingControl {
    std::uint64_t original;
    FloatingControl() {
#if defined(__aarch64__)
        __asm__ volatile("mrs %0, fpcr":"=r"(original));
#else
        original=_mm_getcsr();
#endif
    }
    void set(std::uint64_t value) {
#if defined(__aarch64__)
        __asm__ volatile("msr fpcr, %0\n\tisb"::"r"(value):"memory");
#else
        _mm_setcsr(static_cast<unsigned>(value));
#endif
    }
    ~FloatingControl(){set(original);}
};
#endif
std::size_t oracle(const char* path) {
    std::ifstream in(path,std::ios::binary);char magic[8]{};in.read(magic,8);check(std::string_view(magic,8)=="MH4UMR01","oracle magic");
    const auto count=read(in);check(count<=100000,"oracle capacity");
    for(std::uint32_t i=0;i<count;++i) {
        Request r;r.layer=read(in);r.selector=read(in);r.attributes=read(in);r.interpolation=std::bit_cast<float>(read(in));r.start_frame=std::bit_cast<float>(read(in));r.speed=std::bit_cast<float>(read(in));
        const auto previous=decode(read_layer(in),r);const auto kind=read(in),slots_count=read(in),skeleton=read(in),frames=read(in),loop=read(in);
        check(slots_count<=65535&&kind<=4,"oracle resource bounds");
        std::array<std::vector<mh4u::motion_events::Record>,4> groups;Action action{frames,std::bit_cast<std::int32_t>(loop),true,{}};
        for(unsigned g=0;g<4;++g) {
            const auto records=read(in);check(records<=1024,"oracle event bounds");
            for(unsigned j=0;j<records;++j){const auto mask=read(in),duration=read(in);groups[g].push_back({mask,duration});}
            action.groups[g]=groups[g];
        }
        const auto accepted=read(in);const auto expected=decode(read_layer(in),r);
        std::vector<std::optional<Action>> slots(slots_count);
        if(kind==4&&(r.selector&255U)<slots.size())slots[r.selector&255U]=action;
        Model model;model.skeleton_present=skeleton!=0;
        model.banks[(r.selector>>8)&15U]=kind==0?Bank{Absent{}}:kind==1?Bank{Resource{1,std::nullopt}}:Bank{Resource{1,std::span<const std::optional<Action>>(slots)}};
        const auto result=apply(previous,model,r);const auto* output=std::get_if<Output>(&result);
        if(!output)throw std::runtime_error("unexpected error in ARM fixture "+std::to_string(i));
        try {check(output->accepted==(accepted!=0),"return value");equal(output->layer,expected);}
        catch(const std::exception& e){throw std::runtime_error("ARM fixture "+std::to_string(i)+": "+e.what());}
    }
    check(in.peek()==std::char_traits<char>::eof(),"oracle trailing bytes");return count;
}
void boundaries() {
    Layer layer;Model model;model.skeleton_present=true;Request request{0,0,0,6,0,1};
    error(apply(layer,model,request),ErrorCode::unknown_bank);
    request.selector=0xffff;check(!std::get<Output>(apply(layer,model,request)).accepted,"sentinel bypasses unknown bank");request.selector=0;
    Action action{120,-1,true,{}};std::vector<std::optional<Action>> slots{action};model.banks[0]=Resource{1,std::span<const std::optional<Action>>(slots)};
    auto result=std::get<Output>(apply(layer,model,request));check(result.accepted&&result.layer.attributes==4,"negative loop sets stop flag");
    slots[0]->frame_count=0;error(apply(layer,model,request),ErrorCode::frame_domain);slots[0]->frame_count=0x80000000;error(apply(layer,model,request),ErrorCode::frame_domain);slots[0]->frame_count=120;
    slots[0]->event_descriptors_present=false;error(apply(layer,model,request),ErrorCode::event_descriptors);slots[0]->event_descriptors_present=true;
    const std::array<mh4u::motion_events::Record,2> bad{{{1,0x7fffffff},{2,1}}};slots[0]->groups[0]=bad;error(apply(layer,model,request),ErrorCode::event_accumulator);slots[0]->groups[0]={};
    request.start_frame=std::numeric_limits<float>::quiet_NaN();error(apply(layer,model,request),ErrorCode::nonfinite_input);request.start_frame=0;
    request.attributes=0x20000;request.start_frame=-std::numeric_limits<float>::max();layer.offset=-std::numeric_limits<float>::max();error(apply(layer,model,request),ErrorCode::frame_domain);
    request.start_frame=0;layer.offset=-1;result=std::get<Output>(apply(layer,model,request));check(result.layer.frame==-1,"negative effective frame preserved");
    const int before=std::fegetround();std::fesetround(FE_DOWNWARD);const auto rejected=apply(layer,model,request);std::fesetround(before);error(rejected,ErrorCode::floating_environment);
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
    {
        FloatingControl control;
#if defined(__aarch64__)
        constexpr std::array<std::uint64_t,1> masks{1ULL<<24};
#else
        constexpr std::array<std::uint64_t,3> masks{1ULL<<15,1ULL<<6,(1ULL<<15)|(1ULL<<6)};
#endif
        for(auto mask:masks){control.set(control.original|mask);error(apply(layer,model,request),ErrorCode::floating_environment);}
    }
#endif
    model.banks[0]=Resource{0,std::span<const std::optional<Action>>(slots)};error(apply(layer,model,request),ErrorCode::resource_identity);
    std::vector<std::optional<Action>> oversized(65536);model.banks[0]=Resource{1,std::span<const std::optional<Action>>(oversized)};error(apply(layer,model,request),ErrorCode::table_extent);
}
}
int main(int argc,char** argv) {
    try {boundaries();const auto count=argc==2?oracle(argv[1]):0;std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_cases\":"<<count<<"}\n";return 0;}
    catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
