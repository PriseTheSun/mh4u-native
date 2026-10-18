#include "collision_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>
#include <vector>

namespace mh4u::collision_resource {
namespace {
struct Failure {Error error;};
[[noreturn]]void fail(ErrorCode code,std::string message,std::size_t offset=0){throw Failure{{code,std::move(message),offset,{}}};}
void expected(std::string_view hash){
 if(hash.size()!=64||!std::all_of(hash.begin(),hash.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))fail(ErrorCode::invalid_expected,"Expected SHA256 requires64 lowercase hexadecimal digits");
}
void byte_budget(std::uintmax_t n,const Limits& limits){
 if(n>limits.representation_bytes)fail(ErrorCode::resource_limit,"SBC input exceeds representation budget");
 if(n>limits.bytes)fail(ErrorCode::resource_limit,"SBC exceeds selected byte budget");
 if(n>std::numeric_limits<std::uint32_t>::max())fail(ErrorCode::unsupported_format,"SBC exceeds32bit source offset domain");
}
struct Reader {
 std::string_view data;
 void range(std::uint64_t p,std::uint64_t n)const{if(p>data.size()||n>data.size()-p)fail(ErrorCode::malformed_resource,"Source span exceeds SBC bytes",static_cast<std::size_t>(p));}
 std::uint8_t u8(std::size_t p)const{range(p,1);return static_cast<unsigned char>(data[p]);}
 std::uint16_t u16(std::size_t p)const{return std::uint16_t(u8(p)|(std::uint16_t(u8(p+1))<<8));}
 std::uint32_t u32(std::size_t p)const{range(p,4);std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(u8(p+i))<<(8*i);return v;}
 template<std::size_t N>std::array<std::uint32_t,N> words(std::size_t p)const{range(p,4*N);std::array<std::uint32_t,N> out;for(unsigned i=0;i<N;++i)out[i]=u32(p+4*i);return out;}
};
struct Budget {
 const Limits& limits;std::size_t representation{},cells{},indices{};
 void charge(std::uint64_t count,std::size_t size,std::size_t offset){
  if(representation>limits.representation_bytes||count>(limits.representation_bytes-representation)/size)fail(ErrorCode::resource_limit,"Expanded SBC representation exceeds policy",offset);
  representation+=static_cast<std::size_t>(count)*size;
 }
 void grid(std::uint64_t count,std::size_t& used,std::size_t limit,std::size_t offset){
  if(used>limit||count>limit-used)fail(ErrorCode::resource_limit,"Cumulative grid work exceeds policy",offset);used+=static_cast<std::size_t>(count);
 }
};
struct GridData {Grid view;std::vector<Cell> cells;std::vector<std::uint16_t> indices;std::vector<PairBounds> bounds;};
struct ParsedGrid {GridData data;std::uint32_t end{};};
ParsedGrid parse_grid(const Reader& r,const Group& group,std::uint32_t id,std::uint32_t p,Budget& budget){
 r.range(p,46);
 if(r.u32(p)!=0x6f637267U||r.u32(p+4)!=2009111700U)fail(ErrorCode::unsupported_grid,"Only GRCO2009111700 is supported",p);
 const auto nx=r.u16(p+40),nz=r.u16(p+42);const auto kind=r.u8(p+44),compressed=r.u8(p+45);
 if(kind!=1||compressed!=1)fail(ErrorCode::unsupported_grid,"Only compressed GRCO u16 indices are supported",p+44);
 if(nx==0||nz==0||nx>32767||nz>32767)fail(ErrorCode::unsupported_grid,"Grid dimensions outside observed signed-index profile",p+40);
 if(group.pairs.count>65535)fail(ErrorCode::unsupported_grid,"Grid bounds loader narrows group pair count to u16",group.source_offset+48);
 const std::uint64_t nc=std::uint64_t(nx)*nz,cellp=std::uint64_t(p)+46,index_countp=cellp+8*nc;
 r.range(cellp,8*nc+4);const auto ni=r.u32(static_cast<std::size_t>(index_countp));const auto indexp=index_countp+4,boundp=indexp+2ULL*ni,end=boundp+28ULL*group.pairs.count;
 r.range(indexp,2ULL*ni+28ULL*group.pairs.count);
 budget.grid(nc,budget.cells,budget.limits.grid_cells,p+40);budget.grid(ni,budget.indices,budget.limits.grid_indices,static_cast<std::size_t>(index_countp));
 budget.charge(nc,sizeof(Cell),static_cast<std::size_t>(cellp));budget.charge(ni,sizeof(std::uint16_t),static_cast<std::size_t>(indexp));budget.charge(group.pairs.count,sizeof(PairBounds),static_cast<std::size_t>(boundp));
 // Validate every reference before allocating expanded lists.
 for(std::uint64_t i=0;i<nc;++i){const auto q=static_cast<std::size_t>(cellp+8*i);const auto start=r.u32(q+4);const auto count=r.u16(q);if(start>ni||count>ni-start)fail(ErrorCode::malformed_resource,"Grid cell escapes index list",q);}
 for(std::uint64_t i=0;i<ni;++i)if(r.u16(static_cast<std::size_t>(indexp+2*i))>=group.pairs.count)fail(ErrorCode::malformed_resource,"Grid index escapes group-local pair range",static_cast<std::size_t>(indexp+2*i));
 GridData out;out.view.group=id;out.view.source_offset=p;out.view.version=r.u32(p+4);out.view.bbox_min=r.words<4>(p+8);out.view.bbox_max=r.words<4>(p+24);out.view.nx=nx;out.view.nz=nz;out.view.index_kind=kind;out.view.compressed=compressed;
 out.view.cells_source={static_cast<std::uint32_t>(cellp),static_cast<std::uint32_t>(nc),8};out.view.indices_source={static_cast<std::uint32_t>(indexp),ni,2};out.view.bounds_source={static_cast<std::uint32_t>(boundp),group.pairs.count,28};
 out.cells.reserve(static_cast<std::size_t>(nc));out.indices.reserve(ni);out.bounds.reserve(group.pairs.count);
 for(std::uint64_t i=0;i<nc;++i){const auto q=static_cast<std::size_t>(cellp+8*i);out.cells.push_back({r.u16(q),r.u16(q+2),r.u32(q+4)});}
 for(std::uint64_t i=0;i<ni;++i)out.indices.push_back(r.u16(static_cast<std::size_t>(indexp+2*i)));
 for(std::uint64_t i=0;i<group.pairs.count;++i)out.bounds.push_back({r.words<7>(static_cast<std::size_t>(boundp+28*i))});
 return {std::move(out),static_cast<std::uint32_t>(end)};
}
void partition(const std::vector<Group>& groups,Range Group::*field,std::uint32_t total){
 std::vector<Range> ranges;ranges.reserve(groups.size());for(const auto& g:groups){const auto a=g.*field;if(a.first>total||a.count>total-a.first)fail(ErrorCode::malformed_resource,"Group range escapes core array",g.source_offset);if(a.count)ranges.push_back(a);}
 std::sort(ranges.begin(),ranges.end(),[](const Range& a,const Range& b){return a.first<b.first;});std::uint32_t at=0;
 for(const auto& a:ranges){if(a.first!=at)fail(ErrorCode::malformed_resource,"Groups overlap or leave gaps in core array");at+=a.count;}
 if(at!=total)fail(ErrorCode::malformed_resource,"Groups do not cover core array");
}
}
struct Resource::Data {
 std::string bytes,digest;Header header;std::array<Section,5> sections;
 std::vector<Group> groups;std::vector<Pair> pairs;std::vector<Face> faces;std::vector<Vertex> vertices;std::vector<Attribute> attributes;
 std::vector<Result<GridData>> grids;std::uint32_t core_end{};std::optional<std::uint32_t> global_tail;
};
Result<Resource> Resource::decode_owned(std::string bytes,std::string_view hash,Limits limits){
 try{
  expected(hash);byte_budget(bytes.size(),limits);if(picosha2::hash256_hex_string(bytes)!=hash)fail(ErrorCode::hash_mismatch,"SBC differs from expected SHA256");
  Reader r{bytes};r.range(0,72);Header h{};h.magic=r.u32(0);h.version=r.u32(4);
  if(h.magic!=0xff434253U||h.version!=2010091000U)fail(ErrorCode::unsupported_format,"Only SBC-ff2010091000 is supported");
  h.global_tree_type=r.u32(8);h.group_tree_type=r.u32(12);h.group_count=r.u16(16);h.attribute_count=r.u16(18);h.pair_count=r.u32(20);h.face_count=r.u32(24);h.vertex_count=r.u32(28);h.unknown20=r.words<2>(32);h.bbox_min=r.words<4>(40);h.bbox_max=r.words<4>(56);
  if((h.pair_count|h.face_count|h.vertex_count)&0x80000000U)fail(ErrorCode::malformed_resource,"Negative signed core count",20);
  if(!h.group_count||!h.face_count||!h.vertex_count)fail(ErrorCode::unsupported_format,"Empty geometry is outside observed SBC profile",16);
  const std::array<std::uint32_t,5> counts{h.group_count,h.pair_count,h.face_count,h.vertex_count,h.attribute_count},strides{72,10,32,16,32};
  const std::array<std::size_t,5> expanded{sizeof(Group),sizeof(Pair),sizeof(Face),sizeof(Vertex),sizeof(Attribute)};
  std::uint64_t cursor=72,total=0;std::array<Section,5> sections;
  Budget budget{limits};budget.charge(bytes.size(),1,0);budget.charge(1,sizeof(Data)+64,0);budget.charge(h.group_count,sizeof(Result<GridData>)+sizeof(Range),16);
  for(unsigned i=0;i<5;++i){total+=counts[i];if(total>limits.records)fail(ErrorCode::resource_limit,"Core record work exceeds policy",16);r.range(cursor,std::uint64_t(counts[i])*strides[i]);sections[i]={static_cast<std::uint32_t>(cursor),counts[i],strides[i]};budget.charge(counts[i],expanded[i],static_cast<std::size_t>(cursor));cursor+=std::uint64_t(counts[i])*strides[i];}
  auto d=std::make_shared<Data>();d->bytes=std::move(bytes);d->digest=std::string(hash);d->header=h;d->sections=sections;d->core_end=static_cast<std::uint32_t>(cursor);r.data=d->bytes;
  d->groups.reserve(h.group_count);for(std::uint32_t i=0;i<h.group_count;++i){const auto p=sections[0].offset+72*i;d->groups.push_back({p,r.words<4>(p),r.words<4>(p+16),r.words<3>(p+32),{r.u32(p+44),r.u32(p+48)},{r.u32(p+52),r.u32(p+56)},{r.u32(p+60),r.u32(p+64)},r.u32(p+68)});}
  partition(d->groups,&Group::pairs,h.pair_count);partition(d->groups,&Group::faces,h.face_count);partition(d->groups,&Group::vertices,h.vertex_count);
  // Record references use their owning group's local extent; raw fields remain unchanged.
  for(const auto& g:d->groups){
   for(std::uint64_t i=g.pairs.first;i<std::uint64_t(g.pairs.first)+g.pairs.count;++i){const auto p=sections[1].offset+10*i;const auto a=r.u16(p),b=r.u16(p+2);if(a>=g.faces.count||(b!=65535&&b>=g.faces.count))fail(ErrorCode::malformed_resource,"Pair face reference escapes owning group",static_cast<std::size_t>(p));if(r.u16(p+8)>=h.attribute_count)fail(ErrorCode::malformed_resource,"Pair attribute escapes global table",static_cast<std::size_t>(p+8));}
   for(std::uint64_t i=g.faces.first;i<std::uint64_t(g.faces.first)+g.faces.count;++i){const auto p=sections[2].offset+32*i;for(unsigned k=0;k<3;++k)if(r.u16(p+12+2*k)>=g.vertices.count)fail(ErrorCode::malformed_resource,"Face vertex escapes owning group",static_cast<std::size_t>(p+12+2*k));if(r.u16(p+18)>=h.attribute_count)fail(ErrorCode::malformed_resource,"Face attribute escapes global table",static_cast<std::size_t>(p+18));}
  }
  d->pairs.reserve(h.pair_count);for(std::uint64_t i=0;i<h.pair_count;++i){const auto p=static_cast<std::uint32_t>(sections[1].offset+10*i);d->pairs.push_back({p,r.u16(p),r.u16(p+2),{r.u8(p+4),r.u8(p+5),r.u8(p+6),r.u8(p+7)},r.u16(p+8)});}
  d->faces.reserve(h.face_count);for(std::uint64_t i=0;i<h.face_count;++i){const auto p=static_cast<std::uint32_t>(sections[2].offset+32*i);d->faces.push_back({p,r.words<3>(p),{r.u16(p+12),r.u16(p+14),r.u16(p+16)},r.u16(p+18),r.u32(p+20),{r.u8(p+24),r.u8(p+25),r.u8(p+26)},r.u8(p+27),r.u32(p+28)});}
  d->vertices.reserve(h.vertex_count);for(std::uint64_t i=0;i<h.vertex_count;++i){const auto p=static_cast<std::uint32_t>(sections[3].offset+16*i);d->vertices.push_back({p,r.words<4>(p)});}
  d->attributes.reserve(h.attribute_count);for(std::uint64_t i=0;i<h.attribute_count;++i){const auto p=static_cast<std::uint32_t>(sections[4].offset+32*i);d->attributes.push_back({p,r.words<8>(p)});}
  d->grids.reserve(h.group_count);bool offset_known=true;
  for(std::uint32_t i=0;i<h.group_count;++i){
   if(!offset_known){d->grids.emplace_back(Error{ErrorCode::unresolved_grid,"Previous group section leaves this offset unknown",d->core_end,i});continue;}
   try{
    if(h.group_tree_type!=2)fail(ErrorCode::unsupported_grid,"Only group tree type2 GRCO is parsed",12);
    auto grid=parse_grid(r,d->groups[i],i,static_cast<std::uint32_t>(cursor),budget);cursor=grid.end;d->grids.emplace_back(std::move(grid.data));
   }catch(const Failure& f){auto error=f.error;error.group=i;d->grids.emplace_back(std::move(error));offset_known=false;}
   catch(const std::bad_alloc&){d->grids.emplace_back(Error{ErrorCode::resource_limit,"Cannot allocate grid view",static_cast<std::size_t>(cursor),i});offset_known=false;}
   catch(const std::length_error&){d->grids.emplace_back(Error{ErrorCode::resource_limit,"Grid view exceeds host capacity",static_cast<std::size_t>(cursor),i});offset_known=false;}
  }
  if(offset_known)d->global_tail=static_cast<std::uint32_t>(cursor);
  return Resource{std::move(d)};
 }catch(const Failure& f){return f.error;}
 catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate SBC snapshot",0,{}};}
 catch(const std::length_error&){return Error{ErrorCode::resource_limit,"SBC snapshot exceeds host capacity",0,{}};}
}
Result<Resource> Resource::decode(std::string_view bytes,std::string_view hash,Limits limits){
 try{expected(hash);byte_budget(bytes.size(),limits);return decode_owned(std::string(bytes),hash,limits);}
 catch(const Failure& f){return f.error;}catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot copy SBC input",0,{}};}catch(const std::length_error&){return Error{ErrorCode::resource_limit,"SBC input exceeds host capacity",0,{}};}
}
Result<Resource> Resource::load(const std::filesystem::path& path,std::string_view hash,Limits limits){
 try{
  expected(hash);std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)fail(ErrorCode::io,"Cannot open SBC file");const auto n=file.tellg();if(n<0)fail(ErrorCode::io,"Cannot measure SBC file");byte_budget(static_cast<std::uintmax_t>(n),limits);
  if(static_cast<std::uintmax_t>(n)>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))fail(ErrorCode::resource_limit,"SBC exceeds stream capacity");
  std::string bytes(static_cast<std::size_t>(n),'\0');file.seekg(0);if(!file.read(bytes.data(),static_cast<std::streamsize>(bytes.size()))||file.peek()!=std::char_traits<char>::eof())fail(ErrorCode::io,"SBC read failed or size changed");return decode_owned(std::move(bytes),hash,limits);
 }catch(const Failure& f){return f.error;}catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate SBC file snapshot",0,{}};}catch(const std::length_error&){return Error{ErrorCode::resource_limit,"SBC file exceeds host capacity",0,{}};}
}
std::string_view Resource::sha256()const noexcept{return data_->digest;}
std::span<const std::byte> Resource::bytes()const noexcept{return{reinterpret_cast<const std::byte*>(data_->bytes.data()),data_->bytes.size()};}
const Header& Resource::header()const noexcept{return data_->header;}
std::array<Section,5> Resource::sections()const noexcept{return data_->sections;}
std::span<const Group> Resource::groups()const noexcept{return data_->groups;}
std::span<const Pair> Resource::pairs()const noexcept{return data_->pairs;}
std::span<const Face> Resource::faces()const noexcept{return data_->faces;}
std::span<const Vertex> Resource::vertices()const noexcept{return data_->vertices;}
std::span<const Attribute> Resource::attributes()const noexcept{return data_->attributes;}
Result<ResolvedPair> Resource::resolve_pair(std::uint32_t group,std::uint32_t local_pair)const{
 if(group>=data_->groups.size())return Error{ErrorCode::index_out_of_range,"Group ID outside source array",0,group};const auto& g=data_->groups[group];
 if(local_pair>=g.pairs.count)return Error{ErrorCode::index_out_of_range,"Local pair ID outside group",g.source_offset,group};const auto id=g.pairs.first+local_pair;const auto& pair=data_->pairs[id];
 const auto face=[&](std::uint16_t local){const auto id=g.faces.first+local;const auto& f=data_->faces[id];ResolvedFace out;out.id=id;out.face=&f;for(unsigned k=0;k<3;++k){out.vertex_ids[k]=g.vertices.first+f.vertex_local[k];out.vertices[k]=&data_->vertices[out.vertex_ids[k]];}return out;};
 ResolvedPair out{data_->digest,group,id,&g,&pair,&data_->attributes[pair.attribute_id],face(pair.face_a_local),{}};if(pair.face_b_local!=65535)out.second=face(pair.face_b_local);return out;
}
Result<Grid> Resource::grid(std::uint32_t group)const{
 if(group>=data_->groups.size())return Error{ErrorCode::index_out_of_range,"Group ID outside source array",0,group};const auto& value=data_->grids[group];if(const auto* error=std::get_if<Error>(&value))return *error;
 const auto& storage=std::get<GridData>(value);auto out=storage.view;out.cells=storage.cells;out.pair_ids=storage.indices;out.pair_bounds=storage.bounds;return out;
}
Result<std::span<const std::uint16_t>> Resource::cell_pairs(std::uint32_t group,std::uint32_t cell)const{
 const auto result=grid(group);if(const auto* error=std::get_if<Error>(&result))return *error;const auto& value=std::get<Grid>(result);if(cell>=value.cells.size())return Error{ErrorCode::index_out_of_range,"Cell ID outside grid",value.source_offset,group};const auto c=value.cells[cell];return value.pair_ids.subspan(c.start,c.count);
}
std::uint32_t Resource::core_end_offset()const noexcept{return data_->core_end;}
std::span<const std::byte> Resource::acceleration_bytes()const noexcept{return bytes().subspan(data_->core_end);}
std::optional<std::uint32_t> Resource::global_tail_offset()const noexcept{return data_->global_tail;}
} // namespace mh4u::collision_resource
