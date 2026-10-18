#include "player_state1_dynamics.hpp"
#include "player_controller_step.hpp"
#include <bit>
#include <cfenv>
#include <iostream>
#include <stdexcept>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace {
namespace d=mh4u::player_state1_dynamics;
namespace ps=mh4u::player_selection;
using Player=mh4u::player_controller_step::PlayerFields;
using Fields=mh4u::player_fields::Fields;
std::size_t checks{};
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
template<class T,class... U>T value(std::variant<U...> v){check(std::holds_alternative<T>(v),"Result variant");return std::get<T>(std::move(v));}
Player player(){Player p;for(auto& id:p.selection_pbw.fields2fc)id=0;p.selection_pbw.field358=bits(600);return p;}
Fields fields(){Fields f;f.pw.field1f8=0;f.pw.field220_bits=0;f.pw.field838_bits=0;f.pbw.field354_bits=bits(200);f.pbw.field10_bits=bits(1);f.pbw.field14_bits=bits(2);f.pbw.field18_bits=bits(3);f.pw.field7f0=199;f.pw.field14c=0x7fc01234;return f;}
void error(d::Result r,d::ErrorCode code,const char* field){const auto e=value<d::Error>(std::move(r));check(e.code==code&&e.field==field,"Typed field/cause");}
void membership(){
    ps::PbwFields p;const auto unknown=value<ps::Error>(ps::contains_id(p,111));
    check(unknown.code==ps::ErrorCode::unknown_selection_context,"Unknown is not absent");
    p.fields2fc[0]=111;check(value<bool>(ps::contains_id(p,111)),"First equality ignores unknown suffix");
    p.fields2fc[0]=112;check(std::holds_alternative<ps::Error>(ps::contains_id(p,111)),"Earlier unknown prevents later match");
    for(auto& id:p.fields2fc)id=0;
    check(!value<bool>(ps::contains_id(p,111)),"All20 known absent");
    p.fields2fc[19]=65535;check(value<bool>(ps::contains_id(p,65535)),"Full halfword and final slot");
}
void cost(){
    auto p=player();auto f=fields();const auto savedp=p;const auto savedf=f;
    const auto result=value<d::Output>(d::cost(p,f,bits(1),0));auto expected=f;expected.pbw.field354_bits=bits(199.5F);
    check(result.updated&&result.fields==expected,"Basic subtraction stores only354");
    check(p==savedp&&f==savedf,"Cost inputs immutable");
    p.global.field2b=2;p.pbw.field30=3;
    auto skipped=value<d::Output>(d::cost(p,{},std::nullopt,std::nullopt));check(!skipped.updated&&skipped.fields==Fields{},"Closed local gate is lazy");
    p=player();error(d::cost(p,{},bits(1),0),d::ErrorCode::unknown_field,"pw+1f8");
    p.selection_pbw.fields2fc[0].reset();const auto lookup=value<d::Error>(d::cost(p,f,bits(1),0));
    check(lookup.selection&&lookup.selection->code==ps::ErrorCode::unknown_selection_context,"Selection cause retained");
    p=player();auto unknown=f;unknown.pw.field220_bits.reset();unknown.pw.field838_bits.reset();unknown.pbw.field354_bits.reset();p.selection_pbw.field358.reset();
    skipped=value<d::Output>(d::cost(p,unknown,std::nullopt,100));check(!skipped.updated&&skipped.fields==unknown,"Context100 skips later operands");
    error(d::cost(p,unknown,std::nullopt,std::nullopt),d::ErrorCode::unknown_field,"context+ad50");
    unknown.pw.field220_bits=bits(1);skipped=value<d::Output>(d::cost(p,unknown,std::nullopt,0));check(!skipped.updated&&skipped.fields==unknown,"220 gate ignores838/delta/cap");
    unknown.pw.field220_bits=0;unknown.pw.field838_bits=bits(1);skipped=value<d::Output>(d::cost(p,unknown,std::nullopt,0));check(!skipped.updated,"838 gate ignores delta/cap");
    p=player();f=fields();f.pw.field1f8=0xc0;
    check(value<d::Output>(d::cost(p,f,bits(1),0)).fields.pbw.field354_bits==bits(198.5F),"80 takes priority over40");
    f.pw.field1f8=0x40;check(value<d::Output>(d::cost(p,f,bits(1),0)).fields.pbw.field354_bits==bits(199),"40 multiplier2");
    p.selection_pbw.fields2fc[0]=111;p.selection_pbw.fields2fc[1]=272;f.pw.field1f8=0;
    check(value<d::Output>(d::cost(p,f,bits(1),0)).fields.pbw.field354_bits==bits(199.75F),"111/272 multiplier once");
    p.selection_pbw.fields2fc[2]=112;
    check(value<d::Output>(d::cost(p,f,bits(1),0)).fields.pbw.field354_bits==bits(199.7F),"112 source literal");
    p=player();p.pw.field1f4=0x10000000;p.selection_pbw.fields2fc[0]=37;p.selection_pbw.fields2fc[1]=36;
    check(value<d::Output>(d::cost(p,f,bits(1),0)).fields.pbw.field354_bits==bits(199.75F),"37 takes priority over36");
    p.selection_pbw.fields2fc[0]=0;check(value<d::Output>(d::cost(p,f,bits(1),0)).fields.pbw.field354_bits==bits(199.875F),"36 multiplier0.25");
    p=player();const auto zero=value<d::Output>(d::cost(p,f,0,0));check(zero.updated&&zero.fields==f,"Zero delta still executes store");
    f.pbw.field354_bits=0x80000000;check(value<d::Output>(d::cost(p,f,0,0)).fields.pbw.field354_bits==0,"First clamp writes positive zero");
    p.selection_pbw.field358=0x80000000;check(value<d::Output>(d::cost(p,f,0,0)).fields.pbw.field354_bits==0x80000000,"Final cap preserves negative zero");
    p.selection_pbw.field358=bits(-3);check(value<d::Output>(d::cost(p,f,0,0)).fields.pbw.field354_bits==bits(-3),"Literal clamp order accepts negative cap");
    p=player();f=fields();error(d::cost(p,f,std::nullopt,0),d::ErrorCode::unknown_field,"player+14");
    error(d::cost(p,f,0x7fc00001,0),d::ErrorCode::nonfinite_operand,"player+14");
    f.pw.field220_bits=0x7f800000;error(d::cost(p,f,bits(1),0),d::ErrorCode::nonfinite_operand,"pw+220");
    f=fields();f.pw.field1f8=0x80;error(d::cost(p,f,0x7f7fffff,0),d::ErrorCode::numeric_overflow,"b2a58c delta product");
    f=fields();f.pbw.field354_bits=0xff7fffff;error(d::cost(p,f,0x7f7fffff,0),d::ErrorCode::numeric_overflow,"b2a58c sum");
    f=fields();p.selection_pbw.field358.reset();error(d::cost(p,f,bits(1),0),d::ErrorCode::unknown_field,"pbw+358");
}
void advance(){
    auto p=player();auto f=fields();const auto savedp=p;const auto savedf=f;
    // Table row0 is exactly(0,1); these expectations are direct source values,
    // not a host trigonometric approximation or a second table interpreter.
    auto expected=f;expected.pbw.field18_bits=bits(5.5F);const auto out=value<d::Output>(d::advance(p,f,bits(1)));
    check(out.updated&&out.fields==expected,"Zero angles source translation");check(p==savedp&&f==savedf,"Advance inputs immutable");
    p.pbw.field20=0x12340000;p.pbw.field24=0xabcd0000;p.pbw.field28=0xffff0000;
    check(value<d::Output>(d::advance(p,f,bits(1))).fields==out.fields,"Angles consume only low16");
    check(value<d::Output>(d::advance(p,f,0)).fields==f,"Delta0 preserves finite positive positions");
    f.pbw.field14_bits=0x80000000;check(value<d::Output>(d::advance(p,f,0)).fields.pbw.field14_bits==0,"Forced zero Y is still added");
    f=fields();error(d::advance(p,f,std::nullopt),d::ErrorCode::unknown_field,"player+14");
    error(d::advance(p,f,0x7f800000),d::ErrorCode::nonfinite_operand,"player+14");
    error(d::advance(p,f,0x7f7fffff),d::ErrorCode::numeric_overflow,"b2a318 delta z");
    for(unsigned i=0;i<3;++i){f=fields();auto* v=i==0?&f.pbw.field10_bits:i==1?&f.pbw.field14_bits:&f.pbw.field18_bits;v->reset();error(d::advance(p,f,0),d::ErrorCode::unknown_field,i==0?"pbw+10":i==1?"pbw+14":"pbw+18");}
    f=fields();f.pbw.field18_bits=0x7f7fffff;const auto before=f;
    error(d::advance(p,f,bits(1e38F)),d::ErrorCode::numeric_overflow,"b2a318 position z");check(f==before,"Late failure leaves all fields untouched");
    f=fields();f.pbw.field14_bits=0x7fc00001;error(d::advance(p,f,0),d::ErrorCode::nonfinite_operand,"pbw+14");
}
void environment(){
    auto p=player();const auto f=fields();const auto old=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"Set rounding");
    error(d::advance(p,f,bits(1)),d::ErrorCode::unsupported_floating_environment,"binary32 rounding");
    p.global.field2b=2;p.pbw.field30=3;check(std::holds_alternative<d::Output>(d::cost(p,{},std::nullopt,std::nullopt)),"Skipped cost ignores FP environment");
    check(std::fesetround(old)==0,"Restore rounding");
#if defined(__aarch64__)
    std::uint64_t saved;asm volatile("mrs %0, fpcr":"=r"(saved));const auto flushed=saved|(1ULL<<24);asm volatile("msr fpcr, %0"::"r"(flushed));
    const auto result=d::advance(player(),f,bits(1));asm volatile("msr fpcr, %0"::"r"(saved));
    error(result,d::ErrorCode::unsupported_floating_environment,"binary32 gradual underflow");
#elif defined(__SSE__)
    const auto saved=_mm_getcsr();_mm_setcsr(saved|0x8040);const auto result=d::advance(player(),f,bits(1));_mm_setcsr(saved);
    error(result,d::ErrorCode::unsupported_floating_environment,"binary32 gradual underflow");
#endif
}
}
int main(){try{membership();cost();advance();environment();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_executions\":0}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
