// Pure composition boundaries. Diagnostic inputs reuse the existing test seed;
// continuous original comparison and persistent rendering belong to other tests.
#include "player_animation.hpp"
#include "test_support/player_actor_state.hpp"
#include "test_support/player_update_dispatch_actor_fixture.hpp"
#include "test_support/player_update_notifications_actor_fixture.hpp"
#include "test_support/player_update_manager_actor_fixture.hpp"
#include "test_support/player_update_contact_actor_fixture.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
namespace pa=mh4u::player_animation;namespace pb=mh4u::player_position_bridge;
namespace jp=mh4u::joint_pose;namespace jo=mh4u::joint_output;namespace pt=mh4u::player_transition;
namespace ad=mh4u::player_update_admission;namespace ds=mh4u::player_update_dispatch;
namespace post=mh4u::player_update_post_dispatch;namespace direction=mh4u::player_update_direction;
namespace manager=mh4u::player_update_manager;namespace contact=mh4u::player_update_contact;
namespace pm=mh4u::player_manager_update;namespace oq=mh4u::player_object_query;
namespace wl=mh4u::player_world_lookup;namespace mf=mh4u::model_animation_frame;
namespace ta=mh4u::test_player_actor_state;
using U=std::uint32_t;constexpr U one=0x3f800000U;
constexpr jp::Bits4 identity{0,0,0,one};std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
U bits(float value){return std::bit_cast<U>(value);}
template<class T>std::string describe(const T&e){std::string out;
 if constexpr(requires{e.field;})out+=std::string(e.field);
 if constexpr(requires{e.message;})out+=std::string(e.message);
 if constexpr(requires{e.phase;})out+=" phase="+std::to_string(unsigned(e.phase));
 if constexpr(requires{e.cause.index();})out+=std::visit([](const auto&v){return " -> "+describe(v);},e.cause);
 if constexpr(requires{e.animation;})if(e.animation)out+=" -> "+describe(*e.animation);
 if constexpr(requires{e.pending;})if(e.pending)out+=" -> "+describe(*e.pending);
 return out;}
template<class T,class...V>T take(std::variant<V...> result){if(auto*p=std::get_if<T>(&result))return std::move(*p);throw std::runtime_error(std::visit([](const auto&e){return describe(e);},result));}
const auto& candidate(const pa::UpdateModelOutput&o){return o.contacted.managed.positioned.directed.notified.status.adjusted.dispatched;}
const auto& candidate(const pa::UpdateContactOutput&o){return o.managed.positioned.directed.notified.status.adjusted.dispatched;}
const pb::Error& bridge_error(const pa::Error&e){check(e.phase==pa::Phase::update_model,"Lost bridge phase");return std::get<pb::Error>(e.cause);}
void run(const pt::Constants& constants,const std::array<std::uint8_t,42>&caps,const std::string&caps_sha,
 const mh4u::player_policy_table::Table&t35,const mh4u::player_policy_table::Table&t31){
 using namespace mh4u::test_dispatch_actor_fixture;
 auto seed=adjustment_seed();mh4u::test_notifications_actor_fixture::notification_inputs(seed);
 mh4u::test_manager_actor_fixture::manager_inputs(seed);mh4u::test_contact_actor_fixture::contact_inputs(seed);
 // Explicit zero-joint model exercises the real clock without a duplicate pose.
 pa::State source{};source.controller=seed.controller;source.frame.model=seed.model;
 source.frame.animation.layers.assign(seed.layers.begin(),seed.layers.begin()+2);
 std::copy(seed.layers.begin()+2,seed.layers.end(),source.inactive.begin());
 for(auto& l:source.frame.animation.layers){l.animation.tracks.joints.clear();l.animation.layer.flags=0;l.animation.layer.selector=0;l.animation.layer.end_frame=30;}
 source.frame.animation.layers[0].animation.layer.frame=5;
 source.hierarchy.order=std::vector<std::uint8_t>{};source.hierarchy.flags4=0x55;
 source.hierarchy.attachment_mode_high16=0x3210;
 source.controller.player.fieldsd10={bits(2),bits(3),bits(4)};
 auto fields=seed.shared_fields;fields.pbw.field10_bits=bits(10);fields.pbw.field14_bits=bits(20);fields.pbw.field18_bits=bits(30);
 pb::Fields position{0xa5,0x91,0U,0U,one,2U,4};
 auto objects=dispatch_collection();objects.data[0].field530=1;
 auto queue=mh4u::test_notifications_actor_fixture::queue_seed();
 auto m=mh4u::test_manager_actor_fixture::manager_seed();m.field112b=1;
 wl::Manager world;world.head=wl::Absent{};
 ad::Parameters admission;admission.prefix={{},one,{&world,1}};
 admission.virtual120=ad::Virtual120::uplayer_b3dd60;preserve_input(admission);
 admission.reference=mh4u::player_reference::Points{0,0,0,1,0,one};admission.global_c8c=19;admission.global_c90=0;
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);
 mh4u::motion_request::Model model;model.skeleton_present=true;model.banks.fill(mh4u::motion_request::Absent{});
 mh4u::motion_command::Globals globals{};mh4u::player_state1::Tables tables{t35,t31};
 mh4u::motion_command::Sampling sampling{{jp::Bits3{},jp::Bits3{},identity,identity},{}};
 ds::Resources resources{tables,model,globals,sampling};
 const ds::Context dispatch{{admission,parameters.context,constants},resources};
 const post::Context adjustment{dispatch,post::Virtual10c::uplayer_b25b68};
 const direction::Context directed{adjustment,{mh4u::player_direction_refresh::Virtual84::uplayer_b085bc,mh4u::player_direction_refresh::Virtual144::uplayer_b47aa0}};
 const manager::Context managed{directed,pm::ManagerView{&m,pm::Unknown{}}};
 const contact::Context contacted{managed,{}};
 pa::FrameResources frame;frame.model_reference=jp::PresentModel{std::span<const jp::Matrix>{}};
 frame.parent=jp::Null{};frame.reference_basis=mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};frame.reference_translation=identity;
 const pa::UpdateModelContext context{contacted,frame,pb::OrientationCallback::b05328,pb::OutputCallbacks::standard_95b490_95b940};
 const auto source_before=source;const auto fields_before=fields;const auto objects_before=objects;const auto queue_before=queue;const auto m_before=m;const auto position_before=position;
 auto call=[&](const pa::State&s,const mh4u::player_fields::Fields&f,const pb::Fields&p,const pa::UpdateModelContext&c,pa::Limits limits={}){
  return pa::update_model(s,seed.state1_fields,f,p,&objects,c,queue,limits);};
 auto baseline=take<pa::UpdateContactOutput>(pa::update_contact(source,seed.state1_fields,fields,position,&objects,contacted,queue));
 const auto& bc=candidate(baseline);
 auto result=take<pa::UpdateModelOutput>(call(source,fields,position,context));const auto&out=candidate(result);
 check(result.effects.animation&&result.effects.publication_skipped&&!result.effects.early_return,"Reached real frame, skipped output");
 check(bc.state.frame.animation.layers[0].animation.layer.frame==5&&out.state.frame.animation.layers[0].animation.layer.frame==8,"Candidate speed and source delta advance clock exactly once");
 check(out.effects.dispatch_count==bc.effects.dispatch_count&&result.contacted.managed.positioned.directed.notified.queue==baseline.managed.positioned.directed.notified.queue,"Prefix not repeated or lost");
 check(result.contacted.managed.positioned.directed.notified.queue.counter==2,"Notifications executed once");
 check(out.state.frame.model.position[0]==bc.shared_fields.pbw.field10_bits&&out.shared_fields.pbw.field1c_bits==0,"Bridge consumes contact PBW candidate and writes suffix");
 check(out.state.hierarchy.attachment_mode_high16==0x3210&&result.contacted.position.euler_mode==4,"Separate model38 owners");
 for(U i=0;i<6;++i)check(ta::layer_equal(source.inactive[i],out.state.inactive[i]),"Inactive raw slots preserved");
 // The legacy bridge and new source binder share the same arithmetic body.
 pa::FrameContext legacy{frame.bone_to_joint,{frame.model_reference,{one,resources.sampling.templates},frame.restoration_scale},frame.resolvers,{frame.model_reference,frame.parent,frame.reference_basis,frame.reference_translation}};
 auto independent=take<pb::Output>(pb::apply(bc.state.frame,bc.state.hierarchy,bc.state.controller,bc.shared_fields,baseline.position,{legacy,context.orientation,context.publication}));
 auto expected=bc.state;expected.frame=std::move(independent.frame);expected.hierarchy=std::move(independent.hierarchy);expected.controller=std::move(independent.controller);
 check(ta::state_equal(expected,out.state)&&independent.shared==out.shared_fields&&independent.fields==result.contacted.position,"Single bridge composition matches existing owner");
 admission.prefix.delta_bits=bits(.5F);
 auto half=take<pa::UpdateModelOutput>(call(source,fields,position,context));
 check(candidate(half).state.frame.animation.layers[0].animation.layer.frame==6.5F,"Only canonical prefix delta drives frame clock");
 admission.prefix.delta_bits=one;
 auto pending=source;pending.frame.animation.layers[0].animation.layer.flags|=2;
 std::size_t action_queries=0;
 frame.resolvers.actions={&action_queries,[](const void*opaque,U bank,U slot)->mh4u::motion_pending::ActionResolution{
  ++*static_cast<std::size_t*>(const_cast<void*>(opaque));check(bank==0&&slot==0,"Pending query uses candidate selector");return std::optional<mh4u::motion_binding::Action>{};}};
 resources.sampling.templates.previous_translation_10e6370=jp::Bits3{bits(7),bits(8),bits(9)};
 auto reset=take<pa::UpdateModelOutput>(call(pending,fields,position,context));
 check(action_queries==1&&reset.effects.animation&&reset.effects.animation->initializer_calls==1,"Pending initialization executes exactly once");
 check(candidate(reset).state.frame.animation.layers[0].animation.base_translation==jp::Bits4{bits(7),bits(8),bits(9),0},"Frame reset borrows preceding sampling templates");
 resources.sampling.templates.previous_translation_10e6370.reset();
 const auto missing_template=bridge_error(take<pa::Error>(call(pending,fields,position,context)));
 check(missing_template.animation&&missing_template.animation->pending&&missing_template.animation->pending->code==mh4u::motion_pending::ErrorCode::missing_template,"Missing canonical template stays typed");
 resources.sampling.templates.previous_translation_10e6370=jp::Bits3{};frame.resolvers.actions={};
 auto restricted=source;restricted.controller.motion.fieldsd48[6]|=0x8000;
 check(bridge_error(take<pa::Error>(call(restricted,fields,position,context))).field=="b06b38","Active unsupported bridge route is preserved");
 auto published=position;published.fieldd64=0;auto full=take<pa::UpdateModelOutput>(call(source,fields,published,context));
 check(!full.effects.publication_skipped&&full.effects.animation&&full.effects.animation->output_callback,"Publication owner executed");
 check(candidate(full).state.frame.model.position[0]==bits(12)&&candidate(full).shared_fields.pbw.field10_bits==bits(10),"Publication offset excludes PBW");
 auto missing=context;missing.publication.reset();check(bridge_error(take<pa::Error>(call(source,fields,published,missing))).field=="virtual4c/50","Late publication unknown remains typed");
 auto missing_frame=frame;missing_frame.parent=jp::Unknown{};// References stay bound; use a separate aggregate below.
 const pa::UpdateModelContext bad_parent{contacted,missing_frame,context.orientation,context.publication};
 check(bridge_error(take<pa::Error>(call(source,fields,published,bad_parent))).code==pb::ErrorCode::publication,"Nested hierarchy error preserved");
 auto early=position;early.fieldd64=8;early.fieldd30_bits.reset();early.fieldd34_bits.reset();early.fieldd38_bits.reset();early.euler_mode.reset();
 pa::FrameResources unknown_frame;const pa::UpdateModelContext lazy{contacted,unknown_frame,{},{}};
 auto stopped=take<pa::UpdateModelOutput>(call(source,fields,early,lazy));
 check(stopped.effects.early_return&&!stopped.effects.animation&&candidate(stopped).state.frame.model==bc.state.frame.model,"Early bridge omits all frame resources");
 check(candidate(stopped).state.frame.animation.layers[0].animation.layer.frame==5&&candidate(stopped).shared_fields.pbw.field1c_bits==0,"Early return preserves clock and suffix");
 // Standalone source binding proves unknown delta laziness independently of
 // earlier phases that may consume Player14 before this caller checkpoint.
 const mh4u::motion_initialization::Templates unknown_templates{};
 const pb::SourceContext unknown_delta{unknown_frame,{},unknown_templates,{},{}};
 auto skipped=take<pb::Output>(pb::apply(bc.state.frame,bc.state.hierarchy,bc.state.controller,bc.shared_fields,early,unknown_delta));
 check(skipped.effects.early_return,"Early bridge does not invent or demand delta");
 auto reached=unknown_delta;reached.orientation=context.orientation;
 check(take<pb::Error>(pb::apply(bc.state.frame,bc.state.hierarchy,bc.state.controller,bc.shared_fields,position,reached)).field=="Player14","Reached unknown delta is typed");
 auto bad=source;bad.frame.animation.layers.resize(1);check(take<pa::Error>(call(bad,fields,position,context)).phase==pa::Phase::domain,"2+6 domain validated before prefix");
 pa::Limits bounds;bounds.command.storage_layers=7;check(take<pa::Error>(call(source,fields,position,context,bounds)).phase==pa::Phase::controller,"Storage extent validated before projection");
 bounds={};bounds.frame.layers=1;check(bridge_error(take<pa::Error>(call(source,fields,position,context,bounds))).code==pb::ErrorCode::resource_limit,"Bridge copy budget enforced");
 auto failure_fields=fields;failure_fields.pbw.field01.reset();check(take<pa::Error>(call(source,failure_fields,position,context)).phase==pa::Phase::update_contact,"Earlier contact error identity unchanged");
 // Prefix success is not published if a later bridge field is unknown.
 auto missing_timer=position;missing_timer.fieldd30_bits.reset();check(bridge_error(take<pa::Error>(call(source,fields,missing_timer,context))).field=="d30","Late timer unknown after candidate prefix");
 check(ta::state_equal(source,source_before)&&fields==fields_before&&objects==objects_before&&queue==queue_before&&m==m_before&&position==position_before,"All borrowed owners unchanged after successes and failures");
}
std::vector<std::uint8_t> read(const char*path,std::size_t n){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==static_cast<std::streamoff>(n),"Source extent");std::vector<std::uint8_t>b(n);f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(b.data()),n))&&f.peek()==std::char_traits<char>::eof(),"Source read");return b;}
}
int main(int argc,char**argv){try{check(argc==9,"usage: model-boundaries constants sha caps sha table35 sha table31 sha");
 auto constants=take<pt::Constants>(pt::Constants::decode(read(argv[1],88),{argv[2],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));
 auto bytes=read(argv[3],42);check(picosha2::hash256_hex_string(bytes)==argv[4],"Caps hash");std::array<std::uint8_t,42>caps{};std::copy(bytes.begin(),bytes.end(),caps.begin());
 auto t35=take<mh4u::player_policy_table::Table>(mh4u::player_policy_table::Table::decode(read(argv[5],792),argv[6]));auto t31=take<mh4u::player_policy_table::Table>(mh4u::player_policy_table::Table::decode(read(argv[7],704),argv[8]));
 run(constants,caps,argv[4],t35,t31);std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
