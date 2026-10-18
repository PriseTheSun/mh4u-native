// Transactional model checkpoint with real hunter geometry. Explicit
// diagnostic contexts; original ARM comparison is a separate test.
#include "player_actor.hpp"
#include "test_support/player_update_model_actor_fixture.hpp"
#include "test_support/player_update_manager_actor_fixture.hpp"
#include "test_support/player_update_contact_actor_fixture.hpp"
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
std::size_t checks{},allocation_failures{},first_render_failure{},render_failures{},palette_failures{},allocation_extent{};
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
namespace composed=mh4u::player_update_contact;
namespace contact=mh4u::player_contact_update;
namespace pm=mh4u::player_manager_update;
namespace pr=mh4u::player_reference;
namespace direction=mh4u::player_direction_refresh;
using mh4u::test_update_actor_fixture::banks;
using namespace mh4u::test_dispatch_actor_fixture;
using mh4u::test_notifications_actor_fixture::queue_seed;
using mh4u::test_notifications_actor_fixture::notification_inputs;
constexpr direction::Bindings concrete{direction::Virtual84::uplayer_b085bc,direction::Virtual144::uplayer_b47aa0};
using mh4u::test_manager_actor_fixture::manager_seed;
using mh4u::test_manager_actor_fixture::manager_inputs;

using mh4u::test_contact_actor_fixture::contact_inputs;
namespace bridge=mh4u::player_position_bridge;
using mh4u::test_model_actor_fixture::model_context;
using mh4u::test_model_actor_fixture::model_inputs;
const bridge::Error& model_error(const na::PlayerError& e){
 check(e.driver&&e.driver->phase==pa::Phase::update_model,"Lost model driver cause");
 return std::get<bridge::Error>(e.driver->cause);
}
void run(const char* recipe,const pt::Constants& constants,const std::array<std::uint8_t,42>& caps,const std::string& caps_sha,
 const tbl::Table& table35,const tbl::Table& table31,const mh4u::collision_resource::Resource& collision){
 auto actor=value<na::Actor>(na::Actor::load(recipe));check(actor.part_count()==7&&actor.joint_count()==19,"Expected source hunter geometry");
 const auto make=[&](const na::PlayerInitial& initial){return value<na::Player>(na::Player::from_rest(actor,initial,constants,banks()));};
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};admission.virtual120=ad::Virtual120::uplayer_b3dd60;
 preserve_input(admission);admission.reference=pr::Points{0,0,0,1,bits(0),bits(1)};admission.global_c8c=19;admission.global_c90=0;
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);
 mh4u::player_state1::Tables tables{table35,table31};mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,identity};
 na::PlayerUpdateDispatchContext context{admission,parameters.context,tables,templates};
 const auto model=model_context();
 auto initial=adjustment_seed();notification_inputs(initial);manager_inputs(initial);contact_inputs(initial);model_inputs(initial);
 initial.shared_fields.pbw.field10_bits=bits(17);initial.shared_fields.pbw.field14_bits=bits(23);initial.shared_fields.pbw.field18_bits=bits(91);
 const auto objects_seed=[](){auto x=dispatch_collection();x.data[0].field530=1;x.data[0].position_bits[1]=0;return x;};
 auto subject=make(initial);auto objects=objects_seed();auto queue=queue_seed();auto manager=manager_seed();
 const auto before=save(subject);const auto frame=value<na::Frame>(subject.capture());
 const auto out=value<na::PlayerUpdateModelOutput>(subject.prepare_and_update_model(objects,queue,context,concrete,binding,{manager,&subject},model));
 check(out.effects.animation&&!out.effects.early_return&&!out.effects.publication_skipped,"Model frame/publication was skipped");
 check(subject.completed_steps()==1&&out.contacted.managed.positioned.frame.tick==1,"Model frame counter did not advance exactly once");
 check(subject.position_fields().fieldd30_bits==bits(3)&&subject.position_fields().fieldd44==0,"Model did not use sole admission delta");
 check(subject.shared_fields().pbw.field1c_bits==0&&queue.counter==2&&manager.fields1050[4]==1,"Earlier candidate did not publish exactly once");
 check(out.contacted.managed.positioned.frame.scene.global_transforms!=frame.scene.global_transforms,"Model placement never reached actual rendered geometry");
 const auto& pbw=subject.shared_fields().pbw;
 const std::array position{*pbw.field10_bits,*pbw.field14_bits,*pbw.field18_bits};
 for(unsigned i=0;i<3;++i)check(subject.state().frame.model.position[i]==bits(std::bit_cast<float>(position[i])+float(i+1)),"Model did not consume contact/manager position and publication offset");
 for(unsigned i=0;i<6;++i)check(mh4u::test_player_actor_state::layer_equal(subject.state().inactive[i],before.state.inactive[i]),"Model processed inactive poison slot");
 // Admission delta is the only exposed clock: changing it affects the bridge timer.
 auto half=admission;half.prefix.delta_bits=bits(.5f);na::PlayerUpdateDispatchContext half_context{half,parameters.context,tables,templates};
 auto half_subject=make(initial);auto half_objects=objects_seed();auto half_queue=queue_seed();auto half_manager=manager_seed();
 (void)value<na::PlayerUpdateModelOutput>(half_subject.prepare_and_update_model(half_objects,half_queue,half_context,concrete,binding,{half_manager,&half_subject},model));
 check(half_subject.position_fields().fieldd30_bits==bits(3.5f),"Composed model consumed a different clock");
 // Source early return and output gate do not invent callback requirements.
 auto gated=initial;gated.position_fields.fieldd64=8;gated.position_fields.fieldd30_bits.reset();gated.position_fields.euler_mode.reset();
 auto early=make(gated);auto early_objects=objects_seed();auto early_queue=queue_seed();auto early_manager=manager_seed();
 const auto early_out=value<na::PlayerUpdateModelOutput>(early.prepare_and_update_model(early_objects,early_queue,context,concrete,binding,{early_manager,&early},{}));
 check(early_out.effects.early_return&&!early_out.effects.animation&&early.completed_steps()==0&&early.shared_fields().pbw.field1c_bits==0,"Early model gate consumed frame context or advanced clock");
 auto skipped=initial;skipped.position_fields.fieldd64=2;auto skip_model=model;skip_model.publication.reset();skip_model.parent=jp::Unknown{};
 auto skip=make(skipped);auto skip_objects=objects_seed();auto skip_queue=queue_seed();auto skip_manager=manager_seed();
 const auto skipped_out=value<na::PlayerUpdateModelOutput>(skip.prepare_and_update_model(skip_objects,skip_queue,context,concrete,binding,{skip_manager,&skip},skip_model));
 check(skipped_out.effects.animation&&skipped_out.effects.publication_skipped&&skip.completed_steps()==1,"Skipped publication lost completed model frame");
 // Two real spatial queries finish before a late model error; none of those
 // candidate owners may publish, and a retry must reuse the same owners.
 auto spatial_initial=spatial_seed(2971.5546875f);post_inputs(spatial_initial);notification_inputs(spatial_initial);contact_inputs(spatial_initial,true);model_inputs(spatial_initial);
 spatial_initial.shared_fields.pw.fieldb8_bits=0;spatial_initial.shared_fields.pw.fieldb4=0;
 auto spatial=make(spatial_initial);auto scene=spatial_scene(collision,check);auto spatial_objects=spatial_collection();spatial_objects.data[0].field530=1;auto spatial_queue=queue_seed();pm::ManagerState spatial_manager;spatial_manager.field112b=1;
 na::PlayerUpdateDispatchContext spatial_context{admission,parameters.context,tables,templates,{}, {},&scene,0};
 const auto spatial_before=save(spatial);const auto old_scene=scene;const auto old_objects=spatial_objects;const auto old_queue=spatial_queue;const auto old_manager=spatial_manager;
 auto invalid=model;invalid.publication.reset();
 const auto rejected=value<na::PlayerError>(spatial.prepare_and_update_model(spatial_objects,spatial_queue,spatial_context,concrete,binding,{spatial_manager,pm::Unknown{}},invalid));
 check(model_error(rejected).field=="virtual4c/50","Model failure did not follow real frame/contact");
 assertions.unchanged(spatial,spatial_before);assertions.grids_equal(scene,old_scene);check(spatial_objects==old_objects&&spatial_queue==old_queue&&spatial_manager==old_manager,"Late model failure published preceding owners");
 const auto retry=value<na::PlayerUpdateModelOutput>(spatial.prepare_and_update_model(spatial_objects,spatial_queue,spatial_context,concrete,binding,{spatial_manager,pm::Unknown{}},model));
 check(retry.contacted.effects.height&&scene.grid_states[0].counter==2&&spatial_queue.counter==2&&spatial.completed_steps()==1,"Model retry replayed or discarded earlier phases");
 // A foreign Player and both geometry renders share the same commit. Inject
 // every allocation failure and retry the exact owners after each failure.
 auto combined=initial;contact_inputs(combined,true);combined.controller.player.selection_pbw.field9=3;
 combined.shared_fields.pbw.field10_bits=bits(-6662.23779296875f);combined.shared_fields.pbw.field14_bits=bits(2971.5546875f);combined.shared_fields.pbw.field18_bits=bits(-1482.9044189453125f);
 auto measured=make(combined);auto peer=make(initial);auto mc=objects_seed();auto mq=queue_seed();auto mm=manager_seed();auto ms=spatial_scene(collision,check);
 na::PlayerUpdateDispatchContext measured_context{admission,parameters.context,tables,templates,{}, {},&ms,0};
 std::optional<na::PlayerResult<na::PlayerUpdateModelOutput>> measured_result;
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_update_model(mc,mq,measured_context,concrete,binding,{mm,&peer},model));allocation_extent=allocation_probe::calls;}
 const auto measured_out=value<na::PlayerUpdateModelOutput>(std::move(*measured_result));
 check(measured_out.contacted.managed.foreign_frame&&measured_out.contacted.effects.height&&measured_out.effects.animation&&allocation_extent>0,"Allocation trace omitted a required stage");
 for(std::size_t ordinal=1;ordinal<=allocation_extent;++ordinal){
  auto caller=make(combined);auto target=make(initial);auto coll=objects_seed();auto q=queue_seed();auto manager0=manager_seed();auto scene0=spatial_scene(collision,check);
  na::PlayerUpdateDispatchContext call_context{admission,parameters.context,tables,templates,{}, {},&scene0,0};
  const auto saved=save(caller),saved_target=save(target);const auto saved_coll=coll;const auto saved_q=q;const auto saved_manager=manager0;const auto saved_scene=scene0;
  const auto saved_frame=value<na::Frame>(caller.capture()),saved_target_frame=value<na::Frame>(target.capture());
  std::optional<na::PlayerResult<na::PlayerUpdateModelOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(caller.prepare_and_update_model(coll,q,call_context,concrete,binding,{manager0,&target},model));}
  const auto error=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(error.code==na::PlayerErrorCode::capacity||error.code==na::PlayerErrorCode::driver||error.code==na::PlayerErrorCode::render,"Allocation error lost its owner");
  if(error.code==na::PlayerErrorCode::render){check(error.render.has_value(),"Missing nested renderer cause");if(error.render->code==na::ErrorCode::palette)++palette_failures;++render_failures;if(!first_render_failure)first_render_failure=ordinal;}
  assertions.unchanged(caller,saved);assertions.unchanged(target,saved_target);assertions.grids_equal(scene0,saved_scene);
  check(coll==saved_coll&&q==saved_q&&manager0==saved_manager,"Allocation failure published external owners");
  assertions.frame_unchanged(value<na::Frame>(caller.capture()),saved_frame);assertions.frame_unchanged(value<na::Frame>(target.capture()),saved_target_frame);
  const auto recovered=value<na::PlayerUpdateModelOutput>(caller.prepare_and_update_model(coll,q,call_context,concrete,binding,{manager0,&target},model));
  check(recovered.contacted.managed.foreign_frame&&recovered.contacted.effects.height&&q.counter==2&&scene0.grid_states[0].counter==1&&manager0.fields1050[4]==1,"Same-owner retry split commits");
  check(caller.position_fields().fieldd44==0&&caller.completed_steps()==1&&target.completed_steps()==0,"Retry advanced model more than once or advanced foreign Player");
 }
 check(allocation_failures==allocation_extent&&render_failures>=2&&first_render_failure<allocation_extent,"Allocation coverage incomplete");
 check(picosha2::hash256_hex_string(parameters.record)==parameters.record_sha&&picosha2::hash256_hex_string(caps)==caps_sha,"Borrowed inputs mutated");
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
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"allocation_failures\":"<<allocation_failures<<",\"first_render_failure\":"<<first_render_failure<<",\"render_failures\":"<<render_failures<<",\"palette_failures\":"<<palette_failures<<",\"allocation_extent\":"<<allocation_extent<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
