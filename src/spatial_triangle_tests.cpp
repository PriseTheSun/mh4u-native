// Expected bytes are retained original ARM observations, never native calculations.
#include "spatial_triangle.hpp"
#include "picosha2.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace st=mh4u::spatial_triangle;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks=0;
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
struct Reader {
 const Bytes& data;std::size_t cursor=0;
 Bytes bytes(std::size_t n){if(n>data.size()-cursor)throw std::runtime_error("Truncated fixture");Bytes out(data.begin()+cursor,data.begin()+cursor+n);cursor+=n;return out;}
 std::uint32_t u32(){if(data.size()-cursor<4)throw std::runtime_error("Truncated fixture word");std::uint32_t x=0;for(unsigned k=0;k<4;++k)x|=std::uint32_t(data[cursor++])<<(8*k);return x;}
 template<std::size_t N>std::array<std::uint32_t,N> words(){std::array<std::uint32_t,N> out;for(auto& x:out)x=u32();return out;}
};
Bytes read(const char* path){
 std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open fixture");
 const auto end=in.tellg();if(end<0||end>1024*1024)throw std::runtime_error("Fixture size limit");
 Bytes data(static_cast<std::size_t>(end));in.seekg(0);if(!in.read(reinterpret_cast<char*>(data.data()),static_cast<std::streamsize>(data.size()))||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture read changed or failed");return data;
}
std::string hex(const Bytes& data){constexpr char digits[]="0123456789abcdef";std::string out;out.reserve(data.size()*2);for(auto b:data){out+=digits[b>>4];out+=digits[b&15];}return out;}
void append(Bytes& out,std::uint32_t x){for(unsigned i=0;i<4;++i)out.push_back(static_cast<std::uint8_t>(x>>(8*i)));}
Bytes encode(const st::Hit& hit){Bytes out;out.reserve(41);for(auto x:hit.position)append(out,x);for(auto x:hit.normal)append(out,x);append(out,hit.scalar0_bits);append(out,hit.t_bits);out.push_back(hit.flag);return out;}
void run(const Bytes& bytes){
 Reader r{bytes};check(r.bytes(8)==Bytes({'M','H','4','U','T','R','0','1'}),"Fixture schema");
 check(r.u32()==274&&r.u32()==1464&&r.u32()==102,"Fixture coverage");
 check(hex(r.bytes(32))=="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","Original ELF identity");
 check(hex(r.bytes(32))=="3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07","Source SBC identity");
 check(hex(r.bytes(32))=="af5736c33a28e890700c7e599f268775537470e174c6fe34917375a52cd8fbda","Original trace identity");
 std::array<unsigned,274> cases{};std::array<unsigned,86> pairs{};std::array<unsigned,5> kinds{};
 std::size_t hits=0,misses=0,compared=0;std::uint32_t previous_case=0,previous_step=0;
 for(unsigned i=0;i<1464;++i){const auto record=r.words<8>();const auto ci=record[0],si=record[1],pair=record[2],face=record[3],attribute=record[4],seed=record[5],kind=record[6],wanted=record[7];
  check(ci<274&&pair<86&&face<86&&attribute<2&&kind<5&&wanted<=1,"Record domains");
  check(seed==0||seed==165||seed==255,"Source poison identity");
  check(i==0?(ci==0&&si==0):(ci==previous_case?si==previous_step+1:ci==previous_case+1&&si==0),"Original case/step ordering");
  check(kind==3?si<86:si==0,"Original per-case coverage");previous_case=ci;previous_step=si;++cases[ci];++pairs[pair];++kinds[kind];
  st::Triangle triangle;for(auto& vertex:triangle.vertices)vertex=r.words<3>();const auto source_normal=r.words<3>();
  st::Vertical ray;ray.origin=r.words<3>();ray.direction_y=r.u32();const auto test_vector=r.words<3>();
  check(*ray.direction_y==0xbf800000U&&test_vector==st::Bits3{0,0xbf800000U,0},"Observed vertical profile");
  Bytes expected;std::array<std::uint32_t,4> observed_source{};if(wanted){expected=r.bytes(41);observed_source=r.words<4>();}
  const auto before_triangle=triangle.vertices;const auto before_origin=ray.origin;const auto before_direction=ray.direction_y;
  try{
   const auto gate=st::normal_gate(source_normal,test_vector);check(std::holds_alternative<bool>(gate)&&std::get<bool>(gate),"Original gate passed before cc94f0");
   const auto result=st::intersect(triangle,ray,true);check(std::holds_alternative<st::Output>(result),"Original completed query rejected by native profile");const auto& out=std::get<st::Output>(result);
   check(out.intersects==bool(wanted)&&bool(out.hit)==bool(wanted),"Original hit/miss and output presence");
   if(wanted){const auto actual=encode(*out.hit);check(actual.size()==41,"Complete captured hit extent");for(unsigned k=0;k<41;++k)check(actual[k]==expected[k],"Original raw hit byte mismatch");compared+=41;
    check(observed_source==st::Bits4{source_normal[0],source_normal[1],source_normal[2],0},"Original source-normal attribute is separate");
    check(out.hit->normal!=observed_source,"Do not replace hit normal with source attribute normal");++hits;
   }else ++misses;
   check(triangle.vertices==before_triangle&&ray.origin==before_origin&&ray.direction_y==before_direction,"Borrowed numeric input mutated");
  }catch(const std::exception& e){throw std::runtime_error("Case "+std::to_string(ci)+" step "+std::to_string(si)+" pair "+std::to_string(pair)+": "+e.what());}
 }
 check(r.cursor==bytes.size(),"Trailing fixture bytes");for(auto n:cases)check(n==1||n==86,"Complete source case coverage");for(auto n:pairs)check(n>=3,"All86 source pairs covered");
 check(kinds==std::array<unsigned,5>{86,86,86,1204,2},"Original case-kind counts");check(hits==102&&misses==1362&&compared==4182,"Original result totals");
 std::cout<<"{\"status\":\"passed\",\"cases\":274,\"queries\":1464,\"hits\":"<<hits<<",\"misses\":"<<misses<<",\"compared_hit_bytes\":"<<compared<<",\"checks\":"<<checks<<"}\n";
}
}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("Usage: spatial-triangle-tests fixture sha256");
 const auto data=read(argv[1]);check(picosha2::hash256_hex_string(data)==argv[2],"Fixture SHA256 mismatch");run(data);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
