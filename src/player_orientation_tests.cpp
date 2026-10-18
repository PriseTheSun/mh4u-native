#include "player_orientation.hpp"
#include "picosha2.h"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace {
namespace po=mh4u::player_orientation;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{};
void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
void boundaries(){
    const po::Angles angles{0,0,0};
    for(std::uint16_t mode=0;mode<6;++mode){auto result=po::calculate(angles,mode);check(std::holds_alternative<po::Quaternion>(result),"Known mode accepted");check(std::get<po::Quaternion>(result)==po::Quaternion{0,0,0,0x3f800000},"Identity source orientation");}
    auto missing=po::calculate(angles,std::nullopt);check(std::get<po::Error>(missing).code==po::ErrorCode::unknown_mode,"Missing mode preserved");
    for(const auto mode:{6U,7U,255U,65535U}){auto r=po::calculate(angles,static_cast<std::uint16_t>(mode));check(std::get<po::Error>(r).code==po::ErrorCode::unsupported_mode,"Unsupported mode distinct from unknown");}
    const int saved=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"Set rounding");auto bad=po::calculate(angles,4);auto lazy=po::calculate(angles,std::nullopt);check(std::fesetround(saved)==0,"Restore rounding");
    check(std::get<po::Error>(bad).code==po::ErrorCode::arithmetic&&std::get<po::Error>(bad).arithmetic->code==mh4u::motion_transform_math::ErrorCode::floating_environment,"Shared environment cause");check(std::get<po::Error>(lazy).code==po::ErrorCode::unknown_mode,"Unknown mode precedes arithmetic");
#if defined(__aarch64__)
    std::uint64_t fpcr;asm volatile("mrs %0, fpcr":"=r"(fpcr));const auto flushed=fpcr|(1ULL<<24);asm volatile("msr fpcr, %0"::"r"(flushed));const auto flush_result=po::calculate({1,2,3},4);asm volatile("msr fpcr, %0"::"r"(fpcr));
    check(std::get<po::Error>(flush_result).arithmetic->code==mh4u::motion_transform_math::ErrorCode::floating_environment,"Flush underflow rejected");
#elif defined(__SSE__)
    const auto fpcr=_mm_getcsr();_mm_setcsr(fpcr|0x8040);const auto flush_result=po::calculate({1,2,3},4);_mm_setcsr(fpcr);
    check(std::get<po::Error>(flush_result).arithmetic->code==mh4u::motion_transform_math::ErrorCode::floating_environment,"Flush underflow rejected");
#endif
    const po::Angles max{65535,65535,65535};const auto copy=max;check(std::holds_alternative<po::Quaternion>(po::calculate(max,5))&&max==copy,"Explicit uint16 endpoint and immutable input");
    std::cout<<"{\"status\":\"passed\",\"boundary_checks\":"<<checks<<"}\n";
}
Bytes read(const char* path){
    std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open fixture");const auto n=in.tellg();if(n<12||n>8*1024*1024)throw std::runtime_error("Fixture size bound");Bytes bytes(static_cast<std::size_t>(n));in.seekg(0);if(!in.read(reinterpret_cast<char*>(bytes.data()),n)||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture read failed/changed");return bytes;
}
void oracle(const char* path,const char* digest,std::uint32_t expected){
    const auto data=read(path);check(picosha2::hash256_hex_string(data)==digest,"Fixture SHA mismatch");
    check(std::string(data.begin(),data.begin()+8)=="MH4UOR01","Fixture schema");
    auto u32=[&](std::size_t p){check(p<=data.size()&&data.size()-p>=4,"Fixture word extent");std::uint32_t x=0;for(unsigned k=0;k<4;++k)x|=std::uint32_t(data[p+k])<<(8*k);return x;};
    auto u16=[&](std::size_t p){check(p<=data.size()&&data.size()-p>=2,"Fixture halfword extent");return std::uint16_t(std::uint16_t(data[p])|(std::uint16_t(data[p+1])<<8));};
    const auto count=u32(8);check(count==expected&&count>0&&count<=100000,"Fixture count");check(data.size()==12+std::size_t(count)*24,"Fixture exact length");
    for(std::uint32_t i=0;i<count;++i){const std::size_t p=12+std::size_t(i)*24;const po::Angles angles{u16(p),u16(p+2),u16(p+4)};const auto mode=u16(p+6);check(mode<6,"Fixture mode profile");const auto saved=angles;const auto result=po::calculate(angles,mode);check(std::holds_alternative<po::Quaternion>(result),"Original quaternion rejected");const auto q=std::get<po::Quaternion>(result);
        for(unsigned k=0;k<4;++k){const auto wanted=u32(p+8+k*4);if(q[k]!=wanted)throw std::runtime_error("Quaternion mismatch case="+std::to_string(i)+" component="+std::to_string(k)+" actual="+std::to_string(q[k])+" expected="+std::to_string(wanted));++checks;}
        check(angles==saved,"Angles mutated");
    }
    std::cout<<"{\"status\":\"passed\",\"cases\":"<<count<<",\"quaternion_bytes\":"<<std::uint64_t(count)*16<<",\"checks\":"<<checks<<"}\n";
}
}
int main(int argc,char** argv){try{if(argc==1)boundaries();else if(argc==4){const auto n=std::stoul(argv[3]);if(n>100000)throw std::runtime_error("Expected count bound");oracle(argv[1],argv[2],static_cast<std::uint32_t>(n));}else throw std::runtime_error("Usage: player-orientation-tests [fixture sha256 count]");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
