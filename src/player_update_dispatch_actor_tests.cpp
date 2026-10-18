// Persistent Player composition, with explicit diagnostic context and source-backed
// owners. This is not a constructor/scheduler oracle or a physical input mapping.
#include "player_actor.hpp"
#include "test_support/player_actor_state.hpp"
#include "test_support/player_update_dispatch_actor_fixture.hpp"
#include <bit>
#include <algorithm>
#include "picosha2.h"
#include <fstream>
#include <cstdlib>
#include <limits>
#include <new>
#ifdef _WIN32
#include <malloc.h>
#endif
#include <iostream>
#include <stdexcept>
#include "test_support/player_actor_allocation_probe.hpp"
namespace {
namespace na=mh4u::native_actor;namespace pa=mh4u::player_animation;
namespace pt=mh4u::player_transition;namespace ad=mh4u::player_update_admission;
namespace oq=mh4u::player_object_query;namespace wl=mh4u::player_world_lookup;
namespace jp=mh4u::joint_pose;namespace pq=mh4u::player_parameter_query;
namespace ud=mh4u::player_update_dispatch;namespace st=mh4u::player_state1;namespace sq=mh4u::spatial_query;namespace tbl=mh4u::player_policy_table;
namespace us=mh4u::player_update_statistics;namespace uc=mh4u::player_update_continuation;using U=std::uint32_t;
constexpr U one=0x3f800000;constexpr jp::Bits4 identity{0,0,0,one};std::size_t checks=0,allocation_failures=0,first_render_failure=0;
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
U bits(float f){return std::bit_cast<U>(f);}
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
template<class T,class...V>T value(std::variant<V...> v){if(auto* p=std::get_if<T>(&v))return std::move(*p);throw std::runtime_error(std::visit([](const auto& e){return describe(e);},v));}
using mh4u::test_player_actor_state::state_equal;
void frame_equal(const na::Frame& a,const na::Frame& b){
 check(a.tick==b.tick&&bits(a.source_frame)==bits(b.source_frame)&&a.event_masks==b.event_masks&&a.ended==b.ended,"Continuation advanced frame metadata");
 check(a.scene.global_transforms==b.scene.global_transforms&&a.palettes==b.palettes,"Continuation changed pose/palettes");
 check(a.scene.mesh.triangles.size()==b.scene.mesh.triangles.size()&&a.scene.draws.size()==b.scene.draws.size(),"Geometry extent changed");
 for(std::size_t i=0;i<a.scene.mesh.triangles.size();++i){const auto& p=a.scene.mesh.triangles[i];const auto& q=b.scene.mesh.triangles[i];
  check(std::array{bits(p.position.x),bits(p.position.y),bits(p.position.z),bits(p.normal.x),bits(p.normal.y),bits(p.normal.z),bits(p.uv.x),bits(p.uv.y)}==std::array{bits(q.position.x),bits(q.position.y),bits(q.position.z),bits(q.normal.x),bits(q.normal.y),bits(q.normal.z),bits(q.uv.x),bits(q.uv.y)},"Continuation changed skinned geometry");}
 for(std::size_t i=0;i<a.scene.draws.size();++i){const auto& p=a.scene.draws[i];const auto& q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Draw identity changed");}
}
struct Saved {pa::State state;mh4u::player_fields::Fields fields;mh4u::player_state1::Fields state1;mh4u::player_position_bridge::Fields position;std::uint64_t steps;};
Saved save(const na::Player& p){return {p.state(),p.shared_fields(),p.state1_fields(),p.position_fields(),p.completed_steps()};}
void unchanged(const na::Player& p,const Saved& s){check(state_equal(p.state(),s.state)&&p.shared_fields()==s.fields&&p.state1_fields()==s.state1&&p.position_fields()==s.position&&p.completed_steps()==s.steps,"Continuation failure published Player prefix");}
using mh4u::test_update_actor_fixture::banks;
using mh4u::test_update_actor_fixture::collection;
using mh4u::test_dispatch_actor_fixture::dispatch_collection;
using mh4u::test_dispatch_actor_fixture::spatial_collection;
using mh4u::test_dispatch_actor_fixture::action_seed;
using mh4u::test_dispatch_actor_fixture::spatial_seed;
using mh4u::test_dispatch_actor_fixture::preserve_input;
using mh4u::test_dispatch_actor_fixture::spatial_scene;
void pose_equal(const pa::State& a,const pa::State& b){
 check(a.frame.model==b.frame.model&&a.frame.animation.joints==b.frame.animation.joints&&a.frame.animation.snapshots==b.frame.animation.snapshots&&mh4u::test_player_actor_state::hierarchy_equal(a.hierarchy,b.hierarchy),"Dispatch changed canonical pose or model");
 for(std::size_t i=0;i<6;++i)check(mh4u::test_player_actor_state::layer_equal(a.inactive[i],b.inactive[i]),"Dispatch changed inactive slot");
}
void grid_equal(const sq::Scene& scene,const sq::Scene& before){
 check(scene.grid_states.size()==before.grid_states.size()&&scene.instances.size()==before.instances.size(),"Scene extent changed");
 for(std::size_t i=0;i<scene.grid_states.size();++i){const auto& a=scene.grid_states[i];const auto& b=before.grid_states[i];
  check(a.resource.sha256()==b.resource.sha256()&&a.group==b.group&&a.counter==b.counter&&a.depth==b.depth&&a.pair_marks==b.pair_marks,"Scene grid candidate published on failure");}
}
// Native-only format boundary derived through owner offsets, not an original
// ARM scenario. Keep real faces/vertices, remove every pair/grid reference.
mh4u::collision_resource::Resource zero_pair_resource(const mh4u::collision_resource::Resource& source){
 const auto bytes=source.bytes();const auto& group=source.groups()[0];
 const auto grid=value<mh4u::collision_resource::Grid>(source.grid(0));
 std::string data;
 const auto append=[&](std::size_t start,std::size_t count){data.append(reinterpret_cast<const char*>(bytes.data()+start),count);};
 append(0,source.sections()[1].offset);
 append(source.sections()[2].offset,source.core_end_offset()-source.sections()[2].offset);
 const auto word=[&](std::size_t offset,U value){for(unsigned i=0;i<4;++i)data[offset+i]=char(value>>(8*i));};
 word(20,0);word(group.source_offset+48,0);
 append(grid.source_offset,46); // GRCO header from the owning parser.
 for(const auto& cell:grid.cells){data.push_back(0);data.push_back(0);data.push_back(char(cell.unknown));data.push_back(char(cell.unknown>>8));data.append(4,'\0');}
 data.append(4,'\0'); // No grid indices and no per-pair bounds.
 check(source.global_tail_offset().has_value(),"Derived grid requires observed tail extent");
 append(*source.global_tail_offset(),bytes.size()-*source.global_tail_offset());
 const auto hash=picosha2::hash256_hex_string(data);
 return value<mh4u::collision_resource::Resource>(mh4u::collision_resource::Resource::decode(data,hash));
}
const ud::Error& dispatch_error(const na::PlayerError& error){
 check(error.driver&&error.driver->phase==pa::Phase::update_dispatch,"Original dispatch cause lost");
 return std::get<ud::Error>(error.driver->cause);
}
void run(const char* recipe,const pt::Constants& constants,const std::array<std::uint8_t,42>& caps,const std::string& caps_sha,
 const tbl::Table& table35,const tbl::Table& table31,const mh4u::collision_resource::Resource& collision){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected seven-piece nineteen-joint actor");
 const auto make=[&](const na::PlayerInitial& x,const na::PlayerBanks& b=banks()){return value<na::Player>(na::Player::from_rest(actor,x,constants,b));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};admission.virtual120=ad::Virtual120::uplayer_b3dd60;
 mh4u::test_update_actor_fixture::Parameters parameter_fixture(caps,caps_sha);
 const auto& record=parameter_fixture.record;const auto& record_sha=parameter_fixture.record_sha;
 auto& stats=parameter_fixture.context;
 st::Tables tables{table35,table31};mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,identity};
 na::PlayerUpdateDispatchContext context{admission,stats,tables,templates};
 // State3 runs the shared dispatcher clear without reading unknown actions,
 // banks, templates or timers, after the earlier source preparation.
 auto no_seed=action_seed(0xffff,255);no_seed.controller.player.pbw.field371=3;no_seed.state1_fields.field50_bits=0x7fa12345;
 auto no=make(no_seed,{});const auto no_before=save(no);const auto rest=value<na::Frame>(no.capture());auto no_objects=dispatch_collection();
 const auto no_out=value<na::PlayerUpdateDispatchOutput>(no.prepare_and_dispatch(no_objects,context));
 check(no_out.effects.dispatch_count==1&&no_out.effects.dispatches[0].state==3&&no_out.effects.dispatches[0].action==0xffff&&!no_out.effects.dispatches[0].effects.command,"State3 no-op was fabricated as a state1 request");
 check(no.state().controller.player.selection_pbw.field376==0&&no.shared_fields().pw.field01==0&&no.state1_fields()==no_seed.state1_fields,"Dispatcher clear or timer ownership");
 check(no_out.effects.admission.scan&&no_objects.data[0].field349==1&&no_out.effects.statistics.query_count==7,"Earlier source effects were dropped");
 auto no_animation=no.state();no_animation.controller=no_before.state.controller;check(state_equal(no_animation,no_before.state),"State3 changed physical animation storage");frame_equal(no_out.frame,rest);
 // Supported entry requests and same-selector skip use the real resource owner.
 for(const auto action:{1U,2U,4U,9U}){
  auto initial=action_seed(action);if(action<3)initial.layers[0].animation.layer.selector=3;
  auto player=make(initial);auto objects=dispatch_collection();const auto saved=save(player);const auto before=value<na::Frame>(player.capture());
  const auto out=value<na::PlayerUpdateDispatchOutput>(player.prepare_and_dispatch(objects,context));const auto& effects=out.effects.dispatches[0].effects;
  check(out.effects.dispatch_count==1&&out.effects.dispatches[0].action==action&&effects.command,"Entry dispatch identity");
  check(effects.request_count==(action<3?0:2)&&effects.installation,"Real request/if_changed or tail install lost");
  check(player.shared_fields().pw.field01==0&&player.state().controller.player.pbw.fields5[0]==1&&player.completed_steps()==0,"Entry implied second dispatch/tick");
  check(std::get<tbl::Table>(player.state().controller.player.pw.policy.table).identity()==(action==4?table31.identity():table35.identity()),"Installed table identity changed");
  pose_equal(player.state(),saved.state);frame_equal(out.frame,before);
 }
 // The same persistent Player binds real tracks on an explicit step, then the
 // next combined call executes ID2->ID9 and samples those live tracks.
 auto live_seed=action_seed(2);live_seed.shared_fields.pw.field13c_bits=0;live_seed.shared_fields.pbw.field464_bits=0;
 live_seed.shared_fields.pw.field9dc=live_seed.shared_fields.pw.field9de=live_seed.shared_fields.pw.field9dd=0;
 live_seed.controller.player.pbw.magnitudes[0]=50;live_seed.controller.player.pbw.field3d4=0;
 auto live_admission=admission;preserve_input(live_admission);na::PlayerUpdateDispatchContext live_context{live_admission,stats,tables,templates};
 auto live=make(live_seed);auto live_objects=dispatch_collection();value<na::PlayerUpdateDispatchOutput>(live.prepare_and_dispatch(live_objects,live_context));
 na::PlayerStep step{};step.delta_bits=one;step.templates=templates;step.restoration_scale=jp::Bits3{one,one,one};step.parent=jp::Null{};
 step.reference_basis=mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};step.reference_translation=identity;
 const auto stepped=value<na::PlayerOutput>(live.step(step));check(stepped.animation&&stepped.animation->joint_samples>0,"Explicit step did not bind real LMT tracks");
 const auto live_before=save(live);const auto live_frame=value<na::Frame>(live.capture());
 const auto twice=value<na::PlayerUpdateDispatchOutput>(live.prepare_and_dispatch(live_objects,live_context));
 check(twice.effects.dispatch_count==2&&twice.effects.dispatches[0].action==2&&twice.effects.dispatches[1].action==9&&twice.effects.dispatches[1].substate==0,"Second dispatch did not observe first transition");
 check(twice.effects.dispatches[0].effects.transition&&twice.effects.dispatches[1].effects.command&&twice.effects.dispatches[1].effects.samples>0,"Live sampling branch was bypassed");
 check(live.state().controller.player.pbw.field372==9&&live.state().controller.player.selection_pbw.field376==0&&live.shared_fields().pw.field01==1,"Single PW01 store or second clear lost");
 check(live.completed_steps()==live_before.steps&&live.state1_fields().field50_bits==bits(20),"Composed update advanced clock or lost timer");pose_equal(live.state(),live_before.state);frame_equal(twice.frame,live_frame);
 // A supported first transition followed by unsupported action5 must roll back
 // all preparation, including the canonical aliased Collection write.
 auto late_seed=action_seed(4,1);late_seed.layers[0].animation.layer.flags|=4;
 auto late=make(late_seed);auto late_objects=dispatch_collection();const auto late_before=save(late);const auto late_collection=late_objects;
 const auto late_error=value<na::PlayerError>(late.prepare_and_dispatch(late_objects,context));
 check(dispatch_error(late_error).phase==ud::Phase::second_dispatch&&std::get<st::Error>(dispatch_error(late_error).cause).code==st::ErrorCode::unsupported_action,"Late unsupported second dispatch was hidden");unchanged(late,late_before);check(late_objects==late_collection,"Late dispatch published Collection");
 // Real-SBC spatial acquisition: grid stamps, Player cache and Collection commit
 // together. The previous coordinates come from the same begin-update prefix.
 auto spatial_admission=admission;preserve_input(spatial_admission);spatial_admission.prefix.delta_bits=0;
 auto scene=spatial_scene(collision,check);const auto scene_before=scene;
 na::PlayerUpdateDispatchContext spatial_context{spatial_admission,stats,tables,templates,{}, {},&scene,0};
 auto spatial=make(spatial_seed(2971.5546875f));auto spatial_objects=spatial_collection();const auto spatial_before=save(spatial);const auto spatial_frame=value<na::Frame>(spatial.capture());
 const auto acquired=value<na::PlayerUpdateDispatchOutput>(spatial.prepare_and_dispatch(spatial_objects,spatial_context));
 const auto& effect=acquired.effects.dispatches[0].effects;
 check(acquired.effects.dispatch_count==1&&effect.spatial&&effect.spatial->height&&effect.spatial->cache_written&&!effect.transition,"Expected one complete uncached source query");
 check(effect.spatial->height->height_bits==0x453698e0&&effect.spatial->height->status==1&&effect.spatial->height->query.grid_updates.size()==1,"Original retained height identity changed");
 check(scene.grid_states[0].counter==1&&scene.grid_states[0].pair_marks==effect.spatial->height->query.grid_updates[0].state.pair_marks&&spatial.shared_fields().pw.field6a4_bits==0x453698e0&&spatial.shared_fields().pw.field6a8==1,"Grid/cache did not publish together");
 check(spatial_objects.data[0].field349==1&&scene.instances.size()==scene_before.instances.size(),"Third owner/Scene structure changed");pose_equal(spatial.state(),spatial_before.state);frame_equal(acquired.frame,spatial_frame);
 // Explicit next whole-prefix call invalidates the cache once and queries the
 // current grid stamps. No Player recreation and no independent grid commit.
 const auto next=value<na::PlayerUpdateDispatchOutput>(spatial.prepare_and_dispatch(spatial_objects,spatial_context));
 check(next.effects.dispatch_count==1&&next.effects.dispatches[0].effects.spatial->height&&scene.grid_states[0].counter==2,"Persistent grid counter was reset/replayed");
 auto missing_context=spatial_context;missing_context.spatial_scene=nullptr;const auto current=save(spatial);const auto current_objects=spatial_objects;
 const auto missing=value<na::PlayerError>(spatial.prepare_and_dispatch(spatial_objects,missing_context));check(dispatch_error(missing).phase==ud::Phase::first_dispatch,"Unknown scene was treated as empty");unchanged(spatial,current);check(spatial_objects==current_objects&&scene.grid_states[0].counter==2,"Unknown scene published a prefix");
 // A spatial miss still owns a counter/stamp update, without claiming a hit
 // or a cache write. Publication must not be conditioned on query.hit.
 auto miss_scene=spatial_scene(collision,check);auto miss_context=spatial_context;miss_context.spatial_scene=&miss_scene;
 auto miss_player=make(spatial_seed(2000));auto miss_objects=spatial_collection();
 const auto miss_out=value<na::PlayerUpdateDispatchOutput>(miss_player.prepare_and_dispatch(miss_objects,miss_context));
 const auto& missed=miss_out.effects.dispatches[0].effects.spatial;
 check(missed&&missed->height&&!missed->height->query.hit&&missed->height->status==0&&!missed->cache_written&&miss_player.shared_fields().pw.field6a8==0,"Spatial miss manufactured a cached hit");
 check(miss_scene.grid_states[0].counter==1&&missed->height->query.grid_updates.size()==1,"Spatial miss discarded stamp transaction");
 // Slot count is not the pair-mark word budget: an empty pair set can
 // still produce a counter update. Resource validates this mutated fixture.
 const auto empty_resource=zero_pair_resource(collision);check(empty_resource.pairs().empty(),"Zero-pair boundary did not derive empty pairs");
 auto empty_scene=spatial_scene(empty_resource,check);auto empty_context=spatial_context;empty_context.spatial_scene=&empty_scene;empty_context.query_limits.runtime_words=0;
 auto empty_player=make(spatial_seed(2971.5546875f));auto empty_objects=spatial_collection();
 const auto empty_output=value<na::PlayerUpdateDispatchOutput>(empty_player.prepare_and_dispatch(empty_objects,empty_context));
 check(empty_output.effects.dispatches[0].effects.spatial->height->query.grid_updates.size()==1&&empty_scene.grid_states[0].counter==1&&empty_scene.grid_states[0].pair_marks.empty(),"Counter-only update consumed nonexistent mark words");
 // This height makes the original first handler transition to unsupported
 // state2. A query was possible, but no Player/Collection/Scene is published.
 auto reject_scene=spatial_scene(collision,check);auto reject_context=spatial_context;reject_context.spatial_scene=&reject_scene;
 auto reject=make(spatial_seed(3021.5546875f));auto reject_objects=spatial_collection();const auto reject_before=save(reject);const auto reject_grid=reject_scene;const auto reject_collection=reject_objects;
 const auto rejected=value<na::PlayerError>(reject.prepare_and_dispatch(reject_objects,reject_context));check(dispatch_error(rejected).phase==ud::Phase::second_dispatch,"Expected rejection after successful spatial query");unchanged(reject,reject_before);check(reject_objects==reject_collection,"Spatial second failure published objects");grid_equal(reject_scene,reject_grid);
 // Fault each allocation through a verified renderer failure on a path that
 // changes all three owners. The same Player retries with exactly one query.
 auto measured_scene=spatial_scene(collision,check);auto measured_context=spatial_context;measured_context.spatial_scene=&measured_scene;
 auto measured=make(spatial_seed(2971.5546875f));auto measured_objects=spatial_collection();std::optional<na::PlayerResult<na::PlayerUpdateDispatchOutput>> measured_result;std::size_t extent=0;
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_dispatch(measured_objects,measured_context));extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerUpdateDispatchOutput>(*measured_result)&&extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=extent;++ordinal){
  auto fail_scene=spatial_scene(collision,check);auto fail_context=spatial_context;fail_context.spatial_scene=&fail_scene;
  auto subject=make(spatial_seed(2971.5546875f));auto objects=spatial_collection();const auto saved=save(subject);const auto object_before=objects;const auto grid_before=fail_scene;
  const auto frame=value<na::Frame>(subject.capture());std::optional<na::PlayerResult<na::PlayerUpdateDispatchOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(subject.prepare_and_dispatch(objects,fail_context));}
  const auto failure=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(failure.code==na::PlayerErrorCode::capacity||failure.code==na::PlayerErrorCode::driver||failure.code==na::PlayerErrorCode::render,"Unexpected allocation error category");
  unchanged(subject,saved);check(objects==object_before,"Allocation failure published objects");grid_equal(fail_scene,grid_before);frame_equal(value<na::Frame>(subject.capture()),frame);
  const auto retry=value<na::PlayerUpdateDispatchOutput>(subject.prepare_and_dispatch(objects,fail_context));
  check(fail_scene.grid_states[0].counter==1&&objects.data[0].field349==1&&subject.shared_fields().pw.field6a8==1&&subject.completed_steps()==0,"Same-Player retry lost atomic owners");frame_equal(retry.frame,frame);
  if(failure.code==na::PlayerErrorCode::render){check(failure.render&&failure.render->code==na::ErrorCode::capacity,"Renderer capacity cause lost");first_render_failure=ordinal;break;}
 }
 check(first_render_failure&&allocation_failures==first_render_failure,"Actual render failure was not exercised");
 check(picosha2::hash256_hex_string(record)==record_sha&&picosha2::hash256_hex_string(caps)==caps_sha,"Borrowed resource bytes changed");
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
