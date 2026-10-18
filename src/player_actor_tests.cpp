#include "player_preview.hpp"
#include <bit>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
namespace na=mh4u::native_actor;namespace pa=mh4u::player_animation;namespace ps=mh4u::player_selection;
namespace jp=mh4u::joint_pose;namespace mp=mh4u::motion_pending;namespace pt=mh4u::player_transition;
constexpr std::uint32_t one=0x3f800000;constexpr jp::Bits4 identity{0,0,0,one};constexpr jp::Bits3 ones{one,one,one};
std::size_t checks{},frames{},live_queries{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class T>T get(na::Result<T> result){if(auto* e=std::get_if<na::Error>(&result))throw std::runtime_error(e->message);return std::get<T>(std::move(result));}
template<class T>T get(na::PlayerResult<T> result){if(auto* e=std::get_if<na::PlayerError>(&result)){std::string extra;if(e->driver){if(auto* ce=std::get_if<mh4u::player_controller_step::Error>(&e->driver->cause)){if(auto* selection=std::get_if<ps::Error>(&ce->cause))extra=std::string(selection->message);if(auto* command=std::get_if<mh4u::motion_command::Error>(&ce->cause))extra=std::string(command->message);}if(auto* f=std::get_if<mh4u::model_animation_frame::Error>(&e->driver->cause)){extra=std::string(f->message);if(f->application){extra+=" layer "+std::to_string(f->application->layer);if(f->application->arithmetic)extra+=" "+std::string(f->application->arithmetic->message);}}}throw std::runtime_error(e->message+" "+extra);}return std::get<T>(std::move(result));}
void equal(const na::Frame& a,const na::Frame& b){
 check(a.scene.global_transforms==b.scene.global_transforms,"Matrices differ");
 check(a.scene.mesh.source_positions==b.scene.mesh.source_positions&&a.scene.mesh.triangles.size()==b.scene.mesh.triangles.size(),"Geometry changed");
 check(a.scene.draws.size()==b.scene.draws.size()&&a.palettes==b.palettes,"Draw/palette count changed");
 for(std::size_t i=0;i<a.scene.mesh.triangles.size();++i){const auto& p=a.scene.mesh.triangles[i];const auto& q=b.scene.mesh.triangles[i];
  for(auto v:{std::pair{p.position.x,q.position.x},{p.position.y,q.position.y},{p.position.z,q.position.z},{p.normal.x,q.normal.x},{p.normal.y,q.normal.y},{p.normal.z,q.normal.z},{p.uv.x,q.uv.x},{p.uv.y,q.uv.y}})check(std::bit_cast<std::uint32_t>(v.first)==std::bit_cast<std::uint32_t>(v.second),"Vertex bits differ");}
 for(std::size_t i=0;i<a.scene.draws.size();++i){auto p=a.scene.draws[i],q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Draw identity differs");}
}
na::PlayerInitial inspection_seed(std::size_t count){
 na::PlayerInitial initial{};initial.snapshots.resize(count);
 initial.model.rotation=initial.model.rotation_delta=identity;initial.model.scale=initial.model.null_scale={one,one,one,0};
 for(auto& layer:initial.layers){layer.animation.tracks.joints.resize(count);layer.animation.layer.speed=1;layer.animation.layer.flags=2;layer.animation.prediction_rotation=identity;}
 // Inactive storage is deliberately distinctive and must never become active.
 for(unsigned i=2;i<8;++i){auto& layer=initial.layers[i];layer.animation.layer.frame=std::bit_cast<float>(0x7fc01000U+i);layer.rotation_98={i,0x80000000,0x7f800000,0x7fc00000};}
 initial.controller.player.global.field2b=2;initial.controller.player.pbw.field30=3;
 return initial;
}
na::PlayerStep inspection_step(){na::PlayerStep step;step.delta_bits=one;step.templates={jp::Bits3{},jp::Bits3{},identity,jp::Bits4{}};step.restoration_scale=ones;step.parent=jp::Null{};step.reference_basis=mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};step.reference_translation=identity;return step;}
na::PlayerBanks bank_table(){na::PlayerBanks banks{};banks[0]=na::MotionSource{"c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"};return banks;}
void invariant(const na::Player& player,const na::PlayerInitial& initial){const auto& s=player.state();check(s.frame.animation.layers.size()==2&&s.frame.animation.joints.size()==19&&s.frame.animation.snapshots.size()==19&&s.hierarchy.joints.size()==19,"Canonical real rig extents");for(unsigned i=0;i<6;++i){const auto& a=s.inactive[i];const auto& b=initial.layers[i+2];check(std::bit_cast<std::uint32_t>(a.animation.layer.frame)==std::bit_cast<std::uint32_t>(b.animation.layer.frame)&&a.rotation_98==b.rotation_98&&a.animation.tracks==b.animation.tracks,"Inactive state changed");}}
void position_integration(const na::Actor& actor,const pt::Constants& constants){
 namespace pb=mh4u::player_position_bridge;
 const auto orientation=pb::OrientationCallback::b05328;const auto publication=pb::OutputCallbacks::standard_95b490_95b940;
 auto seed=inspection_seed(actor.joint_count());const auto banks=bank_table();const auto step=inspection_step();
 auto unknown=get(na::Player::from_rest(actor,seed,constants,banks));
 const auto missing=unknown.step_position(step,orientation,publication);
 check(std::holds_alternative<na::PlayerError>(missing)&&std::get<na::PlayerError>(missing).position->field=="PBW10","Actor invented a position seed");
 check(unknown.completed_steps()==0&&unknown.position_fields()==seed.position_fields,"Missing bridge fields mutated actor");
 seed.position_fields={std::uint8_t(1),std::uint8_t(77),0U,0U,one,0U,std::uint16_t(4)};
 seed.shared_fields.pbw.field10_bits=std::bit_cast<std::uint32_t>(17.F);seed.shared_fields.pbw.field14_bits=std::bit_cast<std::uint32_t>(-23.F);seed.shared_fields.pbw.field18_bits=std::bit_cast<std::uint32_t>(91.F);seed.shared_fields.pbw.field1c_bits=0x7fc00001U;
 seed.controller.player.pbw.field20=0xabcd0000;seed.controller.player.pbw.field24=0x12340000;seed.controller.player.pbw.field28=0xffff0000;
 seed.controller.player.fieldsd10={std::bit_cast<std::uint32_t>(1.F),std::bit_cast<std::uint32_t>(2.F),std::bit_cast<std::uint32_t>(3.F)};
 seed.state1_fields.field50_bits=73;
 auto early_seed=seed;early_seed.position_fields.fieldd64=8;
 auto early=get(na::Player::from_rest(actor,early_seed,constants,banks));const auto before=get(early.capture());
 auto early_output=get(early.step_position(step));
 check(early_output.effects.early_return&&!early_output.effects.animation&&early.completed_steps()==0,"Early placement performed a frame step");
 check(early.shared_fields().pbw.field1c_bits==0&&early.state().controller.player==early_seed.controller.player&&early.position_fields().fieldd44==77,"Early placement lost raw field semantics");equal(before,early_output.frame);
 auto player=get(na::Player::from_rest(actor,seed,constants,banks));
 const pa::Command command=ps::CommonArguments{1,4,0,0,1,0};get(player.apply(command,step.templates));
 const auto unplaced=get(player.capture());const auto placed=get(player.step_position(step,orientation,publication));++frames;
 check(placed.effects.animation&&!placed.effects.publication_skipped&&player.completed_steps()==1,"Placement did not use complete frame/output path");
 check(placed.frame.scene.global_transforms!=unplaced.scene.global_transforms,"Placement failed to reach rendered matrices");
 const auto& raw=player.shared_fields().pbw;const std::array position{*raw.field10_bits,*raw.field14_bits,*raw.field18_bits};
 for(unsigned k=0;k<3;++k){const float expected=std::bit_cast<float>(position[k])+std::bit_cast<float>(player.state().controller.player.fieldsd10[k]);check(player.state().frame.model.position[k]==std::bit_cast<std::uint32_t>(expected),"Rendered model did not include presentation offset");}
 check(raw.field1c_bits==0&&player.position_fields().fieldd44==0&&player.state1_fields().field50_bits==73,"Placement lost shared or distinct owner fields");invariant(player,seed);
 auto twin=player;auto invalid=step;invalid.parent=jp::Unknown{};
 const auto failed=player.step_position(invalid,orientation,publication);
 check(std::holds_alternative<na::PlayerError>(failed)&&std::get<na::PlayerError>(failed).position->publication->phase==mh4u::model_animation_output::Phase::hierarchy,"Placement late failure lost typed owner");
 check(player.shared_fields()==twin.shared_fields()&&player.position_fields()==twin.position_fields()&&player.completed_steps()==twin.completed_steps(),"Late placement failure committed fields");equal(get(player.capture()),get(twin.capture()));
 equal(get(player.step_position(step,orientation,publication)).frame,get(twin.step_position(step,orientation,publication)).frame);++frames;
 auto flat_seed=seed;flat_seed.model.scale={0,0,0,0};auto flat=get(na::Player::from_rest(actor,flat_seed,constants,banks));const auto flat_before=get(flat.capture());
 const auto flat_result=flat.step_position(step,orientation,publication);
 check(std::holds_alternative<na::PlayerError>(flat_result)&&std::get<na::PlayerError>(flat_result).code==na::PlayerErrorCode::render,"Placement render failure was not propagated");
 check(flat.completed_steps()==0&&flat.position_fields()==flat_seed.position_fields&&flat.shared_fields()==flat_seed.shared_fields&&flat.state().controller.player==flat_seed.controller.player,"Skinning failure committed position/controller");equal(flat_before,get(flat.capture()));
 auto reset=get(na::Player::from_rest(player,inspection_seed(actor.joint_count())));check(!reset.position_fields().euler_mode&&!reset.shared_fields().pbw.field10_bits,"Reseed reused old placement facts");
}
void run(const char* recipe,const pt::Constants& constants){
 auto actor=get(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected real seven-piece hunter");const auto seed=inspection_seed(actor.joint_count());const auto banks=bank_table();const auto step=inspection_step();
 auto player=get(na::Player::from_rest(actor,seed,constants,banks));check(player.presentation()==actor.presentation(),"Presentation was copied into another owner");check(player.source_files().data()==actor.source_files().data(),"Source inventory was reparsed/copied");
 auto legacy=actor;const auto rest=get(legacy.evaluate(-1,0,false));equal(get(player.capture()),rest);check(player.completed_steps()==0,"Capture advanced time");
 const pa::Command command=ps::CommonArguments{1,4,0,0,1,0};
 auto first=get(player.tick(command,step));++frames;check(first.command&&first.command->selection_request->selector==8&&first.animation&&first.animation->prepared_layers==1,"Explicit state1 request did not traverse controller/frame");
 check(first.frame.scene.mesh.source_positions==4507&&first.frame.scene.mesh.triangles.size()/3==5043&&first.frame.scene.draws.size()==20,"Real multipart geometry changed");
 bool changed=false;for(unsigned i=0;i<12;++i){auto output=get(player.step(step));++frames;changed|=output.frame.scene.global_transforms!=first.frame.scene.global_transforms;invariant(player,seed);}
 check(changed,"Real joints never changed across original frame steps");
 auto twin=player;const auto before=get(player.capture());auto live=get(player.tick(command,step));++frames;live_queries+=live.command->queries;check(live.command->queries>=2&&live.command->samples>0,"Already-playing controller bypassed original sampling");equal(live.frame,get(twin.tick(command,step)).frame);invariant(player,seed);
 auto copied=player;auto failed_step=step;failed_step.parent=jp::Unknown{};auto failed=player.tick(command,failed_step);check(std::holds_alternative<na::PlayerError>(failed),"Unknown parent accepted");const auto& err=std::get<na::PlayerError>(failed);check(err.driver&&err.driver->phase==pa::Phase::hierarchy,"Late failure lost typed owner");check(player.completed_steps()==copied.completed_steps(),"Failed render chain advanced time");equal(get(player.capture()),get(copied.capture()));equal(get(player.step(step)).frame,get(copied.step(step)).frame);++frames;
 // A zero model scale reaches the shared skinning owner and is rejected after
 // successful controller/frame/hierarchy work. Nothing may publish on this error.
 auto flat_seed=seed;flat_seed.model.scale={0,0,0,0};auto flat=get(na::Player::from_rest(actor,flat_seed,constants,banks));const auto flat_before=get(flat.capture());
 auto flat_result=flat.tick(command,step);check(std::holds_alternative<na::PlayerError>(flat_result),"Degenerate skinned normal accepted");const auto& flat_error=std::get<na::PlayerError>(flat_result);check(flat_error.code==na::PlayerErrorCode::render&&flat_error.render&&flat_error.render->code==na::ErrorCode::vertex,"Post-driver failure was not a skinning error");check(flat.completed_steps()==0&&flat.state().frame.animation.layers[0].animation.layer.selector==0xffff&&flat.state().controller.player==flat_seed.controller.player&&flat.state().frame.animation.snapshots==flat_seed.snapshots,"Render failure published canonical state");equal(flat_before,get(flat.capture()));
 auto unsupported=player;auto rejected=player.apply(ps::CommonArguments{2,0,0,0,0,0},step.templates);check(std::holds_alternative<na::PlayerError>(rejected),"Unknown controller state accepted");equal(get(player.capture()),get(unsupported.capture()));
 // A valid controller-only operation never advances or resamples joint poses.
 const auto count=player.completed_steps();const auto view=get(player.capture());auto only=get(player.apply(ps::CommonArguments{3,0,0,0,0,0},step.templates));check(!only.animation&&only.command&&player.completed_steps()==count,"apply secretly advanced time");equal(view,only.frame);
 auto split=player;auto fused=player;get(split.apply(command,step.templates));const auto a=get(split.step(step));const auto b=get(fused.tick(command,step));equal(a.frame,b.frame);check(split.state().controller.player==fused.state().controller.player,"Split and fused controller differ");
 auto bad=seed;bad.snapshots.pop_back();check(std::holds_alternative<na::PlayerError>(na::Player::from_rest(actor,bad,constants,banks)),"Snapshot extent repaired silently");bad=seed;bad.layers[0].animation.tracks.joints.pop_back();check(std::holds_alternative<na::PlayerError>(na::Player::from_rest(actor,bad,constants,banks)),"Binding extent repaired silently");
 for(unsigned which=0;which<4;++which){pa::Limits limit;if(which==0)limit.joints=18;if(which==1)limit.snapshots=18;if(which==2)limit.order=18;if(which==3)limit.bindings=151;auto result=na::Player::from_rest(actor,seed,constants,banks,limit);check(std::holds_alternative<na::PlayerError>(result)&&std::get<na::PlayerError>(result).code==na::PlayerErrorCode::capacity,"Factory allocation bound missed");}
 auto invalid=banks;invalid[0]=na::MotionSource{std::string(64,'0')};check(std::holds_alternative<na::PlayerError>(na::Player::from_rest(actor,seed,constants,invalid)),"Unowned source accepted");invalid[0]=na::MotionSource{"short"};check(std::holds_alternative<na::PlayerError>(na::Player::from_rest(actor,seed,constants,invalid)),"Truncated source identity accepted");
 auto unknown=banks;unknown[0]=mh4u::motion_request::Unknown{};auto deferred=get(na::Player::from_rest(actor,seed,constants,unknown));check(std::holds_alternative<mh4u::motion_request::Unknown>(deferred.banks().banks[0]),"Unknown bank became absent");get(deferred.apply(ps::CommonArguments{3,0,0,0,0,0},step.templates));check(std::holds_alternative<na::PlayerError>(deferred.tick(command,step)),"Consumed unknown bank succeeded");
 auto null_bank=banks;null_bank[0]=mh4u::motion_request::Absent{};auto absent=get(na::Player::from_rest(actor,seed,constants,null_bank));const auto missing=get(absent.apply(command,step.templates));check(missing.command&&!missing.command->requests[0].accepted,"Explicit absent bank was treated as unknown");
 auto area_seed=seed;area_seed.controller.player.selection_pbw.field6f9=0;area_seed.controller.player.selection_pbw.field358=0x44160000;area_seed.controller.player.selection_pbw.field9=21;area_seed.controller.player.selection_pw.field7fc=0;
 auto contextual=get(na::Player::from_rest(actor,area_seed,constants,banks));const pa::Command state0=ps::CommonArguments{0,0,0,0,1,0};check(std::holds_alternative<na::PlayerError>(contextual.tick(state0,step)),"Missing consumed area mode became zero");
 {const ps::AreaContext area{0,{},{}};auto chosen=get(contextual.tick(state0,step,area));check(chosen.command&&chosen.command->selection_request->selector==1,"Borrowed area context did not reach selection");++frames;}
 get(contextual.step(step));++frames; // The per-call AreaContext is already gone.
 auto detached=[&](){auto temporary=get(na::Actor::load(recipe));return get(na::Player::from_rest(temporary,seed,constants,banks));}();get(detached.tick(command,step));++frames;check(detached.presentation()->images.size()==9,"Resource lifetime followed temporary Actor");
 auto movable=actor;auto retained=std::move(movable);check(retained.joint_count()==19&&std::holds_alternative<na::PlayerError>(na::Player::from_rest(movable,seed,constants,banks)),"Moved resource owner was dereferenced");
 // The named preview owns neither a legacy Actor nor a second persistent pose.
 auto preview=[&](){auto temporary=get(na::Actor::load(recipe));return get(na::PlayerPreview::from_actor(temporary,constants));}();
 check(preview.player().completed_steps()==0&&preview.player().state().frame.animation.layers[0].animation.layer.selector==0xffff,"Preview secretly bootstrapped an action");equal(get(preview.capture()).frame,rest);
 for(std::size_t i=1;i<16;++i)check(std::holds_alternative<mh4u::motion_request::Unknown>(preview.player().banks().banks[i]),"Preview guessed another source bank");
 auto requested=get(preview.request(1));check(requested.command&&!requested.animation&&preview.player().completed_steps()==0,"Preview request secretly advanced clock");
 auto preview_first=get(preview.step());check(preview_first.animation&&preview.player().completed_steps()==1,"Preview step did not advance exactly once");++frames;
 auto invalid_preview=preview.request(0);check(std::holds_alternative<na::PlayerError>(invalid_preview)&&preview.player().completed_steps()==1,"Preview guessed state0 initialization");equal(get(preview.capture()).frame,preview_first.frame);
 get(preview.step());++frames;auto stopped=get(preview.request(3));check(stopped.command&&!stopped.animation&&preview.player().completed_steps()==2,"Preview state3 implicitly stepped");
 const auto shared_presentation=preview.presentation();const auto reset=get(preview.reset());check(preview.presentation()==shared_presentation&&preview.player().completed_steps()==0&&!reset.command&&!reset.animation,"Preview reset replaced source ownership or performed hidden work");equal(reset.frame,rest);
 get(preview.request(1));equal(get(preview.step()).frame,preview_first.frame);++frames;
 check(std::holds_alternative<na::PlayerError>(na::PlayerPreview::from_actor(movable,constants)),"Preview dereferenced moved Actor");
 position_integration(actor,constants);
 equal(get(actor.evaluate(-1,0,false)),rest);check(before.scene.global_transforms.size()==19,"Original capture lost hierarchy");
}
}
int main(int argc,char** argv){try{if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=get(na::Actor::load(argv[2]));for(const auto& path:actor.source_files())std::cout<<path.generic_string()<<'\n';return 0;}check(argc==4,"Expected hunter recipe, constants and constant SHA");std::ifstream input(argv[2],std::ios::binary|std::ios::ate);check(bool(input)&&input.tellg()==88,"Constants extent");std::array<std::uint8_t,88> bytes{};input.seekg(0);check(bool(input.read(reinterpret_cast<char*>(bytes.data()),bytes.size()))&&input.peek()==std::char_traits<char>::eof(),"Constant read");auto result=pt::Constants::decode(bytes,{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"});check(std::holds_alternative<pt::Constants>(result),"Constants identity");run(argv[1],std::get<pt::Constants>(result));std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"frames\":"<<frames<<",\"live_queries\":"<<live_queries<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
