#include "player_state1_dynamics.hpp"
#include "picosha2.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace {
namespace d=mh4u::player_state1_dynamics;
using Word=std::uint32_t;
std::size_t checks{};
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
struct Reader {
    std::vector<unsigned char> bytes;std::size_t at{};
    Reader(const char* path,const char* digest){
        std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file)throw std::runtime_error("Fixture open");const auto n=file.tellg();
        if(n<12||n>4*1024*1024)throw std::runtime_error("Fixture capacity");
        bytes.resize(static_cast<std::size_t>(n));file.seekg(0);file.read(reinterpret_cast<char*>(bytes.data()),n);
        if(!file||file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture changed");
        if(picosha2::hash256_hex_string(bytes)!=digest)throw std::runtime_error("Fixture digest");
    }
    Word word(){if(bytes.size()-at<4)throw std::runtime_error("Fixture extent");Word w=0;for(unsigned j=0;j<4;++j)w|=Word(bytes[at++])<<(8*j);return w;}
};
void error(const d::RotationResult& result,d::ErrorCode code){const auto* e=std::get_if<d::Error>(&result);check(e&&e->code==code&&!e->selection,"Typed rotation error");}
void boundaries(){
    const d::Bits3 vector{0,0x7fc12345,0x42700000};const auto saved=vector;
    const auto result=d::rotate_y(vector,0);
    check(std::holds_alternative<d::Bits3>(result),"Known zero-angle result");
    check(std::get<d::Bits3>(result)==vector,"Zero-angle and raw Y");check(vector==saved,"Input immutable");
    for(auto y:{0x7fc00001U,0x7f800000U,0xff800000U,0x80000000U}){
        const auto r=d::rotate_y({0,y,0x42700000},0);check(std::holds_alternative<d::Bits3>(r)&&std::get<d::Bits3>(r)[1]==y,"Y is unconsumed raw");
    }
    for(auto invalid:{0x7f800000U,0xff800000U,0x7fc12345U}){
        error(d::rotate_y({invalid,0,0},0),d::ErrorCode::nonfinite_operand);
        error(d::rotate_y({0,0,invalid},0),d::ErrorCode::nonfinite_operand);
    }
    error(d::rotate_y({0x7f7fffff,0,0x7f7fffff},0x2000),d::ErrorCode::numeric_overflow);
    const auto a=d::rotate_y({0x3f800000,0,0xbf800000},0x1234),b=d::rotate_y({0x3f800000,0,0xbf800000},0xffff1234);
    check(std::get<d::Bits3>(a)==std::get<d::Bits3>(b),"Angle high16 ignored");
    struct Rounding {int saved=std::fegetround();~Rounding(){std::fesetround(saved);}} rounding;
    check(std::fesetround(FE_DOWNWARD)==0,"Set downward rounding");
    error(d::rotate_y(vector,0),d::ErrorCode::unsupported_floating_environment);
    check(std::fesetround(rounding.saved)==0,"Restore rounding");
#if defined(__aarch64__)
    std::uint64_t fpcr;asm volatile("mrs %0, fpcr":"=r"(fpcr));const auto altered=fpcr|(1ULL<<24);
    asm volatile("msr fpcr, %0"::"r"(altered));const auto flushed=d::rotate_y(vector,0);asm volatile("msr fpcr, %0"::"r"(fpcr));
    error(flushed,d::ErrorCode::unsupported_floating_environment);
#elif defined(__SSE__)
    const auto mxcsr=_mm_getcsr();_mm_setcsr(mxcsr|0x8040);const auto flushed=d::rotate_y(vector,0);_mm_setcsr(mxcsr);
    error(flushed,d::ErrorCode::unsupported_floating_environment);
#endif
}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("Usage: player-spatial-offset-tests FIXTURE SHA256");
    Reader r(argv[1],argv[2]);const std::array<unsigned char,8> magic{'M','H','4','U','O','F','0','1'};
    if(!std::equal(magic.begin(),magic.end(),r.bytes.begin()))throw std::runtime_error("Fixture magic");r.at=8;
    const auto count=r.word();if(count==0||count>70000)throw std::runtime_error("Case capacity");
    if(r.bytes.size()-r.at!=std::size_t(count)*40)throw std::runtime_error("Fixture extent");
    std::size_t overflow{},fixed{};
    for(Word i=0;i<count;++i){
        const auto kind=r.word(),angle=r.word();if(kind>2)throw std::runtime_error("Case kind");
        std::array<Word,4> before{},after{};for(auto& w:before)w=r.word();for(auto& w:after)w=r.word();
        if(before[1]!=after[1]||before[3]!=after[3])throw std::runtime_error("Original raw-copy lanes");
        if(kind==0){if(angle!=fixed||before!=std::array<Word,4>{0,0x43960000,0x42700000,0xa5a5a5a5})throw std::runtime_error("Fixed-angle coverage");++fixed;}
        const d::Bits3 input{before[0],before[1],before[2]};const auto saved=input;
        const auto result=d::rotate_y(input,angle);check(input==saved,"Oracle input immutable");
        if((after[0]&0x7f800000U)==0x7f800000U||(after[2]&0x7f800000U)==0x7f800000U){error(result,d::ErrorCode::numeric_overflow);++overflow;}
        else{
            const auto* value=std::get_if<d::Bits3>(&result);check(value!=nullptr,"Original finite output accepted");
            for(unsigned c=0;c<3;++c)check((*value)[c]==after[c],"Original component bits");
        }
    }
    if(r.at!=r.bytes.size())throw std::runtime_error("Fixture tail");boundaries();
    std::cout<<"{\"status\":\"passed\",\"cases\":"<<count<<",\"fixed_angles\":"<<fixed<<",\"numeric_rejections\":"<<overflow<<",\"checks\":"<<checks<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
