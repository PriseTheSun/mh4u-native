// Persistent position checkpoint with real geometry and explicit diagnostic
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
namespace composed=mh4u::player_update_position;
namespace position=mh4u::player_position_timer;
namespace direction=mh4u::player_direction_refresh;
namespace nq=mh4u::notification_queue;
using mh4u::test_update_actor_fixture::banks;
using namespace mh4u::test_dispatch_actor_fixture;
using mh4u::test_notifications_actor_fixture::queue_seed;
using mh4u::test_notifications_actor_fixture::notification_inputs;
constexpr direction::Bindings concrete{direction::Virtual84::uplayer_b085bc,direction::Virtual144::uplayer_b47aa0};
const composed::Error& composed_error(const na::PlayerError& e){
 check(e.driver&&e.driver->phase==pa::Phase::update_position,"Lost position driver cause");
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
 auto initial=adjustment_seed();notification_inputs(initial);initial.controller.player.pbw.field24=0;initial.controller.player.pw.fieldb0=6000;
 auto& f=initial.shared_fields;f.pw.fieldb8_bits=bits(3);f.pbw.field10_bits=bits(2);f.pbw.field14_bits=bits(4);f.pbw.field18_bits=bits(6);f.pbw.field1c_bits=0x7fa12345;
 f.pw.fielda0_bits=bits(0.5f);f.pw.fielda4_bits=bits(-1);f.pw.fielda8_bits=bits(2);
 f.pw.field90_bits=bits(-2);f.pw.field94_bits=bits(5);f.pw.field98_bits=bits(-7);
 auto objects=dispatch_collection();objects.data[0].field530=1;auto player=make(initial);auto queue=queue_seed();
 const auto saved=save(player);const auto frame=value<na::Frame>(player.capture());
 const std::array<std::array<U,3>,4> expected{{{bits(2.5f),bits(3),bits(8)},{bits(3),bits(2),bits(10)},{bits(-2),bits(5),bits(-7)},{bits(-2),bits(5),bits(-7)}}};
 // One persistent Player integrates twice, restores when the timer expires,
 // then skips. PBW current/previous position remain separate canonical fields.
 for(unsigned i=0;i<4;++i){
  const auto previous=player.shared_fields().pbw;
  const auto out=value<na::PlayerUpdatePositionOutput>(player.prepare_and_update_position(objects,queue,context,concrete,binding));
  const auto& fields=player.shared_fields();
  check(fields.pbw.field10_bits==expected[i][0]&&fields.pbw.field14_bits==expected[i][1]&&fields.pbw.field18_bits==expected[i][2],"Persistent position did not use the current candidate");
  check(fields.pw.fieldb8_bits==bits(float(i<2?2-i:0)),"Position timer was reset or decremented twice");
  check(fields.pbw.field6e8_bits==previous.field10_bits&&fields.pbw.field6ec_bits==previous.field14_bits&&fields.pbw.field6f0_bits==previous.field18_bits,"Previous position did not retain begin-update order");
  check(out.effects.timer_called==(i<3)&&out.effects.position_integrated==(i<2)&&out.effects.position_restored==(i==2),"Position branch effects changed");
  check(fields.pbw.field1c_bits==(i<2?0x7fa12345U:0U),"Position W normalized outside the restore branch");
  check(queue.counter==2*(i+1)&&out.notifications.count==2&&out.status.timer_incremented,"Position lost earlier notifications/status");
  check(player.state().controller.player.pbw.field24==(i==0?2002U:i==1?4004U:6000U),"Position lost preceding direction");
  assertions.pose_equal(player,saved);assertions.frame_equal(out.frame,frame);
 }
 // Existing direction API ends before consuming the position/timer fields.
 auto earlier=make(initial);auto earlier_objects=dispatch_collection();earlier_objects.data[0].field530=1;auto earlier_queue=queue_seed();
 value<na::PlayerUpdateDirectionOutput>(earlier.prepare_and_update_direction(earlier_objects,earlier_queue,context,concrete,binding));
 check(earlier.shared_fields().pw.fieldb8_bits==bits(3)&&earlier.shared_fields().pbw.field10_bits==bits(2),"Earlier checkpoint executed position phase");
 // Lazy skip preserves unknown rates and saved fields, and raw nonpositive timer.
 for(U timer:{0U,0x80000000U,0xbf800000U,0xff800000U}){
  auto seed=initial;auto& pf=seed.shared_fields.pw;pf.fieldb8_bits=timer;
  pf.fielda0_bits.reset();pf.fielda4_bits.reset();pf.fielda8_bits.reset();pf.field90_bits.reset();pf.field94_bits.reset();pf.field98_bits.reset();
  auto subject=make(seed);auto collection=dispatch_collection();collection.data[0].field530=1;auto pending=queue_seed();
  const auto out=value<na::PlayerUpdatePositionOutput>(subject.prepare_and_update_position(collection,pending,context,concrete,binding));
  check(!out.effects.timer_called&&!out.effects.position_integrated&&!out.effects.position_restored&&subject.shared_fields().pw.fieldb8_bits==timer,"Lazy position branch manufactured knowledge or changed raw timer");
  check(!subject.shared_fields().pw.fielda0_bits&&!subject.shared_fields().pw.field98_bits&&pending.counter==2,"Skipped position branch consumed unknown fields");
 }
 // An unordered entry reaches decrement, clamps, then restores raw words.
 auto unordered=initial;unordered.shared_fields.pw.fieldb8_bits=0x7fc01234;
 unordered.shared_fields.pw.field90_bits=0x7fa54321;unordered.shared_fields.pw.field94_bits=0x7f800000;unordered.shared_fields.pw.field98_bits=0xff800000;
 unordered.shared_fields.pw.fielda0_bits.reset();unordered.shared_fields.pw.fielda4_bits.reset();unordered.shared_fields.pw.fielda8_bits.reset();
 auto restored=make(unordered);auto restored_objects=dispatch_collection();restored_objects.data[0].field530=1;auto restored_queue=queue_seed();
 const auto restored_out=value<na::PlayerUpdatePositionOutput>(restored.prepare_and_update_position(restored_objects,restored_queue,context,concrete,binding));
 check(restored_out.effects.position_restored&&restored_out.effects.timer_returned==1&&restored.shared_fields().pw.fieldb8_bits==0,"Unordered timer incorrectly skipped restoration");
 check(restored.shared_fields().pbw.field10_bits==0x7fa54321&&restored.shared_fields().pbw.field14_bits==0x7f800000&&restored.shared_fields().pbw.field18_bits==0xff800000&&restored.shared_fields().pbw.field1c_bits==0,"Restore did not preserve raw saved words");
 auto spatial_admission=admission;preserve_input(spatial_admission);spatial_admission.prefix.delta_bits=one;
 auto spatial_initial=spatial_seed(2971.5546875f);post_inputs(spatial_initial);notification_inputs(spatial_initial);
 spatial_initial.shared_fields.pw.fieldb8_bits=one;
 spatial_initial.shared_fields.pw.field90_bits=bits(-3);spatial_initial.shared_fields.pw.field94_bits=bits(7);spatial_initial.shared_fields.pw.field98_bits=bits(11);
 // Missing timer, late Z rate and late saved Z fail after prior spatial/queue
 // writes in the candidate. The older API can still use this same Player.
 for(unsigned scenario=0;scenario<3;++scenario){
  auto seed=spatial_initial;const char* field="PWb8";auto phase=position::Phase::entry;
  if(scenario==0)seed.shared_fields.pw.fieldb8_bits.reset();
  if(scenario==1){seed.shared_fields.pw.fieldb8_bits=bits(3);seed.shared_fields.pw.fielda0_bits=one;seed.shared_fields.pw.fielda4_bits=one;seed.shared_fields.pw.fielda8_bits.reset();field="PWa8";phase=position::Phase::z_multiply;}
  if(scenario==2){seed.shared_fields.pw.field98_bits.reset();field="PW98";phase=position::Phase::restore;}
  auto scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&scene,0};
  auto subject=make(seed);auto collection=spatial_collection();collection.data[0].field530=1;auto pending=queue_seed();
  const auto prior=save(subject);const auto old_collection=collection;const auto old_scene=scene;const auto old_queue=pending;const auto old_frame=value<na::Frame>(subject.capture());
  const auto error=value<na::PlayerError>(subject.prepare_and_update_position(collection,pending,spatial_context,concrete,binding));
  const auto& cause=composed_error(error);check(cause.phase==composed::Phase::position,"Late position failure lost phase");
  const auto& leaf=std::get<position::Error>(cause.cause);check(leaf.code==position::ErrorCode::unknown_field&&leaf.phase==phase&&leaf.field==field,"Position failure lost source field/order");
  assertions.unchanged(subject,prior);check(collection==old_collection&&pending==old_queue,"Position failure published earlier objects/queue");assertions.grids_equal(scene,old_scene);assertions.frame_unchanged(value<na::Frame>(subject.capture()),old_frame);
  value<na::PlayerUpdateDirectionOutput>(subject.prepare_and_update_direction(collection,pending,spatial_context,concrete,binding));
  check(subject.shared_fields().pw.fieldb8_bits==seed.shared_fields.pw.fieldb8_bits&&scene.grid_states[0].counter==1&&pending.counter==2,"Previous checkpoint consumed missing position fields after rejection");
 }
 // Include every allocation through the first actual render preparation, then
 // retry the same Player/Collection/Scene/queue without recreating any owner.
 auto measured_scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&measured_scene,0};
 auto measured=make(spatial_initial);auto measured_objects=spatial_collection();measured_objects.data[0].field530=1;auto measured_queue=queue_seed();
 std::optional<na::PlayerResult<na::PlayerUpdatePositionOutput>> measured_result;std::size_t extent{};
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_update_position(measured_objects,measured_queue,spatial_context,concrete,binding));extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerUpdatePositionOutput>(*measured_result)&&extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=extent;++ordinal){
  auto scene=spatial_scene(collision,check);auto call_context=spatial_context;call_context.spatial_scene=&scene;
  auto subject=make(spatial_initial);auto collection=spatial_collection();collection.data[0].field530=1;auto pending=queue_seed();
  const auto prior=save(subject);const auto old_collection=collection;const auto old_scene=scene;const auto old_queue=pending;const auto old_frame=value<na::Frame>(subject.capture());
  std::optional<na::PlayerResult<na::PlayerUpdatePositionOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(subject.prepare_and_update_position(collection,pending,call_context,concrete,binding));}
  const auto error=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(error.code==na::PlayerErrorCode::capacity||error.code==na::PlayerErrorCode::driver||error.code==na::PlayerErrorCode::render,"Allocation failure category changed");
  assertions.unchanged(subject,prior);check(collection==old_collection&&pending==old_queue,"Allocation failure published queue/objects");assertions.grids_equal(scene,old_scene);assertions.frame_unchanged(value<na::Frame>(subject.capture()),old_frame);
  const auto out=value<na::PlayerUpdatePositionOutput>(subject.prepare_and_update_position(collection,pending,call_context,concrete,binding));
  check(out.effects.position_restored&&scene.grid_states[0].counter==1&&collection.data[0].field349==1&&pending.counter==2&&subject.shared_fields().pw.fieldb8_bits==0,"Allocator retry split commits");
  check(subject.shared_fields().pbw.field10_bits==bits(-3)&&subject.shared_fields().pbw.field14_bits==bits(7)&&subject.shared_fields().pbw.field18_bits==bits(11),"Restored position not published with earlier owners");
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
