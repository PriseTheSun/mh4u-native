// Persistent direction checkpoint with real geometry and explicit diagnostic
// contexts. Original numerical/caller comparison is a separate ARM oracle.
#include "player_actor.hpp"
#include "test_support/player_update_actor_checks.hpp"
#include "test_support/player_update_notifications_actor_fixture.hpp"
#include "test_support/player_update_dispatch_actor_fixture.hpp"
#include "test_support/player_actor_allocation_probe.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace na=mh4u::native_actor;namespace pa=mh4u::player_animation;
namespace pt=mh4u::player_transition;namespace ad=mh4u::player_update_admission;
namespace oq=mh4u::player_object_query;namespace wl=mh4u::player_world_lookup;
namespace jp=mh4u::joint_pose;namespace post=mh4u::player_update_post_dispatch;
namespace sq=mh4u::spatial_query;namespace tbl=mh4u::player_policy_table;
using U=std::uint32_t;constexpr U one=0x3f800000;
constexpr jp::Bits4 identity{0,0,0,one};
constexpr auto binding=post::Virtual10c::uplayer_b25b68;
std::size_t checks{},allocation_failures{},first_render_failure{};
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
U bits(float v){return std::bit_cast<U>(v);}
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
template<class T,class...V>T value(std::variant<V...> result){if(auto* p=std::get_if<T>(&result))return std::move(*p);throw std::runtime_error(std::visit([](const auto& e){return describe(e);},result));}
using mh4u::test_update_actor_checks::save;
const mh4u::test_update_actor_checks::Checks assertions{check};
namespace composed=mh4u::player_update_direction;
namespace direction=mh4u::player_direction_refresh;
namespace nq=mh4u::notification_queue;
using mh4u::test_update_actor_fixture::banks;
using namespace mh4u::test_dispatch_actor_fixture;
using mh4u::test_notifications_actor_fixture::queue_seed;
using mh4u::test_notifications_actor_fixture::notification_inputs;
constexpr direction::Bindings concrete{direction::Virtual84::uplayer_b085bc,direction::Virtual144::uplayer_b47aa0};
const composed::Error& composed_error(const na::PlayerError& e){
 check(e.driver&&e.driver->phase==pa::Phase::update_direction,"Lost direction driver cause");
 return std::get<composed::Error>(e.driver->cause);
}
void run(const char* recipe,const pt::Constants& constants,const std::array<std::uint8_t,42>& caps,const std::string& caps_sha,
 const tbl::Table& table35,const tbl::Table& table31,const mh4u::collision_resource::Resource& collision){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected source hunter geometry");
 const auto make=[&](const na::PlayerInitial& initial){return value<na::Player>(na::Player::from_rest(actor,initial,constants,banks()));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};admission.virtual120=ad::Virtual120::uplayer_b3dd60;
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);
 mh4u::player_state1::Tables tables{table35,table31};mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,identity};
 na::PlayerUpdateDispatchContext context{admission,parameters.context,tables,templates};
 auto initial=adjustment_seed();notification_inputs(initial);initial.controller.player.pbw.field24=0;initial.controller.player.pw.fieldb0=0xdead1770;
 auto objects=dispatch_collection();objects.data[0].field530=1;
 auto player=make(initial);const auto saved=save(player);const auto frame=value<na::Frame>(player.capture());auto queue=queue_seed();
 // Same Player and world queue survive repeated calls. The third call snaps
 // to the full source word; no implicit16-bit normalization or clock advance.
 const direction::Bindings lazy{direction::Virtual84::uplayer_b085bc,{}};
 const std::array<U,3> expected{2002,4004,0xdead1770};
 for(std::uint8_t i=0;i<3;++i){
  const auto out=value<na::PlayerUpdateDirectionOutput>(player.prepare_and_update_direction(objects,queue,context,lazy,binding));
  check(out.effects.rate==4004&&out.effects.step==2002&&out.effects.snapped==(i==2),"Persistent direction result changed");
  check(player.state().controller.player.pbw.field24==expected[i]&&player.state().controller.player.pw.fieldb0==0xdead1770,"Direction canonical owner lost full word");
  check(out.material.path==composed::MaterialPath::not_called&&!player.shared_fields().pw.field9e8,"Non-type13 consumed material selector");
  check(out.notifications.count==2&&queue.counter==U(2*(i+1))&&out.status.timer_incremented,"Earlier notification/status candidate lost");
  check(player.shared_fields().pw.field2fc_bits==bits(float(i+1)),"Direction reused stale status timer");
  assertions.pose_equal(player,saved);assertions.frame_equal(out.frame,frame);
 }
 // Earlier checkpoint does not consume concrete direction identities or write
 // the heading merely because those fields are now present in the model.
 auto earlier=make(initial);auto earlier_objects=dispatch_collection();earlier_objects.data[0].field530=1;auto earlier_queue=queue_seed();
 value<na::PlayerUpdateNotificationsOutput>(earlier.prepare_and_update_notifications(earlier_objects,earlier_queue,context,binding));
 check(earlier.state().controller.player.pbw.field24==0&&earlier_queue.counter==2,"Earlier checkpoint executed direction phase");
 // Actual spatial state and queue are already changed in the private candidate
 // when a late concrete virtual identity is missing. Repair the binding alone.
 auto spatial_admission=admission;preserve_input(spatial_admission);spatial_admission.prefix.delta_bits=one;
 auto spatial_initial=spatial_seed(2971.5546875f);post_inputs(spatial_initial);notification_inputs(spatial_initial);
 spatial_initial.controller.player.pw.fieldb0=0x12345678;spatial_initial.controller.player.pbw.field24=0;
 for(bool missing144:{false,true}){
  auto seed=spatial_initial;seed.controller.player.selection_pbw.field6f9=std::uint8_t(missing144);
  auto scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&scene,0};
  auto subject=make(seed);auto collection=spatial_collection();collection.data[0].field530=1;auto pending=queue_seed();
  const auto prior=save(subject);const auto old_collection=collection;const auto old_scene=scene;const auto old_queue=pending;const auto old_frame=value<na::Frame>(subject.capture());
  auto missing=concrete;if(missing144)missing.virtual144.reset();else missing.virtual84.reset();
  const auto error=value<na::PlayerError>(subject.prepare_and_update_direction(collection,pending,spatial_context,missing,binding));
  const auto& cause=composed_error(error);check(cause.phase==composed::Phase::direction,"Late direction failure lost phase");
  const auto& leaf=std::get<direction::Error>(cause.cause);check(leaf.code==direction::ErrorCode::unknown_binding&&leaf.phase==(missing144?direction::Phase::virtual144:direction::Phase::virtual84),"Concrete virtual failure lost identity");
  assertions.unchanged(subject,prior);check(collection==old_collection&&pending==old_queue,"Late direction failure published objects/queue");assertions.grids_equal(scene,old_scene);assertions.frame_unchanged(value<na::Frame>(subject.capture()),old_frame);
  const auto out=value<na::PlayerUpdateDirectionOutput>(subject.prepare_and_update_direction(collection,pending,spatial_context,concrete,binding));
  check(scene.grid_states[0].counter==1&&collection.data[0].field349==1&&pending.counter==2,"Direction retry split four-owner publication");
  check(subject.state().controller.player.pbw.field24==0&&subject.state().controller.player.pw.fieldb0==0&&out.effects.rate==1456&&out.effects.step==728&&out.effects.snapped&&out.notifications.count==2,"Direction did not consume the earlier dispatch candidate");
  assertions.pose_equal(subject,prior);assertions.frame_equal(out.frame,old_frame);
 }
 // Type13 early returns are exact source predicates. Resource-changing paths
 // reject the whole candidate, including notifications already calculated.
 for(unsigned scenario=0;scenario<5;++scenario){
  auto seed=initial;seed.controller.player.pbw.field31=13;
  // b22164 produces the current ordinal from these timers before b29f7c.
  // Supplying only an initial PW1f4 would test a stale input, not this caller.
  seed.shared_fields.pw.field940_bits=seed.shared_fields.pw.field948_bits=seed.shared_fields.pw.field950_bits=0;
  if(scenario>0&&scenario<4)seed.shared_fields.pw.field948_bits=seed.shared_fields.pw.field950_bits=one;
  if(scenario==1)seed.shared_fields.pw.field9e8=0;
  if(scenario==3)seed.shared_fields.pw.field9e8=2;
  if(scenario==4)seed.shared_fields.pw.field1fc=0x01800000;
  auto subject=make(seed);auto collection=dispatch_collection();collection.data[0].field530=1;auto pending=queue_seed();
  const auto prior=save(subject);const auto old_collection=collection;const auto old_queue=pending;const auto old_frame=value<na::Frame>(subject.capture());
  const auto result=subject.prepare_and_update_direction(collection,pending,context,concrete,binding);
  if(scenario<2){const auto out=value<na::PlayerUpdateDirectionOutput>(result);check(out.material.path==(scenario==0?composed::MaterialPath::unchanged_ordinal:composed::MaterialPath::already_selected),"Type13 early return was inferred incorrectly");check(pending.counter==2&&subject.state().controller.player.pbw.field24==2002,"Type13 early return stopped real direction");}
  else{const auto error=value<na::PlayerError>(result);const auto& cause=composed_error(error);check(cause.phase==composed::Phase::material,"Resource-changing path escaped material boundary");const auto& material=std::get<composed::MaterialError>(cause.cause);check(material.code==(scenario==2?composed::MaterialErrorCode::unknown_field:composed::MaterialErrorCode::unsupported_resource_change),"Material failure category lost");assertions.unchanged(subject,prior);check(collection==old_collection&&pending==old_queue,"Material rejection published earlier owners");assertions.frame_unchanged(value<na::Frame>(subject.capture()),old_frame);}
 }
 // Allocation failures include the real renderer after all native phases.
 auto measured_scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&measured_scene,0};
 auto measured=make(spatial_initial);auto measured_objects=spatial_collection();measured_objects.data[0].field530=1;auto measured_queue=queue_seed();
 std::optional<na::PlayerResult<na::PlayerUpdateDirectionOutput>> measured_result;std::size_t extent{};
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_update_direction(measured_objects,measured_queue,spatial_context,concrete,binding));extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerUpdateDirectionOutput>(*measured_result)&&extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=extent;++ordinal){
  auto scene=spatial_scene(collision,check);auto call_context=spatial_context;call_context.spatial_scene=&scene;
  auto subject=make(spatial_initial);auto collection=spatial_collection();collection.data[0].field530=1;auto pending=queue_seed();
  const auto prior=save(subject);const auto old_collection=collection;const auto old_scene=scene;const auto old_queue=pending;const auto old_frame=value<na::Frame>(subject.capture());
  std::optional<na::PlayerResult<na::PlayerUpdateDirectionOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(subject.prepare_and_update_direction(collection,pending,call_context,concrete,binding));}
  const auto error=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(error.code==na::PlayerErrorCode::capacity||error.code==na::PlayerErrorCode::driver||error.code==na::PlayerErrorCode::render,"Allocation failure category changed");
  assertions.unchanged(subject,prior);check(collection==old_collection&&pending==old_queue,"Allocation failure published queue/objects");assertions.grids_equal(scene,old_scene);assertions.frame_unchanged(value<na::Frame>(subject.capture()),old_frame);
  const auto out=value<na::PlayerUpdateDirectionOutput>(subject.prepare_and_update_direction(collection,pending,call_context,concrete,binding));
  check(out.status.timer_incremented&&scene.grid_states[0].counter==1&&collection.data[0].field349==1&&pending.counter==2&&subject.state().controller.player.pbw.field24==0&&subject.state().controller.player.pw.fieldb0==0&&out.effects.snapped,"Allocator retry split commits");
  assertions.pose_equal(subject,prior);assertions.frame_equal(out.frame,old_frame);
  if(error.code==na::PlayerErrorCode::render){check(error.render&&error.render->code==na::ErrorCode::capacity,"Lost renderer allocation cause");first_render_failure=ordinal;break;}
 }
 check(first_render_failure&&allocation_failures==first_render_failure,"Did not reach real renderer failure");
 check(picosha2::hash256_hex_string(parameters.record)==parameters.record_sha&&picosha2::hash256_hex_string(caps)==caps_sha,"Borrowed parameter source mutated");
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=value<na::Actor>(na::Actor::load(argv[2]));for(const auto& p:actor.source_files())std::cout<<p.generic_string()<<'\n';return 0;}
 check(argc==12,"Expected recipe constants/SHA caps/SHA table35/SHA table31/SHA SBC/SHA");
 const auto read=[](const char* path,std::size_t size){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==static_cast<std::streamoff>(size),"Input extent");std::vector<std::uint8_t> out(size);f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(size)))&&f.peek()==std::char_traits<char>::eof(),"Input read");return out;};
 const auto constants=value<pt::Constants>(pt::Constants::decode(read(argv[2],88),{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));
 const auto raw_caps=read(argv[4],42);check(picosha2::hash256_hex_string(raw_caps)==argv[5],"Cap source hash");std::array<std::uint8_t,42> caps{};std::copy(raw_caps.begin(),raw_caps.end(),caps.begin());
 const auto table35=value<tbl::Table>(tbl::Table::decode(read(argv[6],792),argv[7]));const auto table31=value<tbl::Table>(tbl::Table::decode(read(argv[8],704),argv[9]));
 const auto collision=value<mh4u::collision_resource::Resource>(mh4u::collision_resource::Resource::load(argv[10],argv[11]));
 run(argv[1],constants,caps,argv[5],table35,table31,collision);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"allocation_failures\":"<<allocation_failures<<",\"first_render_failure\":"<<first_render_failure<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
