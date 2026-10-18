#include "player_update_accumulator.hpp"
#include "picosha2.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace ua=mh4u::player_update_accumulator;
namespace pf=mh4u::player_fields;
namespace math=mh4u::motion_transform_math;
using U=std::uint32_t;using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},successes{},numeric_errors{},clear_cases{};
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
constexpr U player=0x3000000,pbw=0x3100000,player_size=0x1000,pbw_size=0x100;
constexpr U count=3634;
struct Reader {
 std::span<const std::uint8_t> bytes;std::size_t pos{};
 std::span<const std::uint8_t> take(std::size_t n){check(pos<=bytes.size()&&n<=bytes.size()-pos,"truncated fixture");auto out=bytes.subspan(pos,n);pos+=n;return out;}
 U word(){auto b=take(4);return U(b[0])|(U(b[1])<<8)|(U(b[2])<<16)|(U(b[3])<<24);}
 void end(){check(pos==bytes.size(),"trailing fixture bytes");}
};
U word(std::span<const std::uint8_t>b,std::size_t offset){check(offset<=b.size()&&4<=b.size()-offset,"arena extent");return U(b[offset])|(U(b[offset+1])<<8)|(U(b[offset+2])<<16)|(U(b[offset+3])<<24);}
void put(Bytes& b,std::size_t o,U v){check(o<=b.size()&&4<=b.size()-o,"write extent");for(U i=0;i<4;++i)b[o+i]=std::uint8_t(v>>(8*i));}
bool finite(U x){return (x&0x7f800000U)!=0x7f800000U;}
const ua::Error& error(const ua::Result& r){check(std::holds_alternative<ua::Error>(r),"expected typed error");return std::get<ua::Error>(r);}
void arithmetic(const ua::Result& r,math::ErrorCode code){const auto& e=error(r);check(e.code==ua::ErrorCode::arithmetic&&e.field=="PBW54+player14"&&e.cause&&e.cause->code==code,"arithmetic cause lost");}
pf::Fields seed(U n){
 pf::Fields f{};
 f.pbw.field3a8=std::uint16_t(n);f.pbw.field3b8=0xffff;f.pbw.field14_bits=0x7fc12345;f.pbw.field478=0xa5a5a5a5;
 f.pbw.field10_bits=0x7f800000;f.pbw.field18_bits=0xff800000;f.pbw.field354_bits=n;f.pbw.field1c_bits=0x80000000;
 f.pbw.field6e8_bits=n;f.pbw.field6ec_bits=0xffffffff;f.pbw.field6f0_bits=0;f.pbw.field6f4_bits=0x7f812345;
 f.pbw.field37a=0xff;f.pbw.field6c0=0x91;f.pbw.field93f1=0x53;f.pbw.field6fb=0x42;
 for(std::size_t i=0;i<f.pbw.words3d8.size();++i){f.pbw.words3d8[i][0]=std::uint8_t(i+n);if(i%2)f.pbw.words3d8[i][2]=std::uint8_t(i^n);}
 for(std::size_t i=0;i<f.pbw.input_other.size();i+=3)f.pbw.input_other[i]=std::uint8_t(i+n);
 f.pbw.field464_bits=0x7fc12345;f.pw.field13c_bits=0xff812345;f.pw.field9dc=0xa5;f.pw.field9de=0x5a;
 f.pw.field14c=0xffffffff;f.pw.field154=0x12345678;f.pw.field15c_bits=0xffc12345;f.pw.field6a4_bits=0x7fc56789;
 f.pw.field150=0xff;f.pw.field158=1;f.pw.field159=2;f.pw.field15a=3;f.pw.field6a8=4;f.pw.field7f0=5;
 f.pw.field188=-32768;f.pw.field224=32767;f.pw.field238=-1;f.pw.field1f8=0x80001000;
 f.pw.field220_bits=0xff812345;f.pw.field838_bits=0x80000000;f.pw.field5a8=n;f.pw.field01=0xa1;f.pw.field0b=0xa2;f.pw.field589=0xa3;f.pw.field6a9=0xa4;
 return f;
}
void oracle(const Bytes& bytes){
 Reader r{bytes};auto magic=r.take(8);check(std::string(magic.begin(),magic.end())=="MH4UPAC1","fixture magic");check(r.word()==count,"case count");
 const auto digest=[&]{auto b=r.take(32);return picosha2::bytes_to_hex_string(b.begin(),b.end());};
 check(digest()=="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","ELF identity");
 check(digest()=="c5990686f976791aa580110f0a90385dd173b835398db7a26923fc8a85d1918e","leaf identity");
 check(r.word()==2&&r.word()==player&&r.word()==player_size&&r.word()==pbw&&r.word()==pbw_size,"arena descriptor");
 for(U i=0;i<count;++i){
  const auto size=r.word();check(size>=16+2*(player_size+pbw_size)+4&&size<=16+2*(player_size+pbw_size)+4+32,"record size");Reader q{r.take(size)};
  check(q.word()==i,"case ordinal");const U status=q.word();check(status<=2,"profile status");
  const U fpscr=q.word();check((fpscr&~0x9fU)==0,"FPSCR outside fixed source profile");check(q.word()==pbw,"source return pointer");
  const auto before_p=q.take(player_size),after_p=q.take(player_size),before_b=q.take(pbw_size),after_b=q.take(pbw_size);
  check(std::equal(before_p.begin(),before_p.end(),after_p.begin()),"original changed player arena");check(word(before_p,0xe30)==pbw,"pointer identity");
  const U stores=q.word();check(stores==1||stores==2,"store count");
  U sum=0;
  for(U s=0;s<stores;++s){check(q.word()==(s==0?0xb09ce8U:0xb09cf8U),"store PC");check(q.word()==pbw+0x54&&q.word()==4,"store destination");const U v=q.word();if(s==0)sum=v;else check(v==0,"clear literal");}
  q.end();const U accumulator=word(before_b,0x54),delta=word(before_p,0x14),observed=word(after_b,0x54);
  const U expected_status=!finite(accumulator)||!finite(delta)?1:!finite(sum)?2:0;
  check(status==expected_status,"profile classification");
  const bool selected=(sum&0x80000000U)==0&&sum>=0x4b189680U;
  check(stores==(selected?2U:1U)&&observed==(selected?0U:sum),"source store evidence consistency");
  clear_cases+=selected;
  auto original_projected=Bytes(before_b.begin(),before_b.end());put(original_projected,0x54,observed);
  check(std::equal(original_projected.begin(),original_projected.end(),after_b.begin()),"original unclaimed write");
  auto input=seed(i);input.pbw.field54_bits=accumulator;const auto saved=input;auto result=ua::apply(input,delta);
  check(input==saved,"borrowed input mutation");
  if(status){arithmetic(result,status==1?math::ErrorCode::nonfinite_operand:math::ErrorCode::numeric_overflow);++numeric_errors;continue;}
  check(std::holds_alternative<ua::Output>(result),"finite source call rejected");const auto& out=std::get<ua::Output>(result);
  auto expected=saved;expected.pbw.field54_bits=observed;
  check(out.fields==expected,"canonical fields differ from original result or unrelated fields changed");
  auto native_projected=Bytes(before_b.begin(),before_b.end());put(native_projected,0x54,*out.fields.pbw.field54_bits);
  check(std::equal(native_projected.begin(),native_projected.end(),after_b.begin()),"native projected arena differs");++successes;
 }
 r.end();check(successes==3325&&numeric_errors==309&&clear_cases==947,"coverage changed");
}
void boundaries(){
 pf::Fields f{};auto r=ua::apply(f,{});check(error(r).code==ua::ErrorCode::unknown_field&&error(r).field=="player14"&&!error(r).cause,"source omission order");
 r=ua::apply(f,0);check(error(r).code==ua::ErrorCode::unknown_field&&error(r).field=="PBW54","unknown accumulator");
 f.pbw.field54_bits=0x80000000U;const auto saved=f;r=ua::apply(f,0x80000000U);check(std::get<ua::Output>(r).fields.pbw.field54_bits==0x80000000U,"negative-zero sum lost");check(f==saved,"negative-zero mutation");
 f.pbw.field54_bits=0;check(std::get<ua::Output>(ua::apply(f,0)).fields==f,"unknown unrelated fields changed");
 const auto rounding=std::fegetround();struct Restore{int mode;~Restore(){std::fesetround(mode);}} restore{rounding};
 for(int mode:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){check(std::fesetround(mode)==0,"set rounding");arithmetic(ua::apply(f,0),math::ErrorCode::floating_environment);check(error(ua::apply(f,{})).code==ua::ErrorCode::unknown_field,"unknown read must precede arithmetic");}
 check(std::fesetround(rounding)==0,"restore rounding");
#if defined(__aarch64__)
 std::uint64_t fpcr;asm volatile("mrs %0, fpcr":"=r"(fpcr));
 struct RestoreFp{std::uint64_t value;~RestoreFp(){asm volatile("msr fpcr, %0\nisb"::"r"(value):"memory");}} restore_fp{fpcr};
 const auto flushed=fpcr|(std::uint64_t{1}<<24);asm volatile("msr fpcr, %0\nisb"::"r"(flushed):"memory");
 arithmetic(ua::apply(f,0),math::ErrorCode::floating_environment);
 asm volatile("msr fpcr, %0\nisb"::"r"(fpcr):"memory");
#elif defined(__SSE__)
 const auto csr=_mm_getcsr();struct RestoreFp{unsigned value;~RestoreFp(){_mm_setcsr(value);}} restore_fp{csr};
 _mm_setcsr(csr|(1U<<15)|(1U<<6));arithmetic(ua::apply(f,0),math::ErrorCode::floating_environment);_mm_setcsr(csr);
#endif
 check(std::get<ua::Output>(ua::apply(f,0)).fields==f,"environment restoration");
}
int main(int argc,char** argv){try{
 check(argc==3,"Expected fixture and SHA256");std::ifstream f(argv[1],std::ios::binary|std::ios::ate);check(bool(f),"fixture open");
 const auto length=f.tellg();check(length>=0&&length<=64*1024*1024,"fixture byte limit");Bytes bytes(static_cast<std::size_t>(length));f.seekg(0);
 check(bool(f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(bytes.size())))&&f.peek()==std::char_traits<char>::eof(),"fixture read");
 check(picosha2::hash256_hex_string(bytes)==argv[2],"fixture SHA256");oracle(bytes);boundaries();
 std::cout<<"{\"status\":\"passed\",\"cases\":"<<count<<",\"finite_outputs\":"<<successes<<",\"typed_numeric_errors\":"<<numeric_errors<<",\"checks\":"<<checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
