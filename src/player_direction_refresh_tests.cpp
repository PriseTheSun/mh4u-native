#include "player_direction_refresh.hpp"
#include "picosha2.h"
#include <array>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace {
namespace dr=mh4u::player_direction_refresh;
namespace pcs=mh4u::player_controller_step;
using U=std::uint32_t;
std::size_t checks{},cases{},calls{},snaps{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class T,class V>const T& value(const V& v){check(std::holds_alternative<T>(v),"Unexpected result variant");return std::get<T>(v);}
const dr::Bindings binding{dr::Virtual84::uplayer_b085bc,dr::Virtual144::uplayer_b47aa0};
bool equal(const pcs::State& a,const pcs::State& b){return a.player==b.player&&a.motion==b.motion;}
pcs::State seed(){
 pcs::State s{};s.player.pbw.field371=7;s.player.pbw.field372=0;s.player.selection_pbw.field6f9=0;
 s.player.pbw.field24=0xaaaa0000;s.player.pw.fieldb0=0xbbbb0001;
 s.player.global.field20=0x1839;s.player.global.field22=0xabcd;s.player.global.field5a=0x93;
 s.player.pw.policy.latches.fill(0xa5);s.player.pw.policy.timer144_bits=0x7fa12345;
 s.player.selection_pbw.field6fa=0x71;s.player.selection_pw.field9a8=0x45;
 s.motion.fieldsd48.fill(0xf0e1d2c3);s.motion.fieldd68=0xff812345;s.motion.fieldsd70.fill(0x01234567);
 return s;
}
void preserved(const pcs::State& before,const pcs::State& source,const dr::Output& result,U expected){
 check(equal(source,before),"Source state mutated");auto want=before;want.player.pbw.field24=expected;
 check(equal(result.state,want),"Non-output canonical state changed");
}
void rejected(const pcs::State& source,std::optional<U> delta,const dr::Bindings& b,dr::Phase phase,dr::ErrorCode code){
 const auto before=source;const auto result=dr::apply(source,delta,b);const auto& error=value<dr::Error>(result);
 check(error.phase==phase&&error.code==code,"Typed error phase/code");
 check(equal(before,source),"Rejected operation mutated input");
 check(bool(error.arithmetic)==(code==dr::ErrorCode::arithmetic),"Arithmetic cause retained");
}
struct Rounding {
 int old=std::fegetround();~Rounding(){std::fesetround(old);}
};
void boundaries(){
 auto s=seed();auto original=s;
 auto r=dr::apply(s,0x3f800000,binding);const auto& o=value<dr::Output>(r);
 check(o.effects.snapped&&o.state.player.pbw.field24==0xbbbb0001,"Snap must preserve full target word");preserved(original,s,o,0xbbbb0001);
 r=dr::apply(s,0,binding);const auto& z=value<dr::Output>(r);
 check(!z.effects.snapped&&z.effects.step==0&&z.state.player.pbw.field24==0,"Zero step still narrows current");
 for(U d:{0U,0x80000000U,1U,0x80000001U,0x7fc12345U,0xffc12345U,0x7f812345U,0xff812345U}){
  r=dr::apply(s,d,binding);const auto& x=value<dr::Output>(r);check(x.effects.step==0,"Tiny/zero/NaN RTZ result");preserved(original,s,x,0);
 }
 r=dr::apply(s,0x7f800000,binding);check(value<dr::Output>(r).effects.step==0x7fffffff,"Positive infinity saturates");
 r=dr::apply(s,0xff800000,binding);check(value<dr::Output>(r).effects.step==0x80000000,"Negative infinity saturates");
 s.player.pbw.field371=1;s.player.pbw.field372=4;
 r=dr::apply(s,0x7f800000,binding);check(value<dr::Output>(r).effects.rate==0&&value<dr::Output>(r).effects.step==0,"Zero rate times infinity converts invalid NaN to zero");
 s=seed();s.player.pbw.field24=0;s.player.pw.fieldb0=2001;
 r=dr::apply(s,0x3f800000,binding);check(value<dr::Output>(r).effects.snapped,"Strict comparison inner edge");
 s.player.pw.fieldb0=2002;r=dr::apply(s,0x3f800000,binding);check(!value<dr::Output>(r).effects.snapped,"Strict comparison exact edge");
 for(U target:{32767U,32768U}){s.player.pw.fieldb0=target;r=dr::apply(s,0x3f800000,binding);check(value<dr::Output>(r).state.player.pbw.field24==(target==32767?2002U:63534U),"Half-turn unsigned branch");}
 s=seed();s.player.selection_pbw.field6f9.reset();rejected(s,{}, {},dr::Phase::byte6f9,dr::ErrorCode::unknown_field);
 s=seed();rejected(s,{},binding,dr::Phase::delta,dr::ErrorCode::unknown_field);
 rejected(s,0x3f800000,{},dr::Phase::virtual84,dr::ErrorCode::unknown_binding);
 auto bad=binding;bad.virtual84=static_cast<dr::Virtual84>(999);rejected(s,0,bad,dr::Phase::virtual84,dr::ErrorCode::unsupported_binding);
 bad=binding;bad.virtual144=static_cast<dr::Virtual144>(999);
 for(auto flag:{0,2,255}){s.player.selection_pbw.field6f9=static_cast<std::uint8_t>(flag);check(std::holds_alternative<dr::Output>(dr::apply(s,0,bad)),"Unreached virtual144 is lazy");}
 s.player.selection_pbw.field6f9=1;rejected(s,{}, {},dr::Phase::virtual144,dr::ErrorCode::unknown_binding);
 rejected(s,0,bad,dr::Phase::virtual144,dr::ErrorCode::unsupported_binding);
 s=seed();
 {Rounding restore;check(std::fesetround(FE_DOWNWARD)==0,"Set rounding mode");rejected(s,0,binding,dr::Phase::rate_conversion,dr::ErrorCode::arithmetic);}
#if defined(__aarch64__)
 {std::uint64_t old;asm volatile("mrs %0, fpcr":"=r"(old));struct Restore{std::uint64_t old;~Restore(){asm volatile("msr fpcr, %0"::"r"(old));}}restore{old};const auto flush=old|(std::uint64_t{1}<<24);asm volatile("msr fpcr, %0"::"r"(flush));rejected(s,1,binding,dr::Phase::rate_conversion,dr::ErrorCode::arithmetic);}
#elif defined(__SSE__)
 {const auto old=_mm_getcsr();struct Restore{unsigned old;~Restore(){_mm_setcsr(old);}}restore{old};_mm_setcsr(old|0x8040U);rejected(s,1,binding,dr::Phase::rate_conversion,dr::ErrorCode::arithmetic);}
#endif
 // Both callers share full-width b43dcc, including its argument-specific state2.
 for(U argument:{0U,1U,256U,0xffffffffU})for(unsigned state:{0U,2U,4U,6U,255U})for(unsigned action:{21U,82U,106U,107U,108U,113U,114U,119U,120U,65535U}){
  s.player.pbw.field371=static_cast<std::uint8_t>(state);s.player.pbw.field372=static_cast<std::uint16_t>(action);
  const bool expected=(state==0&&action>=114&&action<=119)||(state==2&&action==21&&argument==0)||(state==4&&(action==107||action==108))||(state==6&&action==82);
  check(mh4u::player_transition::state_action_gate(s.player.pbw,argument)==expected,"Shared predicate full argument domain");
 }
}
struct Reader {
 std::vector<std::uint8_t> bytes;std::size_t offset{};
 explicit Reader(const char* path,const char* sha){std::ifstream file(path,std::ios::binary|std::ios::ate);check(bool(file),"Fixture open");const auto size=file.tellg();check(size>=0&&size<=1024*1024,"Fixture limit");bytes.resize(static_cast<std::size_t>(size));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()));check(file.gcount()==static_cast<std::streamsize>(bytes.size())&&file.peek()==std::char_traits<char>::eof(),"Fixture changed during read");check(picosha2::hash256_hex_string(bytes)==sha,"Fixture hash");}
 U word(){check(offset<=bytes.size()&&bytes.size()-offset>=4,"Fixture truncated");U out=0;for(unsigned i=0;i<4;++i)out|=U(bytes[offset++])<<(8*i);return out;}
};
void compare(const char* path,const char* sha){
 Reader reader(path,sha);check(reader.word()==0x5534484d&&reader.word()==0x31305244,"Fixture magic");check(reader.word()==3828,"Fixture count");
 for(U i=0;i<3828;++i){std::array<U,20>w{};for(auto& v:w)v=reader.word();check(w[0]==i,"Fixture index");check(w[1]<=255&&w[2]<=65535&&w[3]<=255&&w[7]<=65535,"Fixture input widths");check(w[10]<=65535&&w[13]<=1&&w[14]>=3&&w[14]<=5,"Fixture observation bounds");
  auto s=seed();s.player.pbw.field371=static_cast<std::uint8_t>(w[1]);s.player.pbw.field372=static_cast<std::uint16_t>(w[2]);s.player.selection_pbw.field6f9=static_cast<std::uint8_t>(w[3]);s.player.pbw.field24=w[5];s.player.pw.fieldb0=w[6];const auto before=s;
  const auto result=dr::apply(s,w[4],binding);const auto& o=value<dr::Output>(result);
  check(o.effects.rate==w[8],"Original selected rate");check(o.effects.step==w[9],"Original raw step");check(o.effects.difference==w[10],"Original narrowed difference");check(o.state.player.pbw.field24==w[11],"Original PBW24");check(o.effects.snapped==bool(w[13]),"Original branch");check(o.effects.call_count==w[14],"Original helper count");
  for(unsigned n=0;n<5;++n){check(w[15+n]<=3,"Fixture call tag");if(n<w[14]){check(static_cast<U>(o.effects.calls[n])==w[15+n],"Original helper order");++calls;}else check(w[15+n]==0,"Fixture unused call");}
  preserved(before,s,o,w[11]);++cases;snaps+=o.effects.snapped;
 }
 check(reader.offset==reader.bytes.size(),"Fixture trailing bytes");
}
}
int main(int argc,char**argv){try{if(argc!=1&&argc!=3)throw std::runtime_error("Expected optional fixture and SHA256");boundaries();if(argc==3)compare(argv[1],argv[2]);std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"calls\":"<<calls<<",\"snaps\":"<<snaps<<"}\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
