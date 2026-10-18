#include "joint_bootstrap.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace jb=mh4u::joint_bootstrap;namespace jp=mh4u::joint_pose;namespace mr=mh4u::model_resource;
using Bytes=std::vector<std::uint8_t>;std::size_t checks{};
void check(bool b,const char* text){++checks;if(!b)throw std::runtime_error(text);}
std::uint32_t word(const Bytes& b,std::size_t p){if(p>b.size()||b.size()-p<4)throw std::runtime_error("fixture bounds");return std::uint32_t(b[p])|std::uint32_t(b[p+1])<<8|std::uint32_t(b[p+2])<<16|std::uint32_t(b[p+3])<<24;}
struct Reader{const Bytes& b;std::size_t p{};std::uint32_t u32(){auto v=word(b,p);p+=4;return v;}Bytes bytes(std::size_t n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("fixture extent");Bytes v(b.begin()+p,b.begin()+p+n);p+=n;return v;}};
template<std::size_t N>std::array<std::uint32_t,N> words(const Bytes& b,std::size_t p){std::array<std::uint32_t,N> out;for(std::size_t i=0;i<N;++i)out[i]=word(b,p+i*4);return out;}
jp::Matrix identity(){return {0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000,0,0,0,0,0x3f800000};}
void boundaries(){
 const jp::Bits3 scale{0x7fa12345,0x80000000,0xff800000};std::vector<mr::JointRecord> rec(1);rec[0].fields={17,255,7,9};rec[0].parameter_bits={0,0x7fc12345,0x80000000,0x7fa56789,0xff800000};
 std::vector<jp::Matrix> mats{identity()};std::vector<std::uint32_t> prior{0xaabbccdd};const auto pristine=rec;
 auto run=[&]{return jb::initialize(rec,mats,prior,scale);};
 auto success=run();check(std::holds_alternative<jb::Output>(success),"raw copies must accept nonfinite unconsumed words");const auto& out=std::get<jb::Output>(success);
 check(out.metadata[0].mapping98==0xaaba0007,"preserved mapping high bits");check(out.joints[0].reference_translation==jp::Bits3{0x80000000,0x7fa56789,0xff800000},"record translation bits");
 check(out.joints[0].scale==jp::Bits4{scale[0],scale[1],scale[2],0},"raw scale and zero padding");check(out.parameter8c[0]==0x7fc12345,"raw parameter8c");
 auto error=[&](const jb::Result& r,jb::ErrorCode c){check(std::holds_alternative<jb::Error>(r),"expected error");check(std::get<jb::Error>(r).code==c,"wrong error");};
 error(jb::initialize(rec,mats,prior,std::nullopt),jb::ErrorCode::missing_scale);
 error(jb::initialize(rec,{},prior,scale),jb::ErrorCode::extent);
 error(jb::initialize(rec,mats,{},scale),jb::ErrorCode::extent);
 error(jb::initialize(rec,mats,prior,scale,{0}),jb::ErrorCode::resource_limit);
 rec[0].fields[1]=2;error(run(),jb::ErrorCode::parent_extent);
 rec[0].fields[1]=0;error(run(),jb::ErrorCode::parent_cycle);rec=pristine;
 mats[0][0]=0x7fc12345;error(run(),jb::ErrorCode::matrix);check(rec==pristine,"input resource mutated on error");mats[0]=identity();
 rec.resize(256,rec[0]);mats.resize(256,identity());prior.resize(256,0);error(jb::initialize(rec,mats,prior,scale,{256}),jb::ErrorCode::resource_limit);
 check(std::holds_alternative<jb::Output>(jb::initialize({}, {}, {},std::nullopt)),"zero joints consume no scale");
}
std::size_t oracle(const Bytes& bytes){Reader r{bytes};check(r.bytes(8)==Bytes({'M','H','4','U','J','B','0','1'}),"fixture magic");auto count=r.u32();check(count<=10000,"fixture cases bound");
 for(std::size_t c=0;c<count;++c){const auto n=r.u32();check(n<=255,"joint bound");const auto raw=r.bytes(n*24),mat=r.bytes(n*64);std::vector<mr::JointRecord> rec(n);std::vector<jp::Matrix> matrices(n);std::vector<std::uint32_t> prior(n);
  for(std::size_t i=0;i<n;++i){std::copy_n(raw.begin()+i*24,4,rec[i].fields.begin());rec[i].parameter_bits=words<5>(raw,i*24+4);matrices[i]=words<16>(mat,i*64);prior[i]=r.u32();}
  jp::Bits3 scale;for(auto& w:scale)w=r.u32();auto expected=r.bytes(n*168),order=r.bytes(n);const auto oldrec=rec;const auto oldmat=matrices;const auto oldprior=prior;
  auto result=jb::initialize(rec,matrices,prior,scale);check(std::holds_alternative<jb::Output>(result),"ARM fixture rejected");const auto& out=std::get<jb::Output>(result);
  check(rec==oldrec&&matrices==oldmat&&prior==oldprior,"borrowed state changed");check(out.order==order,"ARM order mismatch");check(out.callbacks_null&&out.owners_self,"callback/owner facts");
  for(std::size_t i=0;i<n;++i){const auto off=i*168;const auto& j=out.joints[i];check(j.reference_translation==words<3>(expected,off+0x48),"referenceT mismatch");check(j.rotation==words<4>(expected,off+0x58),"rotation mismatch");check(j.scale==words<4>(expected,off+0x68),"scale mismatch");check(j.translation==words<4>(expected,off+0x78),"translation mismatch");check(out.matrices[i]==words<16>(expected,off+8),"matrix mismatch");check(out.metadata[i].flags94==word(expected,off+0x94),"flags mismatch");check(out.metadata[i].mapping98==word(expected,off+0x98),"mapping mismatch");check(out.depth90[i]==word(expected,off+0x90),"depth mismatch");check(out.parameter8c[i]==word(expected,off+0x8c),"parameter mismatch");check(word(expected,off+0x88)==0&&word(expected,off+0xa0)==0x2000000,"original callback/owner mismatch");}
 }
 check(r.p==bytes.size(),"fixture trailing");return count;
}
}
int main(int argc,char** argv){try{boundaries();std::size_t cases{};if(argc!=1&&argc!=3)throw std::runtime_error("usage: joint-bootstrap-tests [oracle.bin sha256]");if(argc==3){std::ifstream in(argv[1],std::ios::binary|std::ios::ate);check(bool(in),"fixture open");auto size=in.tellg();check(size>=0&&size<=16*1024*1024,"fixture allocation");in.seekg(0);Bytes bytes(static_cast<std::size_t>(size));in.read(reinterpret_cast<char*>(bytes.data()),size);check(bool(in),"fixture read");check(picosha2::hash256_hex_string(bytes)==argv[2],"fixture hash");cases=oracle(bytes);}std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_cases\":"<<cases<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
