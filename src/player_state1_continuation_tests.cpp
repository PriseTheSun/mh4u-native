// Independent projection of original ARM continuation fixtures. Expected state
// consists exclusively of retained after bytes and observed original calls.
#include "player_state1.hpp"
#include "test_support/player_continuation_arena.hpp"
#include "picosha2.h"
#include <fstream>
#include <iostream>
#include <set>

namespace {
using namespace mh4u::test::player_continuation;
namespace st=mh4u::player_state1;
void run(const Bytes& bytes,const pt::Constants& constants,const std::vector<std::array<std::string,3>>& resource_args,bool projection_only){
 Reader r{bytes};check(r.bytes(8)==Bytes({'M','H','4','U','S','1','D','1'}),"Fixture schema");check(r.integer()==375&&r.integer()==235&&r.integer()==7&&r.integer()==3,"Exact subset counts");
 for(unsigned i=0;i<7;++i)check(r.integer()==bases[i]&&r.integer()==sizes[i],"Arena identity");
 auto [headers,sources]=load_sources(r,resource_args);
 const auto globals_bytes=r.bytes(64);mc::Globals globals;
 for(unsigned i=0;i<3;++i)globals.fields6c_tables[i]=words<2>(globals_bytes,8*i);
 globals.vector10e6370=words<3>(globals_bytes,24);mc::Sampling sampling;
 sampling.templates.base_translation_10e62c0=words<3>(globals_bytes,36);sampling.templates.identity_10e6560=words<4>(globals_bytes,48);
 Context context(sources);Bindings events(sources);sampling.resolver={&context,Context::resolve};
 std::size_t transitions=0,magnitude_queries=0,setters=0,accepted=0,wrapper_count=0,boundary_errors=0,boundary_ignored=0;
 bool layer_boundaries=false;std::set<unsigned> noop_boundaries;
 std::size_t actions1=0,actions4=0,actions9=0;std::uint32_t previous=0;bool first=true;
 for(unsigned index=0;index<235;++index){auto c=next(r);check(first||previous<c.index,"Original subset indices");first=false;previous=c.index;
  try{retain_inactive(c.before[0],2,8,context,events);const auto storage=decode(c.before[0],8,context,events,2);const auto state=project(c,storage.fields);const st::Fields fields{c.optional<std::uint32_t>(2,0x50,4)};const auto resources=model(c,headers,sources);const auto shared=shared_fields(c);
   check(state.player.pbw.field371==1&&state.player.pbw.field372==c.action&&state.player.pbw.fields5[0]==c.substate,"Case state matches source");
   compare_snapshots(serialize_shared(c,state,fields,storage.layers,context,events,shared),c.before,"Input roundtrip");
   if(projection_only){auto unchanged=c;unchanged.after=c.before;unchanged.setters.clear();identities(unchanged,storage.layers,context,events);}
   else{
    st::ContinuationContext continuation;continuation.constants=&constants;
    continuation.shared_fields=&shared;continuation.reference=reference(c,bases[5]);
    continuation.area.mode_c8c=c.optional<std::uint32_t>(3,0xc8c,4);
    check(word(c.before[0],0x234)==2,"Actual active layer prefix");context.resolutions.clear();
    auto result=st::apply(state,fields,storage.layers,2,{},resources,globals,sampling,{},continuation);
    if(auto* e=std::get_if<st::Error>(&result))throw std::runtime_error("Native error "+std::to_string(unsigned(e->code))+": "+std::string(e->message));
    const auto out=std::get<st::Output>(std::move(result));const auto& effects=out.effects;
    compare_snapshots(serialize_shared(c,out.state,out.fields,out.layers,context,events,out.shared_fields),c.after,"Original complete after");
    compare_snapshots(serialize_shared(c,state,fields,storage.layers,context,events,shared),c.before,"Native input unchanged");
    identities(c,out.layers,context,events);
    check(effects.request_count==c.setters.size(),"Original setter count");
    for(unsigned i=0;i<effects.request_count;++i){const auto& call=effects.requests[i];const auto& q=call.request;const auto& expected=c.setters[i];
     check(std::array{q.layer,q.selector,q.attributes,std::bit_cast<std::uint32_t>(q.interpolation),std::bit_cast<std::uint32_t>(q.start_frame),std::bit_cast<std::uint32_t>(q.speed),call.command_flags,std::uint32_t(call.accepted)}==std::array{expected[0],expected[1],expected[2],expected[3],expected[4],expected[5],expected[6],expected[7]},"Original setter arguments and acceptance");accepted+=call.accepted;
    }
    std::vector<std::array<std::uint32_t,3>> transitions_original;std::size_t magnitudes=0;
    for(auto event:c.events){if(event[0]==0xb1c050)transitions_original.push_back({event[2],event[3],event[4]});magnitudes+=event[0]==0xb417a4;}
    check(transitions_original.size()<=1&&bool(effects.transition)==!transitions_original.empty(),"Original transition count");
    if(effects.transition){const auto& args=effects.transition->arguments;check(std::array{args.state_arg,args.action_arg,args.flags_arg}==transitions_original[0],"Original transition full arguments");++transitions;}
    check(effects.magnitude_queries==magnitudes,"Original repeated virtual98 count");magnitude_queries+=magnitudes;
    check(!effects.installation&&c.installs.empty(),"Continuation invented table installation");
    check(effects.queries==0&&effects.samples==0&&c.queries.empty()&&context.resolutions.empty(),"Continuation invented live query/sampling");
    check(c.wrappers.size()==std::size_t(bool(effects.command))+std::size_t(bool(effects.selection_request)),"Original wrapper count");
    if(effects.command){const auto& command=*effects.command;const auto& w=c.wrappers[0];
     check(command.entry==mc::Entry::restart&&w[0]==0xb0c060&&command.selector==w[1]&&std::bit_cast<std::uint32_t>(command.interpolation)==w[2]&&std::bit_cast<std::uint32_t>(command.start_frame)==w[3],"Direct command wrapper arguments");
    }
    if(effects.selection_request){const auto& request=*effects.selection_request;const auto& w=c.wrappers[0];
     check(w[0]==0xb0c198&&request.selector==w[1]&&request.interpolation_bits==w[2]&&request.frame_bits==w[3],"Selection wrapper arguments");
     check(request.mode==0,"Source b1d114 mode0 contract");
    }
    setters+=effects.request_count;wrapper_count+=c.wrappers.size();
    // Additional native-domain controls, not new ARM equivalence cases.
    const auto reject=[&](const pcs::State& input,const st::Fields& extra,const std::vector<mp::Layer>& layer_input,const rq::Model& model_input,const st::ContinuationContext& facts,st::ErrorCode code){
     const auto preserved=serialize(c,input,extra,layer_input,context,events);
     auto bad=st::apply(input,extra,layer_input,2,{},model_input,globals,sampling,{},facts);
     check(std::holds_alternative<st::Error>(bad),"Expected typed boundary error");const auto error=std::get<st::Error>(bad);check(error.code==code,"Boundary error category");
     compare_snapshots(serialize(c,input,extra,layer_input,context,events),preserved,"Boundary input rollback");++boundary_errors;return error;
    };
    if(c.index==0){
     const auto unknown=reject(state,{},storage.layers,resources,continuation,st::ErrorCode::unknown_field);check(unknown.message=="pw+50","Missing consumed timer identity");
     for(auto bits:{0x7fc12345U,0x7f800000U})reject(state,{bits},storage.layers,resources,continuation,st::ErrorCode::nonfinite_operand);
     auto facts=continuation;facts.constants=nullptr;reject(state,fields,storage.layers,resources,facts,st::ErrorCode::unknown_field);
     auto input=state;input.player.selection_pbw.field6f9.reset();reject(input,fields,storage.layers,resources,continuation,st::ErrorCode::unknown_field);
     input=state;input.player.global.field5d=1;const auto error=reject(input,fields,storage.layers,resources,continuation,st::ErrorCode::transition);
     const auto* cause=std::get_if<ps::Error>(&error.cause);check(cause&&cause->code==ps::ErrorCode::transition_rejected&&cause->transition_code.has_value(),"Transition cause preserved");
    }
    if(c.index==6){
     auto facts=continuation;facts.reference=mh4u::player_reference::Unknown{};
     const auto error=reject(state,fields,storage.layers,resources,facts,st::ErrorCode::reference);
     const auto* cause=std::get_if<mh4u::player_reference::Error>(&error.cause);check(cause&&cause->code==mh4u::player_reference::ErrorCode::unknown_field,"Unknown is not absent reference");
     auto ignored=st::apply(state,{},storage.layers,2,{},resources,globals,sampling,{},continuation);check(std::holds_alternative<st::Output>(ignored),"Unconsumed timer demanded");const auto& value=std::get<st::Output>(ignored);check(!value.fields.field50_bits,"Unconsumed unknown timer fabricated");compare_snapshots(serialize_shared(c,value.state,value.fields,value.layers,context,events,value.shared_fields),c.after,"Ignored timer original output");++boundary_ignored;
    }
    if(c.action==4&&!transitions_original.empty()&&!layer_boundaries){
     reject(state,fields,{},resources,continuation,st::ErrorCode::layer_extent);
     std::vector<mp::Layer> one{storage.layers[0]};const auto error=reject(state,fields,one,resources,continuation,st::ErrorCode::layer_extent);
     check(std::holds_alternative<pcs::Error>(error.cause),"Missing layer1 cause preserved");layer_boundaries=true;
    }
    if(c.index==228){
     auto missing=resources;missing.banks[2]=rq::Unknown{};const auto early=reject(state,fields,storage.layers,missing,continuation,st::ErrorCode::controller);
     const auto* ec=std::get_if<pcs::Error>(&early.cause);check(ec&&ec->phase==pcs::Phase::animation_command,"Unknown co2 keeps command cause");
     auto input=state;input.player.global.field5d=1;const auto late=reject(input,fields,storage.layers,resources,continuation,st::ErrorCode::controller);
     const auto* lc=std::get_if<pcs::Error>(&late.cause);check(lc&&lc->phase==pcs::Phase::transition,"Post-command transition cause preserved");
     auto facts=continuation;facts.constants=nullptr;reject(state,fields,storage.layers,resources,facts,st::ErrorCode::unknown_field);
     input=state;input.player.selection_pbw.field358.reset();reject(input,fields,storage.layers,resources,continuation,st::ErrorCode::controller);
    }
    if(c.substate==3&&noop_boundaries.insert(c.action).second){
     const auto ignored=st::apply(state,{},storage.layers,2,{},rq::Model{},mc::Globals{},mc::Sampling{},mc::Limits{},st::ContinuationContext{});
     check(std::holds_alternative<st::Output>(ignored),"No-op eagerly consumed unknown context");const auto& value=std::get<st::Output>(ignored);check(!value.fields.field50_bits&&!value.effects.transition&&!value.effects.command&&!value.effects.selection_request,"No-op fabricated work");compare_snapshots(serialize_shared(c,value.state,value.fields,value.layers,context,events,value.shared_fields),c.after,"No-op original output with unknown context");++boundary_ignored;
    }
   }
   actions1+=c.action==1;actions4+=c.action==4;actions9+=c.action==9;
  }catch(const std::exception& e){throw std::runtime_error("Case "+std::to_string(c.index)+" "+c.name+": "+e.what());}
 }
 check(r.p==bytes.size()&&actions1==83&&actions4==76&&actions9==76,"Complete subset coverage");
 if(!projection_only)check(transitions==17&&magnitude_queries==82&&setters==20&&accepted==10&&wrapper_count==10&&boundary_errors==13&&boundary_ignored==4&&layer_boundaries&&noop_boundaries.size()==3,"Original observed totals and native boundaries");
 std::cout<<"{\"status\":\""<<(projection_only?"projected":"passed")<<"\",\"cases\":235,\"checks\":"<<checks<<",\"actions\":[83,76,76],\"transitions\":"<<transitions<<",\"magnitude_queries\":"<<magnitude_queries<<",\"setters\":"<<setters<<",\"accepted\":"<<accepted<<",\"wrappers\":"<<wrapper_count<<",\"boundary_errors\":"<<boundary_errors<<",\"boundary_ignored\":"<<boundary_ignored<<"}\n";
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
