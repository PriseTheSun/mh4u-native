// Persistent Player composition, with explicit diagnostic context and source-backed
// owners. This is not a constructor/scheduler oracle or a physical input mapping.
#include "player_actor.hpp"
#include "test_support/player_actor_state.hpp"
#include <bit>
#include <fstream>
#include <cstdlib>
#include <limits>
#include <new>
#ifdef _WIN32
#include <malloc.h>
#endif
#include <iostream>
#include <stdexcept>
// Test-only, thread-local one-shot allocation failure. Runtime sources have no
// test hook. Only operator-new allocations in the guarded invocation are counted.
namespace allocation_probe {
thread_local bool enabled=false;
thread_local std::size_t calls=0,fail_at=std::numeric_limits<std::size_t>::max();
void count(){if(enabled&&++calls==fail_at){enabled=false;throw std::bad_alloc{};}}
void* plain(std::size_t n){count();if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc{};}
void* aligned(std::size_t n,std::size_t alignment){count();void* p=nullptr;
#ifdef _WIN32
 p=_aligned_malloc(n?n:1,alignment);
#else
 if(posix_memalign(&p,alignment,n?n:1)!=0)p=nullptr;
#endif
 if(p)return p;throw std::bad_alloc{};
}
void release_aligned(void* p)noexcept{
#ifdef _WIN32
 _aligned_free(p);
#else
 std::free(p);
#endif
}
struct Scope{explicit Scope(std::size_t ordinal=std::numeric_limits<std::size_t>::max()){calls=0;fail_at=ordinal;enabled=true;}~Scope(){enabled=false;}};
}
void* operator new(std::size_t n){return allocation_probe::plain(n);}
void* operator new[](std::size_t n){return allocation_probe::plain(n);}
void operator delete(void* p)noexcept{std::free(p);}
void operator delete[](void* p)noexcept{std::free(p);}
void operator delete(void* p,std::size_t)noexcept{std::free(p);}
void operator delete[](void* p,std::size_t)noexcept{std::free(p);}
void* operator new(std::size_t n,std::align_val_t a){return allocation_probe::aligned(n,static_cast<std::size_t>(a));}
void* operator new[](std::size_t n,std::align_val_t a){return allocation_probe::aligned(n,static_cast<std::size_t>(a));}
void operator delete(void* p,std::align_val_t)noexcept{allocation_probe::release_aligned(p);}
void operator delete[](void* p,std::align_val_t)noexcept{allocation_probe::release_aligned(p);}
void operator delete(void* p,std::size_t,std::align_val_t)noexcept{allocation_probe::release_aligned(p);}
void operator delete[](void* p,std::size_t,std::align_val_t)noexcept{allocation_probe::release_aligned(p);}
namespace {
namespace na=mh4u::native_actor;namespace pa=mh4u::player_animation;
namespace pt=mh4u::player_transition;namespace ad=mh4u::player_update_admission;
namespace oq=mh4u::player_object_query;namespace wl=mh4u::player_world_lookup;
namespace jp=mh4u::joint_pose;using U=std::uint32_t;
constexpr U one=0x3f800000;constexpr jp::Bits4 identity{0,0,0,one};std::size_t checks=0,allocation_failures=0,first_render_failure=0;
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
U bits(float f){return std::bit_cast<U>(f);}
template<class T,class...V>T value(std::variant<V...> v){if(auto* p=std::get_if<T>(&v))return std::move(*p);if constexpr((std::is_same_v<V,na::PlayerError>||...))throw std::runtime_error(std::get<na::PlayerError>(v).message);throw std::runtime_error("Unexpected variant");}
using mh4u::test_player_actor_state::state_equal;
void frame_equal(const na::Frame& a,const na::Frame& b){
 check(a.tick==b.tick&&bits(a.source_frame)==bits(b.source_frame)&&a.event_masks==b.event_masks&&a.ended==b.ended,"Admission advanced frame metadata");
 check(a.scene.global_transforms==b.scene.global_transforms&&a.palettes==b.palettes,"Admission changed pose/palettes");
 check(a.scene.mesh.triangles.size()==b.scene.mesh.triangles.size()&&a.scene.draws.size()==b.scene.draws.size(),"Geometry extent changed");
 for(std::size_t i=0;i<a.scene.mesh.triangles.size();++i){const auto& p=a.scene.mesh.triangles[i];const auto& q=b.scene.mesh.triangles[i];
  check(std::array{bits(p.position.x),bits(p.position.y),bits(p.position.z),bits(p.normal.x),bits(p.normal.y),bits(p.normal.z),bits(p.uv.x),bits(p.uv.y)}==std::array{bits(q.position.x),bits(q.position.y),bits(q.position.z),bits(q.normal.x),bits(q.normal.y),bits(q.normal.z),bits(q.uv.x),bits(q.uv.y)},"Admission changed skinned geometry");}
 for(std::size_t i=0;i<a.scene.draws.size();++i){const auto& p=a.scene.draws[i];const auto& q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Draw identity changed");}
}
struct Saved {pa::State state;mh4u::player_fields::Fields fields;mh4u::player_state1::Fields state1;mh4u::player_position_bridge::Fields position;std::uint64_t steps;};
Saved save(const na::Player& p){return {p.state(),p.shared_fields(),p.state1_fields(),p.position_fields(),p.completed_steps()};}
void unchanged(const na::Player& p,const Saved& s){check(state_equal(p.state(),s.state)&&p.shared_fields()==s.fields&&p.state1_fields()==s.state1&&p.position_fields()==s.position&&p.completed_steps()==s.steps,"Admission failure published Player prefix");}
na::PlayerInitial seed(){
 na::PlayerInitial x{};x.snapshots.resize(19);x.model.rotation=x.model.rotation_delta=identity;x.model.scale=x.model.null_scale={one,one,one,0};
 for(auto& l:x.layers){l.animation.tracks.joints.resize(19);l.animation.layer.speed=1;l.animation.layer.flags=2;l.animation.prediction_rotation=identity;}
 x.layers[0].animation.layer.end_frame=30;x.layers[0].animation.layer.frame=5.25f;x.layers[0].animation.layer.prepared_frame=6.5f;x.layers[1].animation.layer.weight=.375f;
 x.controller.motion.fieldsd48[4]=one;x.controller.motion.fieldsd48[5]=bits(30);
 for(U i=2;i<8;++i){x.layers[i].animation.layer.frame=std::bit_cast<float>(0x7fc00001U+i);x.layers[i].rotation_98={i,0xffffffff,0x80000000,0x7f800000};}
 auto& p=x.controller.player;auto& f=x.shared_fields;
 p.global.field2b=2;p.global.field2f=0;p.global.field5d=0;p.global.field22=0x6a31;p.global.field20=0xc541;p.global.fieldd72=0;
 p.pbw.field30=0;p.pbw.field33=0;p.pbw.field371=1;p.pbw.field372=2;p.selection_pbw.field9=0;p.selection_pbw.field6fa=0x91;p.pw.fielde8=4;p.pw.fieldec=0x80060001;
 f.pbw.field10_bits=f.pbw.field14_bits=f.pbw.field18_bits=0;f.pbw.field54_bits=0;f.pw.field13c_bits=one;f.pw.field1f8=0x1000;f.pw.field6a8=255;f.pw.field6a4_bits=0x7fc12345;
 f.pw.field159=0xa5;f.pw.field15a=0x5a;f.pw.field158=0x91;f.pw.field0a=1;f.pw.field1b8_bits=0x7fa12345;f.pw.field1bc_bits=0xff800000;
 f.pw.field31c=0xffff;f.pw.field224=0;f.pw.field238=0;f.pw.field324_bits=0;
 x.state1_fields.field50_bits=0x7fc12345;x.position_fields.fieldd30_bits=0xff812345;return x;
}
na::PlayerBanks banks(){na::PlayerBanks b{};b[0]=na::MotionSource{"c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"};return b;}
oq::Collection collection(bool positive=false){
 oq::Collection c;c.slots.fill(oq::Absent{});c.slots[0]=oq::ObjectIndex{0};c.slots[1]=oq::ObjectIndex{1};c.objects[0].data=c.objects[1].data=oq::DataIndex{0};
 auto& d=c.data[0];d.field140=1;d.field1b4=1;d.field14c=0;d.field290=1;d.field349=0;d.position_bits={0,{},bits(positive?-10.0f:10.0f)};
 d.field78b8_bits=0x7fa54321;d.field580=oq::OverrideIndex{0xffffffff};return c;
}
void run(const char* recipe,const pt::Constants& constants,const mh4u::player_policy_table::Table& table){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected seven-piece19joint source");
 const auto make=[&](const na::PlayerInitial& x){return value<na::Player>(na::Player::from_rest(actor,x,constants,banks()));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters context;context.prefix={{},one,{&world,1}};context.virtual120=ad::Virtual120::uplayer_b3dd60;
 auto player=make(seed());const auto original=save(player);const auto frame=value<na::Frame>(player.capture());const auto presentation=player.presentation();auto objects=collection();const auto objects_before=objects;
 auto result=value<na::PlayerAdmissionOutput>(player.prepare_to_admission(objects,context));
 check(result.effects.scan&&result.effects.scan->mask_stores==1&&result.effects.scan->direction_queries==1&&result.effects.scan->object_queries==2&&result.effects.scan->slots_examined==32,"First alias scan lost original false/store semantics");
 check(result.effects.virtual120_returned&&result.effects.local_override&&!result.effects.policy&&!result.effects.transition,"Explicit local override entered another branch");
 auto expected_objects=objects_before;expected_objects.data[0].field349=1;check(objects==expected_objects,"Only canonical D349 should change");
 check(player.shared_fields().pw.field159==0&&player.shared_fields().pw.field15a==0xa5&&player.shared_fields().pw.field158==0,"Admission latches mismatch");
 check(player.state().controller.player.pw.fieldec==0x80000001&&player.shared_fields().pw.field1b8_bits==bits(4.0f)&&player.shared_fields().pw.field1bc_bits==bits(4.0f),"Caller source stores missing");
 check(player.shared_fields().pbw.field54_bits==one&&player.shared_fields().pw.field6a8==0&&player.shared_fields().pw.field6a4_bits==0x7fc12345&&player.shared_fields().pw.field5a8==0,"Prefix/cache ownership lost");
 check(player.state().controller.player.global==original.state.controller.player.global&&player.state1_fields()==original.state1&&player.position_fields()==original.position&&player.presentation()==presentation&&player.completed_steps()==0,"Admission changed another owner");
 frame_equal(result.frame,frame);
 const auto after_first=objects;result=value<na::PlayerAdmissionOutput>(player.prepare_to_admission(objects,context));
 check(result.effects.scan&&result.effects.scan->mask_stores==0&&result.effects.scan->direction_queries==0&&objects==after_first,"Second invocation did not reuse aliased D349");
 check(player.shared_fields().pw.field15a==0&&player.shared_fields().pbw.field54_bits==bits(2.0f),"Persistent Player was recreated or prefix repeated incorrectly");frame_equal(result.frame,frame);
 auto positive=make(seed());auto positive_objects=collection(true);const auto accepted=value<na::PlayerAdmissionOutput>(positive.prepare_to_admission(positive_objects,context));
 check(accepted.effects.scan&&accepted.effects.scan->slots_examined==1&&accepted.effects.scan->mask_stores==1&&positive.shared_fields().pw.field159==1&&positive.shared_fields().pw.field158==0x91,"Accepted first query or untouched latch changed");frame_equal(accepted.frame,frame);
 // All failures below occur after prospective prefix writes. The late ones
 // also follow a prospective D349 mutation; neither owner may commit alone.
 for(unsigned which=0;which<4;++which){
  auto initial=seed();auto c=context;auto manager=collection();
  if(which==0)manager.slots[2]=oq::Unknown{};
  if(which==1)c.virtual120.reset();
  if(which==2)initial.shared_fields.pw.field0a.reset();
  if(which==3)manager.data[0].position_bits[0].reset();
  auto subject=make(initial);const auto before=save(subject);const auto collection_before=manager;
  const auto error=value<na::PlayerError>(subject.prepare_to_admission(manager,c));
  check(error.code==na::PlayerErrorCode::update_admission&&error.update_admission,"Admission typed cause lost");
  const auto phase=which==1?ad::Phase::dispatch:which==2?ad::Phase::caller:ad::Phase::scan;
  check(error.update_admission->phase==phase,"Admission failure phase changed");unchanged(subject,before);check(manager==collection_before,"Late Player error published Collection candidate");
  frame_equal(value<na::Frame>(subject.capture()),frame);
  if(which!=2){if(which==0)manager.slots[2]=oq::Absent{};if(which==1)c.virtual120=context.virtual120;if(which==3)manager.data[0].position_bits[0]=0;
   const auto retry=value<na::PlayerAdmissionOutput>(subject.prepare_to_admission(manager,c));check(retry.effects.scan&&retry.effects.scan->mask_stores==1&&subject.shared_fields().pbw.field54_bits==one,"Rollback affected valid retry");frame_equal(retry.frame,frame);}
 }
 // Exercise the real35-record policy owner after pre-policy returnsfalse.
 // The table and explicit seed are distinct from a claimed constructor state.
 auto policy_seed=seed();policy_seed.shared_fields.pw.field0a=0;
 policy_seed.controller.player.pbw.field372=1234;
 policy_seed.controller.player.pw.policy=mh4u::player_policy_table::install(table,0);
 policy_seed.shared_fields.pw.field7fd=policy_seed.shared_fields.pw.field7f1=policy_seed.shared_fields.pw.field7f4=0;
 policy_seed.shared_fields.pw.field188=0;policy_seed.shared_fields.pbw.field93ee=policy_seed.shared_fields.pbw.field6fb=0;policy_seed.shared_fields.pbw.field478=0;
 auto policy_context=context;policy_context.global_c8c=0;policy_context.prefix.filter.manager.ad24=0;
 auto policy_player=make(policy_seed);auto policy_objects=collection();
 const auto policy=value<na::PlayerAdmissionOutput>(policy_player.prepare_to_admission(policy_objects,policy_context));
 check(policy.effects.pre_policy_returned==false&&policy.effects.policy&&policy.effects.policy->visited_count==35&&policy.effects.policy->visited[34]==34,"Real policy did not traverse35 records after false pre-policy");
 check(!policy.effects.transition&&policy.effects.policy&&!policy.effects.policy->returned&&policy_player.state().controller.player.pbw.field372==1234&&policy_objects.data[0].field349==1,"Zero-magnitude policy manufactured a transition");frame_equal(policy.frame,frame);
 // Explicit source-channel magnitude50 is restored by the real input-filter
 // owner after its clear; no SDL/device correspondence is asserted.
 auto moving_seed=policy_seed;moving_seed.shared_fields.pw.field13c_bits=0;moving_seed.shared_fields.pbw.field464_bits=one;
 moving_seed.shared_fields.pw.field9dc=moving_seed.shared_fields.pw.field9de=moving_seed.shared_fields.pw.field9dd=0;
 auto moving_context=policy_context;auto& filter=moving_context.prefix.filter;
 filter.mode2=mh4u::player_input_filter::Absent{};filter.manager.ad24=0;filter.manager.ad50=filter.manager.ad64=0;
 filter.byte108b9b8=filter.byte1083850=filter.byte109bd29=0;filter.pad={0,0,0,0,0,50,0};moving_context.reference=mh4u::player_reference::Absent{};
 auto moving_player=make(moving_seed);auto moving_objects=collection();
 const auto moved=value<na::PlayerAdmissionOutput>(moving_player.prepare_to_admission(moving_objects,moving_context));
 check(moved.effects.filter&&moved.effects.filter->restored&&moved.effects.policy&&moved.effects.policy->visited_count==35&&moved.effects.policy->visited[34]==34,"Restored source channel did not reach final real policy record");
 check(moved.effects.transition&&moved.effects.transition->arguments.state_arg==1&&moved.effects.transition->arguments.action_arg==9&&moving_player.state().controller.player.pbw.field372==9&&moving_objects.data[0].field349==1,"Policy/controller and Collection did not commit together");frame_equal(moved.frame,frame);
 auto missing_policy_seed=policy_seed;missing_policy_seed.controller.player.pw.policy.table=mh4u::player_policy_table::Unknown{};
 auto missing_policy=make(missing_policy_seed);const auto missing_before=save(missing_policy);auto missing_objects=collection();const auto missing_objects_before=missing_objects;
 const auto policy_error=value<na::PlayerError>(missing_policy.prepare_to_admission(missing_objects,policy_context));
 check(policy_error.update_admission&&policy_error.update_admission->phase==ad::Phase::policy,"Late Unknown table lost policy cause");unchanged(missing_policy,missing_before);check(missing_objects==missing_objects_before,"Late policy error published scan store");
 // Provider tables are explicit borrowed identities, never a preselected
 // object/radius. Root4 andRoot9 share D7; Object0 is present but disabled.
 namespace oi=mh4u::player_object_interaction;
 std::array<oi::Link<oi::EntryIndex>,2> provider_table{oq::Unknown{},oi::EntryIndex{0}};
 std::array<oi::Entry,1> entries{{{oi::RecordIndex{0}}}};
 std::array<oi::Record,1> records{{{oi::ParameterIndex{0},oi::ParameterIndex{1}}}};
 std::array<oi::Parameters,2> parameters{{{bits(2),{}},{ {},bits(10)}}};
 const oi::Registry registry{provider_table,entries,records,parameters,{}};
 auto interaction_seed=seed();interaction_seed.shared_fields.pw.field0a=0;interaction_seed.shared_fields.pw.field324_bits=one;
 for(std::size_t i=0;i<7;++i){auto& record=interaction_seed.shared_fields.pbw.linked_records[i];record.present=(i%2)!=0;if(i%2)record.flag=0xa5;}
 oq::Collection targets;targets.slots.fill(oq::Absent{});targets.slots[0]=oq::ObjectIndex{0};targets.objects[0].data=oq::DataIndex{0};targets.data[0].field140=0;
 targets.slots[5]=oq::ObjectIndex{4};targets.slots[6]=oq::ObjectIndex{9};targets.objects[4].data=targets.objects[9].data=oq::DataIndex{7};
 auto& target=targets.data[7];target=collection().data[0];target.position_bits[1]=0;target.field78b8_bits=one;target.field78e5=0;target.field278=1;target.field27c=0;target.field1ca=10;target.field1cb=187;target.field1c0=1;target.field1bc_bits=one;target.field580=oq::Absent{};target.field1c4=0x9123;
 auto interaction_context=context;interaction_context.resources=&registry;
 auto interaction_player=make(interaction_seed);const auto targets_before=targets;
 const auto interacted=value<na::PlayerAdmissionOutput>(interaction_player.prepare_to_admission(targets,interaction_context));
 check(interacted.effects.interaction&&interacted.effects.interaction->selected==oq::ObjectIndex{4}&&interacted.effects.interaction->effects.provider_queries==2&&interacted.effects.cleared_target,"Interaction bypassed provider/root identity");
 check(interacted.effects.transition&&interacted.effects.transition->arguments.state_arg==2&&interacted.effects.transition->arguments.action_arg==25&&interaction_player.shared_fields().pw.field31c==0x9123,"ReloadD1c4 or direct transition lost");
 check(interaction_player.state().controller.player.selection_pbw.field376==1&&interaction_player.state().controller.player.selection_pbw.field6fa==0,"Direct transition canonical flags lost");
 for(std::size_t i=0;i<7;++i){const auto& record=interaction_player.shared_fields().pbw.linked_records[i];check(record.present==interaction_seed.shared_fields.pbw.linked_records[i].present&&record.flag==(i%2?std::optional<std::uint8_t>{1}:std::nullopt),"Seven conditional linked records flattened");}
 check(targets.data[7].field349==1&&targets.objects==targets_before.objects&&targets.slots==targets_before.slots,"Interaction replaced shared identities");frame_equal(interacted.frame,frame);
 // Error after provider lookup/clear-prefix cannot publish either owner.
 auto invalid_targets=targets_before;invalid_targets.data[7].field1c4.reset();const auto invalid_before=invalid_targets;auto failed_interaction=make(interaction_seed);const auto failed_before=save(failed_interaction);
 const auto interaction_error=value<na::PlayerError>(failed_interaction.prepare_to_admission(invalid_targets,interaction_context));
 check(interaction_error.update_admission&&interaction_error.update_admission->phase==ad::Phase::clear_target&&interaction_error.update_admission->field=="D1c4","Late target reload cause lost");unchanged(failed_interaction,failed_before);check(invalid_targets==invalid_before,"Target reload failure published scan/clear prefix");
 invalid_targets.data[7].field1c4=0x9123;const auto interaction_retry=value<na::PlayerAdmissionOutput>(failed_interaction.prepare_to_admission(invalid_targets,interaction_context));
 check(interaction_retry.effects.transition&&failed_interaction.shared_fields().pbw.field54_bits==one&&invalid_targets.data[7].field349==1,"SamePlayer interaction retry changed prefix");frame_equal(interaction_retry.frame,frame);
 // Determine this concrete success path's allocation extent, then fail each
 // ordinal until the first typed render failure. Every failed call is followed
 // by a valid retry on that SAME Player and external Collection.
 auto measured=make(seed());auto measured_objects=collection();std::optional<na::PlayerResult<na::PlayerAdmissionOutput>> measured_result;
 std::size_t allocation_extent=0;
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_to_admission(measured_objects,context));allocation_extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerAdmissionOutput>(*measured_result)&&allocation_extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=allocation_extent;++ordinal){
  auto subject=make(seed());auto manager=collection();const auto saved=save(subject);const auto manager_before=manager;
  std::optional<na::PlayerResult<na::PlayerAdmissionOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(subject.prepare_to_admission(manager,context));}
  const auto err=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(err.code==na::PlayerErrorCode::capacity||err.code==na::PlayerErrorCode::render,"Allocation error classification changed");
  unchanged(subject,saved);check(manager==manager_before,"Allocation failure committed only one owner");
  const auto retry=value<na::PlayerAdmissionOutput>(subject.prepare_to_admission(manager,context));
  check(retry.effects.scan&&retry.effects.scan->mask_stores==1&&manager.data[0].field349==1&&subject.shared_fields().pbw.field54_bits==one&&subject.completed_steps()==0,"Allocation failure affected same-Player retry");
  frame_equal(retry.frame,frame);
  if(err.code==na::PlayerErrorCode::render){check(err.render&&err.render->code==na::ErrorCode::capacity,"Render allocation cause lost");first_render_failure=ordinal;break;}
 }
 check(first_render_failure!=0&&allocation_failures==first_render_failure,"No actual render failure was exercised");

}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=value<na::Actor>(na::Actor::load(argv[2]));for(const auto& p:actor.source_files())std::cout<<p.generic_string()<<'\n';return 0;}
 check(argc==6,"Expected recipe constants constants-SHA table table-SHA");std::ifstream f(argv[2],std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==88,"Input extent");std::array<std::uint8_t,88> bytes{};f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(bytes.data()),88))&&f.peek()==std::char_traits<char>::eof(),"Input read");
 auto constants=value<pt::Constants>(pt::Constants::decode(bytes,{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));std::ifstream tf(argv[4],std::ios::binary|std::ios::ate);check(bool(tf)&&tf.tellg()>=0&&tf.tellg()<=static_cast<std::streamoff>(mh4u::player_policy_table::maximum_source_bytes),"Policy input extent");std::vector<std::uint8_t> raw(static_cast<std::size_t>(tf.tellg()));tf.seekg(0);check(bool(tf.read(reinterpret_cast<char*>(raw.data()),static_cast<std::streamsize>(raw.size())))&&tf.peek()==std::char_traits<char>::eof(),"Policy input read");
 auto table=value<mh4u::player_policy_table::Table>(mh4u::player_policy_table::Table::decode(raw,argv[5]));check(table.records().size()==35,"Expected source35 record table");run(argv[1],constants,table);std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"allocation_failures\":"<<allocation_failures<<",\"first_render_failure\":"<<first_render_failure<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
