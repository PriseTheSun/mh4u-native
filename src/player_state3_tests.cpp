// Source-bound dispatcher state3 is b66ae8/BX LR, after the common clear376.
// Native boundary checks; original execution is in the independent caller oracle.
#include "player_state1.hpp"
#include "test_support/player_actor_state.hpp"
#include <bit>
#include <iostream>
#include <stdexcept>
namespace {
namespace st=mh4u::player_state1;namespace pcs=mh4u::player_controller_step;
namespace mr=mh4u::motion_request;namespace mc=mh4u::motion_command;namespace mp=mh4u::motion_pending;
std::size_t checks{};void check(bool p,const char* message){++checks;if(!p)throw std::runtime_error(message);}
template<class T,class...A>T value(std::variant<A...> result){if(auto* p=std::get_if<T>(&result))return std::move(*p);throw std::runtime_error("Unexpected native result");}
void run(){
 pcs::State state{};state.player.pbw.field371=3;state.player.selection_pbw.field376=0xa5;
 state.player.pbw.fields5[0]=0xff;state.player.pw.fieldf0=0x7fa12345;
 st::Fields fields{0x7fc12345};mh4u::player_fields::Fields shared{};
 shared.pbw.words9404[17]=0xff812345;shared.pbw.descriptors[0][13]=0xa5;
 st::ContinuationContext context;context.shared_fields=&shared;
 const st::Tables tables{};const mr::Model model{};const mc::Globals globals{};
 // Every action word is irrelevant in state3; no helper/model/table is bound.
 for(std::uint32_t action=0;action<65536;++action){
  state.player.pbw.field372=static_cast<std::uint16_t>(action);const auto before=state;
  auto output=value<st::Output>(st::apply(state,fields,{},0xffffffff,tables,model,globals,{}, {},context));
  auto expected=state;expected.player.selection_pbw.field376=0;
  check(output.state.player==expected.player&&output.state.motion==expected.motion&&state.player==before.player,"State3 clear or input preservation");
  check(output.fields==fields&&output.shared_fields==shared&&output.layers.empty(),"State3 consumed irrelevant fields/layers");
  check(!output.effects.command&&!output.effects.installation&&!output.effects.transition&&!output.effects.selection_request&&!output.effects.spatial&&output.effects.request_count==0&&output.effects.queries==0&&output.effects.samples==0&&output.effects.magnitude_queries==0&&output.effects.cost_calls==0&&output.effects.position_updates==0&&output.effects.manager_queries==0,"State3 manufactured a handler effect");
 }
 std::array<mp::Layer,8> layers{};
 for(std::uint32_t i=0;i<layers.size();++i){auto& l=layers[i];l.animation.layer.frame=std::bit_cast<float>(0x7fa10000U+i);l.animation.layer.weight=std::bit_cast<float>(0xff800000U);l.animation.layer.selector=static_cast<std::uint16_t>(0xf000U+i);l.animation.layer.masks={i,0xa5,0x80000000,0xffffffff};l.rotation_98={i,1,2,3};l.animation.tracks.rotation.track=mh4u::motion_binding::TrackReference{0xffff,15,255,i};}
 const auto output=value<st::Output>(st::apply(state,fields,layers,2,tables,model,globals,{}, {},context));
 check(output.layers.size()==8,"Physical storage changed");
 for(std::size_t i=0;i<8;++i)check(mh4u::test_player_actor_state::layer_equal(layers[i],output.layers[i]),"State3 changed unconsumed raw layer");
 for(auto other:{0,2,4,8,255}){auto invalid=state;invalid.player.pbw.field371=static_cast<std::uint8_t>(other);check(value<st::Error>(st::apply(invalid,fields,{},0,tables,model,globals)).code==st::ErrorCode::unsupported_state,"Unknown dispatcher target became no-op");}
 auto invalid=state;invalid.player.pbw.field371=1;invalid.player.pbw.field372=3;
 check(value<st::Error>(st::apply(invalid,fields,{},0,tables,model,globals)).code==st::ErrorCode::unsupported_action,"State1 unknown action became state3");
 mc::Limits limits;limits.storage_layers=7;
 check(value<st::Error>(st::apply(state,fields,layers,2,tables,model,globals,{},limits,context)).code==st::ErrorCode::capacity,"State3 copy capacity bypassed");
 limits={};limits.joint_bindings=1;layers[7].animation.tracks.joints.resize(2);
 check(value<st::Error>(st::apply(state,fields,layers,2,tables,model,globals,{},limits,context)).code==st::ErrorCode::capacity,"State3 binding budget bypassed");
 check(state.player.selection_pbw.field376==0xa5&&shared.pbw.words9404[17]==0xff812345&&!shared.pbw.descriptors[0][12],"Failed calls published or collapsed input");
}
}
int main(int argc,char**){try{if(argc!=1)throw std::runtime_error("Expected no arguments");run();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
