#include "player_position_timer.hpp"
#include "picosha2.h"
#include <array>
#include <cfenv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
namespace {
namespace pt=mh4u::player_position_timer;
namespace pf=mh4u::player_fields;
using U=std::uint32_t;
std::size_t checks{},original_cases{};
void check(bool value,std::string_view why){++checks;if(!value)throw std::runtime_error(std::string(why));}
struct Reader {
    std::string bytes;std::size_t at{};
    std::string_view take(std::size_t n){if(n>bytes.size()-at)throw std::runtime_error("Truncated fixture");auto v=std::string_view(bytes).substr(at,n);at+=n;return v;}
    U word(){U value{};auto v=take(4);for(unsigned i=0;i<4;++i)value|=U(static_cast<unsigned char>(v[i]))<<(8*i);return value;}
};
std::string read(const std::filesystem::path& path){
    std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open fixture");
    const auto n=f.tellg();if(n<0||static_cast<std::uintmax_t>(n)>2*1024*1024)throw std::runtime_error("Fixture capacity");
    std::string bytes(static_cast<std::size_t>(n),'\0');f.seekg(0);
    if(!f.read(bytes.data(),static_cast<std::streamsize>(bytes.size()))||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture changed while reading");
    return bytes;
}
pf::Fields fields(const std::array<U,12>& v){
    pf::Fields f{};f.pw.fieldb8_bits=v[0];f.pw.field90_bits=v[2];f.pw.field94_bits=v[3];f.pw.field98_bits=v[4];
    f.pw.fielda0_bits=v[5];f.pw.fielda4_bits=v[6];f.pw.fielda8_bits=v[7];
    f.pbw.field10_bits=v[8];f.pbw.field14_bits=v[9];f.pbw.field18_bits=v[10];f.pbw.field1c_bits=v[11];
    f.pbw.field6e8_bits=0x7fa54321;f.pbw.field54_bits=0xffc00017;f.pbw.field6ec_bits={};
    f.pbw.words3d8[3]={0x17,{},0x99,{}};f.pbw.descriptors[6][17]=0xa3;f.pbw.words9404[255]=0xabcdef01;
    f.pw.field339=0x91;f.pw.fieldc0_bits=0x7f800003;return f;
}
pf::Fields initial(){return fields({0x40800000,0x3f800000,0x7fa12345,0xff800000,0x80000000,0x40000000,0xc0400000,0x3f000000,0x41200000,0xc1a00000,0x41f00000,0x7fa54321});}
const pt::Output& output(const pt::Result& r){check(std::holds_alternative<pt::Output>(r),"Expected successful tail");return std::get<pt::Output>(r);}
void error(const pt::Result& r,pt::ErrorCode code,pt::Phase phase,std::string_view field){
    check(std::holds_alternative<pt::Error>(r),"Expected typed error");const auto& e=std::get<pt::Error>(r);
    check(e.code==code&&e.phase==phase&&e.field==field,"Error cause/phase/field");
}
void oracle(const std::filesystem::path& path,std::string_view sha){
    Reader r{read(path)};const auto bytes_before=r.bytes;
    check(sha.size()==64&&picosha2::hash256_hex_string(r.bytes)==sha,"Fixture SHA mismatch");
    check(r.take(8)=="MH4UPT01","Fixture magic");const auto elf=r.take(32);
    const auto* elf_bytes=reinterpret_cast<const unsigned char*>(elf.data());
    check(picosha2::bytes_to_hex_string(elf_bytes,elf_bytes+elf.size())=="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","Source identity");
    const auto count=r.word();check(count==4100,"Fixture coverage");std::array<std::size_t,3> totals{};
    for(U i=0;i<count;++i){
        const auto len=r.word();check(len>0&&len<=96,"Fixture name extent");const std::string name(r.take(len));const auto op=r.word();check(op<3,"Fixture operation");++totals[op];
        std::array<U,12> v{};for(auto& x:v)x=r.word();std::array<U,5> expected{};for(auto& x:expected)x=r.word();
        const auto returned=r.word(),flags=r.word(),stores=r.word();check(flags<=7&&stores<=6,"Fixture effects extent");
        for(U j=0;j<stores;++j){const auto arena=r.word(),offset=r.word();(void)r.word();check((arena==1&&offset==0xb8)||(arena==2&&op==2&&offset>=0x10&&offset<=0x1c&&offset%4==0),"Fixture write domain");}
        const auto input=fields(v),before=input;
        if(op==0){const auto result=pt::increment(v[0],v[1]);check(std::holds_alternative<U>(result)&&std::get<U>(result)==expected[0],name);check(flags==0&&returned==0,"Increment metadata");}
        else if(op==1){const auto result=pt::decrement(v[0],v[1]);check(std::holds_alternative<pt::TimerOutput>(result),name);const auto& o=std::get<pt::TimerOutput>(result);check(o.value_bits==expected[0]&&o.returned==returned,name);check(flags==0,"Decrement metadata");}
        else {
            const auto result=pt::apply(input,v[1]);const auto& o=output(result);
            auto after=input;after.pw.fieldb8_bits=expected[0];after.pbw.field10_bits=expected[1];after.pbw.field14_bits=expected[2];after.pbw.field18_bits=expected[3];after.pbw.field1c_bits=expected[4];
            check(o.fields==after,name);check(o.effects.timer_called==bool(flags&1)&&o.effects.position_integrated==bool(flags&2)&&o.effects.position_restored==bool(flags&4),name);
            check(o.effects.timer_returned.has_value()==bool(flags&1),"Timer call optional return");
            if(o.effects.timer_returned)check(*o.effects.timer_returned==((flags&4)?1U:0U),"Timer return follows original path");
        }
        check(input==before,"Caller fields mutated");++original_cases;
    }
    check(totals==std::array<std::size_t,3>{1024,1024,2052},"Operation coverage");check(r.at==r.bytes.size(),"Trailing fixture bytes");check(r.bytes==bytes_before,"Fixture buffer mutated");
}
void boundaries(){
    auto f=initial();auto before=f;
    for(U raw:{0U,0x80000000U,0xbf800000U,0xff800000U}){
        pf::Fields minimal{};minimal.pw.fieldb8_bits=raw;auto r=pt::apply(minimal,{});const auto& o=output(r);
        check(o.fields==minimal&&!o.effects.timer_called&&!o.effects.position_integrated&&!o.effects.position_restored,"Inactive tail consumed or changed state");
    }
    for(U raw:{0U,0x80000000U,0xbf800000U,0xff800000U,0x7fa12345U,0xffc98765U}){
        const auto r=pt::decrement(raw,{});check(std::holds_alternative<pt::TimerOutput>(r),"Clamp consumed dead delta");
        const auto& o=std::get<pt::TimerOutput>(r);check(o.value_bits==0&&o.returned==1,"Clamp return/rawzero");
    }
    f.pw.fieldb8_bits=0x7fa12345;f.pbw.field10_bits={};f.pbw.field14_bits={};f.pbw.field18_bits={};f.pw.fielda0_bits={};f.pw.fielda4_bits={};f.pw.fielda8_bits={};
    auto r=pt::apply(f,{});const auto& restored=output(r);
    check(restored.fields.pbw.field10_bits==0x7fa12345&&restored.fields.pbw.field14_bits==0xff800000&&restored.fields.pbw.field18_bits==0x80000000&&restored.fields.pbw.field1c_bits==0,"Raw restore repaired float bits");
    check(restored.effects.position_restored&&restored.fields.pw.fieldb8_bits==0,"Initial NaN route");
    error(pt::apply(pf::Fields{},{}),pt::ErrorCode::unknown_field,pt::Phase::entry,"PWb8");
    for(int i=0;i<3;++i){auto broken=f;if(i==0)broken.pw.field90_bits={};if(i==1)broken.pw.field94_bits={};if(i==2)broken.pw.field98_bits={};const auto saved=broken;error(pt::apply(broken,{}),pt::ErrorCode::unknown_field,pt::Phase::restore,std::array<std::string_view,3>{"PW90","PW94","PW98"}[i]);check(broken==saved,"Restore failure published prefix");}
    f=initial();error(pt::apply(f,{}),pt::ErrorCode::unknown_field,pt::Phase::decrement,"player14");
    const std::array<std::optional<U> pf::PbwFields::*,3> positions{&pf::PbwFields::field10_bits,&pf::PbwFields::field14_bits,&pf::PbwFields::field18_bits};
    const std::array<std::optional<U> pf::PwFields::*,3> rates{&pf::PwFields::fielda0_bits,&pf::PwFields::fielda4_bits,&pf::PwFields::fielda8_bits};
    const std::array<pt::Phase,3> muls{pt::Phase::x_multiply,pt::Phase::y_multiply,pt::Phase::z_multiply},adds{pt::Phase::x_add,pt::Phase::y_add,pt::Phase::z_add};
    for(unsigned i=0;i<3;++i){auto broken=f;broken.pw.*rates[i]={};const auto saved=broken;error(pt::apply(broken,0x3f800000),pt::ErrorCode::unknown_field,muls[i],std::array<std::string_view,3>{"PWa0","PWa4","PWa8"}[i]);check(broken==saved,"Late multiply failure published prefix");broken=f;broken.pbw.*positions[i]={};const auto saved2=broken;error(pt::apply(broken,0x3f800000),pt::ErrorCode::unknown_field,adds[i],std::array<std::string_view,3>{"PBW10","PBW14","PBW18"}[i]);check(broken==saved2,"Late add failure published prefix");}
    check(std::holds_alternative<pt::Error>(pt::increment({},0))&&std::holds_alternative<pt::Error>(pt::increment(0,{}))&&std::holds_alternative<pt::Error>(pt::decrement({},0)),"Leaf unknown input accepted");
    struct Rounding {int saved=std::fegetround();~Rounding(){std::fesetround(saved);}} rounding;
    check(std::fesetround(FE_DOWNWARD)==0,"Set non-RNE");
    error(pt::apply(f,0x3f800000),pt::ErrorCode::arithmetic,pt::Phase::decrement,"timer-player14");
    check(std::holds_alternative<pt::Error>(pt::increment(0,0)),"Increment ignored rounding environment");
    auto raw=f;raw.pw.fieldb8_bits=0xff800000;check(output(pt::apply(raw,{})).fields==raw,"Raw skip imposed arithmetic environment");
    raw.pw.fieldb8_bits=0x7fa12345;check(output(pt::apply(raw,{})).effects.position_restored,"Raw restore imposed arithmetic environment");
    check(std::fesetround(rounding.saved)==0,"Restore rounding");
#if defined(__aarch64__)
    struct Control {std::uint64_t old{};Control(){asm volatile("mrs %0, fpcr":"=r"(old));}~Control(){asm volatile("msr fpcr, %0"::"r"(old));}} control;
    const auto changed=control.old|(std::uint64_t{1}<<24);asm volatile("msr fpcr, %0"::"r"(changed));
    check(std::holds_alternative<pt::Error>(pt::increment(1,1)),"FZ increment accepted");error(pt::apply(f,1),pt::ErrorCode::arithmetic,pt::Phase::decrement,"timer-player14");
#elif defined(__x86_64__) || defined(_M_X64)
    struct Control {unsigned old=_mm_getcsr();~Control(){_mm_setcsr(old);}} control;_mm_setcsr(control.old|(1U<<15)|(1U<<6));
    check(std::holds_alternative<pt::Error>(pt::increment(1,1)),"FTZ/DAZ increment accepted");error(pt::apply(f,1),pt::ErrorCode::arithmetic,pt::Phase::decrement,"timer-player14");
#endif
    check(f==before,"Boundaries mutated caller input");
}
}
int main(int argc,char** argv){try{if(argc!=1&&argc!=3)throw std::runtime_error("Expected no arguments or fixture and SHA");if(argc==3)oracle(argv[1],argv[2]);boundaries();std::cout<<"{\"status\":\"passed\",\"original_cases\":"<<original_cases<<",\"checks\":"<<checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
