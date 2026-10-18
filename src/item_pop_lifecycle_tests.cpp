#include "test_support/item_pop_lifecycle_fixture.hpp"
#include <cfenv>
#include <iostream>
namespace fx=mh4u::test_support::item_pop_update_fixture;
namespace lf=mh4u::test_support::item_pop_lifecycle_fixture;
namespace f=mh4u::test_support::item_pop_manager;
namespace up=mh4u::item_pop_update;namespace ib=mh4u::item_pop_builder;
namespace wl=mh4u::player_world_lookup;namespace sec=mh4u::item_pop_secondary;
namespace pt=mh4u::player_transition;
using f::U;using f::Bytes;using f::need;
std::size_t updates{},calls{},queries{},hits{},stores{},store_bytes{},typed{},lazy{},changed_manager{},changed_global{};
up::Effects effects{};
void query(const fx::Query& q,const wl::Manager& manager,const mh4u::item_pop_resources::Resource& list){const auto saved=manager;wl::Request req{};req.area=q.area;for(U k=0;k<3;++k)req.point[k]=q.point[k];const auto result=wl::query({&manager,q.mask},req);if(const auto* e=std::get_if<wl::Error>(&result))throw std::runtime_error(std::string("query: ")+std::string(e->field));const auto& out=std::get<wl::Output>(result);const auto expected=fx::expected_lookup(q,list);need(out.match==expected.match,"lookup typed output differs from source");need(out.returned==q.returned&&out.visited==q.visits.size(),"lookup result/visits differs from ARM");need(out.match.has_value()==(q.selected_pointer!=0),"lookup presence differs from ARM");Bytes projected(q.before.begin(),q.before.end());if(out.match){++hits;const auto& m=*out.match;need(f::encode_link(wl::Index{m.index})==q.selected_pointer,"lookup selected identity differs from ARM");need(m.status==q.returned,"lookup status differs from ARM");f::put(projected,0,m.status);for(U k=0;k<4;++k)f::put(projected,0x10+4*k,m.vector34[k]);for(U k=0;k<3;++k)f::put(projected,0x30+4*k,m.words40[k]);need(m.field28.has_value(),"lookup lost copied field28");if(auto p=std::get_if<U>(&*m.field28))f::put(projected,0x50,*p);else{const auto& ref=std::get<wl::ResourceReference>(*m.field28);need(ref.sha256==list.sha256()&&ref.buffer_offset<list.allocation_size(),"lookup field28 identity");f::put(projected,0x50,f::LB+ref.buffer_offset);}}
 need(std::equal(projected.begin(),projected.end(),q.after.begin()),"lookup output bytes differ from ARM");need(manager==saved,"lookup mutated borrowed manager");++queries;}

bool same_context(const up::Context& a,const up::Context& b){
 return a.clock.field50==b.clock.field50&&a.clock.field57==b.clock.field57&&a.clock.field5c_bits==b.clock.field5c_bits&&a.category_e250==b.category_e250&&a.environment.singleton_manager==b.environment.singleton_manager&&a.environment.set==b.environment.set&&a.environment.list==b.environment.list&&a.environment.area.mode_c8c==b.environment.area.mode_c8c&&a.environment.area.mapping.data()==b.environment.area.mapping.data()&&a.environment.area.mapping.size()==b.environment.area.mapping.size()&&a.environment.area.categories.data()==b.environment.area.categories.data()&&a.environment.area.categories.size()==b.environment.area.categories.size()&&a.limits.bytes==b.limits.bytes&&a.limits.loaded_bytes==b.limits.loaded_bytes&&a.limits.records==b.limits.records&&a.limits.representation_bytes==b.limits.representation_bytes;
}
Bytes encode_global(const pt::GlobalFields& g,std::span<const std::uint8_t> before){
 Bytes b(before.begin(),before.end());if(g.field20)f::put(b,0x20,*g.field20,2);f::put(b,0x22,g.field22,2);
 f::put(b,0x2b,g.field2b,1);f::put(b,0x2f,g.field2f,1);f::put(b,0x5d,g.field5d,1);if(g.fieldd72)f::put(b,0xd72,*g.fieldd72,1);return b;
}
void boundaries(const lf::Fixture& fixture,f::Resources& resources){
 auto example=[&](U index){const auto& c=fixture.cases[index];const auto& s=c.steps[0];const auto& list=resources.get(c.sources[1].path,c.sources[1].sha,c.sources[1].size);return f::decode_manager(s.before,list,f::BindingProfile::constructor_observed);};
 auto reject=[&](U index,auto change,up::ErrorCode code,std::string_view field, bool nested,sec::ErrorCode nested_code=sec::ErrorCode::unknown_field){
  const auto& c=fixture.cases[index];const auto& step=c.steps[0];const auto& set=resources.get(c.sources[0].path,c.sources[0].sha,c.sources[0].size);const auto& list=resources.get(c.sources[1].path,c.sources[1].sha,c.sources[1].size);
  auto manager=example(index);auto global=lf::global(step.global_before,step.facts[7]);auto context=lf::context(step,manager,set,list);change(manager,global,context);const auto saved=manager;const auto gs=global;const auto cs=context;
  const auto result=up::apply(manager,global,context);need(std::holds_alternative<up::Error>(result),"negative accepted");const auto& e=std::get<up::Error>(result);if(e.code!=code||e.field!=field)throw std::runtime_error("negative cause: "+std::string(e.field)+" expected "+std::string(field));need(!nested||e.secondary.has_value(),"typed secondary cause lost");if(nested)need(e.secondary->code==nested_code,"typed secondary distinction lost");need(manager==saved&&global==gs&&same_context(context,cs),"negative mutated borrowed owners");++typed;
 };
 using E=up::ErrorCode;
 for(U index:{6U,22U})reject(index,[&](auto& m,auto&,auto& c){const U slot=index==6?0:1;m.head=wl::Index{slot};m.records[slot].next=wl::Absent{};m.records[slot].field1c_bits.reset();c.clock.field5c_bits.reset();},E::unknown_field,"record1c",false);
 for(U index:{0U,6U,22U}){
  const auto code=index==22?E::secondary_failure:E::dependency;
  reject(index,[](auto&,auto&,auto& c){c.environment.singleton_manager=wl::Unknown{};},code,"singleton_manager",true);
  reject(index,[](auto&,auto&,auto& c){c.environment.singleton_manager=wl::Absent{};},code,"singleton_manager",true,sec::ErrorCode::null_binding);
  const auto foreign=example(index);reject(index,[&](auto&,auto&,auto& c){c.environment.singleton_manager=&foreign;},code,"singleton_manager",true,sec::ErrorCode::foreign_manager);
 }
 for(U index:{6U,22U}){
  const auto code=index==22?E::secondary_failure:E::dependency;
  reject(index,[](auto&,auto&,auto& c){c.environment.set=wl::Unknown{};},code,"manager14/Set",true);
  reject(index,[](auto&,auto&,auto& c){c.environment.list=wl::Unknown{};},code,"manager18/List",true);
 }
 reject(22,[](auto&,auto& g,auto&){g.field20.reset();},E::unknown_field,"global20",false);
 reject(22,[](auto&,auto&,auto& c){c.environment.area.mode_c8c.reset();},E::secondary_failure,"area remap",true,sec::ErrorCode::selection_failure);
 reject(22,[](auto& m,auto&,auto&){m.records[1].field57=255;},E::secondary_failure,"Remapped ordinal escapes admitted descriptor table",true,sec::ErrorCode::unsupported_descriptor);
 reject(6,[](auto& m,auto&,auto&){m.records[0].field2f=255;},E::resource_failure,"resource",false);
 reject(0,[](auto& m,auto&,auto&){m.field4a20.reset();},E::unknown_field,"manager4a20",false);
 reject(0,[](auto&,auto&,auto& c){c.limits.records=0;},E::resource_limit,"update work",false);
 reject(0,[](auto&,auto&,auto& c){c.limits.representation_bytes=0;},E::resource_limit,"update output",false);
 // Retained ARM waiting/dead branches, further narrowed native unknowns.
 for(U index:{51U,52U,53U,55U,56U}){
  const auto& c=fixture.cases[index];const auto& step=c.steps[0];const auto& set=resources.get(c.sources[0].path,c.sources[0].sha,c.sources[0].size);const auto& list=resources.get(c.sources[1].path,c.sources[1].sha,c.sources[1].size);auto m=example(index);auto g=lf::global(step.global_before,step.facts[7]);auto context=lf::context(step,m,set,list);
  if(index==52)context.environment.singleton_manager=wl::Unknown{};
  const auto result=up::apply(m,g,context);need(std::holds_alternative<up::Output>(result),"lazy unused fact consumed");need(std::get<up::Output>(result).manager==f::decode_manager(step.after,list,f::BindingProfile::constructor_observed),"lazy output differs from original");++lazy;
 }
 const auto rounding=std::fegetround();need(std::fesetround(FE_DOWNWARD)==0,"rounding control");reject(6,[](auto& m,auto&,auto&){m.head=wl::Index{0};m.records[0].next=wl::Absent{};},E::arithmetic,"record1c-delta",false);need(std::fesetround(rounding)==0,"rounding restore");
}
int main(int argc,char**argv){try{
 need(argc==4,"usage: item-pop-lifecycle-tests fixture sha256 root");auto fixture=lf::load(argv[1],argv[2]);const auto raw=fixture.bytes;f::Resources resources{argv[3],{},{}};
 for(const auto& c:fixture.cases){const auto& set=resources.get(c.sources[0].path,c.sources[0].sha,c.sources[0].size);const auto& list=resources.get(c.sources[1].path,c.sources[1].sha,c.sources[1].size);const auto result=ib::build_fresh(c.set_present?&set:nullptr,c.list_present?&list:nullptr,c.builder);need(std::holds_alternative<ib::Built>(result),"builder rejected");const auto& built=std::get<ib::Built>(result);auto manager=built.manager;auto global=built.global;
 need(manager==f::decode_manager(c.built,list,f::BindingProfile::constructor_observed),"builder state differs from original");need(global==lf::global(c.global_after,f::read(c.global_after,0x20,2)),"builder global differs from original");need(encode_global(global,c.global_before)==Bytes(c.global_after.begin(),c.global_after.end()),"builder global bytes differ");
 Bytes previous(c.built.begin(),c.built.end()),gp(c.global_after.begin(),c.global_after.end());
 for(const auto& step:c.steps){
  for(auto m:step.mutations)f::put(previous,0x1c+0x94*m.slot+m.offset,m.value,m.width);
  for(auto m:step.external_mutations){if(m.address>=f::M&&m.address<f::M+0x5000)f::put(previous,m.address-f::M,m.value,m.width);else if(m.address>=f::G&&m.address<f::G+4096)f::put(gp,m.address-f::G,m.value,m.width);else need(m.address==0xfb8a1c,"external singleton address");}
  need(previous==Bytes(step.before.begin(),step.before.end())&&gp==Bytes(step.global_before.begin(),step.global_before.end()),"sequence mutations continuity");
  if(!step.mutations.empty()||!step.external_mutations.empty())manager=f::decode_manager(step.before,list,f::BindingProfile::constructor_observed);
  need(manager==f::decode_manager(step.before,list,f::BindingProfile::constructor_observed),"sequential Manager differs");
  global=lf::global(step.global_before,step.facts[7]);const auto gs=global;const auto ms=manager;auto context=lf::context(step,manager,set,list);const auto cs=context;
  Bytes replay(step.before.begin(),step.before.end()),gr(step.global_before.begin(),step.global_before.end());
  for(auto s:step.stores){if(s.address>=f::M&&s.address<f::M+0x5000)f::put(replay,s.address-f::M,s.value,s.width);else f::put(gr,s.address-f::G,s.value,s.width);++stores;store_bytes+=s.width;}
  need(replay==Bytes(step.after.begin(),step.after.end())&&gr==Bytes(step.global_after.begin(),step.global_after.end()),"source stores replay");
  up::Effects observed{};std::size_t observed_calls=0;
  for(const auto& call:step.calls){if(call.pc==0x5aa4cc){f::decode_link(call.args[0]);++observed_calls;}else if(call.pc==0x2b555c){need(call.args[1]<2,"source RNG channel");++observed.rng_calls[call.args[1]];}else if(call.pc==0x5b9d84){f::decode_link(call.args[1]);observed.removed+=f::read(step.before,call.args[1]-f::M+4,1)!=0;}else if(call.pc==0x23dd60)++observed.primary_renewed;else if(call.pc==0x5b928c){need(call.result<=1,"source secondary return");if(call.result)++observed.secondary_renewed;else ++observed.secondary_failed;}}
  const auto update=up::apply(manager,global,context);if(const auto* e=std::get_if<up::Error>(&update))throw std::runtime_error(c.name+": "+std::string(e->field));const auto& out=std::get<up::Output>(update);
  need(out.visited==observed_calls&&!out.clock_gated,"source virtual call count");need(out.effects==observed,"source lifecycle effects");
  need(out.manager==f::decode_manager(step.after,list,f::BindingProfile::constructor_observed),"native lifecycle state differs from ARM");need(out.global==lf::global(step.global_after,step.facts[7]?std::optional<U>(f::read(step.global_after,0x20,2)):std::nullopt),"native global differs from ARM");
  previous=f::encode_manager(out.manager,step.before,list);gp=encode_global(out.global,step.global_before);need(previous==Bytes(step.after.begin(),step.after.end()),"native Manager bytes differ from ARM");need(gp==Bytes(step.global_after.begin(),step.global_after.end()),"native Global bytes differ from ARM");
  need(manager==ms&&global==gs&&same_context(context,cs),"update changed borrowed owners");for(std::size_t i=0;i<step.before.size();++i)changed_manager+=step.before[i]!=step.after[i];for(std::size_t i=0;i<step.global_before.size();++i)changed_global+=step.global_before[i]!=step.global_after[i];
  ++updates;calls+=out.visited;for(U i=0;i<2;++i)effects.rng_calls[i]+=out.effects.rng_calls[i];effects.removed+=out.effects.removed;effects.primary_renewed+=out.effects.primary_renewed;effects.secondary_renewed+=out.effects.secondary_renewed;effects.secondary_failed+=out.effects.secondary_failed;
  manager=out.manager;global=out.global;for(const auto& q:step.queries)query(q,manager,list);
 }}boundaries(fixture,resources);need(fixture.bytes==raw,"fixture storage changed");const auto bytes=resources.unchanged();
 need(updates==100&&calls==2276&&queries==236&&hits==182&&stores==5773&&store_bytes==18357,"corpus counts");
 std::cout<<"{\"status\":\"passed\",\"builders\":"<<fixture.cases.size()<<",\"updates\":"<<updates<<",\"record_calls\":"<<calls<<",\"queries\":"<<queries<<",\"hits\":"<<hits<<",\"rng0\":"<<effects.rng_calls[0]<<",\"rng1\":"<<effects.rng_calls[1]<<",\"removed\":"<<effects.removed<<",\"primary_renewed\":"<<effects.primary_renewed<<",\"secondary_renewed\":"<<effects.secondary_renewed<<",\"secondary_failed\":"<<effects.secondary_failed<<",\"source_stores\":"<<stores<<",\"source_store_bytes\":"<<store_bytes<<",\"changed_manager_bytes\":"<<changed_manager<<",\"changed_global_bytes\":"<<changed_global<<",\"manager_projected_bytes\":"<<updates*0x5000<<",\"global_projected_bytes\":"<<updates*4096<<",\"query_projected_bytes\":"<<queries*4096<<",\"lookup_written_bytes\":"<<hits*36<<",\"resource_immutable_bytes\":"<<bytes<<",\"typed_errors\":"<<typed<<",\"lazy_controls\":"<<lazy<<",\"checks\":"<<f::checks<<"}\n";
 return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
