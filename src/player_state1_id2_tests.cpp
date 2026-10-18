// Independent expected bytes/calls from complete original ID2 ARM returns.
#include "test_support/player_continuation_arena.hpp"
#include <iostream>
#include <map>

namespace {
using namespace mh4u::test::player_continuation;
namespace oq=mh4u::player_object_query;
std::optional<std::uint32_t> absolute(const Case& c,std::uint32_t address,unsigned width){
 for(unsigned i=0;i<7;++i)if(address>=bases[i]&&std::uint64_t(address)+width<=std::uint64_t(bases[i])+sizes[i])return c.optional<std::uint32_t>(i,address-bases[i],width);
 throw std::runtime_error("Object field outside retained arenas");
}
oq::Collection object_collection(const Case& c){
 oq::Collection out;std::map<std::uint32_t,std::uint32_t> roots,data;
 for(unsigned i=0;i<32;++i){
  const auto pointer=absolute(c,bases[5]+0x18+4*i,4);
  if(!pointer)continue;
  if(!*pointer){out.slots[i]=oq::Absent{};continue;}
  const auto [root_it,new_root]=roots.emplace(*pointer,static_cast<std::uint32_t>(roots.size()));
  const auto root=root_it->second;out.slots[i]=oq::ObjectIndex{root};
  if(!new_root)continue;
  check(root<out.objects.size()&&*pointer<=0xffffffffU-0xe2c,"Declared root object extent");
  const auto d=absolute(c,*pointer+0xe28,4);if(!d)continue;
  if(!*d){out.objects[root].data=oq::Absent{};continue;}
  const auto [data_it,new_data]=data.emplace(*d,static_cast<std::uint32_t>(data.size()));
  const auto index=data_it->second;out.objects[root].data=oq::DataIndex{index};
  if(!new_data)continue;
  check(index<out.data.size()&&*d<=0xffffffffU-0x34d,"Declared object subobject extent");
  auto& object=out.data[index];
  const auto byte=[&](std::uint32_t offset)->std::optional<std::uint8_t>{auto v=absolute(c,*d+offset,1);return v?std::optional<std::uint8_t>{std::uint8_t(*v)}:std::nullopt;};
  object.field140=byte(0x140);object.field34c=byte(0x34c);object.field290=byte(0x290);object.field1c0=byte(0x1c0);object.field14c=byte(0x14c);object.field1b4=absolute(c,*d+0x1b4,4);
  object.position_bits={absolute(c,*d+0x198,4),absolute(c,*d+0x19c,4),absolute(c,*d+0x1a0,4)};
  object.field349=byte(0x349);if(const auto v=absolute(c,*d+0x1c4,2))object.field1c4=static_cast<std::uint16_t>(*v);
 }
 return out;
}
oq::DataFields& slot_data(oq::Collection& collection,std::size_t slot){
 const auto root=std::get<oq::ObjectIndex>(collection.slots.at(slot)).value;
 const auto index=std::get<oq::DataIndex>(collection.objects.at(root).data).value;
 return collection.data.at(index);
}
void run(const Bytes& bytes,const pt::Constants& constants,const std::vector<std::array<std::string,3>>& args,bool projection){
 Reader r{bytes};check(r.bytes(8)==Bytes({'M','H','4','U','S','2','D','2'}),"ID2 fixture schema");check(r.integer()==252&&r.integer()==7&&r.integer()==3&&r.integer()==2,"ID2 corpus counts");
 check(ints<3>(r)==std::array{375U,140U,0x2500000U}&&ints<3>(r)==std::array{112U,112U,0x2408000U},"Corpus and distinct reference manager identities");
 for(unsigned i=0;i<7;++i)check(r.integer()==bases[i]&&r.integer()==sizes[i],"Arena identity");
 auto [headers,sources]=load_sources(r,args);const auto globalbytes=r.bytes(64);mc::Globals globals;
 for(unsigned i=0;i<3;++i)globals.fields6c_tables[i]=words<2>(globalbytes,8*i);
 globals.vector10e6370=words<3>(globalbytes,24);mc::Sampling sampling;sampling.templates.base_translation_10e62c0=words<3>(globalbytes,36);sampling.templates.identity_10e6560=words<4>(globalbytes,48);
 Context resolver(sources);Bindings events(sources);sampling.resolver={&resolver,Context::resolve};
 std::array<std::size_t,2> counts{};std::array<std::uint32_t,2> previous{};std::array<std::size_t,9> observed_totals{};
 std::size_t transitions=0,magnitude=0,setters=0,accepted=0,wrappers=0,compared_bytes=0,boundary_errors=0,boundary_ignored=0;bool noop_checked=false;
 for(unsigned index=0;index<252;++index){const auto c=next(r,true);check(counts[c.corpus]==0||previous[c.corpus]<c.index,"Corpus original indices");previous[c.corpus]=c.index;++counts[c.corpus];
  try{
   retain_inactive(c.before[0],2,8,resolver,events);const auto storage=decode(c.before[0],8,resolver,events,2);const auto state=project(c,storage.fields);const auto shared=shared_fields(c);const st::Fields fields{c.optional<std::uint32_t>(2,0x50,4)};
   check(state.player.pbw.field371==1&&state.player.pbw.field372==2&&state.player.pbw.fields5[0]==c.substate,"Original ID2 state");check(word(c.before[0],0x234)==2,"Original active prefix");
   compare_snapshots(serialize_shared(c,state,fields,storage.layers,resolver,events,shared),c.before,"Complete ID2 input roundtrip");
   const auto slots=object_collection(c);const auto preserved_slots=slots;const auto resources=model(c,headers,sources);
   st::ContinuationContext context;context.constants=&constants;context.shared_fields=&shared;context.reference=reference(c,c.corpus?0x2408000U:0x2500000U);context.area.mode_c8c=c.optional<std::uint32_t>(3,0xc8c,4);context.delta_time_bits=c.optional<std::uint32_t>(0,0x14,4);context.context_ad50=c.optional<std::uint32_t>(4,0xad50,4);context.manager=&slots;
   if(projection){auto initial=c;initial.after=c.before;initial.setters.clear();identities(initial,storage.layers,resolver,events);continue;}
   resolver.resolutions.clear();const auto result=st::apply(state,fields,storage.layers,2,{},resources,globals,sampling,{},context);
   if(const auto* e=std::get_if<st::Error>(&result))throw std::runtime_error("Native error "+std::to_string(unsigned(e->code))+": "+std::string(e->message));
   const auto& out=std::get<st::Output>(result);const auto& e=out.effects;
   compare_snapshots(serialize_shared(c,out.state,out.fields,out.layers,resolver,events,out.shared_fields),c.after,"Complete ID2 original after");
   compare_snapshots(serialize_shared(c,state,fields,storage.layers,resolver,events,shared),c.before,"Complete ID2 input unchanged");identities(c,out.layers,resolver,events);check(slots==preserved_slots,"Borrowed object facts unchanged");
   check(e.request_count==c.setters.size(),"Original setter count");
   for(unsigned i=0;i<e.request_count;++i){const auto& call=e.requests[i];const auto& q=call.request;const auto& wanted=c.setters[i];
    check(std::array{q.layer,q.selector,q.attributes,std::bit_cast<std::uint32_t>(q.interpolation),std::bit_cast<std::uint32_t>(q.start_frame),std::bit_cast<std::uint32_t>(q.speed),call.command_flags,std::uint32_t(call.accepted)}==std::array{wanted[0],wanted[1],wanted[2],wanted[3],wanted[4],wanted[5],wanted[6],wanted[7]},"Original setter full arguments/acceptance");accepted+=call.accepted;
   }
   std::vector<std::array<std::uint32_t,3>> calls;std::size_t mags=0;
   for(auto event:c.events){if(event[0]==0xb1c050)calls.push_back({event[2],event[3],event[4]});mags+=event[0]==0xb417a4;}
   check(calls.size()<=1&&bool(e.transition)==!calls.empty(),"Original transition count");
   if(e.transition){const auto& a=e.transition->arguments;check(std::array{a.state_arg,a.action_arg,a.flags_arg}==calls[0],"Original transition full arguments");++transitions;}
   check(e.magnitude_queries==mags,"Original repeated magnitude calls");magnitude+=mags;
   check(c.installs.empty()&&!e.installation,"ID2 continuation has no table installation");
   check(c.queries.empty()&&e.queries==0&&e.samples==0&&resolver.resolutions.empty(),"Original continuation does not run live query/binder");
   check(!e.selection_request&&c.wrappers.size()==std::size_t(bool(e.command)),"Original direct wrapper count");
   if(e.command){const auto& q=*e.command;const auto& w=c.wrappers[0];check(q.entry==mc::Entry::restart&&w[0]==0xb0c060&&q.selector==w[1]&&std::bit_cast<std::uint32_t>(q.interpolation)==w[2]&&std::bit_cast<std::uint32_t>(q.start_frame)==w[3],"Original restart wrapper arguments");}
   const auto& o=c.observations;
   check(std::array<std::uint32_t,4>{e.cost_calls,e.cost_updates,e.position_updates,e.manager_queries}==std::array{o[0],o[1],o[2],o[3]},"Original cost/position stores and manager calls");
   check(bool(e.object_query)==(o[3]!=0),"Object-query presence tracks source call");
   if(e.object_query){const auto& q=*e.object_query;check(std::array<std::uint32_t,4>{q.slots_examined,q.object_queries,q.direction_queries,q.matched}==std::array{o[4],o[5],o[6],o[7]},"Original slots/candidate/direction/return branch");}
   // Native-domain guards; these are not additional original-ARM cases.
   if(c.corpus==1&&c.index==2){
    check(e.object_query&&e.object_query->matched,"Discriminating manager case");
    const auto reject=[&](const pcs::State& input,sf::Fields extra,st::ContinuationContext facts,st::ErrorCode expected,bool null_shared=false){
     facts.shared_fields=null_shared?nullptr:&extra;const auto before_extra=extra;
     const auto before=serialize_shared(c,input,fields,storage.layers,resolver,events,extra);
     const auto result=st::apply(input,fields,storage.layers,2,{},resources,globals,sampling,{},facts);
     check(std::holds_alternative<st::Error>(result),"Typed ID2 boundary rejection");const auto error=std::get<st::Error>(result);check(error.code==expected,"ID2 boundary category");
     compare_snapshots(serialize_shared(c,input,fields,storage.layers,resolver,events,extra),before,"ID2 late error does not publish prefix");check(extra==before_extra,"ID2 shared-field omission preserved on error");++boundary_errors;return error;
    };
    reject(state,shared,context,st::ErrorCode::dynamics,true);
    auto facts=context;facts.delta_time_bits.reset();reject(state,shared,facts,st::ErrorCode::dynamics);
    facts=context;facts.context_ad50.reset();reject(state,shared,facts,st::ErrorCode::dynamics);
    auto extra=shared;extra.pbw.field354_bits.reset();reject(state,extra,context,st::ErrorCode::dynamics);
    extra=shared;extra.pbw.field10_bits.reset();reject(state,extra,context,st::ErrorCode::dynamics);
    extra=shared;extra.pw.field7f0.reset();reject(state,extra,context,st::ErrorCode::unknown_field);
    extra=shared;extra.pw.field6a8=0;auto spatial_error=reject(state,extra,context,st::ErrorCode::spatial_acquisition);
    check(std::get<mh4u::player_spatial_acquisition::Error>(spatial_error.cause).code==mh4u::player_spatial_acquisition::ErrorCode::unknown_field,"Uncached acquisition retains missing-field cause");
    extra=shared;extra.pw.field6a4_bits=0x7fc12345;spatial_error=reject(state,extra,context,st::ErrorCode::spatial_acquisition);
    check(std::get<mh4u::player_spatial_acquisition::Error>(spatial_error.cause).code==mh4u::player_spatial_acquisition::ErrorCode::nonfinite_operand,"Cached acquisition retains numeric cause");
    facts=context;facts.manager=oq::Unknown{};auto failure=reject(state,shared,facts,st::ErrorCode::object_query);check(std::get<oq::Error>(failure.cause).code==oq::ErrorCode::unknown_field,"Unknown manager reason retained");
    facts=context;auto invalid_collection=slots;invalid_collection.slots[0]=oq::ObjectIndex{32};facts.manager=&invalid_collection;failure=reject(state,shared,facts,st::ErrorCode::object_query);check(std::get<oq::Error>(failure.cause).code==oq::ErrorCode::invalid_index,"Manager index reason retained");
    auto changed=slots;changed.slots[0]=oq::Unknown{};facts=context;facts.manager=&changed;failure=reject(state,shared,facts,st::ErrorCode::object_query);check(std::get<oq::Error>(failure.cause).slot==0,"Unknown slot identity retained");
    changed=slots;slot_data(changed,0).field140.reset();reject(state,shared,facts,st::ErrorCode::object_query);
    for(auto kind:{24,110}){changed=slots;slot_data(changed,0).field1c0=static_cast<std::uint8_t>(kind);failure=reject(state,shared,facts,st::ErrorCode::object_query);check(std::get<oq::Error>(failure.cause).code==oq::ErrorCode::unsupported_position,"Unrecovered position helper is not false");}
    changed=slots;slot_data(changed,0).position_bits[0].reset();reject(state,shared,facts,st::ErrorCode::object_query);
    changed=slots;slot_data(changed,0).position_bits[0]=0x7f800000;failure=reject(state,shared,facts,st::ErrorCode::object_query);check(std::get<oq::Error>(failure.cause).reference.has_value(),"Angular numeric cause retained");
    auto input=state;input.player.global.field5d=1;failure=reject(input,shared,context,st::ErrorCode::object_query);check(std::get<oq::Error>(failure.cause).code==oq::ErrorCode::unsupported_availability,"Unrecovered network availability is not false");
    facts=context;facts.constants=nullptr;reject(state,shared,facts,st::ErrorCode::unknown_field);
   }
   if(c.substate==3&&!noop_checked){
    const auto ignored=st::apply(state,{},storage.layers,2,{},rq::Model{},mc::Globals{},mc::Sampling{},mc::Limits{},st::ContinuationContext{});
    check(std::holds_alternative<st::Output>(ignored),"ID2 no-op does not consume new context");const auto& value=std::get<st::Output>(ignored);check(value.shared_fields==sf::Fields{}&&!value.fields.field50_bits,"No-op invents no shared fields");compare_snapshots(serialize_shared(c,value.state,value.fields,value.layers,resolver,events,value.shared_fields),c.after,"ID2 no-op original result");++boundary_ignored;noop_checked=true;
   }
   for(unsigned i=0;i<9;++i)observed_totals[i]+=o[i];setters+=e.request_count;wrappers+=c.wrappers.size();for(auto n:sizes)compared_bytes+=n;
  }catch(const std::exception& e){throw std::runtime_error("Corpus "+std::to_string(c.corpus)+" case "+std::to_string(c.index)+" "+c.name+": "+e.what());}
 }
 check(r.p==bytes.size()&&counts==std::array<std::size_t,2>{140,112},"Complete252 original ID2 coverage");
 if(!projection)check(transitions==114&&magnitude==265&&setters==44&&accepted==22&&wrappers==22&&observed_totals==std::array<std::size_t,9>{216,191,178,137,1492,105,88,76,114},"Exact original observation totals");
 if(!projection)check(boundary_errors==18&&boundary_ignored==1,"ID2 guard coverage");
 std::cout<<"{\"status\":\""<<(projection?"projected":"passed")<<"\",\"cases\":252,\"checks\":"<<checks<<",\"transitions\":"<<transitions<<",\"magnitude_queries\":"<<magnitude<<",\"setters\":"<<setters<<",\"accepted\":"<<accepted<<",\"wrappers\":"<<wrappers<<",\"compared_bytes\":"<<compared_bytes<<",\"boundary_errors\":"<<boundary_errors<<",\"boundary_ignored\":"<<boundary_ignored<<"}\n";
}
}
int main(int argc,char** argv){try{
 bool projection=false;int p=1;if(argc>1&&std::string(argv[1])=="--project-only"){projection=true;++p;}
 if(argc-p!=13)throw std::runtime_error("Usage: [--project-only] fixture sha constants sha entry path sha entry path sha entry path sha");
 const auto fixture=read(argv[p],256U*1024U*1024U);check(picosha2::hash256_hex_string(fixture)==argv[p+1],"Fixture hash");
 auto c=pt::Constants::decode(read(argv[p+2],88),{argv[p+3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"});if(auto* e=std::get_if<pt::Error>(&c))throw std::runtime_error(std::string(e->message));
 std::vector<std::array<std::string,3>> resources;for(int i=p+4;i<argc;i+=3)resources.push_back({argv[i],argv[i+1],argv[i+2]});
 run(fixture,std::get<pt::Constants>(c),resources,projection);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
