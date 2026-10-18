#pragma once
// Test-only decoder of original observations. No renewal/removal selection here.
#include "item_pop_update_fixture.hpp"
namespace mh4u::test_support::item_pop_lifecycle_fixture {
namespace f=item_pop_manager;namespace previous=item_pop_update_fixture;
using U=f::U;using Bytes=f::Bytes;using Source=previous::Source;
using Query=previous::Query;using Mutation=previous::Mutation;using Store=previous::Store;
struct ExternalMutation{U address,width,value;};
struct Call{U pc;std::array<U,4> args;U result,instruction,end_instruction;};
struct Step{
 std::vector<Mutation> mutations;std::vector<ExternalMutation> external_mutations;
 // clock50,delta,clock57,category,Set,List,mode,channel0,singleton; presence is
 // from the original guard's initialized ranges, not a native guessed default.
 std::array<std::optional<U>,9> facts;
 std::vector<Call> calls;
 std::span<const std::uint8_t> before,after,global_before,global_after;
 std::vector<Store> stores;std::vector<Query> queries;
};
struct Case{
 U index;std::string name;std::array<Source,2> sources;bool set_present,list_present;
 item_pop_builder::Context builder;
 std::span<const std::uint8_t> global_before,global_after,built;
 std::vector<Step> steps;
};
struct Fixture{Bytes bytes;std::vector<Case> cases;};
inline player_transition::GlobalFields global(std::span<const std::uint8_t> b,
 std::optional<U> channel0){
 player_transition::GlobalFields g{};g.field22=std::uint16_t(f::read(b,0x22,2));
 g.field2b=std::uint8_t(f::read(b,0x2b,1));g.field2f=std::uint8_t(f::read(b,0x2f,1));
 g.field5d=std::uint8_t(f::read(b,0x5d,1));g.fieldd72=std::uint8_t(f::read(b,0xd72,1));
 f::need(!channel0||*channel0<=65535,"channel0 width");
 if(channel0)g.field20=std::uint16_t(*channel0);return g;
}
inline item_pop_update::Context context(const Step& s,const player_world_lookup::Manager& manager,
 const item_pop_resources::Resource& set,const item_pop_resources::Resource& list){
 item_pop_update::Context c{};c.clock.field50=s.facts[0];c.clock.field5c_bits=s.facts[1];
 auto byte=[](std::optional<U> x){f::need(!x||*x<=255,"byte width");return x?std::optional<std::uint8_t>(std::uint8_t(*x)):std::nullopt;};
 c.clock.field57=byte(s.facts[2]);c.category_e250=byte(s.facts[3]);
 auto bind=[]<class T>(std::optional<U> raw,U address,const T& owner)->item_pop_secondary::Binding<T>{
  if(!raw)return player_world_lookup::Unknown{};if(!*raw)return player_world_lookup::Absent{};
  f::need(*raw==address,"source borrowed binding identity");return &owner;};
 c.environment.set=bind(s.facts[4],0x2100000,set);c.environment.list=bind(s.facts[5],0x2200000,list);
 c.environment.area.mode_c8c=s.facts[6];c.environment.singleton_manager=bind(s.facts[8],f::M,manager);return c;
}
inline Fixture load(const std::filesystem::path& path,const std::string& expected){
 Fixture out{};const auto size=std::filesystem::file_size(path);f::need(size<=32U*1024U*1024U,"fixture capacity");
 std::ifstream file(path,std::ios::binary);out.bytes.resize(size);file.read(reinterpret_cast<char*>(out.bytes.data()),std::streamsize(size));
 f::need(file.gcount()==std::streamsize(size)&&file.peek()==std::char_traits<char>::eof(),"fixture length");
 f::need(picosha2::hash256_hex_string(out.bytes)==expected,"fixture SHA");f::Reader r{out.bytes};auto magic=r.take(8);
 f::need(std::string(magic.begin(),magic.end())=="MHIPLC01","schema");f::need(r.word()==57,"case count");
 f::need(r.hash()=="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","ELF identity");
 f::need(r.hash()=="63219b5fa0c7537e37c2dce32eda74f70b2ca500aaaaf2b8218fbb57ad11dec9","original execution identity");
 for(U i=0;i<57;++i){const auto n=r.word();f::need(n<=2U*1024U*1024U,"row extent");f::Reader q{r.take(n)};Case c{};
  c.index=q.word();f::need(c.index==i,"case order");c.name=q.text();for(auto& s:c.sources){s.path=q.text();s.sha=q.hash();s.size=q.word();}
  f::need(q.word()==0xe649d8&&q.word()==0x5aa4cc,"source vtable target");const auto set=q.word(),list=q.word();f::need(set<=1&&list<=1,"source presence");c.set_present=set;c.list_present=list;c.builder.area.mode_c8c=q.word();
  for(auto& v:c.builder.position_template)v=q.word();for(auto& v:c.builder.auxiliary_template)v=q.word();c.builder.id_counter=q.word();
  c.global_before=q.take(4096);c.global_after=q.take(4096);c.built=q.take(0x5000);c.builder.global=global(c.global_before,f::read(c.global_before,0x20,2));
  const auto count=q.word();f::need(count>0&&count<=16,"step count");for(U k=0;k<count;++k){Step s{};
   const auto nm=q.word();f::need(nm<=128,"mutation count");for(U j=0;j<nm;++j){Mutation m{q.word(),q.word(),q.word(),q.word()};f::need(m.slot<128&&(m.width==1||m.width==2||m.width==4)&&m.offset+m.width<=0x94,"mutation extent");s.mutations.push_back(m);}
   const auto ne=q.word();f::need(ne<=128,"external mutation count");for(U j=0;j<ne;++j){ExternalMutation m{q.word(),q.word(),q.word()};const auto end=std::uint64_t(m.address)+m.width;f::need((m.width==1||m.width==2||m.width==4)&&((m.address>=f::M&&end<=f::M+0x5000)||(m.address>=f::G&&end<=f::G+4096)||(m.address==0xfb8a1c&&m.width==4)),"external mutation extent");s.external_mutations.push_back(m);}
   for(auto& v:s.facts)v=f::optional(q);
   const auto nc=q.word();f::need(nc<=2048,"call count");for(U j=0;j<nc;++j){Call call{};call.pc=q.word();for(auto& a:call.args)a=q.word();call.result=q.word();call.instruction=q.word();call.end_instruction=q.word();f::need(call.end_instruction>=call.instruction,"call extent");f::need(call.pc==0x5aa4cc||call.pc==0x2b555c||call.pc==0x5b928c||call.pc==0x5b9d84||call.pc==0x23dd60||call.pc==0x2aafe4,"call identity");s.calls.push_back(call);}
   s.before=q.take(0x5000);s.after=q.take(0x5000);s.global_before=q.take(4096);s.global_after=q.take(4096);
   const auto ns=q.word();f::need(ns<=4096,"store count");for(U j=0;j<ns;++j){const auto pc=q.word(),a=q.word(),width=q.word(),lo=q.word(),hi=q.word();const auto end=std::uint64_t(a)+width;f::need((width==1||width==2||width==4||width==8)&&((a>=f::M&&end<=f::M+0x5000)||(a>=f::G&&end<=f::G+4096)),"store range");f::need((pc>=0x5aa4cc&&pc<0x5ab2f4)||(pc>=0x5b928c&&pc<0x5b94a4)||(pc>=0x5b9d84&&pc<0x5b9e4c)||(pc>=0x2b555c&&pc<0x2b55a4),"store PC");s.stores.push_back({pc,a,width,std::uint64_t(lo)|(std::uint64_t(hi)<<32)});}
   s.queries=previous::queries(q);c.steps.push_back(std::move(s));}
  q.end();out.cases.push_back(std::move(c));}r.end();return out;
}
} // namespace mh4u::test_support::item_pop_lifecycle_fixture
