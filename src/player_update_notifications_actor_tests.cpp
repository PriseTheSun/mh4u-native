// Persistent notification checkpoint with real geometry and explicit diagnostic
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
namespace status=mh4u::player_status_update;
namespace composed=mh4u::player_update_notifications;
namespace nq=mh4u::notification_queue;namespace notify=mh4u::player_status_notifications;
using mh4u::test_update_actor_fixture::banks;
using namespace mh4u::test_dispatch_actor_fixture;
using mh4u::test_status_actor_fixture::status_inputs;
const composed::Error& composed_error(const na::PlayerError& e){
 check(e.driver&&e.driver->phase==pa::Phase::update_notifications,"Lost notification driver cause");
 return std::get<composed::Error>(e.driver->cause);
}
using mh4u::test_notifications_actor_fixture::queue_seed;
using mh4u::test_notifications_actor_fixture::notification_inputs;
void inserted(const notify::Effects& effects,std::uint8_t first){
 check(effects.count==2&&effects.attempts[0].id==50&&effects.attempts[1].id==55,"Source notification order changed");
 for(std::uint8_t i=0;i<2;++i)check(effects.attempts[i].permitted&&effects.attempts[i].inserted==nq::PoolIndex{std::uint8_t(first+i)},"Notification insertion/permission lost");
}
void run(const char* recipe,const pt::Constants& constants,const std::array<std::uint8_t,42>& caps,const std::string& caps_sha,
 const tbl::Table& table35,const tbl::Table& table31,const mh4u::collision_resource::Resource& collision){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected seven-piece nineteen-joint actor");
 const auto make=[&](const na::PlayerInitial& initial){return value<na::Player>(na::Player::from_rest(actor,initial,constants,banks()));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};admission.virtual120=ad::Virtual120::uplayer_b3dd60;
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);
 mh4u::player_state1::Tables tables{table35,table31};mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,identity};
 na::PlayerUpdateDispatchContext context{admission,parameters.context,tables,templates};
 auto initial=adjustment_seed();notification_inputs(initial);
 auto objects=dispatch_collection();objects.data[0].field530=1;
 auto player=make(initial);const auto prior=save(player);const auto frame=value<na::Frame>(player.capture());auto queue=queue_seed();
 for(std::uint8_t i=0;i<3;++i){
  const auto out=value<na::PlayerUpdateNotificationsOutput>(player.prepare_and_update_notifications(objects,queue,context,binding));
  inserted(out.effects,std::uint8_t(i*2));
  check(out.dispatch.dispatch_count==1&&out.adjustment.virtual_returned&&out.status.timer_incremented,"Earlier composed effects lost");
  check(queue.counter==2U*(i+1)&&player.shared_fields().pw.field2fc_bits==bits(float(i+1)),"Queue/Player updates split");
  check(player.shared_fields().pw.field200==0&&player.shared_fields().pw.field1fc==0&&player.shared_fields().pw.field586==0x4d5,"Previous masks or selector implicitly published");
  check(!queue.pool[i*2][2]&&queue.pool[i*2][4]==0x95,"Unknown/opaque payload overwritten");
  assertions.pose_equal(player,prior);assertions.frame_equal(out.frame,frame);
 }
 // A second Player uses the same external world queue, without resetting it
 // or changing the first Player. A separate queue remains independent.
 auto other=make(initial);auto other_objects=dispatch_collection();other_objects.data[0].field530=1;
 const auto first_saved=save(player);auto separate_queue=queue_seed();const auto separate_before=separate_queue;
 const auto other_out=value<na::PlayerUpdateNotificationsOutput>(other.prepare_and_update_notifications(other_objects,queue,context,binding));
 inserted(other_out.effects,6);check(queue.counter==8&&separate_queue==separate_before,"World queue owner was duplicated");assertions.unchanged(player,first_saved);
 // Queue exhaustion is a source result, not a transactional error. No slot or
 // counter is consumed when every record is occupied.
 auto full=queue_seed();for(auto& record:full.pool)record[0]=1;for(auto& slot:full.slots)slot=nq::Unknown{};full.counter.reset();const auto full_before=full;
 auto full_player=make(initial);auto full_objects=dispatch_collection();full_objects.data[0].field530=1;
 const auto full_out=value<na::PlayerUpdateNotificationsOutput>(full_player.prepare_and_update_notifications(full_objects,full,context,binding));
 check(full_out.effects.count==2&&full==full_before&&full_player.shared_fields().pw.field2fc_bits==one,"Full queue stopped the source caller");
 for(std::uint8_t i=0;i<2;++i)check(full_out.effects.attempts[i].queue&&!full_out.effects.attempts[i].inserted,"Full result confused with denied permission");
 // Full slots may remove the first alias of a free record, return null, then
 // admit the next notification. Counter wraps; repeated aliases survive.
 auto aliases=queue_seed();for(auto& record:aliases.pool)record[0]=1;aliases.pool[2][0]=0;
 for(auto& slot:aliases.slots)slot=nq::Opaque{77};aliases.slots[5]=nq::PoolIndex{2};aliases.slots[8]=nq::PoolIndex{2};
 auto alias_player=make(initial);auto alias_objects=dispatch_collection();alias_objects.data[0].field530=1;
 const auto alias_out=value<na::PlayerUpdateNotificationsOutput>(alias_player.prepare_and_update_notifications(alias_objects,aliases,context,binding));
 check(alias_out.effects.count==2&&!alias_out.effects.attempts[0].inserted&&alias_out.effects.attempts[0].queue&&alias_out.effects.attempts[0].queue->cleanup_slot==5,"Source cleanup/null result lost");
 check(alias_out.effects.attempts[1].inserted==nq::PoolIndex{2}&&aliases.counter==0&&aliases.slots[7]==nq::Slot{nq::PoolIndex{2}}&&aliases.slots[15]==nq::Slot{nq::PoolIndex{2}},"Sequential cleanup/insertion state lost");
 // A second notification fails after the first has changed its private queue.
 // Supplying only the missing external slot permits retry on the same Player.
 auto unknown=queue_seed();unknown.slots[1]=nq::Unknown{};const auto unknown_before=unknown;
 auto failed_player=make(initial);auto failed_objects=dispatch_collection();failed_objects.data[0].field530=1;const auto saved=save(failed_player);const auto collection_before=failed_objects;const auto original=value<na::Frame>(failed_player.capture());
 const auto failure=value<na::PlayerError>(failed_player.prepare_and_update_notifications(failed_objects,unknown,context,binding));
 const auto& failure_cause=composed_error(failure);check(failure_cause.phase==composed::Phase::notifications,"Wrong queue failure phase");
 const auto& queue_error=std::get<notify::Error>(failure_cause.cause);check(queue_error.code==notify::ErrorCode::queue&&queue_error.attempt==1&&queue_error.queue&&queue_error.queue->code==nq::ErrorCode::unknown_slot,"Lost second attempt queue cause");
 assertions.unchanged(failed_player,saved);check(unknown==unknown_before&&failed_objects==collection_before,"Late queue failure published a prefix");assertions.frame_unchanged(value<na::Frame>(failed_player.capture()),original);
 unknown.slots[1]=nq::Absent{};
 const auto retry=value<na::PlayerUpdateNotificationsOutput>(failed_player.prepare_and_update_notifications(failed_objects,unknown,context,binding));inserted(retry.effects,0);check(unknown.counter==2&&failed_player.shared_fields().pw.field2fc_bits==one,"Same Player queue repair retry failed");
 // Earlier API does not require notification facts or consume external queue.
 auto old_seed=initial;old_seed.shared_fields.pw.field1fc.reset();old_seed.shared_fields.pw.field200.reset();old_seed.shared_fields.pw.field586.reset();auto old=make(old_seed);auto old_objects=dispatch_collection();old_objects.data[0].field530=1;
 const auto earlier=value<na::PlayerUpdateStatusOutput>(old.prepare_and_update_status(old_objects,context,binding));
 check(earlier.effects.timer_incremented&&!old.shared_fields().pw.field1fc&&!old.shared_fields().pw.field200&&!old.shared_fields().pw.field586,"Earlier checkpoint consumed notification state");
 // A real spatial update precedes notifications. Both a queue error after the
 // first request and an earlier unsupported status event preserve all4 owners.
 auto spatial_admission=admission;preserve_input(spatial_admission);spatial_admission.prefix.delta_bits=one;
 auto spatial_initial=spatial_seed(2971.5546875f);post_inputs(spatial_initial);notification_inputs(spatial_initial);
 for(bool crossing:{false,true}){
  auto seed=spatial_initial;if(crossing)seed.shared_fields.pw.field2fc_bits=bits(179);
  auto scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&scene,0};
  auto subject=make(seed);auto collection=spatial_collection();collection.data[0].field530=1;auto pending=queue_seed();if(!crossing)pending.slots[1]=nq::Unknown{};
  const auto saved_state=save(subject);const auto old_collection=collection;const auto old_scene=scene;const auto old_queue=pending;const auto old_frame=value<na::Frame>(subject.capture());
  const auto error=value<na::PlayerError>(subject.prepare_and_update_notifications(collection,pending,spatial_context,binding));const auto& cause=composed_error(error);
  check(cause.phase==(crossing?composed::Phase::status:composed::Phase::notifications),"Wrong spatial failure checkpoint");
  if(crossing){const auto& e=std::get<mh4u::player_update_status::Error>(cause.cause);check(e.phase==mh4u::player_update_status::Phase::status&&std::get<status::Error>(e.cause).code==status::ErrorCode::unsupported_event,"Lost earlier event cause");}
  assertions.unchanged(subject,saved_state);check(collection==old_collection&&pending==old_queue,"Spatial failure published objects/queue");assertions.grids_equal(scene,old_scene);assertions.frame_unchanged(value<na::Frame>(subject.capture()),old_frame);
  if(crossing)collection.data[0].field530=2;else pending.slots[1]=nq::Absent{};
  const auto out=value<na::PlayerUpdateNotificationsOutput>(subject.prepare_and_update_notifications(collection,pending,spatial_context,binding));inserted(out.effects,0);
  check(scene.grid_states[0].counter==1&&collection.data[0].field349==1&&pending.counter==2,"Retry split four-owner publication");
  check(crossing?out.status.timer_cleared:out.status.timer_incremented,"Earlier status state lost on retry");assertions.pose_equal(subject,saved_state);assertions.frame_equal(out.frame,old_frame);
 }
 // One-shot allocator failures through the first renderer failure verify the
 // queue remains unpublished even after all notification calculations succeed.
 auto measured_scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&measured_scene,0};
 auto measured=make(spatial_initial);auto measured_objects=spatial_collection();measured_objects.data[0].field530=1;auto measured_queue=queue_seed();
 std::optional<na::PlayerResult<na::PlayerUpdateNotificationsOutput>> measured_result;std::size_t extent{};
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_update_notifications(measured_objects,measured_queue,spatial_context,binding));extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerUpdateNotificationsOutput>(*measured_result)&&extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=extent;++ordinal){
  auto scene=spatial_scene(collision,check);auto call_context=spatial_context;call_context.spatial_scene=&scene;
  auto subject=make(spatial_initial);auto collection=spatial_collection();collection.data[0].field530=1;auto pending=queue_seed();
  const auto saved_state=save(subject);const auto old_collection=collection;const auto old_scene=scene;const auto old_queue=pending;const auto old_frame=value<na::Frame>(subject.capture());
  std::optional<na::PlayerResult<na::PlayerUpdateNotificationsOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(subject.prepare_and_update_notifications(collection,pending,call_context,binding));}
  const auto e=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(e.code==na::PlayerErrorCode::capacity||e.code==na::PlayerErrorCode::driver||e.code==na::PlayerErrorCode::render,"Unexpected allocation failure category");
  assertions.unchanged(subject,saved_state);check(collection==old_collection&&pending==old_queue,"Allocation failure published queue/objects");assertions.grids_equal(scene,old_scene);assertions.frame_unchanged(value<na::Frame>(subject.capture()),old_frame);
  const auto out=value<na::PlayerUpdateNotificationsOutput>(subject.prepare_and_update_notifications(collection,pending,call_context,binding));inserted(out.effects,0);
  check(out.status.timer_incremented&&subject.shared_fields().pw.field2fc_bits==one&&scene.grid_states[0].counter==1&&collection.data[0].field349==1&&pending.counter==2,"Allocator retry split owner commits");
  assertions.pose_equal(subject,saved_state);assertions.frame_equal(out.frame,old_frame);
  if(e.code==na::PlayerErrorCode::render){check(e.render&&e.render->code==na::ErrorCode::capacity,"Lost nested renderer failure");first_render_failure=ordinal;break;}
 }
 check(first_render_failure&&allocation_failures==first_render_failure,"Did not reach actual renderer failure");
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
