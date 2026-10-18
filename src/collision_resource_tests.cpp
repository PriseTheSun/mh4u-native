#include "collision_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
namespace cr=mh4u::collision_resource;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks=0;
void check(bool v,const char* text){++checks;if(!v)throw std::runtime_error(text);}
struct Reader {
 const Bytes& data;std::size_t at{};
 std::uint32_t u32(){if(data.size()-at<4)throw std::runtime_error("Truncated fixture");std::uint32_t n=0;for(unsigned i=0;i<4;++i)n|=std::uint32_t(data[at++])<<(8*i);return n;}
 Bytes bytes(std::size_t n){if(n>data.size()-at)throw std::runtime_error("Truncated fixture blob");Bytes out(data.begin()+at,data.begin()+at+n);at+=n;return out;}
 Bytes blob(std::size_t cap){const auto n=u32();if(n>cap)throw std::runtime_error("Fixture field limit");return bytes(n);}
 template<std::size_t N>auto words(){std::array<std::uint32_t,N> out;for(auto& x:out)x=u32();return out;}
};
Bytes read(const std::filesystem::path& path,std::size_t cap){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot read fixture");auto n=f.tellg();if(n<0||std::uintmax_t(n)>cap)throw std::runtime_error("Fixture size limit");Bytes b(static_cast<std::size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()))||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture changed/read failed");return b;}
std::string text(const Bytes& b){return{reinterpret_cast<const char*>(b.data()),b.size()};}
std::string digest(const Bytes& b){return picosha2::hash256_hex_string(b);}
std::string hex(const Bytes& b){constexpr char d[]="0123456789abcdef";std::string out;for(auto x:b){out+=d[x>>4];out+=d[x&15];}return out;}
void put(Bytes& b,std::uint32_t v,unsigned n=4){for(unsigned i=0;i<n;++i)b.push_back(std::uint8_t(v>>(i*8)));}
template<class T>void put_array(Bytes& b,const T& values,unsigned n=4){for(auto x:values)put(b,x,n);}
void edit(Bytes& b,std::size_t p,std::uint32_t v,unsigned n=4){check(p<=b.size()&&n<=b.size()-p,"Mutation extent");for(unsigned i=0;i<n;++i)b[p+i]=std::uint8_t(v>>(i*8));}
cr::Resource decoded(const Bytes& b,cr::Limits limits={}){auto r=cr::Resource::decode(text(b),digest(b),limits);if(auto* e=std::get_if<cr::Error>(&r))throw std::runtime_error(e->message);return std::get<cr::Resource>(r);}
Bytes core_bytes(const cr::Resource& resource){
 Bytes out;const auto& h=resource.header();put(out,h.magic);put(out,h.version);put(out,h.global_tree_type);put(out,h.group_tree_type);put(out,h.group_count,2);put(out,h.attribute_count,2);put(out,h.pair_count);put(out,h.face_count);put(out,h.vertex_count);put_array(out,h.unknown20);put_array(out,h.bbox_min);put_array(out,h.bbox_max);
 for(const auto& g:resource.groups()){check(out.size()==g.source_offset,"Group source offset");put_array(out,g.bbox_min);put_array(out,g.bbox_max);put_array(out,g.source_fields20);for(auto r:{g.pairs,g.faces,g.vertices}){put(out,r.first);put(out,r.count);}put(out,g.index_id_raw);}
 for(const auto& p:resource.pairs()){check(out.size()==p.source_offset,"Pair source offset");put(out,p.face_a_local,2);put(out,p.face_b_local,2);put_array(out,p.order_raw,1);put(out,p.attribute_id,2);}
 for(const auto& f:resource.faces()){check(out.size()==f.source_offset,"Face source offset");put_array(out,f.normal_bits);put_array(out,f.vertex_local,2);put(out,f.attribute_id,2);put(out,f.field14);put_array(out,f.adjacency_raw,1);put(out,f.field1b,1);put(out,f.field1c);}
 for(const auto& v:resource.vertices()){check(out.size()==v.source_offset,"Vertex source offset");put_array(out,v.position_bits);}
 for(const auto& a:resource.attributes()){check(out.size()==a.source_offset,"Attribute source offset");put_array(out,a.words);}return out;
}
Bytes grid_bytes(const cr::Grid& g){Bytes out;put(out,0x6f637267U);put(out,g.version);put_array(out,g.bbox_min);put_array(out,g.bbox_max);put(out,g.nx,2);put(out,g.nz,2);put(out,g.index_kind,1);put(out,g.compressed,1);for(const auto& c:g.cells){put(out,c.count,2);put(out,c.unknown,2);put(out,c.start);}put(out,static_cast<std::uint32_t>(g.pair_ids.size()));put_array(out,g.pair_ids,2);for(const auto& b:g.pair_bounds)put_array(out,b.words);return out;}
void same(const Bytes& a,std::span<const std::byte> b,const char* why){check(a.size()==b.size(),why);for(unsigned i=0;i<a.size();++i)check(a[i]==std::to_integer<std::uint8_t>(b[i]),why);}
std::size_t boundaries(const Bytes& raw,const Bytes& multi){
 std::size_t n=0;auto base=decoded(raw);const auto source=digest(raw);const auto core=base.core_end_offset();const auto grid=std::get<cr::Grid>(base.grid(0));
 auto reject=[&](const Bytes& input,cr::ErrorCode code,cr::Limits limits={}){auto result=cr::Resource::decode(text(input),digest(input),limits);check(std::holds_alternative<cr::Error>(result)&&std::get<cr::Error>(result).code==code,"Typed core rejection");++n;};
 auto grid_reject=[&](const Bytes& input,cr::ErrorCode code,cr::Limits limits={}){auto r=decoded(input,limits);auto result=r.grid(0);check(std::holds_alternative<cr::Error>(result)&&std::get<cr::Error>(result).code==code,"Typed grid rejection");check(!r.global_tail_offset()&&r.pairs().size()==base.pairs().size(),"Bad grid preserves core without fabricated tail");check(std::holds_alternative<cr::Error>(r.cell_pairs(0,0)),"Bad grid cannot become empty cell success");++n;};
 auto b=raw;b[0]=0;reject(b,cr::ErrorCode::unsupported_format);b=raw;edit(b,4,0);reject(b,cr::ErrorCode::unsupported_format);b=raw;edit(b,20,0xffffffffU);reject(b,cr::ErrorCode::malformed_resource);b=raw;edit(b,24,0x7fffffffU);reject(b,cr::ErrorCode::resource_limit);
 b=raw;edit(b,72+44,1);reject(b,cr::ErrorCode::malformed_resource);b=multi;edit(b,72+72+44,0);reject(b,cr::ErrorCode::malformed_resource);
 b=raw;edit(b,base.sections()[1].offset,65535,2);reject(b,cr::ErrorCode::malformed_resource);b=raw;edit(b,base.sections()[1].offset+2,base.faces().size(),2);reject(b,cr::ErrorCode::malformed_resource);
 b=raw;edit(b,base.sections()[1].offset+8,base.attributes().size(),2);reject(b,cr::ErrorCode::malformed_resource);b=raw;edit(b,base.sections()[2].offset+18,base.attributes().size(),2);reject(b,cr::ErrorCode::malformed_resource);b=raw;edit(b,base.sections()[2].offset+12,base.vertices().size(),2);reject(b,cr::ErrorCode::malformed_resource);
 b=raw;b.resize(core-1);reject(b,cr::ErrorCode::malformed_resource);b=raw;b.resize(71);reject(b,cr::ErrorCode::malformed_resource);
 cr::Limits lim;lim.bytes=raw.size()-1;reject(raw,cr::ErrorCode::resource_limit,lim);lim={};lim.records=0;reject(raw,cr::ErrorCode::resource_limit,lim);lim={};lim.representation_bytes=raw.size();reject(raw,cr::ErrorCode::resource_limit,lim);
 b=raw;edit(b,12,0);grid_reject(b,cr::ErrorCode::unsupported_grid);b=raw;edit(b,core,0);grid_reject(b,cr::ErrorCode::unsupported_grid);b=raw;edit(b,core+44,0,1);grid_reject(b,cr::ErrorCode::unsupported_grid);b=raw;edit(b,core+45,0,1);grid_reject(b,cr::ErrorCode::unsupported_grid);b=raw;edit(b,core+40,0,2);grid_reject(b,cr::ErrorCode::unsupported_grid);
 b=raw;edit(b,grid.cells_source.offset+4,0xffffffffU);grid_reject(b,cr::ErrorCode::malformed_resource);b=raw;edit(b,grid.indices_source.offset,base.pairs().size(),2);grid_reject(b,cr::ErrorCode::malformed_resource);b=raw;b.resize(core+10);grid_reject(b,cr::ErrorCode::malformed_resource);
 lim={};lim.grid_cells=0;grid_reject(raw,cr::ErrorCode::resource_limit,lim);lim={};lim.grid_indices=0;grid_reject(raw,cr::ErrorCode::resource_limit,lim);
 // The core stores a full u32 pair count, but the original grid-bound loader
 // consumes its low16. Do not silently reinterpret 65536 as zero bounds.
 const auto pair_start=base.sections()[1].offset,pair_end=base.sections()[2].offset;
 b.assign(raw.begin(),raw.begin()+pair_start);const Bytes one(raw.begin()+pair_start,raw.begin()+pair_start+10);
 for(unsigned i=0;i<65536;++i)b.insert(b.end(),one.begin(),one.end());
 b.insert(b.end(),raw.begin()+pair_end,raw.end());edit(b,20,65536);edit(b,72+48,65536);
 auto narrowed=decoded(b);check(narrowed.pairs().size()==65536&&std::get<cr::Error>(narrowed.grid(0)).code==cr::ErrorCode::unsupported_grid&&!narrowed.global_tail_offset(),"Full count retained; narrowed grid interpretation rejected");++n;
 auto multi_base=decoded(multi);b=multi;edit(b,multi_base.core_end_offset()+44,0,1);auto partial=decoded(b);check(std::get<cr::Error>(partial.grid(1)).code==cr::ErrorCode::unresolved_grid,"Later group offset remains unknown");++n;
 auto badhash=cr::Resource::decode(text(raw),std::string(64,'0'));check(std::get<cr::Error>(badhash).code==cr::ErrorCode::hash_mismatch,"Expected identity enforced");++n;
 auto badexpected=cr::Resource::decode(text(raw),"not-a-digest");check(std::get<cr::Error>(badexpected).code==cr::ErrorCode::invalid_expected,"Expected digest syntax");++n;
 auto missing=cr::Resource::load("/nonexistent-mh4u-collision-resource-fixture",source);check(std::get<cr::Error>(missing).code==cr::ErrorCode::io,"File I/O error typed");++n;
 check(std::get<cr::Error>(base.resolve_pair(1,0)).code==cr::ErrorCode::index_out_of_range,"Bad group lookup");check(std::get<cr::Error>(base.resolve_pair(0,base.pairs().size())).code==cr::ErrorCode::index_out_of_range,"Bad local pair lookup");check(std::get<cr::Error>(base.cell_pairs(0,grid.cells.size())).code==cr::ErrorCode::index_out_of_range,"Bad cell lookup");n+=3;
 // Raw facts must survive without geometric repair or float execution.
 b=raw;edit(b,base.sections()[2].offset,0x7fc12345U);edit(b,base.sections()[3].offset+12,0x7f800000U);edit(b,72+32,0xdeadbeefU);edit(b,grid.cells_source.offset+2,0xa55a,2);edit(b,8,0x7fffffffU);auto opaque=decoded(b);
 check(opaque.faces()[0].normal_bits[0]==0x7fc12345U&&opaque.vertices()[0].position_bits[3]==0x7f800000U&&opaque.groups()[0].source_fields20[0]==0xdeadbeefU,"Raw nonfinite/unknown source words preserved");check(std::get<cr::Grid>(opaque.grid(0)).cells[0].unknown==0xa55a&&opaque.header().global_tree_type==0x7fffffffU&&opaque.global_tail_offset()==base.global_tail_offset(),"Uninterpreted fields and global tree not invented");++n;
 same(raw,base.bytes(),"Original source remains immutable");return n;
}
void run(const Bytes& fixture,const std::filesystem::path& root){
 Reader r{fixture};check(r.bytes(8)==Bytes({'M','H','4','U','S','B','C','1'})&&r.u32()==7,"Source fixture schema/count");std::size_t groups=0,pairs=0,faces=0,vertices=0,attributes=0,cells=0,indices=0,bytes_compared=0,second_faces=0;Bytes village,multi;
 for(unsigned sample=0;sample<7;++sample){
  const auto path=text(r.blob(512));check(path.starts_with("assets/local/collision-probe/")&&path.find("..") ==std::string::npos,"Declared local fixture path");const auto source_sha=hex(r.bytes(32));const auto raw=r.blob(1024*1024);check(digest(raw)==source_sha,"Original SBC hash");auto resource=decoded(raw);
  auto loaded=cr::Resource::load(root/path,source_sha);check(std::holds_alternative<cr::Resource>(loaded),"Public file loader");check(resource.sha256()==source_sha&&std::get<cr::Resource>(loaded).sha256()==source_sha,"Same content identity for independent loads");
  const auto end=r.u32(),tail=r.u32();check(resource.core_end_offset()==end&&resource.global_tail_offset()==tail,"Source core/global-tail boundaries");const auto sections=resource.sections();for(auto s:sections)check(r.words<3>()==std::array{s.offset,s.count,s.stride},"Parser-derived section offset/count/stride");
  const auto core=core_bytes(resource);same(core,resource.bytes().first(end),"Complete core raw-byte roundtrip");bytes_compared+=core.size();same(raw,resource.bytes(),"Owned immutable original file");
  auto alias=resource;check(alias.bytes().data()==resource.bytes().data()&&alias.pairs().data()==resource.pairs().data(),"Copies share one immutable representation");const auto saved=alias.vertices();resource=std::get<cr::Resource>(loaded);check(saved.data()==alias.vertices().data()&&saved.size()==alias.vertices().size(),"Borrow lifetime retained through a Resource copy");resource=alias;
  const auto ng=r.u32();check(ng==resource.groups().size(),"Grid per source group");
  for(unsigned j=0;j<ng;++j){const auto info=r.words<11>();auto result=resource.grid(j);check(std::holds_alternative<cr::Grid>(result),"Source grid variant supported");const auto& g=std::get<cr::Grid>(result);check(std::array{g.group,g.source_offset,std::uint32_t(g.bounds_source.offset+g.bounds_source.count*28),std::uint32_t(g.nx),std::uint32_t(g.nz),g.cells_source.offset,g.cells_source.count,g.indices_source.offset,g.indices_source.count,g.bounds_source.offset,g.bounds_source.count}==info,"Independent GRCO offsets and counts");
   const auto emitted=grid_bytes(g);same(emitted,resource.bytes().subspan(g.source_offset,info[2]-g.source_offset),"Complete compressed grid/bounds roundtrip");bytes_compared+=emitted.size();
   for(unsigned k=0;k<g.cells.size();++k){const auto refs=resource.cell_pairs(j,k);check(std::holds_alternative<std::span<const std::uint16_t>>(refs),"Cell lookup succeeds");const auto ids=std::get<std::span<const std::uint16_t>>(refs);check(ids.size()==g.cells[k].count&&ids.data()==g.pair_ids.data()+g.cells[k].start,"Cell references retain slice identity/order");}
   cells+=g.cells.size();indices+=g.pair_ids.size();
  }
  const auto np=r.u32();check(np==resource.pairs().size(),"Resolved pair coverage");
  for(unsigned j=0;j<np;++j){const auto e=r.words<9>();check(e[0]==j,"Original pair order");auto result=resource.resolve_pair(e[1],e[2]);check(std::holds_alternative<cr::ResolvedPair>(result),"Pair lookup succeeds");const auto& p=std::get<cr::ResolvedPair>(result);check(p.source_sha256==source_sha&&p.group_id==e[1]&&p.pair_id==e[0],"Content/local/global identity separation");check(p.group==&resource.groups()[e[1]]&&p.pair==&resource.pairs()[j]&&p.attribute==&resource.attributes()[e[5]],"Canonical record ownership");check(p.first.id==e[3]&&p.first.face==&resource.faces()[e[3]]&&p.first.vertex_ids==std::array{e[6],e[7],e[8]},"Group-local face/vertex resolution");for(unsigned k=0;k<3;++k)check(p.first.vertices[k]==&resource.vertices()[e[6+k]],"Borrowed canonical vertex");check(bool(p.second)==(e[4]!=0xffffffffU),"Second face sentinel preserved");if(p.second){const auto v=r.words<3>();check(p.second->id==e[4]&&p.second->face==&resource.faces()[e[4]]&&p.second->vertex_ids==v,"Second face group-relative resolution");for(unsigned k=0;k<3;++k)check(p.second->vertices[k]==&resource.vertices()[v[k]],"Second face canonical vertex");++second_faces;}}
  check(resource.acceleration_bytes().data()==resource.bytes().data()+end&&resource.acceleration_bytes().size()==raw.size()-end,"Unparsed acceleration bytes retained");groups+=ng;pairs+=np;faces+=resource.faces().size();vertices+=resource.vertices().size();attributes+=resource.attributes().size();if(sample==3)village=raw;if(sample==5)multi=raw;
 }
 check(r.at==fixture.size(),"No unparsed fixture tail");check(std::array{groups,pairs,faces,vertices,attributes,cells,indices}==std::array<std::size_t,7>{19,5711,5834,3357,41,15232,57629},"Complete seven-resource corpus totals");const auto errors=boundaries(village,multi);
 std::cout<<"{\"status\":\"passed\",\"resources\":7,\"groups\":19,\"pairs\":5711,\"faces\":5834,\"vertices\":3357,\"attributes\":41,\"cells\":15232,\"indices\":57629,\"second_faces\":"<<second_faces<<",\"compared_bytes\":"<<bytes_compared<<",\"boundaries\":"<<errors<<",\"checks\":"<<checks<<"}\n";
}
}
int main(int argc,char** argv){try{if(argc!=4)throw std::runtime_error("Usage: collision-resource-tests fixture sha256 project-root");const auto b=read(argv[1],2U*1024U*1024U);check(digest(b)==argv[2],"Fixture SHA256");run(b,argv[3]);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
