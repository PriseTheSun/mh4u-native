#include "player_reference.hpp"
#include "player_input.hpp"
#include <array>
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
namespace ref=mh4u::player_reference;
namespace input=mh4u::player_input;
namespace {
std::size_t checks=0;
void check(bool ok,const char* what) { ++checks;if(!ok)throw std::runtime_error(what); }
std::uint32_t checked_heading(const ref::State& state) {
    const auto r=ref::heading(state);check(std::holds_alternative<std::uint32_t>(r),"heading unexpectedly rejected");return std::get<std::uint32_t>(r);
}
bool sector(const input::Channels& channels,std::uint32_t id,const input::SectorScope& scope,const input::ReferenceContext& context) {
    const auto r=input::in_sector(channels,id,scope,context);check(std::holds_alternative<bool>(r),"sector unexpectedly rejected");return std::get<bool>(r);
}
struct Fixture {
    std::vector<unsigned char> data;
    std::size_t pos=8;
    std::uint32_t word() {
        if(data.size()-pos<4)throw std::runtime_error("oracle truncated");
        std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(data[pos++])<<(8*i);return n;
    }
    ref::State state() {
        const auto flags=word(),mode=word(),stored=word(),present=word();
        check(mode<=255&&present<=1,"oracle state domain");
        const ref::Points p{std::bit_cast<float>(word()),std::bit_cast<float>(word()),std::bit_cast<float>(word()),std::bit_cast<float>(word())};
        return {flags,static_cast<std::uint8_t>(mode),stored,present?std::optional<ref::Points>(p):std::nullopt};
    }
};
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
    void set(std::uint64_t control) {
#if defined(__aarch64__)
        __asm__ volatile("msr fpcr, %0\n\tisb"::"r"(control):"memory");
#else
        _mm_setcsr(static_cast<unsigned>(control));
#endif
    }
    ~FloatingControl() {set(original);}
};
#endif
}
int main(int argc,char** argv) try {
    check(argc<=2,"usage: player-reference-tests [oracle.bin]");
    const auto nan=std::numeric_limits<float>::quiet_NaN();
    const auto inf=std::numeric_limits<float>::infinity();
    // External-reference absence is independent of unknown stored fields. The
    // projection resolves only the path heading() can consume, not all facts.
    const auto absent=ref::resolve_state(0,std::nullopt,std::nullopt,ref::Absent{});
    check(std::holds_alternative<ref::State>(absent),"explicitly absent reference rejected");
    check(checked_heading(std::get<ref::State>(absent))==0,"absent reference invented heading");
    const auto external_unknown=ref::resolve_state(0,std::nullopt,std::nullopt,ref::Unknown{});
    check(std::get<ref::Error>(external_unknown).code==ref::ErrorCode::unknown_field&&
          std::get<ref::Error>(external_unknown).message=="manager+1c","unknown reference became absent");
    const auto unknown_mode=ref::resolve_state(0x4000,std::nullopt,std::nullopt,ref::Unknown{});
    check(std::holds_alternative<ref::State>(unknown_mode),"projection eagerly consumed manager before mode");
    const auto unknown_mode_error=ref::orient(std::get<ref::State>(unknown_mode),0);
    check(std::get<ref::Error>(unknown_mode_error).message=="pw+150","mode failure lost precedence/cause");
    const auto unknown_heading=ref::resolve_state(0x4000,1,std::nullopt,ref::Unknown{});
    check(std::holds_alternative<ref::State>(unknown_heading),"stored heading branch read external reference");
    check(std::get<ref::Error>(ref::orient(std::get<ref::State>(unknown_heading),0)).message=="pw+14c","heading failure lost cause");
    const auto stored=ref::resolve_state(0x4000,1,0xfffffff0U,ref::Unknown{});
    check(std::get<std::uint32_t>(ref::orient(std::get<ref::State>(stored),0x4030))==0x20,"orientation unsigned wrap/low16 word differs");
    check(std::get<std::uint32_t>(ref::orient(std::get<ref::State>(absent),0))==0xc000,"null-reference orientation underflow differs");
    check(std::holds_alternative<ref::Error>(ref::resolve_state(0x4000,2,0x1234,ref::Unknown{})),"non1 mode incorrectly selected stored heading");
    const auto vector=ref::resolve_state(0,1,0xdeadbeef,ref::Points{0,0,1,0});
    check(std::get<std::uint32_t>(ref::orient(std::get<ref::State>(vector),0x1234))==0x1234,"vector orientation used unconsumed stored heading");
    const auto bad_vector=ref::resolve_state(0,0,0,ref::Points{nan,0,0,0});
    check(std::holds_alternative<ref::State>(bad_vector),"projection eagerly evaluated points");
    const auto bad_heading_result=ref::heading(std::get<ref::State>(bad_vector));
    const auto bad_orientation_result=ref::orient(std::get<ref::State>(bad_vector),0);
    check(std::get<ref::Error>(bad_orientation_result).code==std::get<ref::Error>(bad_heading_result).code&&
          std::get<ref::Error>(bad_orientation_result).message==std::get<ref::Error>(bad_heading_result).message,"orientation replaced heading failure");
    ref::State s{0,0,0,ref::Points{0,0,0,0}};
    const ref::State ignored{0,std::nullopt,std::nullopt,std::nullopt};
    check(checked_heading(ignored)==0,"ignored unknown stored fields must stay lazy");
    auto unknown=ignored;unknown.flags=0x4000;
    check(std::get<ref::Error>(ref::heading(unknown)).code==ref::ErrorCode::unknown_field,"consumed unknown mode accepted");
    unknown.stored_mode=1;
    check(std::get<ref::Error>(ref::heading(unknown)).code==ref::ErrorCode::unknown_field,"consumed unknown heading accepted");
    unknown.stored_mode=0;check(checked_heading(unknown)==0,"unused heading must remain unknown");
    for(const auto& point:std::array<std::array<float,3>,9>{{
            {0,0,0},{0,1,0},{1,1,8192},{1,0,16384},{1,-1,24576},
            {0,-1,32768},{-1,-1,40960},{-1,0,49152},{-1,1,57344}}}) {
        s.points=ref::Points{0,0,point[0],point[1]};check(checked_heading(s)==static_cast<std::uint32_t>(point[2]),"cardinal/octant heading");
    }
    s.points=ref::Points{nan,0,0,0};auto error=ref::heading(s);
    check(std::get<ref::Error>(error).code==ref::ErrorCode::nonfinite_input,"NaN accepted");
    s.points=ref::Points{0,0,inf,0};error=ref::heading(s);
    check(std::get<ref::Error>(error).code==ref::ErrorCode::nonfinite_input,"infinity accepted");
    const auto max=std::numeric_limits<float>::max();s.points=ref::Points{-max,0,max,0};error=ref::heading(s);
    check(std::get<ref::Error>(error).code==ref::ErrorCode::nonfinite_difference,"overflow accepted");
    s={0x4000,1,0x89abcdefU,ref::Points{nan,nan,nan,nan}};
    check(checked_heading(s)==0x89abcdefU,"stored heading truncated or ignored points read");
    const int prior=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"rounding setup");
    check(checked_heading(s)==0x89abcdefU,"stored branch validated floating environment");
    check(std::get<std::uint32_t>(ref::orient(s,0xffff))==0x8dee,"stored orientation validated floating environment or truncated at wrong point");
    s.flags=0;s.points.reset();check(checked_heading(s)==0,"null source validated floating environment");
    s.points=ref::Points{0,0,1,1};error=ref::heading(s);
    check(std::get<ref::Error>(error).code==ref::ErrorCode::unsupported_floating_environment,"nondefault rounding accepted");
    check(std::fesetround(prior)==0,"rounding restoration");
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
    {
        FloatingControl control;
#if defined(__aarch64__)
        constexpr std::array<std::uint64_t,1> masks{1ULL<<24};
#else
        constexpr std::array<std::uint64_t,3> masks{1ULL<<15,1ULL<<6,(1ULL<<15)|(1ULL<<6)};
#endif
        const float tiny=std::bit_cast<float>(std::uint32_t{1});
        for(auto mask:masks) {
            control.set(control.original|mask);
            s={0,0,0,ref::Points{0,0,tiny,tiny}};error=ref::heading(s);
            check(std::get<ref::Error>(error).code==ref::ErrorCode::unsupported_floating_environment,"flush-to-zero environment accepted");
            s.flags=0x4000;s.stored_mode=1;s.stored_heading=0x89abcdef;
            check(checked_heading(s)==s.stored_heading,"stored branch touched subnormal probe");
            s.flags=0;s.points.reset();check(checked_heading(s)==0,"null branch touched subnormal probe");
        }
    }
#endif
    input::ReferenceContext context{123,s};context.source.points=ref::Points{nan,0,0,0};
    const input::Channels channels{{0,0},{0,0}};
    check(!sector(channels,0xffffffffU,{1,255},context),"unknown ID did not return first");
    auto result=input::in_sector(channels,0x400,{1,255},context);
    check(std::get<input::Error>(result).code==input::ErrorCode::unsupported_mode,"mode gate order");
    result=input::in_sector(channels,0x400,{0,1},context);
    const auto& nested=std::get<input::Error>(result);
    check(nested.code==input::ErrorCode::reference_failure&&nested.reference_reason==ref::ErrorCode::nonfinite_input,"reference must resolve before low magnitude and preserve reason");
    check(!sector(channels,0x400,{0,0},context),"disabled reference read ignored context");
    context.source.flags=0x4000;
    check(!sector(channels,0x400,{0,1},context),"flag suppressed reference read ignored context");
    std::uint32_t references=0,sectors=0;
    if(argc==2) {
        std::ifstream stream(argv[1],std::ios::binary|std::ios::ate);check(bool(stream),"oracle open");
        const auto size=stream.tellg();check(size>=16&&size<=16*1024*1024,"oracle size");stream.seekg(0);
        Fixture f;f.data.resize(static_cast<std::size_t>(size));stream.read(reinterpret_cast<char*>(f.data.data()),size);check(bool(stream),"oracle read");
        const std::array<unsigned char,8> magic{'M','H','4','U','R','E','F','1'};
        check(std::equal(magic.begin(),magic.end(),f.data.begin()),"oracle magic");
        references=f.word();sectors=f.word();check(references<=100000&&sectors<=100000,"oracle bounds");
        check(f.data.size()==16+std::size_t(references)*36+std::size_t(sectors)*68,"oracle counts");
        for(std::uint32_t i=0;i<references;++i) {
            const auto state=f.state();const auto expected=f.word();
            check(checked_heading(state)==expected,"ARM reference mismatch");
        }
        for(std::uint32_t i=0;i<sectors;++i) {
            const auto state=f.state();const auto mode=f.word(),id=f.word(),reference=f.word(),orientation=f.word();
            const auto a0=f.word(),a1=f.word(),m0=f.word(),m1=f.word(),expected=f.word();
            check(reference<=255&&a0<=65535&&a1<=65535&&m0<=65535&&m1<=65535&&expected<=1,"oracle sector domain");
            const input::Channels c{{static_cast<std::uint16_t>(a0),static_cast<std::uint16_t>(a1)},{static_cast<std::uint16_t>(m0),static_cast<std::uint16_t>(m1)}};
            check(sector(c,id,{mode,static_cast<std::uint8_t>(reference)},{orientation,state})==bool(expected),"ARM composed sector mismatch");
        }
        check(f.pos==f.data.size(),"oracle trailing data");
    }
    std::cout<<"{\"checks\":"<<checks<<",\"reference_arm_rows\":"<<references<<",\"sector_arm_rows\":"<<sectors<<",\"mismatches\":0}\n";
    return 0;
} catch(const std::exception& e) { std::cerr<<e.what()<<'\n';return 1; }
