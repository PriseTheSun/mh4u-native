// Persistent status checkpoint with real geometry and explicit diagnostic
// contexts. Original numerical/caller comparison is a separate ARM oracle.
#include "player_actor.hpp"
#include "test_support/player_update_actor_checks.hpp"
#include "test_support/player_update_status_actor_fixture.hpp"
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
namespace composed=mh4u::player_update_status;
using mh4u::test_update_actor_fixture::banks;
using namespace mh4u::test_dispatch_actor_fixture;
const status::Error& status_error(const na::PlayerError& e){
 check(e.driver&&e.driver->phase==pa::Phase::update_status,"Lost composed status cause");
 const auto& c=std::get<composed::Error>(e.driver->cause);
 check(c.phase==composed::Phase::status,"Wrong composed failure phase");
 return std::get<status::Error>(c.cause);
}
using mh4u::test_status_actor_fixture::status_inputs;

void run(const char* recipe,const pt::Constants& constants,const std::array<std::uint8_t,42>& caps,const std::string& caps_sha,
 const tbl::Table& table35,const tbl::Table& table31,const mh4u::collision_resource::Resource& collision){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected seven-piece nineteen-joint actor");
 const auto make=[&](const na::PlayerInitial& initial){return value<na::Player>(na::Player::from_rest(actor,initial,constants,banks()));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};admission.virtual120=ad::Virtual120::uplayer_b3dd60;
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);
 mh4u::player_state1::Tables tables{table35,table31};mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,identity};
 na::PlayerUpdateDispatchContext context{admission,parameters.context,tables,templates};
 auto initial=adjustment_seed();status_inputs(initial);
 auto objects=dispatch_collection();objects.data[0].field530=1;
 auto player=make(initial);const auto prior=save(player);const auto frame=value<na::Frame>(player.capture());
 // Three calls use the same Player and collection. Status consumes the
 // post-dispatch state; source timers advance explicitly but frame clock does not.
 for(unsigned i=1;i<=3;++i){
  const auto out=value<na::PlayerUpdateStatusOutput>(player.prepare_and_update_status(objects,context,binding));
  check(out.dispatch.dispatch_count==1&&out.dispatch.dispatches[0].state==3&&out.adjustment.virtual_returned&&out.effects.local,"Composed checkpoint effects lost");
  check(out.effects.scan&&out.effects.scan->matched&&out.effects.timer_incremented&&!out.effects.timer_cleared,"Status scan/timer missing");
  check(player.shared_fields().pw.field2fc_bits==bits(float(i))&&player.shared_fields().pw.field299==0,"Persistent canonical timer/secondary flags not published");
  check(player.shared_fields().pw.fieldc0_bits==out.adjustment.primary_bits&&objects.data[0].field349==1&&player.state().controller.player.selection_pbw.field6fa==0,"Post/controller/collection changes not published together");
  assertions.pose_equal(player,prior);assertions.frame_equal(out.frame,frame);
 }
 // A distinct instance keeps its explicit seed, including unknown raw facts.
 auto isolated=make(initial);const auto isolation=save(isolated);auto isolated_objects=dispatch_collection();const auto isolated_collection=isolated_objects;
 objects.data[0].field530=2;
 const auto clear=value<na::PlayerUpdateStatusOutput>(player.prepare_and_update_status(objects,context,binding));
 check(clear.effects.scan&&!clear.effects.scan->matched&&clear.effects.timer_cleared&&player.shared_fields().pw.field2fc_bits==0,"False scan did not clear timer");
 assertions.unchanged(isolated,isolation);check(isolated_objects==isolated_collection,"Another Player's objects changed");
 // Unknown D530 is consumed after original admission writes. The same Player
 // succeeds after the caller supplies only that missing collection fact.
 auto unknown=make(initial);auto unknown_objects=dispatch_collection();const auto unknown_before=save(unknown);const auto unknown_collection=unknown_objects;
 const auto unknown_frame=value<na::Frame>(unknown.capture());
 const auto missing=value<na::PlayerError>(unknown.prepare_and_update_status(unknown_objects,context,binding));
 const auto& missing_cause=status_error(missing);check(missing_cause.phase==status::Phase::object_scan&&missing_cause.code==status::ErrorCode::dependency,"Unknown object was converted to miss");
 const auto& object_cause=std::get<oq::Error>(missing_cause.cause);check(object_cause.code==oq::ErrorCode::unknown_field&&object_cause.field=="D+530","Lost exact missing object fact");
 assertions.unchanged(unknown,unknown_before);check(unknown_objects==unknown_collection,"Unknown status published collection");assertions.frame_unchanged(value<na::Frame>(unknown.capture()),unknown_frame);
 unknown_objects.data[0].field530=1;
 const auto repaired=value<na::PlayerUpdateStatusOutput>(unknown.prepare_and_update_status(unknown_objects,context,binding));
 check(repaired.effects.timer_incremented&&unknown.shared_fields().pw.field2fc_bits==one,"Same-Player missing-fact retry failed");
 // A later secondary-flag error must retain the input even after a successful
 // status scan and timer increment inside the discarded candidate.
 auto late_seed=initial;late_seed.shared_fields.pw.field2cc_bits.reset();auto late=make(late_seed);auto late_objects=dispatch_collection();late_objects.data[0].field530=1;
 const auto late_before=save(late);const auto late_collection=late_objects;
 const auto late_error=value<na::PlayerError>(late.prepare_and_update_status(late_objects,context,binding));
 const auto& late_cause=status_error(late_error);check(late_cause.phase==status::Phase::secondary_flags&&late_cause.code==status::ErrorCode::unknown_field&&late_cause.field=="PW2cc","Lost late status field cause");
 assertions.unchanged(late,late_before);check(late_objects==late_collection,"Late unknown published collection");
 // Earlier APIs do not silently run the added checkpoint or require its facts.
 const auto earlier=value<na::PlayerUpdateAdjustmentOutput>(late.prepare_and_adjust(late_objects,context,binding));
 check(earlier.effects.virtual_returned&&!late.shared_fields().pw.field2cc_bits&&late.shared_fields().pw.field2fc_bits==0,"Old adjustment API implicitly ran status");
 auto earlier_dispatch=make(late_seed);auto earlier_objects=dispatch_collection();
 const auto dispatched=value<na::PlayerUpdateDispatchOutput>(earlier_dispatch.prepare_and_dispatch(earlier_objects,context));
 check(dispatched.effects.dispatch_count==1&&!earlier_dispatch.shared_fields().pw.field2cc_bits&&earlier_dispatch.shared_fields().pw.field2fc_bits==0,"Old dispatch API implicitly ran status");
 // Real SBC candidate writes precede status. A missing object fact and an
 // unsupported threshold event each roll back Player, collection and grid.
 auto spatial_admission=admission;preserve_input(spatial_admission);spatial_admission.prefix.delta_bits=one;
 auto spatial_initial=spatial_seed(2971.5546875f);post_inputs(spatial_initial);status_inputs(spatial_initial);
 for(bool crossing:{false,true}){
  auto seed=spatial_initial;if(crossing)seed.shared_fields.pw.field2fc_bits=bits(179);
  auto scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&scene,0};
  auto subject=make(seed);auto collection=spatial_collection();if(crossing)collection.data[0].field530=1;
  const auto saved=save(subject);const auto collection_before=collection;const auto scene_before=scene;const auto original=value<na::Frame>(subject.capture());
  const auto e=value<na::PlayerError>(subject.prepare_and_update_status(collection,spatial_context,binding));const auto& cause=status_error(e);
  check(crossing?(cause.phase==status::Phase::event&&cause.code==status::ErrorCode::unsupported_event):(cause.phase==status::Phase::object_scan&&cause.code==status::ErrorCode::dependency),"Wrong spatial late failure");
  assertions.unchanged(subject,saved);check(collection==collection_before,"Late status published spatial objects");assertions.grids_equal(scene,scene_before);assertions.frame_unchanged(value<na::Frame>(subject.capture()),original);
  collection.data[0].field530=crossing?2:1;
  const auto out=value<na::PlayerUpdateStatusOutput>(subject.prepare_and_update_status(collection,spatial_context,binding));
  const auto& acquired=out.dispatch.dispatches[0].effects.spatial;
  check(acquired&&acquired->height&&acquired->cache_written&&scene.grid_states[0].counter==1&&collection.data[0].field349==1,"Retry failed to commit real spatial query");
  check(crossing?(out.effects.timer_cleared&&subject.shared_fields().pw.field2fc_bits==0):(out.effects.timer_incremented&&subject.shared_fields().pw.field2fc_bits==one),"Retry did not preserve source timer branch");
  assertions.pose_equal(subject,saved);assertions.frame_equal(out.frame,original);
 }
 // Sequential one-shot allocator failures cover the combined producer and
 // publication plan through the first actual renderer capacity failure.
 auto measured_scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&measured_scene,0};
 auto measured=make(spatial_initial);auto measured_objects=spatial_collection();measured_objects.data[0].field530=1;
 std::optional<na::PlayerResult<na::PlayerUpdateStatusOutput>> measured_result;std::size_t extent{};
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_update_status(measured_objects,spatial_context,binding));extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerUpdateStatusOutput>(*measured_result)&&extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=extent;++ordinal){
  auto scene=spatial_scene(collision,check);auto call_context=spatial_context;call_context.spatial_scene=&scene;
  auto subject=make(spatial_initial);auto collection=spatial_collection();collection.data[0].field530=1;
  const auto saved=save(subject);const auto collection_before=collection;const auto scene_before=scene;const auto original=value<na::Frame>(subject.capture());
  std::optional<na::PlayerResult<na::PlayerUpdateStatusOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(subject.prepare_and_update_status(collection,call_context,binding));}
  const auto e=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(e.code==na::PlayerErrorCode::capacity||e.code==na::PlayerErrorCode::driver||e.code==na::PlayerErrorCode::render,"Unexpected allocation failure category");
  assertions.unchanged(subject,saved);check(collection==collection_before,"Allocation failure published collection");assertions.grids_equal(scene,scene_before);assertions.frame_unchanged(value<na::Frame>(subject.capture()),original);
  const auto retry=value<na::PlayerUpdateStatusOutput>(subject.prepare_and_update_status(collection,call_context,binding));
  check(retry.effects.timer_incremented&&subject.shared_fields().pw.field2fc_bits==one&&scene.grid_states[0].counter==1&&collection.data[0].field349==1&&subject.shared_fields().pw.field6a8==1,"Allocator retry split owner commits");
  assertions.pose_equal(subject,saved);assertions.frame_equal(retry.frame,original);
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
