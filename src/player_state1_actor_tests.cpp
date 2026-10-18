// Composition regression; raw state comparison helpers retained from player_policy_actor_tests.
#include "player_preview.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace na=mh4u::native_actor;namespace pa=mh4u::player_animation;namespace mp=mh4u::motion_pending;
namespace pcs=mh4u::player_controller_step;namespace ps=mh4u::player_selection;namespace pt=mh4u::player_transition;
namespace ppi=mh4u::player_policy_interpreter;namespace tbl=mh4u::player_policy_table;namespace jp=mh4u::joint_pose;
std::size_t checks{};constexpr std::uint32_t one=0x3f800000;constexpr jp::Bits4 identity{0,0,0,one};
void check(bool good,const char* message){++checks;if(!good)throw std::runtime_error(message);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
template<class T,class...U>T value(std::variant<U...> result){
 if(auto* e=std::get_if<T>(&result))return std::move(*e);
 if constexpr((std::is_same_v<U,na::PlayerError>||...)){const auto& e=std::get<na::PlayerError>(result);throw std::runtime_error(e.message);}
 throw std::runtime_error("Unexpected variant");
}
bool layer_equal(const mp::Layer& x,const mp::Layer& y){const auto& a=x.animation;const auto& b=y.animation;
 if(a.tracks!=b.tracks||a.prediction_rotation!=b.prediction_rotation||a.prediction_translation!=b.prediction_translation||a.previous_rotation!=b.previous_rotation||a.base_rotation!=b.base_rotation||a.previous_translation!=b.previous_translation||a.base_translation!=b.base_translation||x.rotation_98!=y.rotation_98||x.translation_c8!=y.translation_c8)return false;
 const auto& l=a.layer;const auto& r=b.layer;if(l.selector!=r.selector||l.flags!=r.flags||l.field08!=r.field08||l.attributes!=r.attributes||l.bindings!=r.bindings||l.masks!=r.masks||l.previous_masks!=r.previous_masks||l.interpolation_curve_bits!=r.interpolation_curve_bits)return false;
 const std::array ll{l.requested_frame,l.offset,l.interpolation,l.remaining_interpolation,l.weight,l.frame,l.previous_frame,l.end_frame,l.field58,l.speed,l.field60,l.prepared_frame,l.prepared_next_frame,l.previous_step};
 const std::array rr{r.requested_frame,r.offset,r.interpolation,r.remaining_interpolation,r.weight,r.frame,r.previous_frame,r.end_frame,r.field58,r.speed,r.field60,r.prepared_frame,r.prepared_next_frame,r.previous_step};for(std::size_t i=0;i<ll.size();++i)if(bits(ll[i])!=bits(rr[i]))return false;return true;
}
bool hierarchy_equal(const mh4u::model_animation_output::Hierarchy& a,const mh4u::model_animation_output::Hierarchy& b){if(a.local_rt!=b.local_rt||a.world!=b.world||a.flags4!=b.flags4||a.flags104!=b.flags104||a.flags230!=b.flags230||a.attachment_index!=b.attachment_index||a.attachment_mode!=b.attachment_mode||a.dirty!=b.dirty||a.fieldc4c!=b.fieldc4c||a.joints!=b.joints||a.order.index()!=b.order.index())return false;if(const auto* x=std::get_if<std::vector<std::uint8_t>>(&a.order))return *x==std::get<std::vector<std::uint8_t>>(b.order);return true;}
bool state_equal(const pa::State& a,const pa::State& b){if(a.controller.player!=b.controller.player||a.controller.motion!=b.controller.motion||a.frame.model!=b.frame.model||!hierarchy_equal(a.hierarchy,b.hierarchy))return false;
 const auto& x=a.frame.animation;const auto& y=b.frame.animation;if(x.joints!=y.joints||x.snapshots!=y.snapshots||x.calculation_flags!=y.calculation_flags||x.accumulated_flags!=y.accumulated_flags||x.layers.size()!=y.layers.size())return false;
 for(std::size_t i=0;i<x.layers.size();++i)if(!layer_equal(x.layers[i],y.layers[i]))return false;for(std::size_t i=0;i<6;++i)if(!layer_equal(a.inactive[i],b.inactive[i]))return false;return true;
}

void frame_equal(const na::Frame& a,const na::Frame& b){
 check(a.scene.global_transforms==b.scene.global_transforms&&a.palettes==b.palettes,"Pose/palette changed");
 check(a.scene.mesh.triangles.size()==b.scene.mesh.triangles.size()&&a.scene.draws.size()==b.scene.draws.size(),"Geometry extent changed");
 for(std::size_t i=0;i<a.scene.mesh.triangles.size();++i){const auto& p=a.scene.mesh.triangles[i];const auto& q=b.scene.mesh.triangles[i];
  check(std::array{bits(p.position.x),bits(p.position.y),bits(p.position.z),bits(p.normal.x),bits(p.normal.y),bits(p.normal.z),bits(p.uv.x),bits(p.uv.y)}==std::array{bits(q.position.x),bits(q.position.y),bits(q.position.z),bits(q.normal.x),bits(q.normal.y),bits(q.normal.z),bits(q.uv.x),bits(q.uv.y)},"Skinned vertex bits changed");}
 for(std::size_t i=0;i<a.scene.draws.size();++i){const auto& p=a.scene.draws[i];const auto& q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Draw identity changed");}
}
na::PolicyPreviewSeed policy_seed(const tbl::Table& table){
 na::PolicyPreviewSeed seed{};auto& p=seed.player;auto& x=seed.fields;
 p.pbw.field30=1;p.pbw.field33=1;p.pbw.field371=3;p.pbw.field372=1234;
 p.global.field2b=1;p.global.field2f=1;p.selection_pbw.field6f9=0;p.selection_pw.field9a8=1;
 p.pw.policy=tbl::install(table,0);p.pw.fieldf0=0x11000005;p.pw.fieldf4=0x12345678;p.pw.field261=3;
 x.pbw.flags3d8.fill(0);x.pbw.field3a8=x.pbw.field3ac=x.pbw.field3b8=x.pbw.field3bc=0;
 x.pbw.field14_bits=x.pbw.field478=0;x.pbw.field6fb=x.pbw.field6fc=x.pbw.field93ee=0;
 x.pw.field14c=x.pw.field154=x.pw.field15c_bits=x.pw.field6a4_bits=0;
 x.pw.field150=x.pw.field158=x.pw.field159=x.pw.field15a=0;x.pw.field6a8=1;
 x.pw.field188=x.pw.field224=x.pw.field238=0;return seed;
}
na::PlayerInitial initial(const na::PolicyPreviewSeed& seed){
 na::PlayerInitial out{};out.snapshots.resize(19);out.controller.player=seed.player;out.shared_fields=seed.fields;out.state1_fields=seed.state1_fields;
 out.model.rotation=out.model.rotation_delta=identity;out.model.scale=out.model.null_scale={one,one,one,0};
 for(auto& l:out.layers){l.animation.tracks.joints.resize(19);l.animation.layer.speed=1;l.animation.layer.flags=2;l.animation.prediction_rotation=identity;}
 out.layers[0].animation.layer.end_frame=30;out.layers[1].animation.layer.weight=.375f;
 out.controller.motion.fieldsd48[4]=one;out.controller.motion.fieldsd48[5]=bits(30);
 for(unsigned i=2;i<8;++i){out.layers[i].animation.layer.frame=std::bit_cast<float>(0x7fc00001U+i);out.layers[i].rotation_98={i,0xffffffff,0x80000000,0x7f800000};}
 return out;
}
na::PlayerPolicyContext context(){return {one,mh4u::player_reference::Absent{},0,0,std::array<std::uint16_t,2>{0x4000,0xc000}};}
na::PlayerPolicyInput input(std::uint16_t magnitude,std::uint32_t flags=0,std::uint16_t angle=0){na::PlayerPolicyInput in;in.channels=mh4u::player_input::Channels{{angle,0},{magnitude,0}};in.field3d4=flags;return in;}
na::PlayerBanks banks(){na::PlayerBanks b{};b[0]=na::MotionSource{"c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"};return b;}
na::PlayerStep frame_parameters(){na::PlayerStep p;p.delta_bits=one;
 p.templates={jp::Bits3{},jp::Bits3{},identity,jp::Bits4{}};p.restoration_scale=jp::Bits3{one,one,one};p.parent=jp::Null{};
 p.reference_basis=mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};p.reference_translation=identity;return p;}
void run(const char* recipe,const pt::Constants& constants,const tbl::Table& table35,const tbl::Table& table31){
 namespace st=mh4u::player_state1;namespace rq=mh4u::motion_request;
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected real seven-piece hunter");
 const auto seed=policy_seed(table35);const st::Tables tables{table35,table31};const auto params=frame_parameters();
 const auto make=[&](const na::PlayerInitial& init,const na::PlayerBanks& bank){return value<na::Player>(na::Player::from_rest(actor,init,constants,bank));};
 const auto entry=[&](unsigned id,unsigned selector=0xffff){auto init=initial(seed);init.controller.player.pbw.field371=1;
  init.controller.player.pbw.field372=static_cast<std::uint16_t>(id);init.controller.player.selection_pbw.field376=0xff;
  init.controller.player.pbw.fields5={0,0xa5,0x7e};init.layers[0].animation.layer.selector=static_cast<std::uint16_t>(selector);return init;};
 const auto pristine=make(initial(seed),banks());const auto rest=value<na::Frame>(pristine.capture());
 check(rest.scene.mesh.source_positions==4507&&rest.scene.mesh.triangles.size()/3==5043&&rest.scene.draws.size()==20,"Real geometry identity");
 // The real policy table decides each numeric action. Dispatch is a distinct
 // user/API operation; the next explicit step alone binds and animates it.
 for(const auto row:std::array<std::array<unsigned,5>,4>{{{110,0,0,1,3},{50,1,0,2,3},{110,0,49152,4,42},{50,0,0,9,49}}}){
  auto p=pristine;const auto policy=value<na::PlayerPolicyOutput>(p.apply_policy(input(row[0],row[1],row[2]),context()));
  check(policy.transition&&policy.transition->arguments.state_arg==1&&policy.transition->arguments.action_arg==row[3],"Real policy did not decide requested numeric action");
  check(p.completed_steps()==0&&p.state().frame.animation.layers[0].animation.layer.selector==0xffff,"Policy automatically requested/ticked");
  const auto prior=p.state();const auto extras=p.shared_fields();
  const auto dispatched=value<na::PlayerState1Output>(p.dispatch_state1(tables,params.templates));
  check(dispatched.effects.command&&dispatched.effects.command->selector==row[4],"Handler wrapper selector");
  check(dispatched.effects.request_count==2&&dispatched.effects.requests[0].accepted&&!dispatched.effects.requests[1].accepted,"Expected valid source action and layer1 sentinel");
  check(p.state().frame.animation.layers[0].animation.layer.selector==row[4]&&bits(p.state().frame.animation.layers[1].animation.layer.weight)==one,"Canonical layer selector/weight");
  check(p.completed_steps()==0&&dispatched.frame.tick==0&&p.shared_fields()==extras,"Dispatch ticked or changed policy extras");
  check(p.state().frame.animation.joints==prior.frame.animation.joints&&p.state().frame.animation.snapshots==prior.frame.animation.snapshots&&hierarchy_equal(p.state().hierarchy,prior.hierarchy),"Dispatch changed pose/snapshots/hierarchy");
  for(unsigned i=0;i<2;++i)check(p.state().frame.animation.layers[i].animation.tracks==prior.frame.animation.layers[i].animation.tracks,"Pending tracks were rebound early");
  for(unsigned i=0;i<6;++i)check(layer_equal(p.state().inactive[i],prior.inactive[i]),"Inactive slot changed");
  const auto& installed=std::get<tbl::Table>(p.state().controller.player.pw.policy.table);
  check(installed.identity()==(row[3]==4?table31.identity():table35.identity()),"Distinct source table lost");
  check(dispatched.effects.installation&&dispatched.effects.installation->before_command==(row[3]==1),"Installation order lost");
  check(p.state1_fields().field50_bits==(row[3]==4?std::nullopt:std::optional<std::uint32_t>{bits(20)}),"New timer omitted/known semantics");
  if(row[3]==2)check(p.state().controller.player.pw.policy.timer144_bits==bits(30),"Post-install timer144 lost");
  frame_equal(dispatched.frame,rest);
  const auto frame=value<na::PlayerOutput>(p.step(params));
  check(frame.animation&&frame.animation->pending_found&&frame.animation->initializer_calls>0&&frame.animation->joint_samples>0&&p.completed_steps()==1,"Explicit step did not bind and sample pending source action");
  check(p.state().frame.animation.layers[0].animation.tracks!=prior.frame.animation.layers[0].animation.tracks,"Actual source tracks not bound");
  if(row[3]==1||row[3]==4){
   // Obtain the gate from the actual clock, without inserting a synthetic
   // animation-end flag. Dispatch itself must not advance time or pose.
   unsigned advances=1;
   while(!(p.state().frame.animation.layers[0].animation.layer.flags&4U)&&advances<4096){
    value<na::PlayerOutput>(p.step(params));++advances;
   }
   check(advances<4096,"Real animation did not reach its continuation gate");
   const auto gated=p.state();const auto gated_fields=p.shared_fields();const auto gated_timer=p.state1_fields();
   const auto gated_frame=value<na::Frame>(p.capture());const auto ticks=p.completed_steps();
   const auto continued=value<na::PlayerState1Output>(p.dispatch_state1(tables,params.templates,{mh4u::player_reference::Absent{},{}}));
   check(p.completed_steps()==ticks&&continued.frame.tick==ticks&&p.shared_fields()==gated_fields&&p.state1_fields()==gated_timer,"Continuation advanced clock or changed canonical extras");
   check(p.state().frame.animation.joints==gated.frame.animation.joints&&p.state().frame.animation.snapshots==gated.frame.animation.snapshots&&hierarchy_equal(p.state().hierarchy,gated.hierarchy),"Continuation changed pose/snapshots/hierarchy");
   for(unsigned i=0;i<6;++i)check(layer_equal(p.state().inactive[i],gated.inactive[i]),"Continuation changed inactive slot");
   frame_equal(continued.frame,gated_frame);
   if(row[3]==1){
    check(continued.effects.command&&continued.effects.command->selector==4&&continued.effects.request_count==2&&continued.effects.requests[0].accepted,"ID1 did not restart selector4 at real gate");
    check(!continued.effects.transition&&p.state().controller.player.pbw.fields5[0]==2,"ID1 did not preserve continuation substate");
    const auto next=value<na::PlayerOutput>(p.step(params));
    check(next.animation&&next.animation->pending_found&&next.animation->joint_samples>0&&p.completed_steps()==ticks+1,"ID1 continuation did not bind on next explicit step");
   }else{
    check(continued.effects.transition&&continued.effects.transition->arguments.state_arg==1&&continued.effects.transition->arguments.action_arg==5,"ID4 did not transition to action5");
    check(!continued.effects.command&&continued.effects.request_count==0&&p.state().controller.player.pbw.field372==5,"ID4 fabricated an animation request");
   }
  }
 }
 // Same selector skips the setter but retains entry/suffix installation and
 // timers. Unknown source banks are not eagerly read in that branch.
 for(const auto id:{1U,2U})for(const auto current:{3U,4U}){
  auto p=make(entry(id,current),na::PlayerBanks{});const auto prior=p.state();
  const auto out=value<na::PlayerState1Output>(p.dispatch_state1(tables,{}));
  check(out.effects.command&&out.effects.request_count==0&&out.effects.queries==0&&out.effects.samples==0,"if_changed fabricated setter/query");
  for(unsigned i=0;i<2;++i)check(layer_equal(p.state().frame.animation.layers[i],prior.frame.animation.layers[i]),"Skipped command changed canonical layer");
  check(p.state().controller.player.pbw.fields5==std::array<std::uint8_t,3>{std::uint8_t(current==4?2:1),0xa5,0x7e},"Substate width or adjacent bytes");
  check(p.state1_fields().field50_bits==bits(20)&&p.completed_steps()==0,"Skipped request lost tail timer or ticked");
 }
 // Known absent bank is an original setter refusal, not a failed transaction.
 auto absent=banks();absent[0]=rq::Absent{};auto refused=make(entry(2),absent);
 const auto refusal=value<na::PlayerState1Output>(refused.dispatch_state1(tables,params.templates));
 check(refusal.effects.request_count==2&&!refusal.effects.requests[0].accepted&&!refusal.effects.requests[1].accepted,"Refusal became native error");
 check(refused.state1_fields().field50_bits==bits(20)&&refused.state().controller.player.pw.policy.timer144_bits==bits(30),"Refusal discarded suffix");
 // Unknown bank fails after candidate prefix; every live owner stays unchanged.
 auto unknown=make(entry(1),na::PlayerBanks{});const auto before=unknown.state();const auto ef=unknown.shared_fields();const auto sf=unknown.state1_fields();
 const auto failure=unknown.dispatch_state1(tables,params.templates);check(std::holds_alternative<na::PlayerError>(failure),"Unknown bank fabricated absent");
 const auto& err=std::get<na::PlayerError>(failure);check(err.driver&&err.driver->phase==pa::Phase::state1,"Dispatch error phase lost");
 check(state_equal(unknown.state(),before)&&unknown.shared_fields()==ef&&unknown.state1_fields()==sf&&unknown.completed_steps()==0,"Failed request published prefix");
 // Native rendering consumes the real FP profile. Same-selector dispatch does
 // no float arithmetic, so this error occurs after a valid handler candidate.
 auto render_failure=make(entry(1,3),na::PlayerBanks{});const auto rf_before=render_failure.state();const auto rf_fields=render_failure.state1_fields();
 check(std::fesetround(FE_DOWNWARD)==0,"Cannot set diagnostic rounding mode");
 const auto failed_render=render_failure.dispatch_state1(tables,{});const int restored=std::fesetround(FE_TONEAREST);
 check(restored==0,"Cannot restore rounding mode");check(std::holds_alternative<na::PlayerError>(failed_render)&&std::get<na::PlayerError>(failed_render).code==na::PlayerErrorCode::render,"Expected real render-domain rejection");
 check(state_equal(render_failure.state(),rf_before)&&render_failure.state1_fields()==rf_fields&&render_failure.shared_fields()==seed.fields&&render_failure.completed_steps()==0,"Render error published candidate");
 frame_equal(value<na::Frame>(render_failure.capture()),rest);check(value<na::PlayerState1Output>(render_failure.dispatch_state1(tables,{})).effects.request_count==0,"Render failure poisoned valid retry");
 // Raw fullword copies and overwrite-only fields. No finite check on old data.
 auto raw=entry(2);raw.controller.player.pbw.field24=0xfedc1234;raw.controller.player.pbw.field39c=0x7fc00011;raw.controller.player.pw.fieldc4=0x7f800000;
 raw.controller.player.pw.field17c=0xa4;raw.controller.player.selection_pw.fielda10=0xff800000;raw.state1_fields.field50_bits=0x7fc00022;
 auto copied=make(raw,banks());value<na::PlayerState1Output>(copied.dispatch_state1(tables,params.templates));
 check(copied.state().controller.player.pw.fieldb0==0xfedc1234&&copied.state().controller.player.pw.field17c==0xa5,"Fullword/byte preservation");
 check(copied.state().controller.player.pbw.field39c==bits(1.25f)&&copied.state().controller.player.pw.fieldc4==bits(1.25f)&&copied.state().controller.player.selection_pw.fielda10==0,"Overwrite-only source fields");
 auto raw4=entry(4);raw4.controller.player.pw.fieldb0=0xface9876;auto copied4=make(raw4,banks());
 value<na::PlayerState1Output>(copied4.dispatch_state1(tables,params.templates));check(copied4.state().controller.player.pbw.field24==0xface9876&&!copied4.state1_fields().field50_bits,"ID4 raw copy/unknown timer");
 // Known no-op substate has no scheduler side effects. Unknown consumed
 // context fails instead of automatically replaying entry or requesting.
 auto noop=entry(9);noop.controller.player.pbw.fields5[0]=7;auto no=make(noop,banks());const auto no_before=no.state();
 auto no_expected=no_before;no_expected.controller.player.selection_pbw.field376=0;
 const auto no_out=value<na::PlayerState1Output>(no.dispatch_state1({},{}));check(!no_out.effects.command&&state_equal(no.state(),no_expected)&&!no.state1_fields().field50_bits,"No-op dispatched work");
 auto cont=entry(1);cont.controller.player.pbw.fields5[0]=1;auto continuation=make(cont,banks());const auto cont_before=continuation.state();
 check(std::holds_alternative<na::PlayerError>(continuation.dispatch_state1(tables,{}))&&state_equal(continuation.state(),cont_before),"Unknown consumed timer fabricated result");
 // Canonical policy extras provide the stored orientation. An unused NaN
 // timer and unknown external reference must survive this integer-only path.
 for(const auto id:{1U,9U}){
  auto oriented=entry(id);auto& p=oriented.controller.player;
  p.pbw.fields5[0]=1;p.pbw.magnitudes[0]=110;p.pbw.angles[0]=0xabcd;
  p.pw.fieldf0|=0x4000U;oriented.shared_fields.pw.field150=1;
  oriented.shared_fields.pw.field14c=0xfedc4321;oriented.state1_fields.field50_bits=0x7fc12345;
  auto subject=make(oriented,banks());const auto saved=subject.state();const auto fields=subject.shared_fields();
  check(std::fesetround(FE_DOWNWARD)==0,"Cannot set continuation render diagnostic");
  const auto failed=subject.dispatch_state1(tables,params.templates);const auto restore=std::fesetround(FE_TONEAREST);
  check(restore==0,"Cannot restore continuation render profile");
  check(std::holds_alternative<na::PlayerError>(failed)&&std::get<na::PlayerError>(failed).code==na::PlayerErrorCode::render,"Expected late continuation render rejection");
  check(state_equal(subject.state(),saved)&&subject.shared_fields()==fields&&subject.state1_fields()==oriented.state1_fields&&subject.completed_steps()==0,"Continuation render failure published candidate");
  const auto out=value<na::PlayerState1Output>(subject.dispatch_state1(tables,params.templates));
  check(subject.state().controller.player.pw.fieldb0==((0xfedc4321U-0x4000U+0xabcdU)&0xffffU),"Stored heading did not use canonical policy extras");
  check(subject.shared_fields()==fields&&subject.state1_fields()==oriented.state1_fields&&out.effects.magnitude_queries==1&&!out.effects.transition&&out.effects.request_count==0,"Orientation consumed unused context or emitted work");
  frame_equal(out.frame,rest);
 }
 // ID9's actual selection wrapper resolves bank2/slot3 from real co2. The
 // selection request and transition share the same persistent layer owners.
 auto selected=entry(9);selected.controller.player.pbw.fields5[0]=1;
 selected.controller.player.selection_pw.field9a8=0;selected.controller.player.selection_pw.field7fc=0;
 selected.controller.player.selection_pw.field594=0;selected.controller.player.selection_pbw.field358=0;
 selected.state1_fields.field50_bits=0;
 auto co2=banks();co2[2]=na::MotionSource{"6dc7bd0b51f5c47722883efbaff8a95fc2504ebfc0e436a0e0f811ba51e04274"};
 auto selected_player=make(selected,co2);
 const auto chosen=value<na::PlayerState1Output>(selected_player.dispatch_state1(tables,params.templates));
 check(chosen.effects.selection_request&&chosen.effects.request_count==2&&chosen.effects.requests[0].accepted&&chosen.effects.transition,"ID9 selection did not request verified co2");
 check(selected_player.state().frame.animation.layers[0].animation.layer.selector==515&&selected_player.completed_steps()==0,"ID9 lost selected bank/slot or ticked");
 frame_equal(chosen.frame,rest);
 const auto selected_frame=value<na::PlayerOutput>(selected_player.step(params));
 check(selected_frame.animation&&selected_frame.animation->pending_found&&selected_frame.animation->initializer_calls>0&&selected_frame.animation->joint_samples>0&&selected_player.completed_steps()==1,"co2 did not bind/sample on next explicit step");
 auto absent_co2=banks();absent_co2[2]=rq::Absent{};auto selected_refusal=make(selected,absent_co2);
 const auto denied=value<na::PlayerState1Output>(selected_refusal.dispatch_state1(tables,params.templates));
 check(denied.effects.transition&&denied.effects.request_count==2&&!denied.effects.requests[0].accepted&&selected_refusal.completed_steps()==0,"Known absent co2 did not retain original refusal/transition");
 auto selected_unknown=make(selected,banks());const auto selected_before=selected_unknown.state();
 const auto selected_error=selected_unknown.dispatch_state1(tables,params.templates);
 check(std::holds_alternative<na::PlayerError>(selected_error)&&state_equal(selected_unknown.state(),selected_before)&&selected_unknown.shared_fields()==selected.shared_fields&&selected_unknown.state1_fields()==selected.state1_fields&&selected_unknown.completed_steps()==0,"Unknown co2 published wrapper prefix");
 // ID2's source cost and PBW position are canonical shared extras, distinct
 // from the renderer's model transform. They commit with the whole candidate.
 namespace oq=mh4u::player_object_query;
 const auto id2_initial=[&]{
  auto init=entry(2,3);auto& p=init.controller.player;auto& x=init.shared_fields;
  p.pbw.fields5[0]=1;p.pbw.magnitudes[0]=110;p.pbw.field3d4=1;
  p.pw.fieldf0|=0x4100U;p.selection_pw.field9a8=0;p.selection_pbw.field9=0;
  p.selection_pbw.field358=bits(500);p.selection_pbw.fields2fc.fill(std::uint16_t{0});
  x.pw.field150=1;x.pw.field14c=0;x.pw.field1f8=0;x.pw.field220_bits=x.pw.field838_bits=0;
  x.pbw.field10_bits=x.pbw.field14_bits=x.pbw.field18_bits=0;x.pbw.field354_bits=bits(200);
  x.pw.field7f0=0;x.pbw.field478=0;x.pw.field6a8=1;x.pw.field6a4_bits=bits(1000);
  init.state1_fields.field50_bits=bits(20);return init;
 };
 std::array<oq::Slot,32> empty_manager;for(auto& slot:empty_manager)slot=oq::Absent{};
 na::PlayerState1Context id2_context;id2_context.delta_time_bits=one;id2_context.context_ad50=0;
 id2_context.manager=std::span<const oq::Slot>{empty_manager};
 for(unsigned branch=0;branch<3;++branch){
  auto init=id2_initial();
  if(branch==0){init.shared_fields.pbw.field14_bits=bits(200);init.shared_fields.pw.field6a4_bits=bits(100);}
  if(branch==1)init.shared_fields.pbw.field354_bits=bits(100);
  auto manager=empty_manager;oq::Object candidate;candidate.field140=1;candidate.field1b4=1;
  candidate.field34c=1;candidate.field290=2;candidate.field1c0=0;candidate.field14c=0;
  candidate.position_bits=std::array<std::uint32_t,3>{0,bits(20),bits(-100)};
  manager[15]=candidate;auto disabled=candidate;disabled.field140=0;manager[0]=disabled;
  auto ctx=id2_context;if(branch==2)ctx.manager=std::span<const oq::Slot>{manager};
  auto player=make(init,banks());const auto old=player.state();
  const auto out=value<na::PlayerState1Output>(player.dispatch_state1(tables,params.templates,ctx));
  check(out.effects.cost_calls==1&&out.effects.cost_updates==1&&out.effects.position_updates==1,"ID2 did not execute original cost/position owners");
  check(player.shared_fields().pbw.field354_bits==bits(branch==1?99.5f:199.5f)&&player.shared_fields().pbw.field18_bits==bits(2.5f),"ID2 raw cost/position did not commit");
  check(player.shared_fields().pbw.field10_bits==0&&player.shared_fields().pbw.field14_bits==init.shared_fields.pbw.field14_bits,"ID2 position axes diverged");
  check(player.state().frame.model==old.frame.model&&player.state().frame.animation.joints==old.frame.animation.joints&&hierarchy_equal(player.state().hierarchy,old.hierarchy),"Raw PBW position became renderer coordinates");
  check(out.effects.transition&&out.effects.transition->arguments.state_arg==(branch==0?2U:1U)&&out.effects.transition->arguments.action_arg==(branch==0?0U:branch==1?11U:13U),"ID2 destination branch mismatch");
  check(player.completed_steps()==0&&out.frame.tick==0&&player.state1_fields()==init.state1_fields&&!out.effects.command,"ID2 dispatch ticked or decremented timer");
  if(branch==2)check(out.effects.object_query&&out.effects.object_query->matched&&out.effects.object_query->object_queries==1&&out.effects.object_query->slots_examined==16&&(player.state().controller.player.pw.field17c&1U),"Manager first disabled/valid ordering or post-transition flag lost");
  else check(!out.effects.object_query&&out.effects.manager_queries==0,"Earlier transition consumed unused manager");
  frame_equal(out.frame,rest);
 }
 // A complete actual animation reaches the source gate. Empty known manager
 // allows the ID2 helper to return false and the handler to restart selector4.
 auto cycling=make(id2_initial(),banks());unsigned id2_steps=0;
 while(!(cycling.state().frame.animation.layers[0].animation.layer.flags&4U)&&id2_steps<4096){value<na::PlayerOutput>(cycling.step(params));++id2_steps;}
 check(id2_steps>0&&id2_steps<4096,"ID2 real pending animation did not reach gate");
 const auto cycled=value<na::PlayerState1Output>(cycling.dispatch_state1(tables,params.templates,id2_context));
 check(cycled.effects.command&&cycled.effects.command->selector==4&&cycled.effects.requests[0].accepted&&cycling.state().controller.player.pbw.fields5[0]==2,"ID2 did not request4 after real gate");
 check(cycled.effects.object_query&&!cycled.effects.object_query->matched&&cycled.effects.object_query->slots_examined==32&&cycling.completed_steps()==id2_steps,"Known empty manager or explicit clock lost");
 const auto bound=value<na::PlayerOutput>(cycling.step(params));check(bound.animation&&bound.animation->pending_found&&bound.animation->joint_samples>0,"ID2 requested action did not bind/sample");
 // These errors occur after original cost/position have produced a candidate.
 // Every live owner must remain identical, and a valid retry must still work.
 for(unsigned failure_kind=0;failure_kind<2;++failure_kind){
  auto init=id2_initial();if(failure_kind==1)init.shared_fields.pw.field6a8=0;
  auto player=make(init,banks());const auto saved=player.state();const auto fields=player.shared_fields();
  auto ctx=id2_context;ctx.manager=oq::Unknown{};
  const auto failed=player.dispatch_state1(tables,params.templates,ctx);
  check(std::holds_alternative<na::PlayerError>(failed)&&std::get<na::PlayerError>(failed).driver&&std::get<na::PlayerError>(failed).driver->phase==pa::Phase::state1,"Unclosed ID2 context fabricated success");
  check(state_equal(player.state(),saved)&&player.shared_fields()==fields&&player.state1_fields()==init.state1_fields&&player.completed_steps()==0,"Late ID2 error published cost/position");
  frame_equal(value<na::Frame>(player.capture()),rest);
  if(failure_kind==0){const auto retry=value<na::PlayerState1Output>(player.dispatch_state1(tables,params.templates,id2_context));check(retry.effects.cost_updates==1&&player.shared_fields().pbw.field354_bits==bits(199.5f),"Failed ID2 call changed subsequent cost");}
 }
 // Driver bounds are checked before joining split layer storage. Neither a
 // short active prefix nor a cumulative binding policy can publish anything.
 auto low=pristine.state();const auto low_before=low;auto limits=pa::Limits{};limits.command.storage_layers=7;
 const mh4u::motion_command::Globals globals{};const pa::State1Context ctx{tables,pristine.banks(),globals,{}};
 check(std::holds_alternative<pa::Error>(pa::dispatch_state1(low,{},ctx,limits))&&state_equal(low,low_before),"Storage policy ignored");
 limits={};limits.command.joint_bindings=151;check(std::holds_alternative<pa::Error>(pa::dispatch_state1(low,{},ctx,limits)),"Cumulative inactive bindings ignored");
 low.frame.animation.layers.pop_back();check(std::holds_alternative<pa::Error>(pa::dispatch_state1(low,{},ctx)),"Short active prefix accepted");
 // Reset preserves the explicit extra-field recipe, never its later live timer.
 auto reset_seed=seed;reset_seed.state1_fields.field50_bits=0x7fc12345;
 auto preview=value<na::PlayerPreview>(na::PlayerPreview::from_actor_with_policy(actor,constants,reset_seed));
 value<na::PlayerPolicyOutput>(preview.apply_policy(input(110),context()));
 value<na::PlayerState1Output>(preview.dispatch_state1(tables));check(preview.player().state1_fields().field50_bits==bits(20)&&preview.player().completed_steps()==0,"Preview implicit tick or missing dispatch");
 const auto owner=preview.presentation();value<na::PlayerOutput>(preview.reset());
 check(preview.presentation()==owner&&preview.player().state1_fields()==reset_seed.state1_fields&&preview.player().shared_fields()==reset_seed.fields&&preview.player().completed_steps()==0,"Reset did not restore explicit extras");
 auto untouched=value<na::PlayerPreview>(na::PlayerPreview::from_actor(actor,constants));
 check(!untouched.player().state1_fields().field50_bits&&std::holds_alternative<na::PlayerError>(untouched.dispatch_state1(tables)),"Factory fabricated entry/default timer");
 check(table35.records().size()==35&&table31.records().size()==31,"Borrowed table bytes changed");
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=value<na::Actor>(na::Actor::load(argv[2]));for(const auto& p:actor.source_files())std::cout<<p.generic_string()<<'\n';return 0;}
 check(argc==8,"Expected recipe constants constant-SHA table35 table35-SHA table31 table31-SHA");
 const auto read=[](const char* path,std::size_t size){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==static_cast<std::streamoff>(size),"Input extent");std::vector<std::uint8_t> out(size);f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(out.size())))&&f.peek()==std::char_traits<char>::eof(),"Input read");return out;};
 const auto constants=value<pt::Constants>(pt::Constants::decode(read(argv[2],88),{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));
 const auto table35=value<tbl::Table>(tbl::Table::decode(read(argv[4],792),argv[5]));
 const auto table31=value<tbl::Table>(tbl::Table::decode(read(argv[6],704),argv[7]));run(argv[1],constants,table35,table31);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
