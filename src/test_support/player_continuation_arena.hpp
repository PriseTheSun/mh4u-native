#pragma once
// Test-only source projection shared by original continuation comparators.
// It never derives expected behavior; native output is reinserted into raw arenas.
#include "player_state1.hpp"
#include "player_fields.hpp"
#include "motion_command_arena.hpp"
#include "picosha2.h"
#include <fstream>
#include <set>

namespace mh4u::test::player_continuation {
using namespace mh4u::test::motion_arena;
namespace st=mh4u::player_state1;
namespace sf=mh4u::player_fields;
#include "player_arena_projection.hpp"
constexpr std::array<std::uint32_t,7> bases{0x2000000,0x2100000,0x2200000,0x2300000,0x2400000,0x2500000,0x2600000};
constexpr std::array<std::size_t,7> sizes{0xb000,0xa000,0x1000,0x1000,0xe000,0x1000,0x1000};
using Snapshot=std::array<Bytes,7>;
template<std::size_t N>auto ints(Reader& r){std::array<std::uint32_t,N> out{};for(auto& x:out)x=r.integer();return out;}
template<std::size_t N>auto records(Reader& r,std::size_t cap){const auto n=r.integer();check(n<=cap,"Record capacity");std::vector<std::array<std::uint32_t,N>> out;out.reserve(n);for(unsigned i=0;i<n;++i)out.push_back(ints<N>(r));return out;}
inline Bytes read(const std::filesystem::path& path,std::size_t cap){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"Open input");const auto n=f.tellg();check(n>=0&&std::uint64_t(n)<=cap,"Bound before allocation");Bytes b(static_cast<std::size_t>(n));f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(b.data()),n))&&f.peek()==std::char_traits<char>::eof(),"Stable complete input");return b;}
struct SourceHeader{std::uint32_t entry,base,wrapper,size;std::string sha;};
struct Case {
 std::uint32_t corpus{},index,action,substate,return_r0;std::string name;
 std::vector<std::array<std::uint32_t,2>> initialized;
 std::vector<std::array<std::uint32_t,7>> events;
 std::vector<std::array<std::uint32_t,5>> wrappers;
 std::vector<std::array<std::uint32_t,10>> setters;
 std::vector<std::array<std::uint32_t,3>> installs,queries;
 std::array<std::uint32_t,9> observations{};
 Snapshot before,after;
 bool known(unsigned arena,std::size_t offset,unsigned width)const{
  const auto p=std::uint64_t(bases[arena])+offset,q=p+width;
  for(const auto& range:initialized)if(range[0]<=p&&q<=range[1])return true;
  return false;
 }
 template<class T>std::optional<T> optional(unsigned arena,std::size_t p,unsigned width)const{
  if(!known(arena,p,width))return{};return static_cast<T>(word(before[arena],p,width));
 }
};
inline Case next(Reader& r,bool id2=false){
 Case c;if(id2)c.corpus=r.integer();c.index=r.integer();c.action=r.integer();c.substate=r.integer();c.return_r0=r.integer();const auto n=r.integer();check(n<=1024,"Name bound");const auto name=r.bytes(n);c.name.assign(name.begin(),name.end());
 check(c.corpus<2&&c.index<(c.corpus?112U:375U)&&(id2?c.action==2:(c.action==1||c.action==4||c.action==9))&&c.substate<=255,"Case domain");
 c.initialized=records<2>(r,1024);std::uint32_t last=0;for(auto range:c.initialized){check(range[0]<range[1]&&range[0]>last,"Initialized ranges");last=range[1];}
 c.events=records<7>(r,256);last=0;for(auto e:c.events){check(e[6]>last,"Original event order");last=e[6];}
 c.wrappers=records<5>(r,4);c.setters=records<10>(r,2);c.installs=records<3>(r,2);c.queries=records<3>(r,128);if(id2)c.observations=ints<9>(r);
 for(unsigned i=0;i<7;++i){c.before[i]=r.bytes(sizes[i]);c.after[i]=r.bytes(sizes[i]);}return c;
}
inline Arenas core(const Snapshot& raw){return {raw[0],raw[1],raw[2],raw[3]};}
inline pcs::State project(const Case& c,const mc::Fields& fields){
 auto out=controller(core(c.before),fields);auto& pbw=out.player.selection_pbw;auto& pw=out.player.selection_pw;
 pbw.field6f9=c.optional<std::uint8_t>(1,0x6f9,1);pbw.field358=c.optional<std::uint32_t>(1,0x358,4);pbw.field9=c.optional<std::uint8_t>(1,9,1);
 for(unsigned i=0;i<20;++i)pbw.fields2fc[i]=c.optional<std::uint16_t>(1,0x2fc+i*2,2);
 pw.field7fc=c.optional<std::uint8_t>(2,0x7fc,1);pw.field800=c.optional<std::uint32_t>(2,0x800,4);return out;
}
inline Snapshot serialize(const Case& c,const pcs::State& state,const st::Fields& fields,const std::vector<mp::Layer>& layers,const Context& context,const Bindings& events){
 auto out=c.before;auto raw=core(c.before);raw.player=encode(raw.player,layers,state.motion,context,events);encode_controller(raw,state);
 const auto patch=[](Bytes& b,std::size_t p,auto v,unsigned n){if(v)for(unsigned j=0;j<n;++j)b.at(p+j)=static_cast<std::uint8_t>(*v>>(8*j));};
 const auto& pbw=state.player.selection_pbw;const auto& pw=state.player.selection_pw;
 patch(raw.pbw,0x6f9,pbw.field6f9,1);patch(raw.pbw,0x358,pbw.field358,4);patch(raw.pbw,9,pbw.field9,1);
 for(unsigned i=0;i<20;++i)patch(raw.pbw,0x2fc+i*2,pbw.fields2fc[i],2);
 patch(raw.pw,0x7fc,pw.field7fc,1);patch(raw.pw,0x800,pw.field800,4);patch(raw.pw,0x50,fields.field50_bits,4);
 out[0]=std::move(raw.player);out[1]=std::move(raw.pbw);out[2]=std::move(raw.pw);out[3]=std::move(raw.global);return out;
}
inline void compare_snapshots(const Snapshot& got,const Snapshot& expected,const std::string& label){for(unsigned i=0;i<7;++i)same(got[i],expected[i],label+" arena"+std::to_string(i));}
inline std::pair<std::vector<SourceHeader>,std::vector<Source>> load_sources(Reader& r,const std::vector<std::array<std::string,3>>& args){
 std::vector<SourceHeader> headers;std::vector<Source> sources;std::set<std::uint32_t> entries,wrappers,bases_seen;
 for(unsigned i=0;i<3;++i){SourceHeader h{r.integer(),r.integer(),r.integer(),r.integer(),{}};const auto digest=r.bytes(32);picosha2::bytes_to_hex_string(digest.begin(),digest.end(),h.sha);
  check(entries.insert(h.entry).second&&wrappers.insert(h.wrapper).second&&bases_seen.insert(h.base).second,"Unique resource identity");
  bool found=false;for(const auto& arg:args){std::size_t used=0;const auto id=std::stoull(arg[0],&used);check(used==arg[0].size()&&id<=0xffffffffU,"Entry argument");if(id!=h.entry)continue;check(!found&&arg[2]==h.sha,"Source argument identity");auto resource=mr::Resource::load(arg[1],arg[2]);if(auto* e=std::get_if<mr::Error>(&resource))throw std::runtime_error(e->message);auto value=std::get<mr::Resource>(std::move(resource));check(value.byte_size()==h.size,"Source extent");sources.push_back({h.entry,h.base,std::move(value)});found=true;}
  check(found,"Missing source argument");headers.push_back(std::move(h));
 }
 return {std::move(headers),std::move(sources)};
}
inline rq::Model model(const Case& c,const std::vector<SourceHeader>& headers,const std::vector<Source>& sources){
 rq::Model out;out.skeleton_present=word(c.before[0],0xf0)!=0;
 for(unsigned bank=0;bank<16;++bank){const auto address=word(c.before[0],0xc04+4*bank);if(!address){out.banks[bank]=rq::Absent{};continue;}
  bool found=false;for(unsigned i=0;i<headers.size();++i)if(address==headers[i].wrapper){check(headers[i].entry==sources[i].entry,"Explicit wrapper/source identity");out.banks[bank]=sources[i].resource.request_resource();found=true;break;}check(found,"Unknown fixture resource wrapper");
 }return out;
}
// Pointer projection ignores bank because an ARM pointer contains none. Compare
// the logical bank separately: no binder ran, so tracks retain their pre-request
// binding even when the selector and event bindings move to bank2/co2.
inline void identities(const Case& c,const std::vector<mp::Layer>& out,const Context& context,const Bindings& events){
 check(out.size()==8,"Full layer storage");
 for(unsigned i=0;i<8;++i){const auto p=0x240+i*304;const auto old_bank=(word(c.before[0],p+4,2)>>8)&15;auto event_bank=old_bank;
  for(auto request:c.setters)if(request[0]==i&&request[7])event_bank=(request[1]>>8)&15;
  const auto& a=out[i].animation;
  check(a.tracks.translation==context.channel(c.after[0],p+0xd8,old_bank,i>=2),"Logical translation track/bank/key identity");
  check(a.tracks.rotation==context.channel(c.after[0],p+0xe8,old_bank,i>=2),"Logical rotation track/bank/key identity");
  for(unsigned g=0;g<4;++g)check(a.layer.bindings[g]==events.decode(word(c.after[0],p+0xf8+4*g),event_bank,i>=2),"Logical event resource/bank/slot/group identity");
 }
}
inline sf::Fields shared_fields(const Case& c){sf::Fields out;
 out.pbw.field3a8=c.optional<std::uint16_t>(1,0x3a8,2);
 out.pbw.field3ac=c.optional<std::uint16_t>(1,0x3ac,2);
 out.pbw.field3b8=c.optional<std::uint16_t>(1,0x3b8,2);
 out.pbw.field3bc=c.optional<std::uint16_t>(1,0x3bc,2);
 out.pbw.field14_bits=c.optional<std::uint32_t>(1,0x14,4);
 out.pbw.field478=c.optional<std::uint32_t>(1,0x478,4);
 out.pbw.field10_bits=c.optional<std::uint32_t>(1,0x10,4);
 out.pbw.field18_bits=c.optional<std::uint32_t>(1,0x18,4);
 out.pbw.field354_bits=c.optional<std::uint32_t>(1,0x354,4);
 out.pbw.field6fb=c.optional<std::uint8_t>(1,0x6fb,1);
 out.pbw.field6fc=c.optional<std::uint8_t>(1,0x6fc,1);
 out.pbw.field93ee=c.optional<std::uint8_t>(1,0x93ee,1);
 out.pbw.field93f1=c.optional<std::uint8_t>(1,0x93f1,1);
 out.pw.field14c=c.optional<std::uint32_t>(2,0x14c,4);
 out.pw.field154=c.optional<std::uint32_t>(2,0x154,4);
 out.pw.field15c_bits=c.optional<std::uint32_t>(2,0x15c,4);
 out.pw.field6a4_bits=c.optional<std::uint32_t>(2,0x6a4,4);
 out.pw.field1f8=c.optional<std::uint32_t>(2,0x1f8,4);
 out.pw.field220_bits=c.optional<std::uint32_t>(2,0x220,4);
 out.pw.field838_bits=c.optional<std::uint32_t>(2,0x838,4);
 out.pw.field150=c.optional<std::uint8_t>(2,0x150,1);
 out.pw.field158=c.optional<std::uint8_t>(2,0x158,1);
 out.pw.field159=c.optional<std::uint8_t>(2,0x159,1);
 out.pw.field15a=c.optional<std::uint8_t>(2,0x15a,1);
 out.pw.field6a8=c.optional<std::uint8_t>(2,0x6a8,1);
 out.pw.field7f0=c.optional<std::uint8_t>(2,0x7f0,1);
 if(auto bits=c.optional<std::uint16_t>(2,0x188,2))out.pw.field188=std::bit_cast<std::int16_t>(*bits);
 if(auto bits=c.optional<std::uint16_t>(2,0x224,2))out.pw.field224=std::bit_cast<std::int16_t>(*bits);
 if(auto bits=c.optional<std::uint16_t>(2,0x238,2))out.pw.field238=std::bit_cast<std::int16_t>(*bits);
 for(unsigned i=0;i<19;++i)for(unsigned j=0;j<4;++j)out.pbw.words3d8[i][j]=c.optional<std::uint8_t>(1,0x3d8+4*i+j,1);
 return out;
}
inline void encode_shared(Snapshot& raw,const sf::Fields& fields){
 const auto patch=[](Bytes& b,std::size_t p,auto value,unsigned n){if(value){const auto bits=static_cast<std::uint32_t>(*value);for(unsigned i=0;i<n;++i)b.at(p+i)=static_cast<std::uint8_t>(bits>>(8*i));}};
 patch(raw[1],0x3a8,fields.pbw.field3a8,2);
 patch(raw[1],0x3ac,fields.pbw.field3ac,2);
 patch(raw[1],0x3b8,fields.pbw.field3b8,2);
 patch(raw[1],0x3bc,fields.pbw.field3bc,2);
 patch(raw[1],0x14,fields.pbw.field14_bits,4);
 patch(raw[1],0x478,fields.pbw.field478,4);
 patch(raw[1],0x10,fields.pbw.field10_bits,4);
 patch(raw[1],0x18,fields.pbw.field18_bits,4);
 patch(raw[1],0x354,fields.pbw.field354_bits,4);
 patch(raw[1],0x6fb,fields.pbw.field6fb,1);
 patch(raw[1],0x6fc,fields.pbw.field6fc,1);
 patch(raw[1],0x93ee,fields.pbw.field93ee,1);
 patch(raw[1],0x93f1,fields.pbw.field93f1,1);
 patch(raw[2],0x14c,fields.pw.field14c,4);
 patch(raw[2],0x154,fields.pw.field154,4);
 patch(raw[2],0x15c,fields.pw.field15c_bits,4);
 patch(raw[2],0x6a4,fields.pw.field6a4_bits,4);
 patch(raw[2],0x1f8,fields.pw.field1f8,4);
 patch(raw[2],0x220,fields.pw.field220_bits,4);
 patch(raw[2],0x838,fields.pw.field838_bits,4);
 patch(raw[2],0x150,fields.pw.field150,1);
 patch(raw[2],0x158,fields.pw.field158,1);
 patch(raw[2],0x159,fields.pw.field159,1);
 patch(raw[2],0x15a,fields.pw.field15a,1);
 patch(raw[2],0x6a8,fields.pw.field6a8,1);
 patch(raw[2],0x7f0,fields.pw.field7f0,1);
 patch(raw[2],0x188,fields.pw.field188,2);
 patch(raw[2],0x224,fields.pw.field224,2);
 patch(raw[2],0x238,fields.pw.field238,2);
 for(unsigned i=0;i<19;++i)for(unsigned j=0;j<4;++j)patch(raw[1],0x3d8+4*i+j,fields.pbw.words3d8[i][j],1);
}
inline Snapshot serialize_shared(const Case& c,const pcs::State& state,const st::Fields& fields,const std::vector<mp::Layer>& layers,const Context& context,const Bindings& events,const sf::Fields& shared){
 auto out=serialize(c,state,fields,layers,context,events);encode_shared(out,shared);return out;
}
inline mh4u::player_reference::Reference reference(const Case& c,std::uint32_t manager_base){
 for(unsigned arena=0;arena<7;++arena)if(manager_base>=bases[arena]&&std::uint64_t(manager_base)+0x20<=std::uint64_t(bases[arena])+sizes[arena]){
  const auto offset=manager_base-bases[arena]+0x1c;
  if(!c.known(arena,offset,4))return mh4u::player_reference::Unknown{};
  const auto address=word(c.before[arena],offset);
  if(!address)return mh4u::player_reference::Absent{};
  for(unsigned object=0;object<7;++object)if(address>=bases[object]&&std::uint64_t(address)+0x6c<=std::uint64_t(bases[object])+sizes[object]){
   const auto p=address-bases[object];
   if(c.known(object,p+0x40,4)&&c.known(object,p+0x48,4)&&c.known(object,p+0x60,4)&&c.known(object,p+0x68,4))
    return mh4u::player_reference::Points{std::bit_cast<float>(word(c.before[object],p+0x40)),std::bit_cast<float>(word(c.before[object],p+0x48)),std::bit_cast<float>(word(c.before[object],p+0x60)),std::bit_cast<float>(word(c.before[object],p+0x68))};
  }
  return mh4u::player_reference::Unknown{};
 }
 throw std::runtime_error("Reference manager outside retained arenas");
}
} // namespace mh4u::test::player_continuation
