// Persistent post-dispatch checkpoint with real geometry and explicit diagnostic
// contexts. Original numerical/caller comparison is a separate ARM oracle.
#include "player_actor.hpp"
#include "test_support/player_update_actor_checks.hpp"
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
const post::Error& post_error(const na::PlayerError& e){check(e.driver&&e.driver->phase==pa::Phase::update_adjustment,"Lost post-dispatch typed cause");return std::get<post::Error>(e.driver->cause);}
using mh4u::test_update_actor_fixture::banks;
using mh4u::test_dispatch_actor_fixture::dispatch_collection;
using mh4u::test_dispatch_actor_fixture::spatial_collection;
using mh4u::test_dispatch_actor_fixture::action_seed;
using mh4u::test_dispatch_actor_fixture::spatial_seed;
using mh4u::test_dispatch_actor_fixture::preserve_input;
using mh4u::test_dispatch_actor_fixture::spatial_scene;
using mh4u::test_dispatch_actor_fixture::post_inputs;
using mh4u::test_dispatch_actor_fixture::adjustment_seed;
void run(const char* recipe,const pt::Constants& constants,const std::array<std::uint8_t,42>& caps,const std::string& caps_sha,
 const tbl::Table& table35,const tbl::Table& table31,const mh4u::collision_resource::Resource& collision){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected seven-piece nineteen-joint actor");
 const auto make=[&](const na::PlayerInitial& initial){return value<na::Player>(na::Player::from_rest(actor,initial,constants,banks()));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};admission.virtual120=ad::Virtual120::uplayer_b3dd60;
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);
 mh4u::player_state1::Tables tables{table35,table31};mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,identity};
 na::PlayerUpdateDispatchContext context{admission,parameters.context,tables,templates};
 auto initial=adjustment_seed();auto player=make(initial);auto objects=dispatch_collection();const auto saved=save(player);const auto before=value<na::Frame>(player.capture());
 const auto first=value<na::PlayerUpdateAdjustmentOutput>(player.prepare_and_adjust(objects,context,binding));
 check(first.dispatch.dispatch_count==1&&first.dispatch.dispatches[0].state==3&&first.effects.virtual_returned&&first.effects.primary_rescaled,"Dispatch/post phase order or effect lost");
 check(first.effects.base_bits==one&&first.effects.primary_bits==bits(3)&&first.effects.secondary_bits==bits(-2),"Canonical speed effects");
 const auto& l0=player.state().frame.animation.layers[0].animation.layer;const auto& l1=player.state().frame.animation.layers[1].animation.layer;
 check(bits(l0.speed)==bits(3)&&bits(l0.remaining_interpolation)==bits(4)&&bits(l0.interpolation)==bits(2)&&bits(l1.speed)==bits(-2)&&bits(l1.interpolation)==0x7fa98765,"Canonical speed fields not published or secondary modified interpolation");
 check(player.shared_fields().pw.fieldc0_bits==bits(3)&&player.state().controller.motion.fieldd68==bits(6)&&player.state().controller.player.selection_pbw.field6fa==0&&objects.data[0].field349==1,"Rate/policy/Collection ownership");
 check(!player.shared_fields().pw.field10_bits&&first.effects.query_count==3,"State3 consumed PW10 or source begin-update failed to clear the old flag");assertions.pose_equal(player,saved);assertions.frame_equal(first.frame,before);
 // Same persistent object: next call scans the explicitly absent table,
 // retaining the updated interpolation fields with no new clock/pose owner.
 const auto second=value<na::PlayerUpdateAdjustmentOutput>(player.prepare_and_adjust(objects,context,binding));
 check(second.effects.query_count==3&&player.completed_steps()==0&&bits(player.state().frame.animation.layers[0].animation.layer.remaining_interpolation)==bits(4),"Persistent second adjustment reset state or clock");
 // Source state1 entry is real and installs the table before PW10 is consumed.
 for(bool stopped:{false,true}){
  auto seed=action_seed(1);post_inputs(seed);seed.layers[0].animation.layer.selector=3;
  seed.shared_fields.pw.field10_bits.reset();seed.controller.player.pw.fieldsc[1]=stopped?1:0;
  auto subject=make(seed);auto collection=dispatch_collection();const auto prior=save(subject);const auto collection_before=collection;
  auto result=subject.prepare_and_adjust(collection,context,binding);
  if(!stopped){const auto e=value<na::PlayerError>(std::move(result));check(post_error(e).phase==post::Phase::rate&&post_error(e).field=="PW10","Unknown reached PW10 was invented");assertions.unchanged(subject,prior);check(collection==collection_before,"PW10 failure published Collection");}
  else{const auto out=value<na::PlayerUpdateAdjustmentOutput>(std::move(result));check(out.effects.base_bits==0&&out.effects.primary_bits==0&&out.effects.secondary_bits==0x80000000U&&out.dispatch.dispatches[0].effects.installation.has_value(),"Dead PW10 blocked stop rate after real entry");}
 }
 auto slow_seed=initial;slow_seed.controller.player.pw.fieldsc[0]=1;slow_seed.controller.player.pbw.field39c=bits(10);slow_seed.controller.player.pw.fieldc4=bits(-10);
 auto slow=make(slow_seed);auto slow_objects=dispatch_collection();const auto slow_out=value<na::PlayerUpdateAdjustmentOutput>(slow.prepare_and_adjust(slow_objects,context,binding));
 check(slow_out.effects.base_bits==0x3dcccccd&&slow_out.effects.primary_bits==one&&slow_out.effects.secondary_bits==0xbf800000,"Slow rate not composed into speed");
 for(U raw:{0x80000000U,0x7f800000U,0x7f812345U}){
  auto seed=initial;seed.controller.player.pbw.field39c=raw;auto subject=make(seed);auto collection=dispatch_collection();const auto prior=save(subject);
  auto out=value<na::PlayerUpdateAdjustmentOutput>(subject.prepare_and_adjust(collection,context,binding));
  check(bits(subject.state().frame.animation.layers[0].animation.layer.speed)==out.effects.primary_bits&&subject.shared_fields().pw.fieldc0_bits==out.effects.primary_bits,"Exceptional numeric output was repaired or split between owners");assertions.pose_equal(subject,prior);
 }
 // Policy and eligibility are later than the speed stores. Failure still
 // preserves all earlier owner changes and the geometry revision.
 for(unsigned which=0;which<2;++which){
  auto seed=initial;if(which==0){seed.controller.player.selection_pbw.field6fa=0;seed.controller.player.selection_pw.field594=0;seed.controller.player.pw.policy.table=tbl::Unknown{};}
  else{seed.controller.player.pw.fieldec|=0x08000000;seed.controller.player.pw.field334=0x10000000;seed.shared_fields.pw.field339.reset();}
  auto subject=make(seed);auto collection=dispatch_collection();const auto prior=save(subject);const auto collection_before=collection;const auto frame=value<na::Frame>(subject.capture());
  const auto e=value<na::PlayerError>(subject.prepare_and_adjust(collection,context,binding));check(post_error(e).phase==(which?post::Phase::eligibility:post::Phase::policy)&&post_error(e).field==(which?"PW339":"PW100"),"Late typed cause lost");assertions.unchanged(subject,prior);check(collection==collection_before,"Late post failure published Collection");assertions.frame_equal(value<na::Frame>(subject.capture()),frame);
 }
 auto eligible_seed=initial;eligible_seed.controller.player.pw.fieldec|=0x08000000;eligible_seed.controller.player.pw.field334=0x10000000;eligible_seed.shared_fields.pw.field339=1;
 auto eligible=make(eligible_seed);auto eligible_objects=dispatch_collection();const auto eligible_out=value<na::PlayerUpdateAdjustmentOutput>(eligible.prepare_and_adjust(eligible_objects,context,binding));
 check(eligible_out.effects.eligible==true&&eligible_out.effects.eligibility_selector==37&&!(eligible.state().controller.player.pw.fieldec&0x08000000)&&eligible.shared_fields().pw.field339==0,"Eligibility did not publish flags with speed");
 // A real SBC query has already prepared cache/stamps before virtual+10c.
 // Unknown binding rejects all of it; retry on the SAME Player uses one query.
 auto spatial_admission=admission;preserve_input(spatial_admission);spatial_admission.prefix.delta_bits=0;
 auto scene=spatial_scene(collision,check);auto spatial_initial=spatial_seed(2971.5546875f);post_inputs(spatial_initial);
 na::PlayerUpdateDispatchContext spatial_context{spatial_admission,parameters.context,tables,templates,{}, {},&scene,0};
 auto spatial=make(spatial_initial);auto spatial_objects=spatial_collection();const auto spatial_before=save(spatial);const auto objects_before=spatial_objects;const auto scene_before=scene;const auto spatial_frame=value<na::Frame>(spatial.capture());
 const auto error=value<na::PlayerError>(spatial.prepare_and_adjust(spatial_objects,spatial_context,{}));check(post_error(error).phase==post::Phase::virtual_call,"Unknown concrete virtual treated as no-op");assertions.unchanged(spatial,spatial_before);check(spatial_objects==objects_before,"Unknown virtual published objects");assertions.grids_equal(scene,scene_before);assertions.frame_equal(value<na::Frame>(spatial.capture()),spatial_frame);
 const auto retry=value<na::PlayerUpdateAdjustmentOutput>(spatial.prepare_and_adjust(spatial_objects,spatial_context,binding));
 const auto& acquired=retry.dispatch.dispatches[0].effects.spatial;
 check(acquired&&acquired->height&&acquired->cache_written&&acquired->height->height_bits==0x453698e0&&scene.grid_states[0].counter==1&&spatial_objects.data[0].field349==1&&spatial.shared_fields().pw.fieldc0_bits==retry.effects.primary_bits,"Post retry did not publish three owners together");assertions.pose_equal(spatial,spatial_before);assertions.frame_equal(retry.frame,spatial_frame);
 // Each allocation prefix through an actual renderer error remains retryable.
 auto measured_scene=spatial_scene(collision,check);auto measured_context=spatial_context;measured_context.spatial_scene=&measured_scene;
 auto measured=make(spatial_initial);auto measured_objects=spatial_collection();std::optional<na::PlayerResult<na::PlayerUpdateAdjustmentOutput>> measured_result;std::size_t extent{};
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_adjust(measured_objects,measured_context,binding));extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerUpdateAdjustmentOutput>(*measured_result)&&extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=extent;++ordinal){
  auto fail_scene=spatial_scene(collision,check);auto fail_context=spatial_context;fail_context.spatial_scene=&fail_scene;
  auto subject=make(spatial_initial);auto collection=spatial_collection();const auto prior=save(subject);const auto collection_before=collection;const auto grid_before=fail_scene;const auto frame=value<na::Frame>(subject.capture());
  std::optional<na::PlayerResult<na::PlayerUpdateAdjustmentOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(subject.prepare_and_adjust(collection,fail_context,binding));}
  const auto failure=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(failure.code==na::PlayerErrorCode::capacity||failure.code==na::PlayerErrorCode::driver||failure.code==na::PlayerErrorCode::render,"Unexpected allocation category");
  assertions.unchanged(subject,prior);check(collection==collection_before,"Allocation error published Collection");assertions.grids_equal(fail_scene,grid_before);assertions.frame_equal(value<na::Frame>(subject.capture()),frame);
  const auto success=value<na::PlayerUpdateAdjustmentOutput>(subject.prepare_and_adjust(collection,fail_context,binding));
  check(fail_scene.grid_states[0].counter==1&&collection.data[0].field349==1&&subject.shared_fields().pw.field6a8==1&&subject.shared_fields().pw.fieldc0_bits==success.effects.primary_bits&&subject.completed_steps()==0,"Same-Player allocation retry split owners");assertions.frame_equal(success.frame,frame);
  if(failure.code==na::PlayerErrorCode::render){check(failure.render&&failure.render->code==na::ErrorCode::capacity,"Lost actual renderer cause");first_render_failure=ordinal;break;}
 }
 check(first_render_failure&&allocation_failures==first_render_failure,"No actual render failure exercised");
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
