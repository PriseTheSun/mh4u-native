// Transactional contact checkpoint with real hunter geometry. Explicit
// diagnostic contexts; original ARM comparison is a separate test.
#include "player_actor.hpp"
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
const contact::Error& contact_error(const na::PlayerError& e){
 check(e.driver&&e.driver->phase==pa::Phase::update_contact,"Lost contact driver cause");
 const auto& cause=std::get<composed::Error>(e.driver->cause);
 check(cause.phase==composed::Phase::contact,"Lost contact checkpoint phase");
 return std::get<contact::Error>(cause.cause);
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
 auto initial=adjustment_seed();notification_inputs(initial);manager_inputs(initial);contact_inputs(initial);
 const auto objects_seed=[](){auto x=dispatch_collection();x.data[0].field530=1;x.data[0].position_bits[1]=0;return x;};
 auto subject=make(initial);auto objects=objects_seed();auto queue=queue_seed();auto manager=manager_seed();
 const auto before=save(subject);const auto frame=value<na::Frame>(subject.capture());
 const auto out=value<na::PlayerUpdateContactOutput>(subject.prepare_and_update_contact(objects,queue,context,concrete,binding,{manager,&subject}));
 check(out.managed.effects.caller_vertical_written&&!out.effects.height,"Same-owner manager or lazy contact changed");
 check(subject.position_fields().fieldd2c==0xa5&&subject.position_fields().fieldd44==0x91&&subject.shared_fields().pbw.field6fb==0,"Position owner or caller clear not committed");
 check(subject.shared_fields().pw.field9e9==0&&queue.counter==2&&manager.fields1050[4]==1,"Earlier manager/queue publication was lost");
 assertions.pose_equal(subject,before);assertions.frame_equal(out.managed.positioned.frame,frame);
 // A missing late field must discard both a foreign manager patch and queue.
 auto missing=initial;missing.shared_fields.pbw.field01.reset();auto tested=make(missing);auto peer=make(initial);
 auto collection=objects_seed();auto pending=queue_seed();auto m=manager_seed();
 const auto old=save(tested),old_peer=save(peer);const auto old_collection=collection;const auto old_pending=pending;const auto old_manager=m;
 const auto rejected=value<na::PlayerError>(tested.prepare_and_update_contact(collection,pending,context,concrete,binding,{m,&peer}));
 check(contact_error(rejected).field=="pbw+1","Did not reach late permission read");
 assertions.unchanged(tested,old);assertions.unchanged(peer,old_peer);check(collection==old_collection&&pending==old_pending&&m==old_manager,"Late contact failure published preceding owners");
 // Two real queries share the same grid candidate; the source counter reaches2.
 auto spatial_initial=spatial_seed(2971.5546875f);post_inputs(spatial_initial);notification_inputs(spatial_initial);contact_inputs(spatial_initial,true);
 spatial_initial.shared_fields.pw.fieldb8_bits=0;spatial_initial.shared_fields.pw.fieldb4=0;
 auto scene=spatial_scene(collision,check);na::PlayerUpdateDispatchContext spatial_context{admission,parameters.context,tables,templates,{}, {},&scene,0};
 auto spatial_player=make(spatial_initial);auto spatial_objects=spatial_collection();spatial_objects.data[0].field530=1;auto spatial_queue=queue_seed();pm::ManagerState spatial_manager;spatial_manager.field112b=1;
 const auto spatial_out=value<na::PlayerUpdateContactOutput>(spatial_player.prepare_and_update_contact(spatial_objects,spatial_queue,spatial_context,concrete,binding,{spatial_manager,pm::Unknown{}}));
 const auto& dispatch=spatial_out.managed.positioned.dispatch;
 check(dispatch.dispatch_count>0&&dispatch.dispatches[0].effects.spatial&&dispatch.dispatches[0].effects.spatial->height,"Earlier dispatch did not query actual SBC");
 check(dispatch.dispatches[0].effects.spatial->height->query.grid_updates[0].state.counter==1,"Earlier query effect was overwritten");
 check(spatial_out.effects.height&&spatial_out.effects.height->query.grid_updates[0].state.counter==2&&scene.grid_states[0].counter==2,"Contact did not consume preceding grid candidate");
 check(spatial_queue.counter==2&&spatial_objects.data[0].field349==1,"Two-query commit lost earlier owners");
 // Fail after both queries, then retry the same Scene and external owners.
 auto bad_spatial=spatial_initial;auto missing_global=admission;missing_global.global_c90.reset();
 auto sp=make(bad_spatial);auto sc=spatial_scene(collision,check);auto so=spatial_collection();so.data[0].field530=1;auto sq0=queue_seed();pm::ManagerState sm;sm.field112b=1;
 na::PlayerUpdateDispatchContext scontext{missing_global,parameters.context,tables,templates,{}, {},&sc,0};
 const auto sp_old=save(sp);const auto sc_old=sc;const auto so_old=so;const auto sq_old=sq0;const auto sm_old=sm;
 const auto failure=value<na::PlayerError>(sp.prepare_and_update_contact(so,sq0,scontext,concrete,binding,{sm,pm::Unknown{}}));
 check(contact_error(failure).field=="global+c90","Spatial failure did not follow contact");
 assertions.unchanged(sp,sp_old);assertions.grids_equal(sc,sc_old);check(so==so_old&&sq0==sq_old&&sm==sm_old,"Late failure published cumulative grid candidate");
 missing_global.global_c90=0;
 const auto spatial_retry=value<na::PlayerUpdateContactOutput>(sp.prepare_and_update_contact(so,sq0,scontext,concrete,binding,{sm,pm::Unknown{}}));
 check(spatial_retry.effects.height&&sc.grid_states[0].counter==2&&sq0.counter==2,"Same owners did not recover after late contact rejection");
 // Complete allocation trace combines a new height query, a foreign patch,
 // queue/collection changes and both actual geometry renders. Every failure
 // retries the exact same owners with the allocator restored.
 auto combined=initial;contact_inputs(combined,true);combined.controller.player.selection_pbw.field9=3;
 combined.shared_fields.pbw.field10_bits=bits(-6662.23779296875f);combined.shared_fields.pbw.field14_bits=bits(2971.5546875f);combined.shared_fields.pbw.field18_bits=bits(-1482.9044189453125f);
 auto measured=make(combined);auto measured_peer=make(initial);auto measured_objects=objects_seed();auto measured_queue=queue_seed();auto measured_manager=manager_seed();auto measured_scene=spatial_scene(collision,check);
 na::PlayerUpdateDispatchContext measured_context{admission,parameters.context,tables,templates,{}, {},&measured_scene,0};
 std::optional<na::PlayerResult<na::PlayerUpdateContactOutput>> measured_result;
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_update_contact(measured_objects,measured_queue,measured_context,concrete,binding,{measured_manager,&measured_peer}));allocation_extent=allocation_probe::calls;}
 const auto measured_out=value<na::PlayerUpdateContactOutput>(std::move(*measured_result));
 check(measured_out.managed.foreign_frame&&measured_out.effects.height&&allocation_extent>0,"Combined allocation measurement missed grid or foreign Player");
 for(std::size_t ordinal=1;ordinal<=allocation_extent;++ordinal){
  auto caller=make(combined);auto target=make(initial);auto coll=objects_seed();auto q=queue_seed();auto manager0=manager_seed();auto scene0=spatial_scene(collision,check);
  na::PlayerUpdateDispatchContext call_context{admission,parameters.context,tables,templates,{}, {},&scene0,0};
  const auto saved=save(caller),saved_target=save(target);const auto saved_coll=coll;const auto saved_q=q;const auto saved_manager=manager0;const auto saved_scene=scene0;
  const auto saved_frame=value<na::Frame>(caller.capture()),saved_target_frame=value<na::Frame>(target.capture());
  std::optional<na::PlayerResult<na::PlayerUpdateContactOutput>> failed;
  {allocation_probe::Scope scope(ordinal);failed.emplace(caller.prepare_and_update_contact(coll,q,call_context,concrete,binding,{manager0,&target}));}
  const auto error=value<na::PlayerError>(std::move(*failed));++allocation_failures;
  check(error.code==na::PlayerErrorCode::capacity||error.code==na::PlayerErrorCode::driver||error.code==na::PlayerErrorCode::render,"Allocation failure category changed");
  if(error.code==na::PlayerErrorCode::render){check(error.render.has_value(),"Missing nested renderer cause");if(error.render->code==na::ErrorCode::palette)++palette_failures;++render_failures;if(!first_render_failure)first_render_failure=ordinal;}
  assertions.unchanged(caller,saved);assertions.unchanged(target,saved_target);assertions.grids_equal(scene0,saved_scene);
  check(coll==saved_coll&&q==saved_q&&manager0==saved_manager,"Allocation failure published external owners");
  assertions.frame_unchanged(value<na::Frame>(caller.capture()),saved_frame);assertions.frame_unchanged(value<na::Frame>(target.capture()),saved_target_frame);
  const auto retry=value<na::PlayerUpdateContactOutput>(caller.prepare_and_update_contact(coll,q,call_context,concrete,binding,{manager0,&target}));
  check(retry.managed.foreign_frame&&retry.effects.height&&q.counter==2&&scene0.grid_states[0].counter==1&&manager0.fields1050[4]==1,"Same-owner allocation retry split commits");
  check(caller.position_fields().fieldd2c==0xa5&&caller.shared_fields().pbw.field6fb==0,"Retry omitted contact placement fields");
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
