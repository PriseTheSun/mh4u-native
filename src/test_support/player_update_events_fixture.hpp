#pragma once
#include "test_support/player_update_model_case.hpp"
#include "player_update_events.hpp"
namespace mh4u::test_update_events {
using namespace mh4u::test_update_model;
namespace ev=mh4u::player_update_events;
constexpr std::array<U,2> registry_address{0x3301000,0x3329000};
constexpr const char* model_path="research/private/player-update-model-v1/oracle/derived-complete/oracle.bin";
constexpr const char* model_hash="e897d16a5b0a478b3924f643e7c825f3c703ee151b5c8d5335c80f02729ac08b";
struct EventRecord {U index{},stop{},sp{},fpscr{};ModelRecord model;std::array<Bytes,2>registry;std::array<std::vector<std::pair<U,U>>,2>known;std::vector<U>trace;std::vector<Call>calls;std::vector<StatusStore>stores;std::array<std::string,42>hashes;std::optional<std::array<U,7>>input;std::vector<std::array<U,5>>prefixes;};
std::size_t event_complete{},event_lookups{},event_prefix_reads{},event_prefix_stores{},event_stores{},event_store_bytes{};
EventRecord read_event(Reader&r,U ci,std::span<const Bytes>prior,std::size_t scenes){
 const auto n=r.u();check(n<=8*1024*1024,"events record bound");Reader q{r.take(n)};EventRecord v;v.index=q.u();check(v.index==ci,"events record identity");const auto mode=q.u();const auto hash=digest(q);check(mode==(ci<9?0U:1U),"compact model reference mode");std::span<const std::uint8_t>encoded;
 if(mode==0){const auto index=q.u();check(index==ci&&index<prior.size(),"compact model reference index");encoded=prior[index];}else{const auto begin=q.at;const auto size=q.u();check(size<=8*1024*1024,"inline model extent");q.take(size);encoded=q.b.subspan(begin,size+4);}
 check(picosha2::hash256_hex_string(encoded)==hash,"compact model source identity");Reader m{encoded};v.model=read_model(m,ci,scenes);m.end();
 for(U j=0;j<2;++j){v.registry[j]=q.bytes(4096);const auto count=q.u();check(count<=1024,"registry knowledge extent");for(U i=0;i<count;++i){const auto off=q.u(),size=q.u();check(size&&off<=4096&&size<=4096-off&&(v.known[j].empty()||v.known[j].back().first+v.known[j].back().second<off),"registry knowledge shape");v.known[j].emplace_back(off,size);}}
 v.stop=q.u();v.sp=q.u();v.fpscr=q.u();const bool prefix=v.model.contact.manager.position.direction.notification.status.prefix;check(v.stop==(prefix?0xb22508U:0xb148f0U)&&v.sp==(prefix?0x3003e10U:0x3003ea0U),"events exact checkpoint");auto count=q.u();check(count<=1024,"events trace bound");for(U i=0;i<count;++i)v.trace.push_back(q.u());check(prefix?v.trace.empty():!v.trace.empty()&&v.trace.front()==0xb14724,"contiguous events entry");count=q.u();check(count<=4,"events call bound");for(U i=0;i<count;++i){Call c;c.pc=q.u();c.ret=q.u();c.value=q.u();for(auto&x:c.args)x=q.u();c.start=q.u();c.end=q.u();check((c.pc==0xaf7378||c.pc==0xb07e80)&&c.start>0&&c.start<=v.trace.size()&&v.trace[c.start-1]==c.pc&&c.end>=c.start&&c.end<=v.trace.size(),"events actual call scope");v.calls.push_back(c);}v.stores=read_stores(q,16);for(auto&h:v.hashes)h=digest(q);count=q.u();check(count==(prefix?0U:1U),"event mask observation presence");if(count){std::array<U,7>input;for(auto&x:input)x=q.u();check(input[0]>0&&input[0]<=v.trace.size()&&v.trace[input[0]-1]==0xb14888&&input[6]==0,"event original mask ABI/r8");v.input=input;}count=q.u();check(count<=3,"prefix observation bound");for(U i=0;i<count;++i){std::array<U,5>x;for(auto&z:x)z=q.u();check(x[0]>0&&x[0]<=v.trace.size()&&v.trace[x[0]-1]==0xb148bc&&x[2]<32&&x[3]==i&&x[4]<=65535&&(v.prefixes.empty()||v.prefixes.back()[2]<x[2]),"ordered prefix source read");v.prefixes.push_back(x);}q.end();return v;
}
const Case& source_case(const EventRecord&r){return r.model.contact.manager.position.direction.notification.status.post.original.row.c;}
std::array<st::IndexedRow,256> registry_rows(const EventRecord&r){
 std::array<st::IndexedRow,256>out{};const auto&z=r.model.contact.manager.position.direction.notification.status.post.original.row;
 for(U id=0;id<256;++id)for(U selector:{1U,7U}){const U at=0x3300000+672*id+0x230+4*selector;std::optional<U>value;
  if(at>=0x3300000&&at<=0x3301000-4&&known(z.c,at,4))value=word(z.extra_before[2],at-0x3300000);
  for(U i=0;i<2;++i)if(at>=registry_address[i]&&at<=registry_address[i]+4096-4){const auto off=at-registry_address[i];for(const auto&[lo,size]:r.known[i])if(lo<=off&&off+4<=lo+size)value=word(r.registry[i],off);}
  if(selector==1)out[id].selector1=value;else out[id].selector7=value;
 }
 return out;
}
void adjust_expected(const EventRecord&r,StatusRecord&expected){
 for(const auto&w:r.stores){check(w.width==2||w.width==4,"events store width");bool found=false;
  if(w.address==caddr[1]+0x808&&w.width==4){put64(expected.final[1],0x808,w.value,w.width);found=true;}
  if(w.address==caddr[2]+0xb0&&w.width==4){put64(expected.final[2],0xb0,w.value,w.width);found=true;}
  if((w.address==caddr[2]+0x97c||w.address==caddr[2]+0x97e)&&w.width==2){put64(expected.final[2],w.address-caddr[2],w.value,w.width);found=true;}
  check(found&&std::find(r.trace.begin(),r.trace.end(),w.pc)!=r.trace.end(),"original events write belongs to source destinations/trace");++event_stores;event_store_bytes+=w.width;
 }
 for(U i=0;i<40;++i){const auto actual=i<3?picosha2::hash256_hex_string(expected.final[i]):r.model.hashes[i];check(actual==r.hashes[i],"complete original events replay and unchanged complement");}
 for(U i=0;i<2;++i)check(picosha2::hash256_hex_string(r.registry[i])==r.hashes[40+i],"registry raw source immutable");
}
void compare_effects(const EventRecord&r,const pa::UpdateEventsOutput&out,const ff::Context&context){
 const auto&e=out.effects;const auto&last=out.modeled.contacted.managed.positioned.directed.notified.status.adjusted.dispatched;U lookups=0,predicates=0;
 for(const auto&c:r.calls){if(c.pc==0xaf7378){check(lookups<e.lookup_count,"original query count");const auto&x=e.lookups[lookups++];check(c.args[0]==0x3300000&&c.args[1]==x.id&&c.args[2]==x.selector&&c.value==x.value&&x.source_offset==672U*x.id+0x230+4*x.selector,"original registry ABI and result");}else{check(c.args[0]==caddr[0],"original predicate Player");++predicates;}}
 check(lookups==e.lookup_count&&predicates==e.predicate_calls,"all original event helper calls");U writes=0,stores=0;for(const auto&w:r.stores){if(w.address==caddr[1]+0x808){check(writes<e.word808_write_count&&e.word808_writes[writes++]==w.value,"ordered original808 intermediate stores");}if(w.pc==0xb148dc)++stores;}
 check(writes==e.word808_write_count&&stores==e.prefix_store_count,"complete effects store counts");check(e.copied_heading==(std::count(r.trace.begin(),r.trace.end(),0xb1486c)==1),"original heading branch");check(e.bits_examined==std::count(r.trace.begin(),r.trace.end(),0xb148a8),"original loop examined bits");check(e.prefix_read_count==r.prefixes.size(),"original prefix count includes third selected read");
 const auto&layer=last.state.frame.animation.layers.at(0).animation.layer;check(r.input&&r.input->at(1)==layer.masks[0]&&r.input->at(2)==context.event_pointer(layer.bindings[0]),"model-produced mask and actual retained resource binding");
 for(U i=0;i<e.prefix_read_count;++i){const auto&x=e.prefix_reads[i];const auto&observed=r.prefixes[i];check(x.bit==observed[2]&&x.value==observed[4]&&x.binding==layer.bindings[0],"original prefix selected value and immutable logical identity");const auto pointer=context.event_pointer(x.binding);check(observed[1]==(pointer?pointer+2*x.bit:0),"original exact prefix source address");}
 event_lookups+=lookups;event_prefix_reads+=e.prefix_read_count;event_prefix_stores+=e.prefix_store_count;++event_complete;
}
} // namespace mh4u::test_update_events
