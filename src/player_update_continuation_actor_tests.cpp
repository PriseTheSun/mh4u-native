// Persistent Player composition, with explicit diagnostic context and source-backed
// owners. This is not a constructor/scheduler oracle or a physical input mapping.
#include "player_actor.hpp"
#include "test_support/player_actor_state.hpp"
#include "test_support/player_update_actor_fixture.hpp"
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
namespace jp=mh4u::joint_pose;namespace pq=mh4u::player_parameter_query;
namespace us=mh4u::player_update_statistics;namespace uc=mh4u::player_update_continuation;using U=std::uint32_t;
constexpr U one=0x3f800000;std::size_t checks=0,allocation_failures=0,first_render_failure=0;
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
U bits(float f){return std::bit_cast<U>(f);}
template<class T,class...V>T value(std::variant<V...> v){if(auto* p=std::get_if<T>(&v))return std::move(*p);if constexpr((std::is_same_v<V,na::PlayerError>||...))throw std::runtime_error(std::get<na::PlayerError>(v).message);throw std::runtime_error("Unexpected variant");}
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
using mh4u::test_update_actor_fixture::continuation_seed;
void check_result(const na::Player& player,const na::PlayerDispatchOutput& result){
 check(result.statistics.local&&result.statistics.query_count==7,"Seven parameter queries were not consumed");
 constexpr std::array<U,7> selectors{6,0,1,2,3,4,5};
 for(std::size_t i=0;i<selectors.size();++i)check(result.statistics.queries[i].selector==selectors[i],"Parameter query order changed");
 check(result.statistics.queries[0].result==150&&result.statistics.queries[1].result==20,"Resource bytes were not queried");
 check(result.statistics.queries[0].effects.write_count==1&&result.statistics.queries[0].effects.writes[0]==pq::FlagWrite{7,0xffffffff,0xffffffbf},"First parameter mutation lost");
 check(result.statistics.queries[1].effects.write_count==1&&result.statistics.queries[1].effects.writes[0]==pq::FlagWrite{7,0xffffffbf,0xffffffbe},"Second parameter did not observe first mutation");
 const auto& fields=player.shared_fields();
 check(fields.pbw.field37c==150&&fields.pbw.field37e==21&&fields.pbw.field380_bits==bits(1.5f)&&fields.pbw.field384_bits==bits(21),"Caller parameter outputs changed");
 for(auto value:fields.pbw.fields388_bits)check(value==0,"Five parameter output changed");
 check(fields.pbw.words9404[7]==0xffffffbe&&fields.pbw.words9404[255]==0x7fa12345,"Mutable query words have another owner");
 check(fields.pbw.words3d8[18]==mh4u::player_input_storage::Word{0,0,0,0},"Input prefix did not clear its canonical byte owner");
 check(fields.pbw.descriptors[1][27]==0x91&&!fields.pbw.descriptors[1][26]&&!fields.pbw.words9404[254],"Unconsumed partial knowledge was collapsed");
 check(result.returns.first==uc::FirstReturn::local_positive_timer&&result.returns.second==uc::SecondReturn::disabled,"Explicit early-return paths changed");
}
void run(const char* recipe,const pt::Constants& constants,const std::array<std::uint8_t,42>& caps,const std::string& caps_sha){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected seven-piece19joint source");
 const auto make=[&](const na::PlayerInitial& x){return value<na::Player>(na::Player::from_rest(actor,x,constants,banks()));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters context;context.prefix={{},one,{&world,1}};context.virtual120=ad::Virtual120::uplayer_b3dd60;
 mh4u::test_update_actor_fixture::Parameters parameter_fixture(caps,caps_sha);
 const auto& record=parameter_fixture.record;const auto& record_sha=parameter_fixture.record_sha;
 auto& stats=parameter_fixture.context;
 const auto& bonuses=parameter_fixture.bonuses;
 auto player=make(continuation_seed());const auto original=save(player);const auto frame=value<na::Frame>(player.capture());auto objects=collection();const auto objects_before=objects;
 auto result=value<na::PlayerDispatchOutput>(player.prepare_to_dispatch(objects,context,stats));check_result(player,result);
 check(result.admission.scan&&result.admission.scan->mask_stores==1&&objects.data[0].field349==1,"Admission Collection effects were not committed");
 auto expected_objects=objects_before;expected_objects.data[0].field349=1;check(objects==expected_objects,"Continuation altered object identities");
 check(player.shared_fields().pw.field4fc==0xdeadbeef&&player.shared_fields().pw.field4f8==0&&player.shared_fields().pbw.field93ee==0&&player.shared_fields().pbw.field93e9==0,"Post-helper caller stores missing");
 check(player.state().controller.player.pw.fieldf8==0xb0000010&&player.state().controller.player.pw.fieldf0==0x10000001,"Caller bit29 transfer changed");
 check(player.state().controller.player.global==original.state.controller.player.global&&player.state1_fields()==original.state1&&player.position_fields()==original.position&&player.completed_steps()==0,"Continuation changed another owner or advanced clock");
 frame_equal(result.frame,frame);
 // Preserve both active and six inactive layers, including NaN/raw poison.
 auto without_controller=player.state();without_controller.controller=original.state.controller;
 check(state_equal(without_controller,original.state),"Continuation copied or advanced animation state");
 const auto first=objects;result=value<na::PlayerDispatchOutput>(player.prepare_to_dispatch(objects,context,stats));
 check(result.admission.scan&&result.admission.scan->mask_stores==0&&objects==first&&player.shared_fields().pbw.field54_bits==bits(2),"Persistent second call recreated an owner");
 check(player.shared_fields().pw.field4fc==0&&player.state().controller.player.pw.fieldf8==0x90000010,"Second caller checkpoint failed");
 check(result.statistics.queries[0].effects.writes[0].before==0xffffffbe&&result.statistics.queries[1].effects.writes[0].before==0xffffffbe,"Second call lost ordered parameter storage");frame_equal(result.frame,frame);
 // Root factories do not invent missing source context. All these errors are
 // late enough to follow prospective admission stores, including aliased D349.
 for(unsigned which=0;which<4;++which){
  auto initial=continuation_seed();auto parameters=stats;
  if(which==0)parameters.parameters.records24[7]=pq::Unknown{};
  if(which==1)initial.shared_fields.pw.fielda15.reset();
  if(which==2){initial.shared_fields.pw.fielda15=1;initial.shared_fields.pw.field294=-1;initial.shared_fields.pw.field296=2;initial.controller.player.selection_pbw.field6f9=1;}
  if(which==3)initial.shared_fields.pw.fields864_bits[4].reset();
  auto subject=make(initial);auto manager=collection();const auto saved=save(subject);const auto manager_before=manager;
  const auto err=value<na::PlayerError>(subject.prepare_to_dispatch(manager,context,parameters));
  check(err.code==na::PlayerErrorCode::update_continuation&&err.update_continuation,"Typed continuation cause lost");
  check(err.update_continuation->phase==((which==1||which==2)?uc::Phase::second_helper:uc::Phase::statistics),"Late error phase changed");
  unchanged(subject,saved);check(manager==manager_before,"Late error committed one owner");frame_equal(value<na::Frame>(subject.capture()),frame);
  if(which==0){parameters.parameters.records24[7]=stats.parameters.records24[7];const auto retry=value<na::PlayerDispatchOutput>(subject.prepare_to_dispatch(manager,context,parameters));check_result(subject,retry);check(subject.shared_fields().pbw.field54_bits==one&&manager.data[0].field349==1,"Same-Player context retry lost transaction");frame_equal(retry.frame,frame);}
 }
 // Both additional source-return routes and a dead/raw copy remain explicit.
 for(unsigned which=0;which<3;++which){auto initial=continuation_seed();initial.shared_fields.pw.field4f8.reset();
  initial.shared_fields.pw.fielda15=1;initial.shared_fields.pw.field294=-2;initial.shared_fields.pw.field296=which==0?-3:1;
  if(which!=0)initial.controller.player.selection_pbw.field6f9=0;
  if(which==2)initial.controller.player.pbw.field371=8;
  auto subject=make(initial);auto manager=collection();const auto output=value<na::PlayerDispatchOutput>(subject.prepare_to_dispatch(manager,context,stats));
  check(output.returns.second==(which==0?uc::SecondReturn::local_ordered_values:uc::SecondReturn::local_zero_flag),"Second helper source return changed");
  check(output.returns.first==(which==2?uc::FirstReturn::state8:uc::FirstReturn::local_positive_timer),"First helper source return changed");
  check(!subject.shared_fields().pw.field4fc&&subject.shared_fields().pw.field4f8==0,"Unknown raw copy was invented");frame_equal(output.frame,frame);
 }
 // Fail each concrete allocation until the first typed renderer failure. The
 // input query buffers stay alive throughout; no runtime test hooks exist.
 auto measured=make(continuation_seed());auto measured_objects=collection();std::optional<na::PlayerResult<na::PlayerDispatchOutput>> measured_result;std::size_t extent=0;
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_to_dispatch(measured_objects,context,stats));extent=allocation_probe::calls;}
 check(std::holds_alternative<na::PlayerDispatchOutput>(*measured_result)&&extent>0,"Allocation measurement failed");
 for(std::size_t ordinal=1;ordinal<=extent;++ordinal){auto subject=make(continuation_seed());auto manager=collection();const auto saved=save(subject);const auto manager_before=manager;
  std::optional<na::PlayerResult<na::PlayerDispatchOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(subject.prepare_to_dispatch(manager,context,stats));}
  const auto err=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(err.code==na::PlayerErrorCode::capacity||err.code==na::PlayerErrorCode::render,"Allocation error classification changed");
  unchanged(subject,saved);check(manager==manager_before,"Allocation failure committed one owner");
  const auto retry=value<na::PlayerDispatchOutput>(subject.prepare_to_dispatch(manager,context,stats));check_result(subject,retry);
  check(manager.data[0].field349==1&&subject.shared_fields().pbw.field54_bits==one&&subject.completed_steps()==0,"Allocation retry lost pending state");frame_equal(retry.frame,frame);
  if(err.code==na::PlayerErrorCode::render){check(err.render&&err.render->code==na::ErrorCode::capacity,"Render allocation cause lost");first_render_failure=ordinal;break;}
 }
 check(first_render_failure!=0&&allocation_failures==first_render_failure,"No actual renderer failure was exercised");
 check(record[8]==150&&record[10]==20&&picosha2::hash256_hex_string(record)==record_sha,"Borrowed parameter source was mutated");
 check(picosha2::hash256_hex_string(caps)==caps_sha,"Borrowed cap source was mutated");
 for(const auto& bonus:bonuses)check(bonus.id==0&&!bonus.value,"Borrowed bonus source was mutated");
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=value<na::Actor>(na::Actor::load(argv[2]));for(const auto& p:actor.source_files())std::cout<<p.generic_string()<<'\n';return 0;}
 check(argc==6,"Expected recipe constants constants-SHA caps caps-SHA");std::ifstream f(argv[2],std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==88,"Input extent");std::array<std::uint8_t,88> bytes{};f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(bytes.data()),88))&&f.peek()==std::char_traits<char>::eof(),"Input read");
 auto constants=value<pt::Constants>(pt::Constants::decode(bytes,{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));
 std::ifstream cf(argv[4],std::ios::binary|std::ios::ate);check(bool(cf)&&cf.tellg()==42,"Caps input extent");std::array<std::uint8_t,42> caps{};cf.seekg(0);check(bool(cf.read(reinterpret_cast<char*>(caps.data()),42))&&cf.peek()==std::char_traits<char>::eof(),"Caps input read");
 check(picosha2::hash256_hex_string(caps)==argv[5],"Caps source hash");run(argv[1],constants,caps,argv[5]);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"allocation_failures\":"<<allocation_failures<<",\"first_render_failure\":"<<first_render_failure<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
