#include "model_resource.hpp"
#include "picosha2.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace m=mh4u::model_resource;
namespace {
std::size_t checks;
void check(bool x,const char* message){++checks;if(!x)throw std::runtime_error(message);}
void put(std::string& b,std::size_t p,std::uint32_t v,unsigned n=4){for(unsigned k=0;k<n;++k)b.at(p+k)=static_cast<char>(v>>(8*k));}
std::string sha(std::string_view b){return picosha2::hash256_hex_string(b.begin(),b.end());}
m::Resource load(std::string_view b,m::Limits limits={}){auto v=m::Resource::decode(b,sha(b),limits);check(std::holds_alternative<m::Resource>(v),"Resource must preserve independent geometry errors");return std::get<m::Resource>(std::move(v));}
m::Geometry geometry(const m::Resource& r,std::size_t i=0){auto v=r.geometry(i);if(auto* e=std::get_if<m::Error>(&v))throw std::runtime_error(e->message);++checks;return std::get<m::Geometry>(v);}
void bad(const m::Resource& r,m::ErrorCode e,std::size_t i=0){auto v=r.geometry(i);check(std::holds_alternative<m::Error>(v),"Expected geometry rejection");check(std::get<m::Error>(v).code==e,"Wrong geometry error");}
std::string fixture(unsigned stride=28,std::uint32_t hash=0xd6784014){
 const unsigned vb=224,ib=vb+15*stride;std::string b(ib+20,'\0');
 put(b,0,0x444f4d);put(b,4,230,2);put(b,8,2,2);put(b,0x34,128);put(b,0x38,vb);put(b,0x18,15*stride);put(b,0x3c,ib);
 put(b,130,5,2);put(b,138,stride,1);put(b,139,4,1);put(b,140,10);put(b,148,hash);put(b,156,9);
 for(unsigned j=0;j<5;++j){const auto p=vb+(j+10)*stride;
  put(b,p,0x7fc12345);put(b,p+4,0x80000000);put(b,p+8,0x7f800000);put(b,p+16,0x3f123456);put(b,p+20,0xff800000);
  put(b,p+12,0x7f00ff80);put(b,p+24,0x070003ff);
  if(stride==36){put(b,p+28,0xff7f0180);put(b,p+32,0x01000809);}
 }
 unsigned k=0;for(auto index:{10,11,12,12,13,14,65535,14,13,12})put(b,ib+2*k++,index,2);
 return b;
}
void boundaries(){
 auto bytes=fixture();auto r=load(bytes);auto g=geometry(r);auto copy=r;
 check(g.resource_identity==r.identity()&&g.primitive_index==0,"Geometry owner identity");
 check(g.stride==28&&g.layout==m::VertexLayout::obj2wt&&g.vertex_start==10,"Source format metadata");
 check(g.vertex_source_offset==504&&g.index_source_offset==644,"Source offsets");
 check(g.vertices.size()==5&&g.raw_indices.size()==10&&g.triangles.size()==3,"Expanded sizes");
 check(g.restart_count==1&&g.degenerate_triangles==2,"Strip statistics");
 check(g.triangles[0]==m::Triangle{0,1,2}&&g.triangles[1]==m::Triangle{2,4,3}&&g.triangles[2]==m::Triangle{4,3,2},"Restart and degenerate parity");
 const auto v=g.vertices[0];check(v.position_bits==std::array<std::uint32_t,3>{0x7fc12345,0x80000000,0x7f800000}&&v.uv_bits==std::array<std::uint32_t,2>{0x3f123456,0xff800000},"Raw float bits without arithmetic");
 check(v.normal==std::array<std::int8_t,4>{-128,-1,0,127},"Raw signed normal four components");
 check(v.influence_count==2&&!v.auxiliary&&v.joints[0]==255&&v.joints[1]==3&&v.weights[0]==0&&v.weights[1]==7,"Zero-weight index and unnormalized weights preserved");
 check(geometry(copy).vertices.data()==g.vertices.data()&&geometry(r).triangles.data()==g.triangles.data(),"Stable borrowed geometry across copies and repeated access");
 bytes.assign(bytes.size(),'x');check(g.vertices[0]==v,"Geometry independent of caller bytes");
 bad(r,m::ErrorCode::unsupported_layout,1);bad(r,m::ErrorCode::table_extent,2);
 auto retained=[](){return load(fixture());}();check(geometry(retained).vertices[0]==v,"Owner lifetime after move/return");
 for(auto hash:{0x01b36016U,0xf606f017U}){r=load(fixture(36,hash));g=geometry(r);check(g.layout==(hash==0x01b36016?m::VertexLayout::obj4wt:m::VertexLayout::obj4wt_t),"Four influence layout");check(g.vertices[0].influence_count==4&&g.vertices[0].auxiliary==std::array<std::int8_t,4>{-128,1,127,-1},"Auxiliary signed bytes");check(g.vertices[0].joints==std::array<std::uint8_t,4>{255,3,9,8}&&g.vertices[0].weights==std::array<std::uint8_t,4>{0,7,0,1},"Four raw influences with zero weights");}
 auto invalid=[&](std::size_t p,std::uint32_t value,unsigned n,m::ErrorCode error){auto b=fixture();put(b,p,value,n);bad(load(b),error);};
 invalid(148,0x12345678,4,m::ErrorCode::unsupported_layout);invalid(138,36,1,m::ErrorCode::unsupported_layout);invalid(139,5,1,m::ErrorCode::unsupported_layout);
 auto b=fixture();put(b,139,0xc4,1);geometry(load(b)); //Unconsumed type high2 bits.
 invalid(0x38,0xffffffff,4,m::ErrorCode::malformed_resource);invalid(0x18,1,4,m::ErrorCode::malformed_resource);
 invalid(140,0xffffffff,4,m::ErrorCode::malformed_resource);invalid(144,0xffffffff,4,m::ErrorCode::malformed_resource);
 invalid(0x3c,0xffffffff,4,m::ErrorCode::malformed_resource);invalid(152,0xffffffff,4,m::ErrorCode::malformed_resource);
 invalid(156,0xffffffff,4,m::ErrorCode::resource_limit);invalid(644,9,2,m::ErrorCode::malformed_resource);invalid(644,15,2,m::ErrorCode::malformed_resource);
 b=fixture();b.pop_back();bad(load(b),m::ErrorCode::malformed_resource);
 for(auto member:{&m::Limits::geometry_vertices,&m::Limits::geometry_indices,&m::Limits::geometry_triangles}){m::Limits limits;limits.*member=1;bad(load(fixture(),limits),m::ErrorCode::resource_limit);}
 b=fixture();for(unsigned i=0;i<10;++i)put(b,644+2*i,65535,2);put(b,130,0,2);r=load(b);g=geometry(r);check(g.vertices.empty()&&g.triangles.empty()&&g.restart_count==10,"Empty vertices with restart-only strip");
 b=fixture();put(b,156,0);r=load(b);g=geometry(r);check(g.raw_indices.size()==1&&g.triangles.empty(),"Source strip count is word plus one");
 b=fixture();put(b,0x10,0xffffffff);geometry(load(b)); //Global count cannot be used as physical extent.
 b=fixture();b.replace(176,48,b.substr(128,48));m::Limits limits;limits.geometry_vertices=5;r=load(b,limits);geometry(r);bad(r,m::ErrorCode::resource_limit,1);
 limits={};limits.geometry_indices=10;r=load(b,limits);geometry(r);bad(r,m::ErrorCode::resource_limit,1);
 limits={};limits.geometry_triangles=3;r=load(b,limits);geometry(r);bad(r,m::ErrorCode::resource_limit,1);
 // Unsupported first primitive does not consume budgets or suppress valid second.
 put(b,148,0);limits={};limits.geometry_vertices=5;r=load(b,limits);bad(r,m::ErrorCode::unsupported_layout);g=geometry(r,1);check(g.primitive_index==1,"Per primitive failure isolation");
 // Invalid strip after bounds validation consumes admitted scan budget, deterministically.
 b=fixture();b.replace(176,48,b.substr(128,48));put(b,644,9,2);limits={};limits.geometry_indices=10;r=load(b,limits);bad(r,m::ErrorCode::malformed_resource);bad(r,m::ErrorCode::resource_limit,1);
 // No palette/index repair is performed by raw geometry owner.
 b=fixture();put(b,165,255,1);r=load(b);g=geometry(r);check(g.vertices[0].joints[0]==255,"Palette facts not replaced by geometry parsing");
 std::cout<<"{\"boundary_checks\":"<<checks<<"}\n";
}
struct Reader {std::string_view bytes;std::size_t offset{};
 std::string_view take(std::size_t n){if(offset>bytes.size()||n>bytes.size()-offset)throw std::runtime_error("Fixture truncated");auto s=bytes.substr(offset,n);offset+=n;return s;}
 std::uint32_t u(unsigned n=4){auto b=take(n);std::uint32_t value=0;for(unsigned k=0;k<n;++k)value|=std::uint32_t(static_cast<unsigned char>(b[k]))<<(8*k);return value;}
};
std::string read(const char* name){std::ifstream f(name,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Fixture open");auto n=f.tellg();if(n<0||n>16*1024*1024)throw std::runtime_error("Fixture allocation bound");std::string b(static_cast<std::size_t>(n),'\0');f.seekg(0);if(!f.read(b.data(),n)||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture read or size changed");return b;}
void source(const char* fixture_path,const char* expected,char** paths){
 auto bytes=read(fixture_path);check(sha(bytes)==expected,"Fixture identity");Reader in{bytes};check(in.take(8)=="M4GEO002"&&in.u()==3,"Fixture header");std::size_t vertices=0,triangles=0,primitives=0,joints=0;
 for(unsigned s=0;s<3;++s){auto digest=in.take(64);auto result=m::Resource::load(paths[s],digest);check(std::holds_alternative<m::Resource>(result),"Real MOD identity/load");auto r=std::get<m::Resource>(std::move(result));const auto length=in.u(),count=in.u(),joint_count=in.u();check(r.bytes().size()==length&&r.primitives().size()==count&&r.joint_count()==joint_count,"Source counts");auto sk=r.skeleton();check(sk&&sk->joint_records_parsed.size()==joint_count,"Parsed source joint records");
  for(const auto& j:sk->joint_records_parsed){for(auto b:j.fields)check(b==in.u(1),"Joint source byte");for(auto b:j.parameter_bits)check(b==in.u(),"Joint raw parameter bits");++joints;}
  for(unsigned p=0;p<count;++p){auto g=geometry(r,p);const auto hash=in.u(),stride=in.u(),vo=in.u(),io=in.u(),start=in.u(),n=in.u(),ni=in.u(),nt=in.u(),nr=in.u(),nd=in.u();check(r.primitives()[p].input_layout_hash==hash&&g.stride==stride&&g.vertex_source_offset==vo&&g.index_source_offset==io&&g.vertex_start==start,"Geometry metadata source facts");check(g.resource_identity==r.identity()&&g.primitive_index==p,"Source identity propagated");check(g.vertices.size()==n&&g.raw_indices.size()==ni&&g.triangles.size()==nt&&g.restart_count==nr&&g.degenerate_triangles==nd,"Geometry counts/statistics");
   for(const auto& v:g.vertices){for(auto b:v.position_bits)check(b==in.u(),"Raw position bits differ");for(auto b:v.uv_bits)check(b==in.u(),"Raw UV bits differ");for(auto b:v.normal)check(std::bit_cast<std::uint8_t>(b)==in.u(1),"Signed normal bytes differ");for(auto b:v.joints)check(b==in.u(1),"Palette index including zero weights differs");for(auto b:v.weights)check(b==in.u(1),"Unnormalized weight differs");check(v.influence_count==in.u(),"Influence count");check(v.auxiliary.has_value()==bool(in.u()),"Auxiliary presence");for(unsigned k=0;k<4;++k)check((v.auxiliary?std::bit_cast<std::uint8_t>((*v.auxiliary)[k]):0)==in.u(1),"Auxiliary raw byte");}
   for(auto index:g.raw_indices)check(index==in.u(2),"Raw strip index");for(const auto& triangle:g.triangles)for(auto index:triangle)check(index==in.u(),"Expanded triangle differs");vertices+=n;triangles+=nt;++primitives;
  }
 }
 check(in.offset==bytes.size(),"Fixture trailing bytes");std::cout<<"{\"models\":3,\"primitives\":"<<primitives<<",\"joints\":"<<joints<<",\"vertices\":"<<vertices<<",\"triangles\":"<<triangles<<",\"checks\":"<<checks<<"}\n";
}
}
int main(int argc,char** argv){try{if(argc==1)boundaries();else if(argc==6)source(argv[1],argv[2],argv+3);else throw std::runtime_error("Expected no arguments, or fixture sha256 three_MOD_paths");return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
