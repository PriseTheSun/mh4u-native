// Native composition tests. Raw state equality helpers match the retained actor tests;
// source block semantics use the canonical transient projection, not a second parser.
#include "player_actor.hpp"
#include "test_support/player_actor_state.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace {
namespace na=mh4u::native_actor;namespace pa=mh4u::player_animation;namespace mp=mh4u::motion_pending;
namespace pt=mh4u::player_transition;namespace jp=mh4u::joint_pose;namespace pf=mh4u::player_input_filter;
namespace block=mh4u::player_input_block;namespace storage=mh4u::player_input_storage;
using U=std::uint32_t;constexpr U one=0x3f800000;constexpr jp::Bits4 identity{0,0,0,one};
std::size_t checks{};
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
U bits(float f){return std::bit_cast<U>(f);}
template<class T,class...V>T value(std::variant<V...> r){if(auto* x=std::get_if<T>(&r))return std::move(*x);if constexpr((std::is_same_v<V,na::PlayerError>||...))throw std::runtime_error(std::get<na::PlayerError>(r).message);throw std::runtime_error("Unexpected variant");}
using mh4u::test_player_actor_state::state_equal;

void frame_equal(const na::Frame& a,const na::Frame& b){
 check(a.scene.global_transforms==b.scene.global_transforms&&a.palettes==b.palettes,"Pose/palette changed");
 check(a.scene.mesh.triangles.size()==b.scene.mesh.triangles.size()&&a.scene.draws.size()==b.scene.draws.size(),"Geometry extent changed");
 for(std::size_t i=0;i<a.scene.mesh.triangles.size();++i){const auto& p=a.scene.mesh.triangles[i];const auto& q=b.scene.mesh.triangles[i];
  check(std::array{bits(p.position.x),bits(p.position.y),bits(p.position.z),bits(p.normal.x),bits(p.normal.y),bits(p.normal.z),bits(p.uv.x),bits(p.uv.y)}==std::array{bits(q.position.x),bits(q.position.y),bits(q.position.z),bits(q.normal.x),bits(q.normal.y),bits(q.normal.z),bits(q.uv.x),bits(q.uv.y)},"Skinned vertex bits changed");}
 for(std::size_t i=0;i<a.scene.draws.size();++i){const auto& p=a.scene.draws[i];const auto& q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Draw identity changed");}
}
na::PlayerInitial seed(){
 na::PlayerInitial x{};x.snapshots.resize(19);x.model.rotation=x.model.rotation_delta=identity;x.model.scale=x.model.null_scale={one,one,one,0};
 for(auto& l:x.layers){l.animation.tracks.joints.resize(19);l.animation.layer.speed=1;l.animation.layer.flags=2;l.animation.prediction_rotation=identity;}
 x.layers[0].animation.layer.end_frame=30;x.layers[0].animation.layer.frame=5.25f;x.layers[0].animation.layer.prepared_frame=6.5f;
 x.layers[1].animation.layer.weight=.375f;x.controller.motion.fieldsd48[4]=one;x.controller.motion.fieldsd48[5]=bits(30);
 for(U i=2;i<8;++i){x.layers[i].animation.layer.frame=std::bit_cast<float>(0x7fc00001U+i);x.layers[i].rotation_98={i,0xffffffff,0x80000000,0x7f800000};}
 auto& p=x.controller.player;auto& f=x.shared_fields;
 p.global.field2b=2;p.pbw.field30=0;p.pbw.field33=0;p.pbw.field371=1;p.pbw.field372=2;p.global.fieldd72=0;
 p.pbw.angles={0x8123,0x4567};p.pbw.magnitudes={91,37};p.pbw.field3d4=0xdeadbeef;p.pbw.field451=0xa5;
 p.selection_pbw.field9=3;p.selection_pbw.field6fa=0x91;p.pw.fielde8=0x80000004U;
 f.pbw.field3a8=0x1234;f.pbw.field3ac=0xabcd;f.pbw.field3b8=0x4321;f.pbw.field3bc=0x5678;
 for(std::size_t i=0;i<f.pbw.words3d8.size();++i){f.pbw.words3d8[i][1]=std::uint8_t(i+7);if(i%2)f.pbw.words3d8[i][3]=0xa5;}
 for(std::size_t i=0;i<f.pbw.input_other.size();i+=3)f.pbw.input_other[i]=std::uint8_t(i+17);
 for(std::size_t i=0;i<storage::remaining_offsets.size();++i)if(storage::remaining_offsets[i]>=0x3cc&&storage::remaining_offsets[i]<0x3d0)f.pbw.input_other[i]=0xff;
 f.pbw.field54_bits=0x4b18967f;f.pbw.field10_bits=bits(1);f.pbw.field14_bits=bits(20);f.pbw.field18_bits=bits(-3);
 f.pbw.field6e8_bits=0x7fc12345;f.pbw.field6ec_bits=0x7f800000;f.pbw.field6f0_bits=0xff800000;f.pbw.field6f4_bits=0x80000000;
 f.pbw.field37a=0xff;f.pbw.field6c0=0x22;f.pbw.field6fc=0x33;f.pbw.field93f1=0x44;f.pbw.field464_bits=0;
 f.pw.field13c_bits=one;f.pw.field6a4_bits=0x7fc56789;f.pw.field6a8=0xff;f.pw.field6a9=0xa5;f.pw.field1f8=0x80001000;
 f.pw.field01=1;f.pw.field0b=2;f.pw.field589=3;f.pw.field9dc=f.pw.field9de=f.pw.field9dd=0;
 x.state1_fields.field50_bits=0x7fc12345;x.position_fields.fieldd30_bits=0xff812345;return x;
}
na::PlayerBanks banks(){na::PlayerBanks b{};b[0]=na::MotionSource{"c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"};return b;}
na::PlayerStep step_parameters(){na::PlayerStep p;p.delta_bits=one;p.templates={jp::Bits3{},jp::Bits3{},identity,jp::Bits4{}};p.restoration_scale=jp::Bits3{one,one,one};p.parent=jp::Null{};p.reference_basis=mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};p.reference_translation=identity;return p;}
pf::Context context(){pf::Context c;c.mode2=pf::Absent{};c.object20=0;c.object21=0;c.manager.ad24=0;c.manager.ad50=0;c.manager.ad64=0;c.byte108b9b8=0;c.byte1083850=0;c.byte109bd29=0;return c;}
void metadata(const na::Frame& a,const na::Frame& b){check(a.tick==b.tick&&bits(a.source_frame)==bits(b.source_frame)&&a.event_masks==b.event_masks&&a.ended==b.ended,"Preparation advanced clock");}
struct Saved {pa::State state;mh4u::player_fields::Fields shared;mh4u::player_state1::Fields state1;mh4u::player_position_bridge::Fields position;std::uint64_t steps;};
Saved save(const na::Player& p){return {p.state(),p.shared_fields(),p.state1_fields(),p.position_fields(),p.completed_steps()};}
void unchanged(const na::Player& p,const Saved& s){check(state_equal(p.state(),s.state)&&p.shared_fields()==s.shared&&p.state1_fields()==s.state1&&p.position_fields()==s.position&&p.completed_steps()==s.steps,"Failure or no-op published state");}
void expected_input(na::Player& p,const Saved& s,const block::Block& bytes){
 // Projection/publish are the existing canonical mapping owner. This checks
 // composition/alias ownership, not a second proof of that source layout.
 auto mapping=value<block::Output>(block::publish(bytes,s.state.controller.player,s.shared));auto expected=s.state;expected.controller.player=mapping.player;
 check(state_equal(p.state(),expected)&&p.shared_fields()==mapping.fields,"Input block did not publish through canonical owners");
 check(p.state1_fields()==s.state1&&p.position_fields()==s.position&&p.completed_steps()==s.steps,"Filter changed another owner");
 check(block::project(p.state().controller.player,p.shared_fields())==bytes,"Published input bytes mismatch");
}
void run(const char* recipe,const pt::Constants& constants){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected seven-piece19joint source");
 const auto make=[&](na::PlayerInitial x){return value<na::Player>(na::Player::from_rest(actor,x,constants,banks()));};
 auto player=make(seed());const auto presentation=player.presentation();
 const auto initial=value<na::Frame>(player.capture());check(initial.scene.mesh.source_positions==4507&&initial.scene.mesh.triangles.size()/3==5043&&initial.scene.draws.size()==20,"Source geometry changed");
 // An explicit prior step distinguishes preparation from an implicit clock
 // update or reset. No action request or original scheduler is presumed.
 value<na::PlayerOutput>(player.step(step_parameters()));check(player.completed_steps()==1,"Explicit step was not counted");
 const auto baseline=value<na::Frame>(player.capture());const auto before=save(player);
 const auto filtered=value<na::PlayerInputFilterOutput>(player.filter_input({}));
 check(filtered.effects==pf::Effects{true,true,false,false,false},"PositivePW13 clear effect mismatch");
 block::Block zeros{};zeros.fill(std::uint8_t{0});expected_input(player,before,zeros);
 check(player.state().controller.player.pbw.angles==std::array<std::uint16_t,2>{}&&player.state().controller.player.pbw.magnitudes==std::array<std::uint16_t,2>{}&&player.state().controller.player.pbw.field3d4==0&&player.state().controller.player.pbw.field451==0,"Existing channel aliases were shadowed");
 check(player.shared_fields().pbw.field54_bits==0x4b18967f&&player.shared_fields().pw.field6a8==255,"Filter changed accumulator/cache early");
 metadata(filtered.frame,baseline);frame_equal(filtered.frame,baseline);
 const auto after_filter=save(player);const auto accumulated=value<na::Frame>(player.update_accumulator(one));
 auto expected_fields=after_filter.shared;expected_fields.pbw.field54_bits=0;
 check(state_equal(player.state(),after_filter.state)&&player.shared_fields()==expected_fields&&player.completed_steps()==1,"Accumulator did not commit alone/clear threshold");
 metadata(accumulated,baseline);frame_equal(accumulated,baseline);
 const auto after_accumulator=save(player);const auto begun=value<na::Frame>(player.begin_update());auto expected_state=after_accumulator.state;
 expected_state.controller.player.selection_pbw.field6fa=0;expected_state.controller.player.pw.fielde8&=~4U;
 expected_fields=after_accumulator.shared;expected_fields.pbw.field6e8_bits=expected_fields.pbw.field10_bits;expected_fields.pbw.field6ec_bits=expected_fields.pbw.field14_bits;expected_fields.pbw.field6f0_bits=expected_fields.pbw.field18_bits;
 expected_fields.pbw.field6f4_bits=0;expected_fields.pbw.field37a=3;expected_fields.pbw.field93f1=0;expected_fields.pbw.field6fc=0;expected_fields.pbw.field6c0=0;
 expected_fields.pw.field0b=0;expected_fields.pw.field589=0;expected_fields.pw.field6a9=0;expected_fields.pw.field01=0;expected_fields.pw.field1f8=0x80000000;expected_fields.pw.field6a8=0;
 check(state_equal(player.state(),expected_state)&&player.shared_fields()==expected_fields&&player.state1_fields()==before.state1&&player.position_fields()==before.position&&player.completed_steps()==1,"Begin lost prior stage state or modified other owners");
 check(block::project(player.state().controller.player,player.shared_fields())==zeros&&player.shared_fields().pw.field6a4_bits==0x7fc56789&&player.presentation()==presentation,"Preparation replaced owner/height/input");
 metadata(begun,baseline);frame_equal(begun,baseline);
 // Early nonlocal exit neither consumes omitted context nor flattens partial
 // knowledge. The persistent block is projected only for the call.
 auto nonlocal_seed=seed();nonlocal_seed.controller.player.pbw.field30=3;nonlocal_seed.shared_fields.pw.field13c_bits.reset();
 auto nonlocal=make(nonlocal_seed);const auto nonlocal_before=save(nonlocal);const auto no_op=value<na::PlayerInputFilterOutput>(nonlocal.filter_input({}));
 check(no_op.effects==pf::Effects{},"Nonlocal call reported stores");unchanged(nonlocal,nonlocal_before);metadata(no_op.frame,initial);
 // Source suppression writes byte lanes inside shared words, while lane6 and
 // its separate word remain untouched. These patterns cannot be represented
 // honestly by optional whole words alone.
 auto partial_seed=seed();partial_seed.shared_fields.pw.field13c_bits=0;auto partial=make(partial_seed);auto partial_context=context();partial_context.byte108b9b8=1;
 const auto partial_before=save(partial);auto wanted=block::project(partial_before.state.controller.player,partial_before.shared);
 const auto zero=[&](unsigned offset,unsigned n){for(unsigned i=0;i<n;++i)wanted[offset-storage::first+i]=0;};
 zero(0x3a0,24);zero(0x452,1);zero(0x454,2);zero(0x458,8);
 for(unsigned i=0;i<4;++i)wanted[0x3cc-storage::first+i]=std::uint8_t(0xfffff5ffU>>(8*i));
 for(unsigned i=0;i<8;++i)if(i!=6){zero(0x406+i,1);zero(0x410+4*i,4);}zero(0x3d4,16);
 const auto suppressed=value<na::PlayerInputFilterOutput>(partial.filter_input(partial_context));
 check(suppressed.effects==pf::Effects{true,false,false,true,true},"Partial suppression source effects mismatch");expected_input(partial,partial_before,wanted);
 const auto bytes=block::project(partial.state().controller.player,partial.shared_fields());
 check(!bytes[0x40c-storage::first]&&bytes[0x407-storage::first]==0&&!bytes[0x404-storage::first]&&bytes[0x405-storage::first].has_value(),"Partial/skip-lane knowledge lost");
 check(partial.state().controller.player.pbw.magnitudes==partial_before.state.controller.player.pbw.magnitudes&&partial.state().controller.player.pbw.angles==partial_before.state.controller.player.pbw.angles,"Suppression wrote restored aliases outside its source range");
 metadata(suppressed.frame,initial);frame_equal(suppressed.frame,initial);
 // Unknowns after prospective clear/restore or partial byte writes must not
 // publish the candidate. Context omission remains different from known zero.
 for(unsigned which=0;which<4;++which){
  auto x=seed();auto c=context();std::string expected;
  if(which==0){x.shared_fields.pw.field13c_bits.reset();expected="PW13c";}
  if(which==1){x.controller.player.global.field2b=1;x.controller.player.global.field2f=0;x.shared_fields.pw.field13c_bits=0;c.object20=1;c.object21.reset();expected="fb5edc+21";}
  if(which==2){x.shared_fields.pw.field13c_bits=0;c.manager.ad24=5;c.pad={1,2,3,4,5,6,{}};expected="pad33a";}
  if(which==3){x.shared_fields.pw.field13c_bits=0;c.byte108b9b8=1;for(std::size_t i=0;i<storage::remaining_offsets.size();++i)if(storage::remaining_offsets[i]==0x3cd)x.shared_fields.pbw.input_other[i].reset();expected="PBW3cc";}
  auto p=make(x);const auto saved=save(p);const auto error=value<na::PlayerError>(p.filter_input(c));
  check(error.code==na::PlayerErrorCode::input_filter&&error.input_filter&&error.input_filter->field==expected,"Filter unknown cause lost");unchanged(p,saved);metadata(value<na::Frame>(p.capture()),initial);
 }
 // Stage calls are individually transactional. A later failure retains the
 // earlier successful stage, rather than pretending the caller requested one
 // atomic three-helper operation.
 auto missing_seed=seed();missing_seed.shared_fields.pbw.field10_bits.reset();auto missing=make(missing_seed);
 value<na::PlayerInputFilterOutput>(missing.filter_input({}));value<na::Frame>(missing.update_accumulator(one));const auto prefix=save(missing);
 auto missing_begin=value<na::PlayerError>(missing.begin_update());check(missing_begin.code==na::PlayerErrorCode::update_begin&&missing_begin.update_begin&&missing_begin.update_begin->field=="PBW10","Begin unknown cause lost");unchanged(missing,prefix);
 for(unsigned which=0;which<3;++which){auto x=seed();if(which==1)x.shared_fields.pbw.field54_bits.reset();if(which==2)x.shared_fields.pbw.field54_bits=0x7f7fffff;auto p=make(x);const auto saved=save(p);
  const auto delta=which==0?std::optional<U>{}:std::optional<U>{which==2?0x7f7fffffU:one};const auto err=value<na::PlayerError>(p.update_accumulator(delta));
  check(err.code==na::PlayerErrorCode::update_accumulator&&err.update_accumulator,"Accumulator error wrapper lost");
  if(which<2)check(err.update_accumulator->code==mh4u::player_update_accumulator::ErrorCode::unknown_field&&err.update_accumulator->field==(which==0?"player14":"PBW54"),"Accumulator unknown cause lost");
  else check(err.update_accumulator->cause&&err.update_accumulator->cause->code==mh4u::motion_transform_math::ErrorCode::numeric_overflow,"Accumulator overflow hidden by original reset");
  unchanged(p,saved);
 }
 // Filter executes only raw/bit operations here. Its candidate must still be
 // withheld if the subsequent shared renderer rejects the FP environment.
 auto late=make(seed());const auto late_before=save(late);struct Restore {int mode=std::fegetround();~Restore(){std::fesetround(mode);}} restore;
 check(std::fesetround(FE_DOWNWARD)==0,"Set late render failure");auto failed=late.filter_input({});check(std::fesetround(restore.mode)==0,"Restore late render environment");
 const auto late_error=value<na::PlayerError>(std::move(failed));check(late_error.code==na::PlayerErrorCode::render,"Filter failure did not reach render");unchanged(late,late_before);
 const auto retried=value<na::PlayerInputFilterOutput>(late.filter_input({}));expected_input(late,late_before,zeros);metadata(retried.frame,initial);frame_equal(retried.frame,initial);
 // The recovered caller prefix is a separate, atomic operation. Its successful
 // state agrees with the explicit stages above, plus its own PW5a8 publication.
 namespace wl=mh4u::player_world_lookup;namespace up=mh4u::player_update_prefix;
 wl::Manager world;world.head=wl::Index{0};auto& record=world.records[0];record.next=wl::Absent{};
 record.field19=1;record.field30=1;record.field61=0;record.field78=0;record.field2f=3;
 record.fields34={bits(1),bits(20),bits(-3)};record.field50_bits=bits(4);record.field4c_bits=bits(2);
 record.fields40={0x7fc12345,0x80000000,0xffffffff};record.field54=17;
 const wl::ResourceReference reference{std::string(64,'a'),44};record.field28=reference;
 up::Context preparation{{},one,{&world,1}};
 auto combined=make(seed());value<na::PlayerOutput>(combined.step(step_parameters()));
 const auto combined_before=save(combined);const auto world_before=world;
 auto prepared=value<na::PlayerPreparationOutput>(combined.prepare_update(preparation));
 auto final_fields=expected_fields;final_fields.pw.field5a8=17;
 check(state_equal(combined.state(),expected_state)&&combined.shared_fields()==final_fields&&combined.state1_fields()==before.state1&&combined.position_fields()==before.position&&combined.completed_steps()==1,"Atomic prefix differs from source stages");
 check(prepared.filter==pf::Effects{true,true,false,false,false}&&prepared.lookup.returned==17&&prepared.lookup.match&&prepared.lookup.match->index==0&&prepared.lookup.visited==1,"Atomic prefix lost returned effects");
 check(prepared.lookup.match->field28==wl::Field28{reference}&&prepared.lookup.match->vector34==std::array<U,4>{bits(1),bits(20),bits(-3),0}&&prepared.lookup.match->words40==std::array<U,3>{0x7fc12345,0x80000000,0xffffffff},"Copyout altered opaque identity/raw words");
 check(world==world_before,"Player mutated borrowed world");metadata(prepared.frame,baseline);frame_equal(prepared.frame,baseline);
 // A miss explicitly resets the canonical owner, without publishing stack
 // copyouts or mutating resources. Skipped record fields stay unknown.
 world.records[0].field19=0;world.records[0].field28.reset();
 const auto missed=value<na::PlayerPreparationOutput>(combined.prepare_update(preparation));
 check(missed.lookup.returned==0xffff&&!missed.lookup.match&&combined.shared_fields().pw.field5a8==0,"Caller miss did not clear stale PW5a8");
 check(combined.shared_fields().pbw.field54_bits==one,"Repeated prefix did not use committed accumulator");
 metadata(missed.frame,baseline);frame_equal(missed.frame,baseline);
 // Exercise errors after each earlier candidate stage has run. No candidate
 // store, including filter clears and begin's cache reset, can escape failure.
 for(unsigned stage=0;stage<4;++stage){
  world=world_before;auto x=seed();auto c=preparation;
  if(stage==0)x.shared_fields.pw.field13c_bits.reset();
  if(stage==1)c.delta_bits.reset();
  if(stage==2)x.shared_fields.pw.field1f8.reset();
  if(stage==3)world.records[0].field28.reset();
  auto subject=make(x);const auto saved=save(subject);
  const auto error=value<na::PlayerError>(subject.prepare_update(c));
  check(error.code==na::PlayerErrorCode::update_prefix&&error.update_prefix&&unsigned(error.update_prefix->phase)==stage,"Prefix phase/cause lost");
  unchanged(subject,saved);metadata(value<na::Frame>(subject.capture()),initial);
  if(stage==3){
   check(std::get<wl::Error>(error.update_prefix->cause).field=="record28","Late copyout failure cause lost");
   world=world_before;const auto retry=value<na::PlayerPreparationOutput>(subject.prepare_update(c));
   check(retry.lookup.returned==17&&subject.shared_fields().pw.field5a8==17&&subject.shared_fields().pw.field6a8==0,"Late prefix retry failed");
   metadata(retry.frame,initial);frame_equal(retry.frame,initial);
  }
 }
 // The outer local gate skips even unknown filter inputs; subsequent helpers
 // still run and preserve the partially known unfiltered input bytes.
 world.head=wl::Absent{};auto remote=make(nonlocal_seed);const auto remote_before=save(remote);
 const auto remote_output=value<na::PlayerPreparationOutput>(remote.prepare_update(preparation));
 check(!remote_output.filter&&remote_output.lookup.returned==0xffff,"Outer gate did not skip filter");
 check(block::project(remote.state().controller.player,remote.shared_fields())==block::project(remote_before.state.controller.player,remote_before.shared)&&remote.shared_fields().pbw.field54_bits==0&&remote.shared_fields().pw.field6a8==0&&remote.shared_fields().pw.field5a8==0,"Nonlocal prefix skipped required later stages or changed input");
 metadata(remote_output.frame,initial);frame_equal(remote_output.frame,initial);
 check(combined_before.shared.pw.field6a8==255,"Atomic test did not start with a live cache");
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=value<na::Actor>(na::Actor::load(argv[2]));for(const auto& p:actor.source_files())std::cout<<p.generic_string()<<'\n';return 0;}
 check(argc==4,"Expected recipe constants constants-SHA");std::ifstream f(argv[2],std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==88,"Input extent");std::array<std::uint8_t,88> bytes{};f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(bytes.data()),88))&&f.peek()==std::char_traits<char>::eof(),"Input read");
 auto constants=value<pt::Constants>(pt::Constants::decode(bytes,{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));run(argv[1],constants);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
