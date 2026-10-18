#include "motion_speed.hpp"
#include "motion_command.hpp"
#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace {
namespace ms=mh4u::motion_speed;namespace mc=mh4u::motion_command;
namespace mr=mh4u::motion_request;namespace fp=mh4u::binary32_arithmetic;
using Word=std::uint32_t;
std::size_t checks{};
void check(bool ok,const char* text){++checks;if(!ok)throw std::runtime_error(text);}
float f(Word w){return std::bit_cast<float>(w);}Word w(float v){return std::bit_cast<Word>(v);}
bool equal(const mr::Layer& a,const mr::Layer& b){
 if(a.selector!=b.selector||a.flags!=b.flags||a.field08!=b.field08||a.attributes!=b.attributes||
    a.bindings!=b.bindings||a.masks!=b.masks||a.interpolation_curve_bits!=b.interpolation_curve_bits||a.previous_masks!=b.previous_masks)return false;
 const std::array av{a.requested_frame,a.offset,a.interpolation,a.remaining_interpolation,a.weight,a.frame,a.previous_frame,a.end_frame,a.field58,a.speed,a.field60,a.prepared_frame,a.prepared_next_frame,a.previous_step};
 const std::array bv{b.requested_frame,b.offset,b.interpolation,b.remaining_interpolation,b.weight,b.frame,b.previous_frame,b.end_frame,b.field58,b.speed,b.field60,b.prepared_frame,b.prepared_next_frame,b.previous_step};
 for(std::size_t i=0;i<av.size();++i)if(w(av[i])!=w(bv[i]))return false;return true;
}
mr::Layer poison(){
 mr::Layer l{};l.selector=0xfffe;l.flags=0xa595;l.field08=0xf081;l.attributes=0xdeadc0de;
 l.requested_frame=f(0x7fa10001);l.offset=f(0xffc10002);l.interpolation=f(0x7f810003);
 l.remaining_interpolation=f(0xff810004);l.weight=f(0x80000000);l.frame=f(0x7f800000);
 l.previous_frame=f(0xff800000);l.end_frame=f(1);l.field58=f(0x80000001);l.speed=f(0x7fc10005);
 l.field60=f(0x7f7fffff);l.prepared_frame=f(0xff7fffff);l.prepared_next_frame=f(0x00800000);l.previous_step=f(0x80800000);
 for(Word i=0;i<4;++i){if(i!=2)l.bindings[i]=mr::Binding{0x1234567800000000ULL+i,15-i,255-i,i};l.masks[i]=0xf0f01000+i;l.previous_masks[i]=0x80123400+i;}
 l.interpolation_curve_bits={0x7f812345,0xffc23456};return l;
}
ms::Output output(ms::Result result){if(auto* p=std::get_if<ms::Output>(&result))return *p;throw std::runtime_error("Unexpected motion speed error");}
struct Rounding {int old=std::fegetround();~Rounding(){std::fesetround(old);}};
void environment_checks(){
 Rounding restore;mc::Fields fields{};fields.fieldd68=0x40c00000;auto layer=poison();layer.interpolation=4;layer.remaining_interpolation=8;
 const auto before=layer;
 for(int mode:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
  check(std::fesetround(mode)==0,"Cannot configure test rounding mode");
  auto result=ms::primary(layer,fields,0x40400000);const auto* error=std::get_if<ms::Error>(&result);
  check(error&&error->phase==ms::Phase::ratio&&error->cause.code==fp::ErrorCode::unsupported_environment,"Consumed arithmetic accepted unsupported rounding");
  check(equal(layer,before),"Environment failure changed caller layer");
  auto lazy=layer;lazy.remaining_interpolation=f(0x7fa12345);auto expected=lazy;expected.speed=f(0xff812345);
  auto p=output(ms::primary(lazy,fields,0xff812345));check(!p.rescaled&&equal(p.layer,expected),"Unordered early return consumed environment or quieted words");
  for(Word zero:{0U,0x80000000U}){expected=layer;expected.speed=f(zero);p=output(ms::primary(layer,fields,zero));check(!p.rescaled&&equal(p.layer,expected),"Zero-speed early return consumed environment");}
  expected=layer;expected.speed=f(0x7f812345);check(equal(ms::secondary(layer,0x7f812345),expected),"Secondary consumed environment");
 }
 check(std::fesetround(FE_TONEAREST)==0,"Cannot restore nearest rounding");
#if defined(__aarch64__)
 struct Control {std::uint64_t old{};Control(){asm volatile("mrs %0, fpcr":"=r"(old));}~Control(){asm volatile("msr fpcr, %0"::"r"(old));}} control;
 const std::uint64_t flush=control.old|(std::uint64_t{1}<<24);asm volatile("msr fpcr, %0"::"r"(flush));
#elif defined(__SSE__)
 struct Control {unsigned old=_mm_getcsr();~Control(){_mm_setcsr(old);}} control;
 _mm_setcsr(control.old|0x8040U);
#endif
#if defined(__aarch64__) || defined(__SSE__)
 auto result=ms::primary(layer,fields,0x40400000);const auto* error=std::get_if<ms::Error>(&result);
 check(error&&error->phase==ms::Phase::ratio&&error->cause.code==fp::ErrorCode::unsupported_environment,"Consumed arithmetic accepted flush-to-zero");
 check(equal(layer,before),"Flush-to-zero failure changed input");
 auto lazy=layer;lazy.remaining_interpolation=0;auto p=output(ms::primary(lazy,fields,1));auto expected=lazy;expected.speed=f(1);
 check(!p.rescaled&&equal(p.layer,expected),"Raw-only path consumed flush-to-zero environment");
#endif
}
void run(){
 mc::Fields fields{};fields.fieldsd48={1,2,3,4,5,6,7};fields.fieldd68=0x7fa00123;fields.fieldsd70={0xffffffff,0,0x80000000,0x12345678};const auto fields_before=fields;
 const std::array<Word,26> patterns{0,0x80000000,1,0x80000001,0x007fffff,0x807fffff,0x00800000,0x80800000,0x3f800000,0xbf800000,0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc00000,0xffc00000,0x7fc12345,0xffc12345,0x7f800001,0xff800001,0x7fa12345,0xffa12345,0x3f000000,0xbf000000,0x40000000,0xc0000000};
 for(auto speed:patterns){auto layer=poison();const auto before=layer;auto expected=layer;expected.speed=f(speed);
  check(equal(ms::secondary(layer,speed),expected),"Secondary raw store or preservation");check(equal(layer,before),"Secondary mutated input");
  for(auto remaining:patterns){if(remaining>0&&remaining<=0x7f800000U)continue;layer=poison();layer.remaining_interpolation=f(remaining);expected=layer;expected.speed=f(speed);
   const auto saved=layer;auto p=output(ms::primary(layer,fields,speed));check(!p.rescaled&&equal(p.layer,expected),"Remaining ordered guard or raw copy");check(equal(layer,saved)&&fields==fields_before,"Primary raw-only path mutated inputs");
  }
 }
 for(auto remaining:patterns)if(remaining>0&&remaining<=0x7f800000U)for(Word speed:{0U,0x80000000U}){
  auto layer=poison();layer.remaining_interpolation=f(remaining);auto expected=layer;expected.speed=f(speed);auto p=output(ms::primary(layer,fields,speed));check(!p.rescaled&&equal(p.layer,expected),"Signed zero speed must bypass prior interpolation/d68");
 }
 // Deliberately simple exact binary32 identities plus raw exceptional results.
 // These are boundary expectations, not a replacement for the original ARM oracle.
 struct Case {Word remaining,interpolation,fieldd68,speed,next_remaining,next_interpolation;};
 constexpr std::array cases{
  Case{0x41000000,0x40800000,0x40c00000,0x40400000,0x40800000,0x40000000},
  Case{0x41000000,0x40800000,0x40c00000,0xc0400000,0xc0800000,0xc0000000},
  Case{0x41000000,0xc0800000,0xc0c00000,0x40400000,0x40800000,0xc0000000},
  Case{0x3f800000,0,0x3f800000,0x3f800000,0x7f800000,0x3f800000},
  Case{0x3f800000,0x80000000,0x3f800000,0x3f800000,0xff800000,0x3f800000},
  Case{0x3f800000,0,0,0x3f800000,0x7fc00000,0},
  Case{0x7f800000,0x7f800000,0x3f800000,0x3f800000,0x7fc00000,0x3f800000},
  Case{0x3f800000,0x3f800000,0x3f800000,0x7f800000,0,0},
  Case{0x3f800000,0x3f800000,0x3f800000,0xff800000,0x80000000,0x80000000},
  Case{1,0x3f800000,0x3f800000,0x40000000,0,0x3f000000},
  Case{2,0x3f800000,0x3f800000,0x40000000,1,0x3f000000},
  Case{0x7f7fffff,0x00800000,0x3f800000,0x3f800000,0x7f800000,0x3f800000},
  Case{0x3f800000,0x7fc12345,0xff800001,0x7fa23456,0x7fe23456,0xffc00001},
  Case{0x3f800000,0xff800001,0x7fc12345,0x3f800000,0xffc00001,0x7fc12345},
  Case{0x3f800000,0x3f800000,0x3f800000,0xff800001,0xffc00001,0xffc00001}
 };
 for(const auto& c:cases){auto layer=poison();layer.remaining_interpolation=f(c.remaining);layer.interpolation=f(c.interpolation);fields.fieldd68=c.fieldd68;const auto before=layer;const auto fields_copy=fields;
  auto expected=layer;expected.speed=f(c.speed);expected.remaining_interpolation=f(c.next_remaining);expected.interpolation=f(c.next_interpolation);
  const auto p=output(ms::primary(layer,fields,c.speed));check(p.rescaled,"Arithmetic branch not marked");check(equal(p.layer,expected),"Binary32 source-order boundary result/preservation");check(equal(layer,before)&&fields==fields_copy,"Arithmetic branch mutated inputs");
 }
 environment_checks();
}
}
int main(int argc,char**){try{if(argc!=1)throw std::runtime_error("Expected no arguments");run();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
