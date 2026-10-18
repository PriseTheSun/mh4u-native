#include "source_rng.hpp"
#include "player_selection.hpp"
#include "picosha2.h"
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
namespace ps=mh4u::player_selection;namespace rng=mh4u::source_rng;
std::size_t checks{};
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
struct Reader {
 std::string bytes;std::size_t at{};
 std::string_view take(std::size_t n){if(n>bytes.size()-at)throw std::runtime_error("Fixture truncated");auto s=std::string_view(bytes).substr(at,n);at+=n;return s;}
 std::uint32_t u32(){const auto s=take(4);std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(static_cast<unsigned char>(s[i]))<<(8*i);return v;}
 std::uint16_t u16(){const auto s=take(2);return static_cast<std::uint16_t>(static_cast<unsigned char>(s[0])|(static_cast<unsigned char>(s[1])<<8));}
};
std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot open fixture");const auto n=f.tellg();if(n<0||static_cast<std::uintmax_t>(n)>1024*1024)throw std::runtime_error("Fixture exceeds limit");std::string s(static_cast<std::size_t>(n),'\0');f.seekg(0);if(!f.read(s.data(),static_cast<std::streamsize>(s.size()))||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture changed");return s;}
void unknown(const ps::Result<std::uint32_t>& r){check(std::holds_alternative<ps::Error>(r)&&std::get<ps::Error>(r).code==ps::ErrorCode::unknown_selection_context,"Expected unknown context error");}
void run(const std::filesystem::path& p,std::string_view digest){Reader r{read(p)};check(digest.size()==64&&picosha2::hash256_hex_string(r.bytes)==digest,"Fixture SHA mismatch");check(r.take(8)=="MH4URM01","Fixture magic");const auto elf=r.take(32);check(picosha2::bytes_to_hex_string(reinterpret_cast<const unsigned char*>(elf.data()),reinterpret_cast<const unsigned char*>(elf.data())+elf.size())=="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","Fixture source identity");const auto ns=r.u32(),nm=r.u32();check(ns==65536&&nm==2048,"Fixture coverage changed");std::array<std::uint16_t,65536> expected{};
 for(std::uint32_t seed=0;seed<65536;++seed){expected[seed]=r.u16();check(rng::advance_seed(static_cast<std::uint16_t>(seed))==expected[seed],"RNG differs from original ARM");}
 for(std::uint16_t initial:{0,1,32768,65535}){auto native=initial,original=initial;for(unsigned i=0;i<256;++i){native=rng::advance_seed(native);original=expected[original];check(native==original,"Sequential seed ownership differs");}}
 std::size_t mapped_reads=0,gate_reads=0;
 for(std::uint32_t i=0;i<nm;++i){const auto id=r.u32(),mode=r.u32(),mapped=r.u32(),gate=r.u32(),returned=r.u32(),mask=r.u32();check(id<256&&mapped<256&&gate<256&&mask<4,"Fixture remap field domain");std::array<ps::AreaMappingRecord,256> mapping{};if(mask&1)mapping[id].mapped=static_cast<std::uint8_t>(mapped);if(mask&2)mapping[id].gate=static_cast<std::uint8_t>(gate);ps::AreaContext context{mode,mapping,{}};
  const auto result=ps::remap_area(static_cast<std::uint8_t>(id),context);check(std::holds_alternative<std::uint32_t>(result)&&std::get<std::uint32_t>(result)==returned,"Remap differs from complete original");check(context.mode_c8c==mode,"Borrowed mode mutated");check(mapping[id].mapped==((mask&1)?std::optional<std::uint8_t>{static_cast<std::uint8_t>(mapped)}:std::nullopt)&&mapping[id].gate==((mask&2)?std::optional<std::uint8_t>{static_cast<std::uint8_t>(gate)}:std::nullopt),"Borrowed mapping mutated");
  context.mode_c8c.reset();unknown(ps::remap_area(static_cast<std::uint8_t>(id),context));context.mode_c8c=mode;
  if(mask&1){++mapped_reads;const auto known=mapping[id].mapped;mapping[id].mapped.reset();unknown(ps::remap_area(static_cast<std::uint8_t>(id),context));mapping[id].mapped=known;}
  if(mask&2){++gate_reads;mapping[id].gate.reset();unknown(ps::remap_area(static_cast<std::uint8_t>(id),context));}
 }
 check(r.at==r.bytes.size(),"Trailing fixture bytes");unknown(ps::remap_area({},{}));unknown(ps::remap_area(0,{}));unknown(ps::remap_area({},ps::AreaContext{0,{},{}}));check(std::get<std::uint32_t>(ps::remap_area(255,{0,{},{}}))==255,"Non13 path consumed mapping");check(std::get<std::uint32_t>(ps::remap_area(0,{13,{},{}}))==0,"Zero ID consumed mapping");
 std::cout<<"{\"status\":\"passed\",\"rng_seeds\":65536,\"remap_cases\":2048,\"mapped_reads\":"<<mapped_reads<<",\"gate_reads\":"<<gate_reads<<",\"checks\":"<<checks<<"}\n";
}
}
int main(int argc,char** argv){try{if(argc!=3)throw std::runtime_error("Expected fixture and SHA");run(argv[1],argv[2]);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
