#include "item_pop_resources.hpp"
#include "picosha2.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>

namespace mh4u::item_pop_resources {
namespace {
struct Failure {Error error;};
[[noreturn]]void fail(ErrorCode code,std::string text,std::size_t offset=0){throw Failure{{code,std::move(text),offset,{},{}}};}
[[noreturn]]void fail_loaded(ErrorCode code,std::string text,std::size_t offset){throw Failure{{code,std::move(text),offset,{},{},OffsetSpace::loaded}};}
void expected(std::string_view h){if(h.size()!=64||!std::all_of(h.begin(),h.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))fail(ErrorCode::invalid_expected,"Expected SHA256 requires64 lowercase hex digits");}
void bytes_limit(std::uintmax_t n,const Limits& l){if(n>l.bytes||n>l.representation_bytes)fail(ErrorCode::resource_limit,"Source byte budget exceeded");if(n>UINT32_MAX)fail(ErrorCode::unsupported_format,"Source exceeds32bit offset domain");}
struct Reader {
 std::string_view bytes;
 void range(std::uint64_t p,std::uint64_t n)const{if(p>bytes.size()||n>bytes.size()-p)fail(ErrorCode::malformed_resource,"File range exceeds source",static_cast<std::size_t>(p));}
 std::uint8_t u8(std::size_t p)const{range(p,1);return static_cast<unsigned char>(bytes[p]);}
 std::uint16_t u16(std::size_t p)const{return std::uint16_t(u8(p)|(std::uint16_t(u8(p+1))<<8));}
 Word u32(std::size_t p)const{range(p,4);Word n=0;for(unsigned i=0;i<4;++i)n|=Word(u8(p+i))<<(8*i);return n;}
};
struct Budget {
 const Limits& limits;std::size_t representation{},records{};
 void charge(std::uint64_t n,std::size_t size,std::size_t off){if(n>(limits.representation_bytes-representation)/size)fail(ErrorCode::resource_limit,"Expanded representation exceeds policy",off);representation+=static_cast<std::size_t>(n)*size;}
 void work(std::uint64_t n,std::size_t off){if(n>limits.records-records)fail(ErrorCode::resource_limit,"Record work exceeds policy",off);records+=static_cast<std::size_t>(n);}
};
struct PositionData {Descriptor descriptor;std::vector<Position> records;};
struct ChoiceData {Descriptor descriptor;std::vector<Choice> records;};
struct GroupData {std::array<Descriptor,3> descriptors;std::vector<Primary> primary;std::vector<PositionData> positions;std::vector<ChoiceData> choices;std::vector<PositionSet> positions_view;std::vector<ChoiceSet> choices_view;Word secondary_count{};};
}
struct Resource::Data {
 std::string source,digest;Kind kind;std::vector<Word> header;std::vector<Segment> segments;
 Word allocation{};std::vector<GroupData> groups;std::array<std::vector<Descriptor>,2> members;
 // All consumed spans must be covered by one stream-loaded segment. Overlapping
 // destinations are rejected below; no allocation-size buffer or gap zeros exist.
 Word source_offset(std::uint64_t offset,std::uint64_t size)const{
  if(offset>allocation||size>std::uint64_t(allocation)-offset)fail_loaded(ErrorCode::malformed_resource,"Loaded range escapes allocation",static_cast<std::size_t>(offset));
  for(const auto& s:segments)if(s.size&&offset>=s.buffer_offset&&offset- s.buffer_offset<=s.size&&size<=std::uint64_t(s.size)-(offset-s.buffer_offset))return s.source_offset+static_cast<Word>(offset-s.buffer_offset);
  fail_loaded(ErrorCode::uninitialized_range,"Range is not contained in one initialized segment",static_cast<std::size_t>(offset));
 }
 Descriptor descriptor(const Reader& r,std::uint64_t table,std::uint64_t i)const{
  const auto p=source_offset(table+8*i,8);return{p,static_cast<Word>(table+8*i),r.u32(p),r.u32(p+4)};
 }
};
Result<Resource> Resource::decode_owned(std::string source,std::string_view hash,Limits limits){
 try{
  expected(hash);bytes_limit(source.size(),limits);if(picosha2::hash256_hex_string(source)!=hash)fail(ErrorCode::hash_mismatch,"Resource differs from expected SHA256");
  Reader r{source};r.range(0,8);const auto magic=r.u32(0);if((magic!=0x00535049U&&magic!=0x004c5049U)||r.u32(4)!=0x2003U)fail(ErrorCode::unsupported_format,"Only IPS/IPL2003 is supported");
  const bool set=magic==0x00535049U;const unsigned nh=set?13:9;r.range(0,4*nh);
  Budget b{limits};b.charge(source.size(),1,0);b.charge(1,sizeof(Data)+64+4*nh+3*sizeof(Segment),0);
  auto d=std::make_shared<Data>();d->source=std::move(source);d->digest=std::string(hash);d->kind=set?Kind::set:Kind::list;r.bytes=d->source;
  d->header.reserve(nh);for(unsigned i=0;i<nh;++i)d->header.push_back(r.u32(i*4));const auto& h=d->header;d->allocation=h[set?3:2];
  if(d->allocation>limits.loaded_bytes)fail(ErrorCode::resource_limit,"Original loaded allocation exceeds selected policy",set?12:8);
  std::uint64_t at=4*nh;
  const auto add_segment=[&](Word offset,Word size,std::size_t header_offset){
   r.range(at,size);if(size){if(offset>d->allocation||size>d->allocation-offset)fail(ErrorCode::malformed_resource,"Stream write escapes original allocation",header_offset);
    for(const auto& prev:d->segments)if(prev.size&&std::uint64_t(offset)<std::uint64_t(prev.buffer_offset)+prev.size&&std::uint64_t(prev.buffer_offset)<std::uint64_t(offset)+size)fail(ErrorCode::unsupported_format,"Overlapping loaded segments require ordered-write interpretation",header_offset);
   }
   d->segments.push_back({offset,static_cast<Word>(at),size});at+=size;
  };
  if(set){add_segment(0,h[6],24);add_segment(h[7],h[9],28);add_segment(h[10],h[12],40);}else{add_segment(0,h[5],20);add_segment(h[6],h[8],24);}
  if(set){
   const auto ng=h[2];b.work(ng,8);b.charge(ng,sizeof(GroupData),8);
   // Validate table extents before allocating any expanded group collection.
   if(ng)for(auto base:{Word(0),h[7],h[10]})d->source_offset(base,8ULL*ng);
   d->groups.reserve(ng);
   for(Word i=0;i<ng;++i){GroupData g;const std::array<Word,3> bases{0,h[7],h[10]};for(unsigned t=0;t<3;++t)g.descriptors[t]=d->descriptor(r,bases[t],i);
    const auto p=g.descriptors[0];b.work(p.count,p.source_offset);b.charge(p.count,sizeof(Primary),p.source_offset);
    Word first=0;if(p.count)first=d->source_offset(p.relative_offset,36ULL*p.count);g.primary.reserve(p.count);
    for(Word j=0;j<p.count;++j){const auto q=first+36*j;Primary a;a.source_offset=q;a.buffer_offset=p.relative_offset+36*j;std::copy_n(reinterpret_cast<const std::byte*>(r.bytes.data()+q),36,a.raw.begin());a.member_word=r.u32(q);for(unsigned k=0;k<3;++k)a.position_bits[k]=r.u32(q+4+4*k);a.radius_bits=r.u32(q+16);a.height_bits=r.u32(q+20);for(unsigned k=0;k<2;++k)a.words18[k]=r.u16(q+24+2*k);for(unsigned k=0;k<8;++k)a.bytes1c[k]=r.u8(q+28+k);g.primary.push_back(a);}
    const auto positions=g.descriptors[1],choices=g.descriptors[2];g.secondary_count=positions.count==choices.count?positions.count:0;
    for(unsigned t=1;t<3;++t){const auto desc=g.descriptors[t];b.work(desc.count,desc.source_offset);b.charge(desc.count,t==1?sizeof(PositionData)+sizeof(PositionSet):sizeof(ChoiceData)+sizeof(ChoiceSet),desc.source_offset);
     const auto table=std::uint64_t(bases[t])+desc.relative_offset;if(desc.count)d->source_offset(table,8ULL*desc.count);
     if(t==1)g.positions.reserve(desc.count);else g.choices.reserve(desc.count);
     for(Word j=0;j<desc.count;++j){auto sub=d->descriptor(r,table,j);const auto record_base=std::uint64_t(bases[t])+sub.relative_offset;const Word stride=t==1?28:6;b.work(sub.count,sub.source_offset);b.charge(sub.count,t==1?sizeof(Position):sizeof(Choice),sub.source_offset);Word start=0;if(sub.count)start=d->source_offset(record_base,std::uint64_t(stride)*sub.count);
      if(t==1){PositionData pset{sub,{}};pset.records.reserve(sub.count);for(Word k=0;k<sub.count;++k){Position x{start+stride*k,static_cast<Word>(record_base+stride*k),{}};std::copy_n(reinterpret_cast<const std::byte*>(r.bytes.data()+x.source_offset),stride,x.raw.begin());const auto q=x.source_offset;for(unsigned n=0;n<3;++n)x.position_bits[n]=r.u32(q+4*n);x.radius_bits=r.u32(q+12);x.height_bits=r.u32(q+16);for(unsigned n=0;n<2;++n)x.words14[n]=r.u16(q+20+2*n);for(unsigned n=0;n<4;++n)x.bytes18[n]=r.u8(q+24+n);pset.records.push_back(x);}g.positions.push_back(std::move(pset));}
      else{ChoiceData cset{sub,{}};cset.records.reserve(sub.count);for(Word k=0;k<sub.count;++k){Choice x{start+stride*k,static_cast<Word>(record_base+stride*k),{}};std::copy_n(reinterpret_cast<const std::byte*>(r.bytes.data()+x.source_offset),stride,x.raw.begin());const auto q=x.source_offset;x.weight=r.u8(q);x.member=r.u8(q+1);for(unsigned n=0;n<2;++n){x.counts[n]=r.u8(q+2+n);x.flags[n]=r.u8(q+4+n);}cset.records.push_back(x);}g.choices.push_back(std::move(cset));}
     }
    }
    g.positions_view.reserve(g.positions.size());for(const auto& v:g.positions)g.positions_view.push_back({v.descriptor,v.records});g.choices_view.reserve(g.choices.size());for(const auto& v:g.choices)g.choices_view.push_back({v.descriptor,v.records});d->groups.push_back(std::move(g));
   }
  }else{
   const std::array<Word,2> bases{0,h[6]},counts{h[4],h[7]};for(unsigned t=0;t<2;++t){b.work(counts[t],t?28:16);b.charge(counts[t],sizeof(Descriptor),t?28:16);if(counts[t])d->source_offset(bases[t],8ULL*counts[t]);d->members[t].reserve(counts[t]);for(Word i=0;i<counts[t];++i)d->members[t].push_back(d->descriptor(r,bases[t],i));}
  }
  return Resource{std::move(d)};
 }catch(const Failure& e){return e.error;}catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate resource snapshot"};}catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Resource snapshot exceeds host capacity"};}
}
Result<Resource> Resource::decode(std::string_view s,std::string_view h,Limits l){try{expected(h);bytes_limit(s.size(),l);return decode_owned(std::string(s),h,l);}catch(const Failure& e){return e.error;}catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot copy input"};}catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Input exceeds capacity"};}}
Result<Resource> Resource::load(const std::filesystem::path& p,std::string_view h,Limits l){try{expected(h);std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)fail(ErrorCode::io,"Cannot open resource");const auto n=f.tellg();if(n<0)fail(ErrorCode::io,"Cannot measure resource");bytes_limit(static_cast<std::uintmax_t>(n),l);if(static_cast<std::uintmax_t>(n)>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))fail(ErrorCode::resource_limit,"Stream capacity exceeded");std::string s(static_cast<std::size_t>(n),'\0');f.seekg(0);if(!f.read(s.data(),static_cast<std::streamsize>(s.size()))||f.peek()!=std::char_traits<char>::eof())fail(ErrorCode::io,"Read failed or source size changed");return decode_owned(std::move(s),h,l);}catch(const Failure& e){return e.error;}catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate file"};}catch(const std::length_error&){return Error{ErrorCode::resource_limit,"File exceeds capacity"};}}
Kind Resource::kind()const noexcept{return data_->kind;}
std::string_view Resource::sha256()const noexcept{return data_->digest;}
std::span<const std::byte> Resource::bytes()const noexcept{return{reinterpret_cast<const std::byte*>(data_->source.data()),data_->source.size()};}
std::span<const Word> Resource::header()const noexcept{return data_->header;}
std::span<const Segment> Resource::segments()const noexcept{return data_->segments;}
Word Resource::allocation_size()const noexcept{return data_->allocation;}
Word Resource::group_count()const noexcept{return static_cast<Word>(data_->groups.size());}
Result<Group> Resource::group(Word i)const{if(kind()!=Kind::set)return Error{ErrorCode::wrong_kind,"Expected Set"};if(i>=data_->groups.size())return Error{ErrorCode::index_out_of_range,"Group outside source table",8,i};const auto& g=data_->groups[i];return Group{g.descriptors,g.primary,g.secondary_count,g.positions_view,g.choices_view};}
Result<std::span<const Descriptor>> Resource::members(unsigned t)const{if(kind()!=Kind::list)return Error{ErrorCode::wrong_kind,"Expected List"};if(t>1)return Error{ErrorCode::index_out_of_range,"Member table must be0 or1"};return std::span<const Descriptor>{data_->members[t]};}
Result<std::optional<Member>> Resource::member(unsigned t,Word i)const{
 try{
 const auto m=members(t);if(const auto* e=std::get_if<Error>(&m))return *e;const auto table=std::get<std::span<const Descriptor>>(m);if(i>=table.size())return std::optional<Member>{};const auto& d=table[i];const std::uint64_t base=t?data_->header[6]:0,off=base+d.relative_offset;
 if(off>=data_->allocation)return Error{ErrorCode::malformed_resource,"Resolved member pointer escapes loaded allocation",d.source_offset,{},i};
 std::optional<Word> source;for(const auto& s:data_->segments)if(s.size&&off>=s.buffer_offset&&off-s.buffer_offset<s.size){source=s.source_offset+static_cast<Word>(off-s.buffer_offset);break;}
 return std::optional<Member>{Member{d,{data_->digest,static_cast<Word>(off)},source}};
 }catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate member identity"};}catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Member identity exceeds capacity"};}
}
Result<std::span<const std::byte>> Resource::loaded_bytes(Word off,Word n)const{try{const auto p=data_->source_offset(off,n);return bytes().subspan(p,n);}catch(const Failure& e){return e.error;}}
} // namespace mh4u::item_pop_resources
