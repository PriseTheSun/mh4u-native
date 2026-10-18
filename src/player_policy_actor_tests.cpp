#include "player_preview.hpp"
#include <bit>
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
 na::PlayerInitial out{};out.snapshots.resize(19);out.controller.player=seed.player;out.shared_fields=seed.fields;
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
void run(const char* recipe,const pt::Constants& constants,const tbl::Table& table){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected real seven-piece hunter");
 const auto seed=policy_seed(table);const auto make=[&](const na::PolicyPreviewSeed& s){return value<na::Player>(na::Player::from_rest(actor,initial(s),constants,banks()));};
 auto original=make(seed);const auto rest=value<na::Frame>(original.capture());
 check(rest.scene.mesh.source_positions==4507&&rest.scene.mesh.triangles.size()/3==5043&&rest.scene.draws.size()==20,"Real geometry identity");
 // The public inverse projection uses the canonical layer1 weight, including
 // raw nonfinite copies, and refuses short extents before any store.
 {auto controller=original.state().controller;auto layers=original.state().frame.animation.layers;auto projected=value<ps::State>(pcs::selection_view(controller,layers));
  projected.transition.player.field3b8=0x7fc01234;projected.transition.pbw.field372=77;
  const auto saved=controller;const auto savedlayer=layers[0];
  check(pcs::store_selection(controller,std::span<mp::Layer>(layers).first(1),projected).has_value(),"Short projection accepted");
  check(controller.player==saved.player&&controller.motion==saved.motion&&layer_equal(layers[0],savedlayer),"Short projection partially stored");
  check(!pcs::store_selection(controller,layers,projected),"Valid projection failed");
  check(bits(layers[1].animation.layer.weight)==0x7fc01234&&controller.player.pbw.field372==77,"Projection lost raw shared weight");
  check(value<ps::State>(pcs::selection_view(controller,layers)).transition==projected.transition,"Projection is not inverse");}
 auto no=original;const auto no_result=value<na::PlayerPolicyOutput>(no.apply_policy(input(0),context()));
 check(!no_result.returned&&!no_result.transition&&no_result.visited_count==35,"No-input did not visit whole real table");
 for(unsigned i=0;i<35;++i)check(no_result.visited[i]==i,"Visit order changed");
 check(no.shared_fields().pbw.field6fc==1&&original.shared_fields().pbw.field6fc==0,"Ordered side effect/independent copies");
 frame_equal(no_result.frame,rest);check(no.completed_steps()==0,"Policy advanced clock");
 for(const auto [mag,flag,row,action,expectedflags]:std::array<std::array<unsigned,5>,4>{{{50,1,32,2,1},{110,0,33,1,1},{50,0,34,9,0},{110,0,33,4,0}}}){
  auto p=original;const auto prior=p.state();const auto out=value<na::PlayerPolicyOutput>(p.apply_policy(input(static_cast<std::uint16_t>(mag),flag,action==4?49152:0),context()));
  check(out.returned&&out.transition&&out.visited_count==row+1,"Tail decision/visits");
  check(out.transition->arguments.state_arg==1&&out.transition->arguments.action_arg==action&&out.transition->arguments.flags_arg==expectedflags,"Original tail transition differs");
  check(p.completed_steps()==0&&p.state().controller.motion==prior.controller.motion,"Policy fabricated animation step/request");
  check(std::holds_alternative<tbl::Absent>(p.state().controller.player.pw.policy.table),"Transition failed to clear table");
  auto expected=prior;expected.controller=p.state().controller;expected.frame.animation.layers[1].animation.layer.weight=p.state().frame.animation.layers[1].animation.layer.weight;
  check(state_equal(p.state(),expected),"Policy changed unrelated canonical animation state");frame_equal(out.frame,rest);
  const auto repeated=value<na::PlayerPolicyOutput>(p.apply_policy({},{}));check(!repeated.returned&&!repeated.transition&&repeated.visited_count==0,"Absent table was auto-reinstalled");
 }
 auto blocked_seed=seed;blocked_seed.player.pw.policy.field148=1;auto blocked=make(blocked_seed);
 const auto blocked_result=value<na::PlayerPolicyOutput>(blocked.apply_policy(input(110),context()));
 check(!blocked_result.returned&&!blocked_result.transition&&blocked_result.visited_count==35&&blocked.state().controller.player.pw.policy.latches[33]==1,"Late148 gate lost latch");
 // Omitted input keeps the last channels and known fields. Explicit zero clears.
 const auto kept=value<na::PlayerPolicyOutput>(blocked.apply_policy({},context()));check(kept.visited_count==35&&blocked.state().controller.player.pbw.magnitudes[0]==110,"Omitted channel became zero");
 value<na::PlayerPolicyOutput>(blocked.apply_policy(input(0),context()));check(blocked.state().controller.player.pbw.magnitudes[0]==0&&blocked.state().controller.player.pw.policy.latches[33]==0,"Explicit zero was omitted");
 // Consumed unknown after earlier table writes must publish nothing, including
 // submitted input and extra fields.
 auto broken_seed=seed;broken_seed.fields.pbw.field6fb.reset();auto broken=make(broken_seed);const auto before=broken.state();const auto extras=broken.shared_fields();
 const auto failure=broken.apply_policy(input(110),context());check(std::holds_alternative<na::PlayerError>(failure),"Unknown late field silently defaulted");
 const auto& err=std::get<na::PlayerError>(failure);check(err.policy&&err.policy->record==31&&err.policy->field=="pbw+6fb","Late error lost source identity");
 check(state_equal(broken.state(),before)&&broken.shared_fields()==extras&&broken.completed_steps()==0,"Failed policy published prefix/input");frame_equal(value<na::Frame>(broken.capture()),rest);
 auto missing_context=original;auto unknown=context();unknown.reference=mh4u::player_reference::Unknown{};
 check(std::holds_alternative<na::PlayerError>(missing_context.apply_policy(input(110),unknown)),"Unknown consumed reference defaulted");
 check(state_equal(missing_context.state(),original.state())&&missing_context.shared_fields()==original.shared_fields(),"Reference failure published state");

 check(value<na::PlayerPolicyOutput>(missing_context.apply_policy(input(110),context())).transition.has_value(),"Failure made future valid context unusable");
 auto unknown_inputs_seed=seed;unknown_inputs_seed.fields.pbw.flags3d8.fill(std::nullopt);
 unknown_inputs_seed.fields.pbw.field3a8.reset();unknown_inputs_seed.fields.pbw.field3ac.reset();unknown_inputs_seed.fields.pbw.field3b8.reset();unknown_inputs_seed.fields.pbw.field3bc.reset();
 auto unknown_inputs=make(unknown_inputs_seed);const auto unknown_before=unknown_inputs.state();
 check(std::holds_alternative<na::PlayerError>(unknown_inputs.apply_policy(input(0),context())),"Omitted unknown input silently zeroed");
 check(state_equal(unknown_inputs.state(),unknown_before)&&unknown_inputs.shared_fields()==unknown_inputs_seed.fields,"Unknown input failure published changes");
 auto known_zero=input(0);known_zero.flags3d8.fill(0);known_zero.masks.fill(0);
 const auto cleared=value<na::PlayerPolicyOutput>(unknown_inputs.apply_policy(known_zero,context()));
 check(!cleared.returned&&cleared.visited_count==35,"Explicit known-zero masks/flags failed");
 check(unknown_inputs.shared_fields().pbw.flags3d8==known_zero.flags3d8&&unknown_inputs.shared_fields().pbw.field3a8==0&&unknown_inputs.shared_fields().pbw.field3bc==0,"Input patch did not retain known zero");
 // A live LMT action is a caller-selected test seed, not a policy result or
 // bootstrap assertion. The policy consumes the same playing layer0 timing.
 auto playing_seed=initial(seed);playing_seed.layers[0].animation.layer.selector=8;
 auto playing=value<na::Player>(na::Player::from_rest(actor,playing_seed,constants,banks()));
 na::PlayerStep step;step.delta_bits=one;step.templates={jp::Bits3{},jp::Bits3{},identity,jp::Bits4{}};step.restoration_scale=jp::Bits3{one,one,one};step.parent=jp::Null{};
 step.reference_basis=mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};step.reference_translation=identity;
 std::size_t sampled=0;for(unsigned i=0;i<6;++i){const auto frame=value<na::PlayerOutput>(playing.step(step));sampled+=frame.frame.samples;}
 check(sampled>0&&playing.completed_steps()==6,"Live source LMT was not sampled");
 const auto playing_before=playing.state();const auto playing_frame=value<na::Frame>(playing.capture());
 const auto live_decision=value<na::PlayerPolicyOutput>(playing.apply_policy(input(110),context()));
 check(live_decision.transition&&live_decision.transition->arguments.action_arg==1&&playing.completed_steps()==6,"Policy on playing layer failed or stepped");
 auto playing_expected=playing_before;playing_expected.controller=playing.state().controller;playing_expected.frame.animation.layers[1].animation.layer.weight=playing.state().frame.animation.layers[1].animation.layer.weight;
 check(state_equal(playing.state(),playing_expected),"Live policy duplicated or mutated pose/cursors");frame_equal(live_decision.frame,playing_frame);
 // Defaults remain unknown. The original factory never installs a table.
 auto old=value<na::PlayerPreview>(na::PlayerPreview::from_actor(actor,constants));
 check(std::holds_alternative<tbl::Unknown>(old.player().state().controller.player.pw.policy.table),"Old factory installed policy");
 check(std::holds_alternative<na::PlayerError>(old.apply_policy(input(110),context())),"Unknown policy accepted");
 auto preview=value<na::PlayerPreview>(na::PlayerPreview::from_actor_with_policy(actor,constants,seed));
 const auto shared=preview.presentation();auto decision=value<na::PlayerPolicyOutput>(preview.apply_policy(input(110),context()));
 check(decision.transition&&decision.transition->arguments.action_arg==1&&preview.player().completed_steps()==0,"Explicit preview policy decision");
 frame_equal(decision.frame,rest);value<na::PlayerOutput>(preview.reset());
 check(preview.presentation()==shared&&std::holds_alternative<tbl::Table>(preview.player().state().controller.player.pw.policy.table)&&preview.player().shared_fields()==seed.fields,"Reset did not preserve explicit recipe/source");
 check(value<na::PlayerPolicyOutput>(preview.apply_policy(input(110),context())).transition.has_value(),"Explicit reset lost policy");
 auto delayed_seed=seed;delayed_seed.player.pw.policy.timer140_bits=one;auto delayed=make(delayed_seed);
 const auto delay=value<na::PlayerPolicyOutput>(delayed.apply_policy(input(110),{}));check(!delay.returned&&delay.visited_count==0&&delayed.state().controller.player.pw.policy.timer140_bits==one,"Timer decremented or unused context required");
 auto latched_seed=seed;latched_seed.player.pw.policy.field106=1;latched_seed.player.pw.policy.latches[33]=255;auto latched=make(latched_seed);
 const auto latched_result=value<na::PlayerPolicyOutput>(latched.apply_policy(input(0),context()));check(latched_result.transition&&latched_result.transition->arguments.action_arg==1,"Source latch required new input");
 // A caller-owned patch cannot alter its arguments or an immutable table.
 const auto args=input(0);auto pristine=original;value<na::PlayerPolicyOutput>(pristine.apply_policy(args,context()));check(args.channels->magnitudes[0]==0&&table.records().size()==35,"Borrowed inputs changed");
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=value<na::Actor>(na::Actor::load(argv[2]));for(const auto& p:actor.source_files())std::cout<<p.generic_string()<<'\n';return 0;}
 check(argc==6,"Expected recipe constants constant-SHA table table-SHA");
 const auto read=[](const char* path,std::size_t size){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==static_cast<std::streamoff>(size),"Input extent");std::vector<std::uint8_t> out(size);f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(out.size())))&&f.peek()==std::char_traits<char>::eof(),"Input read");return out;};
 const auto constants=value<pt::Constants>(pt::Constants::decode(read(argv[2],88),{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));
 const auto table=value<tbl::Table>(tbl::Table::decode(read(argv[4],792),argv[5]));run(argv[1],constants,table);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
