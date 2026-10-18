// Actual immutable MOD/LMT resources through the composed persistent API.
// Diagnostic initialization only; original differential evidence is separate.
#include "player_actor.hpp"
#include "test_support/player_update_actor_checks.hpp"
#include "test_support/player_update_dispatch_actor_fixture.hpp"
#include "test_support/player_update_notifications_actor_fixture.hpp"
#include "test_support/player_update_manager_actor_fixture.hpp"
#include "test_support/player_update_contact_actor_fixture.hpp"
#include "picosha2.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace na=mh4u::native_actor;namespace jp=mh4u::joint_pose;namespace pt=mh4u::player_transition;
namespace tbl=mh4u::player_policy_table;namespace ad=mh4u::player_update_admission;
namespace wl=mh4u::player_world_lookup;namespace direction=mh4u::player_direction_refresh;
namespace bridge=mh4u::player_position_bridge;
using U=std::uint32_t;constexpr U one=0x3f800000;constexpr jp::Bits4 identity{0,0,0,one};
std::size_t checks{};
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
template<class E>std::string describe(const E& e){
 std::string out;
 if constexpr(requires{e.message;})out+=std::string(e.message);
 if constexpr(requires{e.field;})out+=" / "+std::string(e.field);
 if constexpr(requires{e.phase;})out+=" phase="+std::to_string(unsigned(e.phase));
 if constexpr(requires{e.code;})out+=" code="+std::to_string(unsigned(e.code));
 if constexpr(requires{e.cause.has_value();}){if(e.cause)out+=" -> "+describe(*e.cause);}
 else if constexpr(requires{e.cause.index();})out+=std::visit([](const auto& cause){return " -> "+describe(cause);},e.cause);
 else if constexpr(requires{e.cause;})out+=" -> "+describe(e.cause);
 if constexpr(requires{e.driver;})if(e.driver)out+=" -> "+describe(*e.driver);
 return out;
}
template<class T,class... V>T value(std::variant<V...> result){
 if(auto* p=std::get_if<T>(&result))return std::move(*p);
 throw std::runtime_error(std::visit([](const auto& e){return describe(e);},result));
}
std::vector<std::uint8_t> read(const char* path,std::size_t size){
 std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==static_cast<std::streamoff>(size),"Resource extent");
 std::vector<std::uint8_t> bytes(size);f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(size))),"Resource read");return bytes;
}
}
int main(int argc,char** argv){try{
 check(argc==10,"Expected recipe constants/SHA caps/SHA table35/SHA table31/SHA");
 auto actor=value<na::Actor>(na::Actor::load(argv[1]));check(actor.part_count()==7&&actor.joint_count()==19,"Actual hunter extent");
 const auto constants=value<pt::Constants>(pt::Constants::decode(read(argv[2],88),{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));
 const auto bytes=read(argv[4],42);check(picosha2::hash256_hex_string(bytes)==argv[5],"Caps identity");std::array<std::uint8_t,42> caps{};std::copy(bytes.begin(),bytes.end(),caps.begin());
 auto table35=value<tbl::Table>(tbl::Table::decode(read(argv[6],792),argv[7]));auto table31=value<tbl::Table>(tbl::Table::decode(read(argv[8],704),argv[9]));
 auto seed=mh4u::test_dispatch_actor_fixture::adjustment_seed();mh4u::test_notifications_actor_fixture::notification_inputs(seed);
 mh4u::test_manager_actor_fixture::manager_inputs(seed);mh4u::test_contact_actor_fixture::contact_inputs(seed);
 seed.position_fields={0x39,0x91,0,0,one,0,4};seed.controller.player.fieldsd10={0,0,0};
 for(unsigned i=0;i<2;++i){auto& layer=seed.layers[i].animation.layer;layer.interpolation=layer.remaining_interpolation=0;}
 auto& pending=seed.layers[0].animation.layer;pending.selector=8;pending.flags|=2;pending.weight=1;pending.requested_frame=0;
 auto player=value<na::Player>(na::Player::from_rest(actor,seed,constants,mh4u::test_update_actor_fixture::banks()));
 auto before=mh4u::test_update_actor_checks::save(player);
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};
 admission.virtual120=ad::Virtual120::uplayer_b3dd60;mh4u::test_dispatch_actor_fixture::preserve_input(admission);
 admission.reference=mh4u::player_reference::Points{0,0,0,1,0,one};admission.global_c8c=19;admission.global_c90=0;
 const std::string caps_sha=argv[5];
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);mh4u::player_state1::Tables tables{table35,table31};
 mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,jp::Bits4{}};
 na::PlayerUpdateDispatchContext context{admission,parameters.context,tables,templates};
 na::PlayerModelContext model{jp::Bits3{one,one,one},jp::Null{},mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0},identity,bridge::OrientationCallback::b05328,bridge::OutputCallbacks::standard_95b490_95b940};
 auto objects=mh4u::test_dispatch_actor_fixture::dispatch_collection();objects.data[0].field530=1;objects.data[0].position_bits[1]=0;
 auto queue=mh4u::test_notifications_actor_fixture::queue_seed();auto manager=mh4u::test_manager_actor_fixture::manager_seed();
 constexpr direction::Bindings bindings{direction::Virtual84::uplayer_b085bc,direction::Virtual144::uplayer_b47aa0};
 auto out=value<na::PlayerUpdateModelOutput>(player.prepare_and_update_model(objects,queue,context,bindings,
  mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{manager,&player},model));
 check(out.effects.animation.has_value(),"Model frame not reached");const auto& effects=*out.effects.animation;
 check(effects.pending_found&&effects.initializer_calls>0&&effects.joint_samples>0,"Actual LMT action/track resolvers not consumed");
 check(player.state().frame.animation.layers[0].animation.layer.selector==8,"Source selector lost");
 check(player.state().frame.animation.joints!=before.state.frame.animation.joints,"Actual animation never changed pose");
 check(player.completed_steps()==1&&out.contacted.managed.positioned.frame.tick==1,"Actual resource frame repeated");
 check(out.contacted.managed.positioned.frame.samples>=effects.model_samples+effects.joint_samples,"Frame sample instrumentation lost resource work");
 for(unsigned i=0;i<6;++i)check(mh4u::test_player_actor_state::layer_equal(player.state().inactive[i],before.state.inactive[i]),"Actual resource frame touched inactive slot");
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"initializers\":"<<effects.initializer_calls<<",\"model_samples\":"<<effects.model_samples<<",\"joint_samples\":"<<effects.joint_samples<<",\"output_joints\":"<<effects.output_joints<<"}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
