#include "vertex_skinning.hpp"
#include "picosha2.h"
#include <bit>
#include <cfenv>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#if defined(__x86_64__) || defined(_M_X64)
#include <xmmintrin.h>
#endif
namespace {
namespace vs=mh4u::vertex_skinning;namespace mr=mh4u::model_resource;
std::size_t checks{},fixture_cases{},real_queries{},typed_errors{},numeric_words{};
void check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
vs::Palette identity(){return{bits(1),0,0,0,0,bits(1),0,0,0,0,bits(1),0};}
vs::View3 view(){return{bits(1),0,0,0,bits(1),0,0,0,bits(1)};}
mr::Vertex vertex(){mr::Vertex v{};v.position_bits={bits(1),bits(2),bits(3)};v.normal={1,0,0,-128};v.weights={255,0,0,0};v.influence_count=2;return v;}
vs::Output result(vs::Result r){if(const auto* e=std::get_if<vs::Error>(&r))throw std::runtime_error(std::string(e->message));return std::get<vs::Output>(r);}
void rejects(vs::Result r,vs::ErrorCode code){const auto* e=std::get_if<vs::Error>(&r);check(e&&e->code==code,"Typed skinning rejection differs");}
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
struct FloatControl {
 std::uint64_t original;
 FloatControl(){
#if defined(__aarch64__)
  __asm__ volatile("mrs %0, fpcr":"=r"(original));
#else
  original=_mm_getcsr();
#endif
 }
 void set(std::uint64_t x){
#if defined(__aarch64__)
  __asm__ volatile("msr fpcr, %0\n\tisb"::"r"(x):"memory");
#else
  _mm_setcsr(static_cast<unsigned>(x));
#endif
 }
 ~FloatControl(){set(original);}
};
#endif
void boundaries(){
 const std::array<vs::Palette,1> palette{identity()};const auto v=vertex();const auto base=result(vs::skin(v,palette,view()));
 check(vs::weight_scale_bits==0x3b808000U,"Weight literal");check(base.position_model[0]==0x3f7fff00U,"Sum255 must not be normalized to1");check(base.normal_skin[0]==0x3f7fff00U,"Signed normals are not divided by127");
 auto altered=v;altered.joints[2]=altered.joints[3]=255;altered.weights[2]=altered.weights[3]=255;altered.uv_bits={0x7fc12345,0x7f800000};altered.auxiliary=std::array<std::int8_t,4>{-128,127,44,-11};check(result(vs::skin(altered,palette,view()))==base,"Absent slots/UV/auxiliary must remain unconsumed");
 for(auto count:{0,1,3,5}){altered=v;altered.influence_count=static_cast<std::uint8_t>(count);rejects(vs::skin(altered,palette,view()),vs::ErrorCode::unsupported_influences);}
 altered=v;altered.joints[1]=1;const auto missing=vs::skin(altered,palette,view());rejects(missing,vs::ErrorCode::invalid_palette_index);check(std::get<vs::Error>(missing).influence==1&&std::get<vs::Error>(missing).palette_index==1,"Zero-weight invalid index retains identity");
 rejects(vs::skin(v,{},view()),vs::ErrorCode::invalid_palette_index);rejects(vs::skin(v,palette,view(),{0}),vs::ErrorCode::palette_limit);
 std::array<vs::Palette,2> bad{identity(),identity()};bad[1][0]=0x7fc12345;check(result(vs::skin(v,bad,view()))==base,"Unreferenced nonfinite palette is not consumed");altered=v;altered.joints[1]=1;rejects(vs::skin(altered,bad,view()),vs::ErrorCode::nonfinite_operand);
 altered=v;altered.position_bits[0]=0x7f800000;rejects(vs::skin(altered,palette,view()),vs::ErrorCode::nonfinite_operand);auto bad_view=view();bad_view[0]=0x7fc12345;rejects(vs::skin(v,palette,bad_view),vs::ErrorCode::nonfinite_operand);
 altered=v;altered.normal={0,0,0,127};rejects(vs::skin(altered,palette,view()),vs::ErrorCode::degenerate_normal);altered=v;altered.weights={0,0,0,0};rejects(vs::skin(altered,palette,view()),vs::ErrorCode::degenerate_normal);
 bad_view={};rejects(vs::skin(v,palette,bad_view),vs::ErrorCode::degenerate_normal);bad_view=view();bad_view[0]=bits(1e-30F);rejects(vs::skin(v,palette,bad_view),vs::ErrorCode::degenerate_normal);
 bad[0][0]=0x7f7fffff;altered=v;altered.position_bits[0]=bits(2);rejects(vs::skin(altered,bad,view()),vs::ErrorCode::numeric_overflow);
 const auto before=altered;const auto palette_before=bad;rejects(vs::skin(altered,bad,view()),vs::ErrorCode::numeric_overflow);check(altered==before&&bad==palette_before,"Rejected skinning changed inputs");
 const int rounding=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"Set rounding");const auto wrong_round=vs::skin(v,palette,view());check(std::fesetround(rounding)==0,"Restore rounding");rejects(wrong_round,vs::ErrorCode::floating_environment);
#if defined(__aarch64__) || defined(__x86_64__) || defined(_M_X64)
 {
  FloatControl control;
#if defined(__aarch64__)
  constexpr std::array<std::uint64_t,1> masks{1ULL<<24};
#else
  constexpr std::array<std::uint64_t,3> masks{1ULL<<15,1ULL<<6,(1ULL<<15)|(1ULL<<6)};
#endif
  for(auto mask:masks){control.set(control.original|mask);rejects(vs::skin(v,palette,view()),vs::ErrorCode::floating_environment);}
 }
#endif
 check(result(vs::skin(v,palette,view()))==base,"Environment restored after negative controls");
}
struct Reader {
 const std::string& data;std::size_t offset{};
 std::uint8_t byte(){check(offset<data.size(),"Truncated skinning fixture");return static_cast<std::uint8_t>(data[offset++]);}
 std::uint32_t word(){std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(byte())<<(8*i);return v;}
 template<std::size_t N>std::array<std::uint32_t,N> words(){std::array<std::uint32_t,N> a;for(auto& v:a)v=word();return a;}
 mr::Vertex vertex(){mr::Vertex v{};v.position_bits=words<3>();v.uv_bits=words<2>();for(auto& n:v.normal)n=std::bit_cast<std::int8_t>(byte());for(auto& n:v.joints)n=byte();for(auto& n:v.weights)n=byte();const auto influences=word(),aux=word();check(influences<=255&&aux<=1,"Fixture vertex metadata");v.influence_count=static_cast<std::uint8_t>(influences);std::array<std::int8_t,4> values;for(auto& n:values)n=std::bit_cast<std::int8_t>(byte());if(aux)v.auxiliary=values;return v;}
};
void fixture(const char* path,const char* hash,const std::array<const char*,3>& paths){
 std::ifstream stream(path,std::ios::binary|std::ios::ate);check(bool(stream),"Fixture open");const auto bytes=stream.tellg();check(bytes>=0&&bytes<=32*1024*1024,"Fixture size bound");std::string data(static_cast<std::size_t>(bytes),'\0');stream.seekg(0);stream.read(data.data(),bytes);check(bool(stream),"Fixture read");check(picosha2::hash256_hex_string(data)==hash,"Fixture SHA256 mismatch");
 Reader r{data};check(data.substr(0,8)=="M4SKIN01","Fixture magic");r.offset=8;const auto count=r.word(),source_count=r.word();check(count<=100000&&source_count==3,"Fixture counts");std::vector<mr::Resource> sources;
 for(unsigned i=0;i<3;++i){std::array<std::uint8_t,32> digest;for(auto& b:digest)b=r.byte();std::string hex;picosha2::bytes_to_hex_string(digest.begin(),digest.end(),hex);const auto size=r.word();auto loaded=mr::Resource::load(paths[i],hex);if(const auto* e=std::get_if<mr::Error>(&loaded))throw std::runtime_error(e->message);sources.push_back(std::get<mr::Resource>(std::move(loaded)));check(sources.back().bytes().size()==size,"Source size");}
 struct Group {std::vector<vs::Palette> palette;vs::View3 view;};const auto group_count=r.word();check(group_count>0&&group_count<=1024,"Palette group count");std::vector<Group> groups;
 for(unsigned i=0;i<group_count;++i){const auto n=r.word();check(n<=24,"Fixture palette bound");Group g;g.view=r.words<9>();for(unsigned j=0;j<n;++j)g.palette.push_back(r.words<12>());groups.push_back(std::move(g));}
 for(unsigned i=0;i<count;++i){const auto source=r.word(),primitive=r.word(),index=r.word(),group=r.word(),error=r.word();check(source<=3&&group<groups.size()&&error<=7,"Case metadata");auto vertex=r.vertex();const mr::Vertex* input=&vertex;const auto expected=r.words<12>();
  if(source){const auto& owner=sources[source-1];const auto geometry=owner.geometry(primitive);const auto* g=std::get_if<mr::Geometry>(&geometry);check(g&&g->resource_identity==owner.identity()&&g->primitive_index==primitive&&index<g->vertices.size(),"Source geometry owner");check(vertex==g->vertices[index],"Vertex differs from canonical source owner");input=&g->vertices[index];++real_queries;}
  const auto saved=*input;const auto palette_before=groups[group].palette;const auto view_before=groups[group].view;const auto got=vs::skin(*input,groups[group].palette,groups[group].view);check(*input==saved&&groups[group].palette==palette_before&&groups[group].view==view_before,"Skinning mutated borrowed inputs");
  if(error){const auto* failure=std::get_if<vs::Error>(&got);if(!failure||failure->code!=static_cast<vs::ErrorCode>(error-1))throw std::runtime_error("Typed source-algebra rejection differs at query "+std::to_string(i));++checks;++typed_errors;}
  else{const auto out=result(got);const std::array<vs::Bits3,4> fields{out.position_model,out.normal_skin,out.normal_view,out.normal_unit};for(unsigned f=0;f<4;++f)for(unsigned c=0;c<3;++c){if(fields[f][c]!=expected[f*3+c])throw std::runtime_error("Source algebra differs at query "+std::to_string(i)+" field "+std::to_string(f)+" component "+std::to_string(c));++checks;++numeric_words;}}
  ++fixture_cases;
 }
 check(r.offset==data.size(),"Trailing fixture bytes");
}
}
int main(int argc,char** argv){try{boundaries();if(argc==6)fixture(argv[1],argv[2],{argv[3],argv[4],argv[5]});else check(argc==1,"Usage: vertex-skinning-tests [fixture sha256 em062.mod body001.mod one133.mod]");std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"fixture_cases\":"<<fixture_cases<<",\"real_queries\":"<<real_queries<<",\"numeric_words\":"<<numeric_words<<",\"typed_errors\":"<<typed_errors<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
