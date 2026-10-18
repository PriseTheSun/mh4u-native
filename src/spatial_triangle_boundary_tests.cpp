#include "spatial_triangle.hpp"
#include <bit>
#include <cfenv>
#include <iostream>
#include <stdexcept>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace {
namespace st=mh4u::spatial_triangle;
std::size_t checks{};
void check(bool condition,const char* what){++checks;if(!condition)throw std::runtime_error(what);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
st::Bits3 vector(float x,float y,float z){return {bits(x),bits(y),bits(z)};}
st::Triangle triangle(){return {{vector(0,0,0),vector(0,0,1),vector(1,0,0)}};}
st::Vertical ray(float x,float y,float z,float dy=-1){return {vector(x,y,z),bits(dy)};}
template<class T,class... U>T value(std::variant<U...> r){check(std::holds_alternative<T>(r),"Result variant");return std::get<T>(std::move(r));}
void error(st::Result<st::Output> r,st::ErrorCode code,const char* field){const auto e=value<st::Error>(r);check(e.code==code&&e.field==field,"Typed error");}
void geometry(){
    const auto t=triangle();const auto q=ray(.25F,2,.25F);const auto out=value<st::Output>(st::intersect(t,q));
    const st::Hit expected{{bits(.25F),0,bits(.25F),0},{0x80000000,0xbf800000,0x80000000,0},0,bits(2),1};
    check(out.intersects&&out.hit==expected,"Exact horizontal source record");
    check(t.vertices==triangle().vertices&&q.origin==vector(.25F,2,.25F)&&q.direction_y==bits(-1),"Inputs immutable");
    const auto none=value<st::Output>(st::intersect(t,q,false));check(none.intersects&&!none.hit,"Null output retains acceptance");
    const auto below=value<st::Output>(st::intersect(t,ray(.25F,-2,.25F,1)));
    check(below.intersects&&below.hit&&below.hit->normal==st::Bits4{0,0x3f800000,0,0},"Hit normal depends on signed distance");
    const auto away=value<st::Output>(st::intersect(t,ray(.25F,-2,.25F,-1)));check(!away.intersects&&!away.hit,"Negative t rejected");
    for(const auto& p:t.vertices){const auto edge=value<st::Output>(st::intersect(t,{p,bits(-1)}));check(edge.intersects&&edge.hit&&edge.hit->t_bits==0,"Vertex equality admitted");}
    check(value<st::Output>(st::intersect(t,ray(.5F,3,.5F))).intersects,"Edge equality admitted");
    for(const auto& q2:{ray(-.125F,1,.125F),ray(.75F,1,.75F),ray(.125F,1,-.125F)}){
        const auto miss=value<st::Output>(st::intersect(t,q2));check(!miss.intersects&&!miss.hit,"Oriented edge rejection");
    }
    auto reversed=t;std::swap(reversed.vertices[1],reversed.vertices[2]);
    check(!value<st::Output>(st::intersect(reversed,q)).intersects,"Leaf is not a two-sided generic raycast");
    auto zeros=ray(0,-0.0F,.25F);zeros.origin->at(0)=0x80000000;
    const auto signed_zero=value<st::Output>(st::intersect(t,zeros));
    check(signed_zero.hit&&signed_zero.hit->position[0]==0x80000000&&signed_zero.hit->position[1]==0x80000000,"Raw X and computed signed zero Y");
    const auto far=value<st::Output>(st::intersect(t,ray(.25F,1e20F,.25F)));
    check(far.intersects&&far.hit&&far.hit->t_bits==bits(1e20F),"No invented maximum t");
    const st::Triangle flat{{vector(0,0,0),vector(0,0,0),vector(0,0,0)}};
    check(!value<st::Output>(st::intersect(flat,{std::nullopt,bits(-1)})).intersects,"Zero cross product is a source miss");
    // Area/length=2^-24 is below the literal2^-23 normalization threshold.
    const st::Triangle small{{vector(0,0,0),vector(0,0,0x1p-12F),vector(0x1p-12F,0,0)}};
    const auto small_hit=value<st::Output>(st::intersect(small,ray(0x1p-14F,1,0x1p-14F,-1024)));
    check(small_hit.intersects&&small_hit.hit&&small_hit.hit->normal[1]==bits(-0x1p-24F)&&small_hit.hit->t_bits==bits(0x1p-10F),"Small cross is preserved without normalization");
    const st::Triangle threshold{{vector(0,0,0),vector(0,0,0x1p-11F),vector(0x1p-12F,0,0)}};
    const auto threshold_hit=value<st::Output>(st::intersect(threshold,ray(0x1p-14F,1,0x1p-14F)));
    check(threshold_hit.hit&&threshold_hit.hit->normal[1]==bits(-1),"Normalization includes exact threshold");
    auto near=ray(.25F,1,.25F);near.direction_y=0xb727c5ab;
    check(!value<st::Output>(st::intersect(t,near)).intersects,"Denominator immediately below source threshold");
    near.direction_y=0xb727c5ac;check(value<st::Output>(st::intersect(t,near)).intersects,"Denominator equality admitted");
}
void failures(){
    auto t=triangle();auto q=ray(.25F,2,.25F);
    error(st::intersect(t,{std::nullopt,std::nullopt}),st::ErrorCode::unknown_operand,"direction Y");
    error(st::intersect(t,{std::nullopt,bits(-1)}),st::ErrorCode::unknown_operand,"origin");
    for(const auto direction:{0U,0x80000000U,1U,0x80000001U}){
        const auto r=value<st::Output>(st::intersect(t,{std::nullopt,direction}));check(!r.intersects&&!r.hit,"Parallel rejects without origin");
    }
    auto invalid=q;invalid.origin=st::Bits3{0x7fc00001,0x7f800000,0xff800000};invalid.direction_y=0;
    check(!value<st::Output>(st::intersect(t,invalid)).intersects,"Unconsumed origin can contain raw nonfinite words");
    error(st::intersect(t,{std::nullopt,0x7f800000}),st::ErrorCode::nonfinite_operand,"plane direction product");
    invalid.direction_y=bits(-1);error(st::intersect(t,invalid),st::ErrorCode::nonfinite_operand,"distance X");
    error(st::intersect(t,invalid,false),st::ErrorCode::nonfinite_operand,"distance X");
    t.vertices[1][0]=0x7fc00001;error(st::intersect(t,q),st::ErrorCode::nonfinite_operand,"edge1 X");
    t=triangle();t.vertices[0][0]=0xff7fffff;t.vertices[1][0]=0x7f7fffff;
    error(st::intersect(t,q),st::ErrorCode::numeric_overflow,"edge1 X");
    t=triangle();t.vertices[1][2]=bits(1e20F);
    error(st::intersect(t,q),st::ErrorCode::numeric_overflow,"normal square Y");
    t=triangle();q=ray(.25F,std::bit_cast<float>(0x7f7fffffU),.25F);q.direction_y=0xb727c5ac;
    error(st::intersect(t,q),st::ErrorCode::numeric_overflow,"intersection parameter");
    const auto saved=t.vertices;error(st::intersect(t,q,false),st::ErrorCode::numeric_overflow,"intersection parameter");check(t.vertices==saved,"Rejected query leaves vertices intact");
}
void gate(){
    const auto n=vector(0,2,0),down=vector(0,-1,0);
    check(value<bool>(st::normal_gate(n,down)),"True means normal gate accepts");
    check(!value<bool>(st::normal_gate(n,vector(0,1,0))),"Positive dot rejects");
    check(value<bool>(st::normal_gate(n,vector(1,0,0))),"Zero dot accepts");
    check(value<bool>(st::normal_gate(vector(0,0,0),down)),"Gate does not normalize zero normal");
    const auto nonfinite=value<st::Error>(st::normal_gate({0x7fc00001,0,0},down));
    check(nonfinite.code==st::ErrorCode::nonfinite_operand&&nonfinite.arithmetic.has_value(),"Zero weight still consumes normal component and preserves cause");
    const auto overflow=value<st::Error>(st::normal_gate({0x7f7fffff,0,0},vector(2,0,0)));
    check(overflow.code==st::ErrorCode::numeric_overflow,"Normal gate overflow");
}
void environment(){
    const auto old=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"Set rounding");
    const auto invalid=st::intersect(triangle(),ray(.25F,1,.25F));check(std::fesetround(old)==0,"Restore rounding");
    const auto e=value<st::Error>(invalid);check(e.code==st::ErrorCode::floating_environment&&e.arithmetic.has_value(),"Owner environment cause retained");
#if defined(__aarch64__)
    std::uint64_t saved;asm volatile("mrs %0, fpcr":"=r"(saved));const auto flushed=saved|(1ULL<<24);asm volatile("msr fpcr, %0"::"r"(flushed));
    const auto flushed_result=st::intersect(triangle(),ray(.25F,1,.25F));asm volatile("msr fpcr, %0"::"r"(saved));
    check(value<st::Error>(flushed_result).code==st::ErrorCode::floating_environment,"Flush underflow rejected");
#elif defined(__SSE__)
    const auto saved=_mm_getcsr();_mm_setcsr(saved|0x8040);const auto flushed_result=st::intersect(triangle(),ray(.25F,1,.25F));_mm_setcsr(saved);
    check(value<st::Error>(flushed_result).code==st::ErrorCode::floating_environment,"Flush underflow rejected");
#endif
}
}
int main(){try{geometry();failures();gate();environment();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_executions\":0}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
