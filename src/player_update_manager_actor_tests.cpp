// Transactional manager checkpoint with real hunter geometry. Explicit
// diagnostic contexts; original ARM comparison is a separate test.
#include "player_actor.hpp"
#include "test_support/player_update_manager_actor_fixture.hpp"
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
std::size_t checks{},allocation_failures{},first_render_failure{},render_failures{},palette_failures{},allocation_extent{};
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
namespace composed=mh4u::player_update_manager;
namespace pm=mh4u::player_manager_update;
namespace pr=mh4u::player_reference;
namespace direction=mh4u::player_direction_refresh;
using mh4u::test_update_actor_fixture::banks;
using namespace mh4u::test_dispatch_actor_fixture;
using mh4u::test_notifications_actor_fixture::queue_seed;
using mh4u::test_notifications_actor_fixture::notification_inputs;
constexpr direction::Bindings concrete{direction::Virtual84::uplayer_b085bc,direction::Virtual144::uplayer_b47aa0};
using mh4u::test_manager_actor_fixture::manager_seed;
using mh4u::test_manager_actor_fixture::manager_inputs;
const pm::Error& manager_error(const na::PlayerError& e){
 check(e.driver&&e.driver->phase==pa::Phase::update_manager,"Lost manager driver cause");
 const auto& composed_error=std::get<composed::Error>(e.driver->cause);
 check(composed_error.phase==composed::Phase::manager,"Lost manager checkpoint phase");
 return std::get<pm::Error>(composed_error.cause);
}
void run(const char* recipe,const pt::Constants& constants,const std::array<std::uint8_t,42>& caps,const std::string& caps_sha,
 const tbl::Table& table35,const tbl::Table& table31,const mh4u::collision_resource::Resource& collision){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected source hunter geometry");
 const auto make=[&](const na::PlayerInitial& initial){return value<na::Player>(na::Player::from_rest(actor,initial,constants,banks()));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};admission.virtual120=ad::Virtual120::uplayer_b3dd60;
 preserve_input(admission);admission.reference=pr::Points{0,0,0,1,bits(0),bits(1)};
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);
 mh4u::player_state1::Tables tables{table35,table31};mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,identity};
 na::PlayerUpdateDispatchContext context{admission,parameters.context,tables,templates};
 auto initial=adjustment_seed();notification_inputs(initial);manager_inputs(initial);
 const auto objects_seed=[](){auto x=dispatch_collection();x.data[0].field530=1;x.data[0].position_bits[1]=0;return x;};
 // A same-owner binding uses the preceding candidate and never a second render.
 auto subject=make(initial);auto objects=objects_seed();auto queue=queue_seed();auto manager=manager_seed();
 auto saved=save(subject);const auto frame=value<na::Frame>(subject.capture());
 const auto out=value<na::PlayerUpdateManagerOutput>(subject.prepare_and_update_manager(objects,queue,context,concrete,binding,{manager,&subject}));
 check(out.effects.caller_vertical_written&&!out.effects.foreign_vertical_written&&!out.foreign_frame,"Same owner was split into two Players");
 check(subject.state().controller.player.pw.field50d==1&&subject.shared_fields().pw.fielde4==50&&subject.shared_fields().pw.fielde6==0xabcd,"Manager caller writes not published");
 check(subject.state().controller.player.pw.fielddc==subject.shared_fields().pw.fielde0_bits&&subject.shared_fields().pw.fielde0_bits!=initial.shared_fields.pw.fielde0_bits,"Caller vertical result not published");
 check(manager.fields1050[4]==1&&manager.fields1050[0]==0&&manager.field102c==bits(50)&&manager.fields1050[1]==0,"Manager start not committed");
 check(queue.counter==2&&out.positioned.notifications.count==2,"Earlier queue phase was lost or repeated");
 assertions.pose_equal(subject,saved);assertions.frame_equal(out.positioned.frame,frame);
 const auto after_start=manager;
 const auto second=value<na::PlayerUpdateManagerOutput>(subject.prepare_and_update_manager(objects,queue,context,concrete,binding,{manager,pm::Unknown{}}));
 check(subject.state().controller.player.pw.field50d==0&&subject.shared_fields().pw.field50c==0&&!second.effects.caller_vertical_written&&manager==after_start,"Repeated call failed to toggle off lazily");
 const auto third=value<na::PlayerUpdateManagerOutput>(subject.prepare_and_update_manager(objects,queue,context,concrete,binding,{manager,pm::Unknown{}}));
 check(subject.state().controller.player.pw.field50d==1&&!third.effects.caller_vertical_written&&manager==after_start&&queue.counter==6,"Busy start consumed unknown target or duplicated earlier phases");
 // A foreign Player changes only the two words written by b36e04. Its different
 // Global snapshot must not override the caller's shared Global used by source.
 auto foreign_initial=initial;foreign_initial.controller.player.global.field2b=0;foreign_initial.controller.player.global.field2f=9;
 auto caller=make(initial);auto peer=make(foreign_initial);auto foreign_objects=objects_seed();auto foreign_queue=queue_seed();auto foreign_manager=manager_seed();
 const auto caller_old=save(caller);auto peer_expected=save(peer);const auto peer_frame=value<na::Frame>(peer.capture());
 const auto foreign_out=value<na::PlayerUpdateManagerOutput>(caller.prepare_and_update_manager(foreign_objects,foreign_queue,context,concrete,binding,{foreign_manager,&peer}));
 check(foreign_out.effects.foreign_vertical_written&&!foreign_out.effects.caller_vertical_written&&foreign_out.foreign_frame.has_value(),"Foreign target lost canonical caller Global or patch");
 peer_expected.state.controller.player.pw.fielddc=peer.state().controller.player.pw.fielddc;peer_expected.fields.pw.fielde0_bits=peer.shared_fields().pw.fielde0_bits;
 assertions.unchanged(peer,peer_expected);assertions.frame_equal(*foreign_out.foreign_frame,peer_frame);
 check(peer.shared_fields().pw.fielde0_bits==subject.shared_fields().pw.fielde0_bits&&caller.state().controller.player.pw.fielddc==caller_old.state.controller.player.pw.fielddc&&caller.shared_fields().pw.fielde0_bits==caller_old.fields.pw.fielde0_bits,"Foreign update wrote the caller's vertical words");
 // Reached identities and missing Y fail after previous writes remain private.
 for(unsigned scenario=0;scenario<5;++scenario){
  auto tested=make(initial);auto target=make(foreign_initial);auto collection=objects_seed();auto pending=queue_seed();auto m=manager_seed();
  auto reference=admission;na::PlayerManagerLink link=&target;
  if(scenario==0)std::get<pr::Points>(reference.reference).second_y_bits.reset();
  if(scenario==1)link=pm::Unknown{};
  if(scenario==2)link=pm::Absent{};
  if(scenario==3)link=static_cast<na::Player*>(nullptr);
  if(scenario==4)m.field1645.reset();
  na::PlayerUpdateDispatchContext call_context{reference,parameters.context,tables,templates};
  const auto old=save(tested),old_target=save(target);const auto old_collection=collection;const auto old_queue=pending;const auto old_manager=m;
  const auto old_frame=value<na::Frame>(tested.capture()),old_target_frame=value<na::Frame>(target.capture());
  const auto error=value<na::PlayerError>(tested.prepare_and_update_manager(collection,pending,call_context,concrete,binding,{m,link}));
  const auto& cause=manager_error(error);
  const std::array<pm::ErrorCode,5> codes{pm::ErrorCode::unknown_field,pm::ErrorCode::unknown_field,pm::ErrorCode::absent_player,pm::ErrorCode::invalid_identity,pm::ErrorCode::unknown_field};
  check(cause.code==codes[scenario],"Manager failure category changed");
  assertions.unchanged(tested,old);assertions.unchanged(target,old_target);
  check(collection==old_collection&&pending==old_queue&&m==old_manager,"Manager failure published another owner");
  assertions.frame_unchanged(value<na::Frame>(tested.capture()),old_frame);assertions.frame_unchanged(value<na::Frame>(target.capture()),old_target_frame);
  m.field1645=0;
  const auto retry=value<na::PlayerUpdateManagerOutput>(tested.prepare_and_update_manager(collection,pending,context,concrete,binding,{m,&target}));
  check(retry.effects.foreign_vertical_written&&pending.counter==2,"Same owners did not recover from manager rejection");
 }
 // A manager gate failure follows an actual SBC/grid/collection transaction.
 auto spatial_initial=spatial_seed(2971.5546875f);post_inputs(spatial_initial);notification_inputs(spatial_initial);
 spatial_initial.shared_fields.pw.fieldb8_bits=0;spatial_initial.shared_fields.pw.fieldb4=0;
 auto scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{admission,parameters.context,tables,templates,{}, {},&scene,0};
 auto spatial_player=make(spatial_initial);auto spatial_objects=spatial_collection();spatial_objects.data[0].field530=1;auto spatial_queue=queue_seed();pm::ManagerState spatial_manager;
 const auto spatial_old=save(spatial_player);const auto scene_old=scene;const auto spatial_objects_old=spatial_objects;const auto spatial_queue_old=spatial_queue;
 const auto spatial_error=value<na::PlayerError>(spatial_player.prepare_and_update_manager(spatial_objects,spatial_queue,spatial_context,concrete,binding,{spatial_manager,pm::Unknown{}}));
 check(manager_error(spatial_error).field=="manager112b","Spatial checkpoint did not reach manager gate");
 assertions.unchanged(spatial_player,spatial_old);assertions.grids_equal(scene,scene_old);check(spatial_objects==spatial_objects_old&&spatial_queue==spatial_queue_old&&!spatial_manager.field112b,"Late manager failure published earlier spatial writes");
 spatial_manager.field112b=1;
 const auto spatial_out=value<na::PlayerUpdateManagerOutput>(spatial_player.prepare_and_update_manager(spatial_objects,spatial_queue,spatial_context,concrete,binding,{spatial_manager,pm::Unknown{}}));
 check(scene.grid_states[0].counter==1&&spatial_objects.data[0].field349==1&&spatial_queue.counter==2&&!spatial_out.effects.foreign_vertical_written,"Retry split spatial/manager/queue publication");
 // Exhaust the complete successful foreign-call allocation sequence, including
 // both actual geometry preparations. Every failure retries the same owners.
 auto measured=make(initial);auto measured_peer=make(foreign_initial);auto measured_objects=objects_seed();auto measured_queue=queue_seed();auto measured_manager=manager_seed();
 std::optional<na::PlayerResult<na::PlayerUpdateManagerOutput>> measured_result;
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_update_manager(measured_objects,measured_queue,context,concrete,binding,{measured_manager,&measured_peer}));allocation_extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerUpdateManagerOutput>(*measured_result)&&allocation_extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=allocation_extent;++ordinal){
  auto tested=make(initial);auto target=make(foreign_initial);auto collection=objects_seed();auto pending=queue_seed();auto m=manager_seed();
  const auto old=save(tested),old_target=save(target);const auto old_collection=collection;const auto old_queue=pending;const auto old_manager=m;
  const auto old_frame=value<na::Frame>(tested.capture()),old_target_frame=value<na::Frame>(target.capture());
  std::optional<na::PlayerResult<na::PlayerUpdateManagerOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(tested.prepare_and_update_manager(collection,pending,context,concrete,binding,{m,&target}));}
  const auto error=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(error.code==na::PlayerErrorCode::capacity||error.code==na::PlayerErrorCode::driver||error.code==na::PlayerErrorCode::render,"Allocation failure category changed");
  if(error.code==na::PlayerErrorCode::render){check(error.render.has_value(),"Missing nested renderer cause");
   if(error.render->code==na::ErrorCode::palette){check(error.render->message=="Palette: Palette allocation failed","Lost palette allocation cause");++palette_failures;}
   else check(error.render->code==na::ErrorCode::capacity,"Lost direct renderer allocation cause");
   ++render_failures;if(!first_render_failure)first_render_failure=ordinal;}
  assertions.unchanged(tested,old);assertions.unchanged(target,old_target);check(collection==old_collection&&pending==old_queue&&m==old_manager,"Allocation failure published an owner");
  assertions.frame_unchanged(value<na::Frame>(tested.capture()),old_frame);assertions.frame_unchanged(value<na::Frame>(target.capture()),old_target_frame);
  const auto retry=value<na::PlayerUpdateManagerOutput>(tested.prepare_and_update_manager(collection,pending,context,concrete,binding,{m,&target}));
  check(retry.effects.foreign_vertical_written&&retry.foreign_frame&&pending.counter==2&&m.fields1050[4]==1,"Allocator retry split commits");
  assertions.pose_equal(tested,old);assertions.pose_equal(target,old_target);assertions.frame_equal(retry.positioned.frame,old_frame);assertions.frame_equal(*retry.foreign_frame,old_target_frame);
 }
 check(allocation_failures==allocation_extent&&render_failures>=2&&first_render_failure<allocation_extent,"Did not cover complete allocation sequence including both renders");
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
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"allocation_failures\":"<<allocation_failures<<",\"first_render_failure\":"<<first_render_failure<<",\"render_failures\":"<<render_failures<<",\"palette_failures\":"<<palette_failures<<",\"allocation_extent\":"<<allocation_extent<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
