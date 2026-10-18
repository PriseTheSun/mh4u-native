#include "player_controller_step.hpp"
#include "motion_resource.hpp"
#include <bit>
#include <cfenv>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#ifdef MH4U_PLAYER_SELECTION_ZERO_GENERATED
#include "player_selection_zero_fixtures.hpp"
#include "player_transition_constants.hpp"
#endif
namespace {
namespace ps=mh4u::player_selection;namespace pcs=mh4u::player_controller_step;
namespace pt=mh4u::player_transition;namespace mp=mh4u::motion_pending;
namespace mr=mh4u::motion_request;namespace mc=mh4u::motion_command;
std::size_t checks{};
void check(bool good,const std::string& why){++checks;if(!good)throw std::runtime_error(why);}
template<class T,class... U>T value(std::variant<U...> result){if(!std::holds_alternative<T>(result))throw std::runtime_error("Unexpected result variant");return std::get<T>(std::move(result));}
bool bits(float a,float b){return std::bit_cast<std::uint32_t>(a)==std::bit_cast<std::uint32_t>(b);}
bool layers_equal(std::span<const mp::Layer> a,std::span<const mp::Layer> b){
 if(a.size()!=b.size())return false;
 for(std::size_t i=0;i<a.size();++i){const auto& x=a[i].animation;const auto& y=b[i].animation;
  if(x.tracks!=y.tracks||x.prediction_rotation!=y.prediction_rotation||x.prediction_translation!=y.prediction_translation||x.previous_rotation!=y.previous_rotation||x.base_rotation!=y.base_rotation||x.previous_translation!=y.previous_translation||x.base_translation!=y.base_translation||a[i].rotation_98!=b[i].rotation_98||a[i].translation_c8!=b[i].translation_c8)return false;
  const auto& l=x.layer;const auto& r=y.layer;
  if(l.selector!=r.selector||l.flags!=r.flags||l.field08!=r.field08||l.attributes!=r.attributes||l.bindings!=r.bindings||l.masks!=r.masks||l.interpolation_curve_bits!=r.interpolation_curve_bits||l.previous_masks!=r.previous_masks)return false;
  const std::array ls{l.requested_frame,l.offset,l.interpolation,l.remaining_interpolation,l.weight,l.frame,l.previous_frame,l.end_frame,l.field58,l.speed,l.field60,l.prepared_frame,l.prepared_next_frame,l.previous_step};
  const std::array rs{r.requested_frame,r.offset,r.interpolation,r.remaining_interpolation,r.weight,r.frame,r.previous_frame,r.end_frame,r.field58,r.speed,r.field60,r.prepared_frame,r.prepared_next_frame,r.previous_step};
  for(unsigned k=0;k<ls.size();++k)if(!bits(ls[k],rs[k]))return false;
 }
 return true;
}
bool state_equal(const pcs::State& a,const pcs::State& b){return a.player==b.player&&a.motion==b.motion;}
void boundaries(){
 ps::State state;const ps::CommonArguments args{0,0,0,2,1,0};
 auto unknown=[&]{auto before=state;check(value<ps::Error>(ps::begin_common(state,args)).code==ps::ErrorCode::unknown_selection_context,"Unknown context treated as zero");check(state==before,"Rejected prefix changed input");};
 unknown();state.pw.field7fc=0;unknown();state.pbw.field6f9=0;unknown();
 state.pbw.field358=0x43160000;
 check(value<ps::PendingAnimation>(ps::begin_common(state,args)).request().selector==0x203,"Inclusive signed threshold");
 for(auto v:{0x80000000U,0xffffffffU,0U,0x4315ffffU}){state.pbw.field358=v;check(value<ps::PendingAnimation>(ps::begin_common(state,args)).request().selector==0x203,"Signed comparison changed");}
 for(auto v:{0x43160001U,0x44160000U,0x7fffffffU}){state.pbw.field358=v;check(value<ps::Error>(ps::begin_common(state,args)).code==ps::ErrorCode::unknown_selection_context,"Unknown area context accepted");}
 state.pw.field594=1;state.pbw.field358.reset();check(value<ps::PendingAnimation>(ps::begin_common(state,args)).request().selector==0x246,"Unconsumed field358 required");
 state.pw.field9a8=1;state.pw.field7fc.reset();check(value<ps::PendingAnimation>(ps::begin_common(state,args)).request().selector==24,"Unconsumed field7fc required");
 state.pbw.field6f9=1;check(value<ps::Error>(ps::begin_common(state,args)).code==ps::ErrorCode::unsupported_selection_context,"Special type path invented");
 state.pbw.field6f9=255;check(value<ps::PendingAnimation>(ps::begin_common(state,args)).request().selector==24,"Equality-to-one became bool check");
 state.pw.field9a8=0;state.pw.field7fc=1;check(value<ps::Error>(ps::begin_common(state,args)).code==ps::ErrorCode::unsupported_selection_context,"Special byte7fc path invented");
 state.pbw.field6f9.reset();state.pw.field7fc.reset();state.pbw.field358.reset();
 check(std::holds_alternative<ps::PendingAnimation>(ps::begin_common(state,{1,0,0,0,0,0})),"Old state1 API now needs context");
 check(std::holds_alternative<ps::Completed>(ps::begin_common(state,{3,0,0,0,0,0})),"State3 consumed context");
 for(auto v:{2U,4U,256U,257U,0xffffffffU})check(value<ps::Error>(ps::begin_common(state,{v,0,0,0,0,0})).code==ps::ErrorCode::unsupported_pl_state,"Full-width selector narrowed");
}
#ifdef MH4U_PLAYER_SELECTION_ZERO_GENERATED
void original(const std::filesystem::path& root){
 namespace res=mh4u::motion_resource;
 std::array sources{value<res::Resource>(res::Resource::load(root/"research/private/player-motion-banks/lmt/92608.lmt","c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8")),value<res::Resource>(res::Resource::load(root/"research/private/player-motion-banks/lmt/92610.lmt","6dc7bd0b51f5c47722883efbaff8a95fc2504ebfc0e436a0e0f811ba51e04274"))};
 const auto constants=value<pt::Constants>(pt::Constants::decode(pt::generated::constants_bytes,pt::generated::expected_constants()));
 mr::Model resource_model;resource_model.skeleton_present=true;resource_model.banks[0]=sources[0].request_resource();resource_model.banks[2]=sources[1].request_resource();
 auto cases=zero_oracle::cases({sources[0].identity(),sources[1].identity()});const auto globals=zero_oracle::globals();
 std::size_t count=0,calls=0,accepted=0;
 for(const auto& c:cases){auto model=resource_model;if(!c.present){model.banks[0]=mr::Absent{};model.banks[2]=mr::Absent{};}
  auto before=c.before;const auto saved=before;
  const auto result=pcs::apply_common(before.state,before.layers,before.active_count,c.arguments,model,globals,constants);
  if(const auto* e=std::get_if<pcs::Error>(&result))throw std::runtime_error("State0 case "+std::to_string(count)+" rejected in phase "+std::to_string(static_cast<int>(e->phase)));
  const auto& out=std::get<pcs::Output>(result);
  check(state_equal(out.state,c.after.state),"Complete player state differs at case "+std::to_string(count));
  check(layers_equal(out.layers,c.after.layers),"Layer/binding bits differ at case "+std::to_string(count));
  check(state_equal(before.state,saved.state)&&layers_equal(before.layers,saved.layers),"Input mutation");
  check(out.selection_request==c.request,"Router full arguments differ");check(out.animation_request_count==c.count,"Setter count differs");
  for(unsigned i=0;i<c.count;++i){const auto& got=out.animation_requests[i];const auto& want=c.commands[i];
   check(got.request.layer==want.request.layer&&got.request.selector==want.request.selector&&got.request.attributes==want.request.attributes,"Setter numeric arguments differ");
   check(bits(got.request.interpolation,want.request.interpolation)&&bits(got.request.start_frame,want.request.start_frame)&&bits(got.request.speed,want.request.speed),"Setter float arguments differ");
   check(got.command_flags==want.command_flags&&got.accepted==want.accepted,"Setter outcome differs");++calls;accepted+=got.accepted;
  }
  check(out.transition.has_value(),"Transition absent");const auto& t=*out.transition;
  check(std::array{t.arguments.state_arg,t.arguments.action_arg,t.arguments.flags_arg}==c.transition,"Full transition args differ");
  check(t.effective_flags==c.effective_flags&&t.returned_dispatch_call==c.dispatch,"Transition effects differ");
  check(out.queries==0&&out.samples==0,"Unexpected sample outside profile");++count;
 }
 // A late transition failure cannot publish successful command mutations.
 auto late=cases.at(60).before;late.state.player.global.field5d=1;const auto saved=late;
 const auto failed=pcs::apply_common(late.state,late.layers,late.active_count,cases.at(60).arguments,resource_model,globals,constants);
 check(value<pcs::Error>(failed).phase==pcs::Phase::transition,"Late transition failure lost");check(state_equal(late.state,saved.state)&&layers_equal(late.layers,saved.layers),"Late failure changed input");
 auto unknown=cases.at(60).before;unknown.state.player.selection_pbw.field358.reset();const auto initial=unknown;
 check(value<pcs::Error>(pcs::apply_common(unknown.state,unknown.layers,unknown.active_count,cases.at(60).arguments,resource_model,globals,constants)).phase==pcs::Phase::selection,"Unknown source fact did not fail before command");check(state_equal(unknown.state,initial.state)&&layers_equal(unknown.layers,initial.layers),"Unknown context changed input");
 // Resume must retain the state0 branch chosen before request, while state1's
 // post-request pw594 interpretation is covered by its historical oracle.
 auto s=value<ps::State>(pcs::selection_view(cases.at(96).before.state,cases.at(96).before.layers));
 auto pending=value<ps::PendingAnimation>(ps::begin_common(s,cases.at(96).arguments));auto post=pending.before_request();post.pw.field594=0;
 const auto completed=value<ps::Completed>(ps::resume(pending,post,constants));check(completed.transition->arguments.state_arg==10&&completed.transition->arguments.action_arg==0,"State0 suffix re-read pre-request choice");
 check(count==zero_oracle::expected_cases&&calls==zero_oracle::expected_calls&&accepted==zero_oracle::expected_accepted,"Coverage changed");
 std::cout<<"{\"original_cases\":"<<count<<",\"setter_calls\":"<<calls<<",\"accepted_setters\":"<<accepted<<",\"state_mismatches\":0}\n";
}
#endif
}
int main(int argc,char** argv){try{boundaries();
#ifdef MH4U_PLAYER_SELECTION_ZERO_GENERATED
 if(argc!=2)throw std::runtime_error("Expected project root");original(argv[1]);
#else
 (void)argc;(void)argv;
#endif
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
