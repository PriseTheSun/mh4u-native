#include "motion_command.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif

namespace c=mh4u::motion_command;
namespace r=mh4u::motion_request;
namespace {
struct State : c::Fields {std::vector<mh4u::motion_pending::Layer> layers=std::vector<mh4u::motion_pending::Layer>(2);std::uint32_t count234{};};
struct Output {State state;std::array<c::ObservedRequest,2> requests;std::uint8_t request_count;};
using Result=std::variant<Output,c::Error>;
Result apply(const State& state,const r::Model& model,const c::Command& command,const c::Globals& globals){
 auto result=c::apply(state.layers,state.count234,state,model,command,globals);
 if(auto* e=std::get_if<c::Error>(&result))return *e;
 auto out=std::get<c::Output>(std::move(result));State next;static_cast<c::Fields&>(next)=out.fields;next.layers=std::move(out.layers);next.count234=state.count234;return Output{std::move(next),out.requests,out.request_count};
}
std::size_t checks=0;
void check(bool condition,const std::string& message){++checks;if(!condition)throw std::runtime_error(message);}
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
void equal(const r::Layer& x,const r::Layer& y) {
    check(x.selector==y.selector&&x.flags==y.flags&&x.field08==y.field08&&x.attributes==y.attributes,"layer integers");
    const std::array a{x.requested_frame,x.offset,x.interpolation,x.remaining_interpolation,x.weight,x.frame,x.previous_frame,x.end_frame,x.field58,x.speed,x.field60};
    const std::array b{y.requested_frame,y.offset,y.interpolation,y.remaining_interpolation,y.weight,y.frame,y.previous_frame,y.end_frame,y.field58,y.speed,y.field60};
    for(unsigned i=0;i<a.size();++i)check(bits(a[i])==bits(b[i]),"layer float bits "+std::to_string(i));
    check(x.bindings==y.bindings&&x.masks==y.masks,"event identities/masks");
}
void equal(const State& x,const State& y) {
    check(x.count234==y.count234&&x.fieldsd48==y.fieldsd48&&x.fieldd68==y.fieldd68&&x.fieldsd70==y.fieldsd70,"outer fields");
    for(unsigned i=0;i<2;++i){equal(x.layers[i].animation.layer,y.layers[i].animation.layer);check(x.layers[i].animation.layer.interpolation_curve_bits==y.layers[i].animation.layer.interpolation_curve_bits,"layer tables");}
}
void error(const Result& result,c::ErrorCode expected) {
    const auto e=std::get_if<c::Error>(&result);check(e&&e->code==expected,"expected rejection");
}
Output accepted(Result result) {
    if(const auto e=std::get_if<c::Error>(&result))throw std::runtime_error(std::string(e->message));
    return std::get<Output>(std::move(result));
}
struct Fixture {
    std::array<std::optional<r::Action>,4> slots;
    r::Model model;
    State state;
    c::Globals globals;
    c::Command command;
    Fixture() {
        slots[3]=r::Action{120,-1,true,{}};model.skeleton_present=true;model.banks[0]=r::Resource{1,std::span<const std::optional<r::Action>>(slots)};
        state.layers[0].animation.layer.flags=2;state.layers[0].animation.layer.selector=0xffff;
        state.layers[1].animation.layer.selector=7;state.layers[1].animation.layer.weight=0.25f;state.layers[1].animation.layer.interpolation_curve_bits={0x12345678,0xabcdef01};
        state.count234=2;state.fieldsd70={1,2,3,4};
        globals.fields6c_tables={std::array<std::uint32_t,2>{10,11},std::array<std::uint32_t,2>{20,21},std::array<std::uint32_t,2>{30,31}};
        command.entry=c::Entry::dispatch;command.mode=1;command.selector=3;command.interpolation=4;command.start_frame=2;command.speed=99;
    }
};
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
struct FloatingControl {
    std::uint64_t original;
    FloatingControl(){
#if defined(__aarch64__)
        __asm__ volatile("mrs %0, fpcr":"=r"(original));
#else
        original=_mm_getcsr();
#endif
    }
    void set(std::uint64_t value){
#if defined(__aarch64__)
        __asm__ volatile("msr fpcr, %0\n\tisb"::"r"(value):"memory");
#else
        _mm_setcsr(static_cast<unsigned>(value));
#endif
    }
    ~FloatingControl(){set(original);}
};
#endif
void semantics() {
    Fixture f;const auto before=f.state;
    auto output=accepted(apply(f.state,f.model,f.command,f.globals));
    check(output.request_count==2&&output.requests[0].accepted&&!output.requests[1].accepted,"common layer0 then sentinel layer1");
    check(output.requests[0].request.interpolation==6&&output.requests[0].request.speed==1,"wrapper replaces speed");
    check(output.state.layers[1].animation.layer.selector==0xffff&&output.state.layers[1].animation.layer.weight==1,"shared weight changed by sentinel");
    check(output.state.layers[1].animation.layer.interpolation_curve_bits==before.layers[1].animation.layer.interpolation_curve_bits,"failed sentinel preserves table");
    check(output.state.fieldsd70==before.fieldsd70,"flag2 avoids root sampler");
    check(output.state.fieldsd48[0]==0xffff&&output.state.fieldsd48[6]==0x8000,"successful outer fields");
    equal(f.state,before);
    f.command.mode=2;f.command.interpolation=-4;output=accepted(apply(f.state,f.model,f.command,f.globals));
    check(output.requests[0].command_flags==0x13&&output.state.fieldsd48[6]==0x13&&output.state.layers[0].animation.layer.interpolation_curve_bits==std::array<std::uint32_t,2>{30,31},"variant flags and table");
    f.command.entry=c::Entry::layer;f.command.layer=0;f.command.flags=0x38;f.command.interpolation=4;f.command.speed=2;
    output=accepted(apply(f.state,f.model,f.command,f.globals));
    check(output.request_count==1&&output.requests[0].request.interpolation==3&&bits(6)==output.state.fieldd68,"pre-division store order");
    check(output.state.layers[0].animation.layer.interpolation_curve_bits==std::array<std::uint32_t,2>{20,21}&&output.state.layers[0].animation.layer.attributes==0x2084,"table priority and attribute replacement");
    f.command.flags=0;f.command.speed=-1;
    for(std::uint32_t raw:{0x358637bcU,0x358637bdU,0x358637beU}) {
        f.command.interpolation=std::bit_cast<float>(raw|0x80000000U);
        output=accepted(apply(f.state,f.model,f.command,f.globals));
        check(bits(output.requests[0].request.interpolation)==raw,"threshold reached after division");
        check(output.requests[0].request.attributes==(raw>0x358637bdU?0x10000U:0U),"strict signed representation threshold");
    }
    // No-op paths precede gates and floating operands that the source never reads.
    f.command.entry=c::Entry::dispatch;f.command.mode=99;f.command.interpolation=std::numeric_limits<float>::quiet_NaN();f.state.layers[0].animation.layer.flags=0;
    output=accepted(apply(f.state,r::Model{},f.command,c::Globals{}));check(output.request_count==0,"unknown dispatch mode must be noop");equal(output.state,f.state);
    f.command.mode=0;f.command.selector=f.state.layers[0].animation.layer.selector;
    output=accepted(apply(f.state,r::Model{},f.command,c::Globals{}));check(output.request_count==0,"same selector short circuit");
    f.command.selector=0x1ffff;error(apply(f.state,f.model,f.command,f.globals),c::ErrorCode::nonfinite_input);
    f.command.interpolation=4;error(apply(f.state,f.model,f.command,f.globals),c::ErrorCode::query_failed);
    f.model.skeleton_present=false;error(apply(f.state,f.model,f.command,f.globals),c::ErrorCode::unknown_vector);
    f.globals.vector10e6370=std::array<std::uint32_t,3>{0x12345678,0x80000000,0x7fc00001};
    f.model.banks[15]=r::Absent{};
    output=accepted(apply(f.state,f.model,f.command,f.globals));check(output.request_count==2&&output.state.fieldsd70==std::array<std::uint32_t,4>{0x12345678,0x80000000,0x7fc00001,0},"explicit vector copied as bits, second request sees updated flags");
    f=Fixture{}; // Rebind span after assignment: Fixture owns its resource storage.
    f.model.banks[0]=r::Resource{1,std::span<const std::optional<r::Action>>(f.slots)};
    f.globals.fields6c_tables[0].reset();error(apply(f.state,f.model,f.command,f.globals),c::ErrorCode::unknown_table);
    f.globals.fields6c_tables[0]=std::array<std::uint32_t,2>{0,0};f.model.banks[0]=r::Unknown{};
    const auto failed=apply(f.state,f.model,f.command,f.globals);error(failed,c::ErrorCode::request_failed);
    check(std::get<c::Error>(failed).request_error->code==r::ErrorCode::unknown_bank,"underlying error preserved");
    f.model.banks[0]=r::Resource{1,std::span<const std::optional<r::Action>>(f.slots)};
    f.command.entry=c::Entry::layer;f.command.layer=2;error(apply(f.state,f.model,f.command,f.globals),c::ErrorCode::layer_domain);f.command.layer=0;
    f.command.speed=std::numeric_limits<float>::denorm_min();error(apply(f.state,f.model,f.command,f.globals),c::ErrorCode::nonfinite_input);f.command.speed=1;
    const auto rounding=std::fegetround();std::fesetround(FE_DOWNWARD);const auto rounded=apply(f.state,f.model,f.command,f.globals);std::fesetround(rounding);error(rounded,c::ErrorCode::floating_environment);
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
    {
        FloatingControl control;
#if defined(__aarch64__)
        constexpr std::array<std::uint64_t,1> masks{1ULL<<24};
#else
        constexpr std::array<std::uint64_t,3> masks{1ULL<<15,1ULL<<6,(1ULL<<15)|(1ULL<<6)};
#endif
        for(auto mask:masks){control.set(control.original|mask);error(apply(f.state,f.model,f.command,f.globals),c::ErrorCode::floating_environment);}
    }
#endif
}
void live_boundaries() {
 Fixture f;f.state.layers[0].animation.layer.flags=0;f.state.layers[0].animation.layer.previous_frame=.5F;
 c::Sampling sampling;sampling.templates.base_translation_10e62c0=mh4u::motion_initialization::Bits3{bits(2),bits(-3),bits(4)};sampling.templates.identity_10e6560=mh4u::motion_initialization::Bits4{0,0,0,bits(1)};
 c::Limits limits;
 auto invoke=[&](std::uint32_t active){return c::apply(f.state.layers,active,f.state,f.model,f.command,f.globals,sampling,limits);};
 auto rejected=[&](std::uint32_t active,c::ErrorCode code){auto result=invoke(active);check(std::holds_alternative<c::Error>(result)&&std::get<c::Error>(result).code==code,"live typed rejection");};
 rejected(3,c::ErrorCode::query_extent);limits.query.layers=0;rejected(1,c::ErrorCode::resource_limit);limits={};limits.storage_layers=1;rejected(1,c::ErrorCode::resource_limit);limits={};f.state.layers[1].animation.tracks.joints.resize(2);limits.joint_bindings=1;rejected(1,c::ErrorCode::resource_limit);limits={};
 auto result=invoke(0);check(std::holds_alternative<c::Output>(result),"zero active query with physical storage");auto& zero=std::get<c::Output>(result);check(zero.queries==2&&zero.samples==0&&zero.fields.fieldsd70==std::array<std::uint32_t,4>{0,0,0,0},"zero active still executes template queries");check(zero.layers[1].animation.tracks.joints.size()==2,"inactive joint bindings preserved");
 f.state.layers.resize(8);f.command.entry=c::Entry::layer;f.command.layer=7;f.command.selector=0xffff;result=invoke(4);check(std::holds_alternative<c::Output>(result),"physical layer seven with active prefix four");check(std::get<c::Output>(result).layers.size()==8&&std::get<c::Output>(result).requests[0].request.layer==7,"storage and active count remain distinct");
 sampling.templates.identity_10e6560.reset();rejected(0,c::ErrorCode::query_failed);sampling.templates.identity_10e6560=mh4u::motion_initialization::Bits4{0,0,0,bits(1)};
 f.state.layers[0].animation.layer.previous_frame=std::numeric_limits<float>::quiet_NaN();rejected(4,c::ErrorCode::arithmetic);
 f.state.layers[0].animation.layer.flags=2;result=invoke(0xffffffffU);check(std::holds_alternative<c::Output>(result),"flags2 ignores unavailable active prefix and prior frame");
 c::Command noop;noop.entry=c::Entry::dispatch;noop.mode=99;noop.interpolation=std::numeric_limits<float>::quiet_NaN();auto empty=c::apply({},0xffffffffU,{}, {},noop,{});check(std::holds_alternative<c::Output>(empty)&&std::get<c::Output>(empty).layers.empty(),"unknown dispatch ignores absent layers");noop.entry=c::Entry::if_changed;empty=c::apply({},0,{}, {},noop,{});check(std::holds_alternative<c::Error>(empty)&&std::get<c::Error>(empty).code==c::ErrorCode::layer_domain,"consumed getter requires layerzero");
}
class Reader {
    std::ifstream in;
public:
    explicit Reader(const char* path):in(path,std::ios::binary){if(!in)throw std::runtime_error("oracle open");}
    std::uint32_t word(){std::uint32_t value=0;for(unsigned i=0;i<4;++i){int b=in.get();if(b<0)throw std::runtime_error("oracle truncated");value|=std::uint32_t(b)<<(8*i);}return value;}
    std::array<unsigned char,656> state(){std::array<unsigned char,656> b{};in.read(reinterpret_cast<char*>(b.data()),b.size());if(!in)throw std::runtime_error("oracle state truncated");return b;}
    unsigned header(){char magic[8]{};in.read(magic,8);check(std::string_view(magic,8)=="MH4UMC01","oracle magic");auto n=word();check(n&&n<=100000,"oracle count");return n;}
    void finish(){check(in.peek()==std::char_traits<char>::eof(),"oracle trailing bytes");}
};
State decode(const std::array<unsigned char,656>& b,std::uint32_t count,std::uint32_t selector) {
    const auto word=[&](unsigned offset,unsigned width=4){std::uint32_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint32_t(b.at(offset+i))<<(8*i);return value;};
    State state;state.count234=count;
    for(unsigned i=0;i<2;++i) {
        const auto base=i*0x130;auto& layer=state.layers[i];auto& l=layer.animation.layer;
        const auto real=[&](unsigned offset){return std::bit_cast<float>(word(base+offset));};
        l.selector=static_cast<std::uint16_t>(word(base+4,2));l.flags=static_cast<std::uint16_t>(word(base+6,2));l.field08=static_cast<std::uint16_t>(word(base+8,2));l.attributes=word(base+12);
        l.requested_frame=real(0x10);l.offset=real(0x14);l.interpolation=real(0x40);l.remaining_interpolation=real(0x44);l.weight=real(0x48);l.frame=real(0x4c);l.previous_frame=real(0x50);l.end_frame=real(0x54);l.field58=real(0x58);l.speed=real(0x5c);l.field60=real(0x60);
        layer.animation.layer.interpolation_curve_bits={word(base+0x6c),word(base+0x70)};
        for(unsigned g=0;g<4;++g) {
            const auto p=word(base+0xf8+4*g);
            if(p==0)l.bindings[g]=std::nullopt;
            else if(p==0x2600000+72*g)l.bindings[g]=r::Binding{2,11,19,g};
            else if(p==0x2400000+72*g)l.bindings[g]=r::Binding{1,(selector>>8)&15U,selector&255U,g};
            else throw std::runtime_error("oracle pointer identity");
            l.masks[g]=word(base+0x108+4*g);
        }
    }
    for(unsigned i=0;i<7;++i)state.fieldsd48[i]=word(608+4*i);
    state.fieldd68=word(636);for(unsigned i=0;i<4;++i)state.fieldsd70[i]=word(640+4*i);
    return state;
}
std::size_t oracle(const char* path) {
    Reader in(path);const auto count=in.header();
    for(unsigned i=0;i<count;++i) {
        c::Command command;command.entry=static_cast<c::Entry>(in.word());command.mode=in.word();command.layer=in.word();command.selector=in.word();command.flags=in.word();command.interpolation=std::bit_cast<float>(in.word());command.start_frame=std::bit_cast<float>(in.word());command.speed=std::bit_cast<float>(in.word());
        const auto count234=in.word(),skeleton=in.word(),kind=in.word(),slot_count=in.word(),frames=in.word(),loop=in.word(),reserved=in.word();
        check(kind<=4&&slot_count<=65535&&reserved==0,"oracle resource bounds");
        c::Globals globals;for(auto& t:globals.fields6c_tables){const auto x=in.word(),y=in.word();t=std::array<std::uint32_t,2>{x,y};}
        const auto x=in.word(),y=in.word(),z=in.word();globals.vector10e6370=std::array<std::uint32_t,3>{x,y,z};
        const auto previous=decode(in.state(),count234,command.selector);
        std::array<std::vector<mh4u::motion_events::Record>,4> records;r::Action action{frames,std::bit_cast<std::int32_t>(loop),true,{}};
        for(unsigned g=0;g<4;++g){const auto n=in.word();check(n<=64,"oracle event count");for(unsigned j=0;j<n;++j){const auto mask=in.word(),duration=in.word();records[g].push_back({mask,duration});}action.groups[g]=records[g];}
        const auto expected=decode(in.state(),count234,command.selector);const auto n=in.word();check(n<=2,"oracle request count");
        std::array<c::ObservedRequest,2> expected_requests{};
        for(unsigned j=0;j<n;++j){auto& e=expected_requests[j];e.request.layer=in.word();e.request.selector=in.word();e.request.attributes=in.word();e.request.interpolation=std::bit_cast<float>(in.word());e.request.start_frame=std::bit_cast<float>(in.word());e.request.speed=std::bit_cast<float>(in.word());e.command_flags=in.word();e.accepted=in.word()!=0;}
        std::vector<std::optional<r::Action>> slots(slot_count);if(kind==4&&(command.selector&255U)<slot_count)slots[command.selector&255U]=action;
        r::Model model;model.skeleton_present=skeleton!=0;model.banks[(command.selector>>8)&15U]=kind==0?r::Bank{r::Absent{}}:kind==1?r::Bank{r::Resource{1,std::nullopt}}:r::Bank{r::Resource{1,std::span<const std::optional<r::Action>>(slots)}};
        try {
            const auto output=accepted(apply(previous,model,command,globals));equal(output.state,expected);check(output.request_count==n,"request count");
            for(unsigned j=0;j<n;++j) {
                const auto& a=output.requests[j];const auto& b=expected_requests[j];
                check(a.request.layer==b.request.layer&&a.request.selector==b.request.selector&&a.request.attributes==b.request.attributes&&a.command_flags==b.command_flags&&a.accepted==b.accepted,"observed request integers");
                check(bits(a.request.interpolation)==bits(b.request.interpolation)&&bits(a.request.start_frame)==bits(b.request.start_frame)&&bits(a.request.speed)==bits(b.request.speed),"observed request float bits");
            }
        }catch(const std::exception& e){throw std::runtime_error("ARM case "+std::to_string(i)+": "+e.what());}
    }
    in.finish();return count;
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=1&&argc!=3)throw std::runtime_error("usage: motion-command-tests [--oracle PATH]");
        semantics();live_boundaries();std::size_t count=0;
        if(argc==3){if(std::string_view(argv[1])!="--oracle")throw std::runtime_error("unknown argument");count=oracle(argv[2]);}
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_cases\":"<<count<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
