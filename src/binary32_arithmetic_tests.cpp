#include "binary32_arithmetic.hpp"
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
namespace fp=mh4u::binary32_arithmetic;
using U=std::uint32_t;
std::size_t checks{};
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
struct Reader {
 std::string data;std::size_t at{};
 std::string_view take(std::size_t n){if(n>data.size()-at)throw std::runtime_error("Truncated fixture");auto s=std::string_view(data).substr(at,n);at+=n;return s;}
 U word(){const auto s=take(4);U v{};for(unsigned i=0;i<4;++i)v|=U(static_cast<unsigned char>(s[i]))<<(i*8);return v;}
};
std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open fixture");const auto n=f.tellg();if(n<0||static_cast<std::uintmax_t>(n)>1024*1024)throw std::runtime_error("Fixture capacity");std::string s(static_cast<std::size_t>(n),'\0');f.seekg(0);if(!f.read(s.data(),static_cast<std::streamsize>(s.size()))||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture changed while reading");return s;}
using Operation=fp::Result(*)(U,U);
constexpr std::array<Operation,4> operations{fp::add,fp::sub,fp::div,fp::div};
using Conversion=fp::Result(*)(U);
constexpr std::array<Conversion,4> conversions{fp::from_signed_word,fp::from_unsigned_word,fp::to_signed_word_rtz,fp::to_unsigned_word_rtz};
void rejected(const fp::Result& r){check(std::holds_alternative<fp::Error>(r)&&std::get<fp::Error>(r).code==fp::ErrorCode::unsupported_environment,"Unsupported environment was not rejected");}
void environments(){
 struct Rounding {int saved=std::fegetround();~Rounding(){std::fesetround(saved);}} mode;
 for(int rounding:{FE_DOWNWARD,FE_UPWARD,FE_TOWARDZERO}){
  check(std::fesetround(rounding)==0,"Cannot set rounding test");
  for(auto op:{fp::add,fp::sub,fp::mul,fp::div}){rejected(op(0x3f800000,0x33800000));rejected(op(0x7fa12345,0xffc54321));}
  for(auto op:conversions){rejected(op(0x3f800000));rejected(op(0x7fa12345));}
 }
 check(std::fesetround(mode.saved)==0,"Cannot restore rounding");
#if defined(__aarch64__)
 struct FpControl {std::uint64_t old{};FpControl(){asm volatile("mrs %0, fpcr":"=r"(old));}~FpControl(){asm volatile("msr fpcr, %0"::"r"(old));}} control;
 const auto changed=control.old|(std::uint64_t{1}<<24);asm volatile("msr fpcr, %0"::"r"(changed));
 for(auto op:{fp::add,fp::sub,fp::mul,fp::div})rejected(op(1,2));
 for(auto op:conversions)rejected(op(1));
#elif defined(__x86_64__) || defined(_M_X64)
 struct FpControl {unsigned old=_mm_getcsr();~FpControl(){_mm_setcsr(old);}} control;
 _mm_setcsr(control.old|(1U<<15)|(1U<<6));
 for(auto op:{fp::add,fp::sub,fp::mul,fp::div})rejected(op(1,2));
 for(auto op:conversions)rejected(op(1));
#endif
}
void run(const std::filesystem::path& path,std::string_view sha){
 Reader r{read(path)};const auto before=r.data;check(sha.size()==64&&picosha2::hash256_hex_string(r.data)==sha,"Fixture SHA mismatch");const auto magic=r.take(8);const bool multiply=magic=="MH4UMU01",conversion=magic=="MH4UCV01";check(multiply||conversion||magic=="MH4UFP01","Fixture magic");const auto elf=r.take(32);const auto p=reinterpret_cast<const unsigned char*>(elf.data());check(picosha2::bytes_to_hex_string(p,p+32)=="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","Source identity");const auto count=r.word();check(count==(multiply?6408U:conversion?16384U:12816U),"Fixture coverage");const U per_operation=conversion?4096U:3204U;std::array<U,4> totals{};
 for(U i=0;i<count;++i){const auto op=r.word(),left=r.word(),right=r.word(),expected=r.word(),fpscr=r.word();check(op<(multiply?2U:4U)&&(fpscr&~0x9fU)==0,"Fixture operation/status domain");check(op==i/per_operation,"Fixture operation order");fp::Result result;if(conversion){check(right==0,"Conversion reserved operand");result=conversions[op](left);}else if(!multiply)result=operations[op](left,right);else if(op==0)result=fp::mul(left,right);else{const auto product=fp::mul(right,right);check(std::holds_alternative<U>(product),"VMLA product failed");result=fp::add(left,std::get<U>(product));}check(std::holds_alternative<U>(result)&&std::get<U>(result)==expected,"Native output differs from original VFP instruction");++totals[op];}
 check(r.at==r.data.size(),"Trailing fixture bytes");check(r.data==before,"Fixture mutated");for(std::size_t i=0;i<totals.size();++i)check(totals[i]==(i<(multiply?2U:4U)?per_operation:0U),"Per-operation coverage");environments();const auto restored=fp::add(1,1);check(std::holds_alternative<U>(restored)&&std::get<U>(restored)==2,"Floating environment was not restored");
 std::cout<<"{\"status\":\"passed\",\"original_instruction_cases\":"<<count<<",\"checks\":"<<checks<<"}\n";
}
}
int main(int argc,char** argv){try{if(argc!=3)throw std::runtime_error("Expected fixture and SHA");run(argv[1],argv[2]);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
