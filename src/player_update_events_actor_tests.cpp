// Persistent current-event checkpoint with immutable hunter MOD/LMT resources.
// Diagnostic initialization only; original differential evidence is separate.
#include "player_actor.hpp"
#include "test_support/player_update_model_actor_fixture.hpp"
#include "test_support/player_actor_allocation_probe.hpp"
#include "test_support/player_update_actor_checks.hpp"
#include "test_support/player_update_dispatch_actor_fixture.hpp"
#include "test_support/player_update_notifications_actor_fixture.hpp"
#include "test_support/player_update_manager_actor_fixture.hpp"
#include "test_support/player_update_contact_actor_fixture.hpp"
#include "picosha2.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace na=mh4u::native_actor;namespace jp=mh4u::joint_pose;namespace pt=mh4u::player_transition;
namespace tbl=mh4u::player_policy_table;namespace ad=mh4u::player_update_admission;
namespace wl=mh4u::player_world_lookup;namespace direction=mh4u::player_direction_refresh;
namespace bridge=mh4u::player_position_bridge;
namespace us=mh4u::player_update_statistics;namespace pa=mh4u::player_animation;namespace rq=mh4u::motion_request;
using U=std::uint32_t;constexpr U one=0x3f800000;constexpr jp::Bits4 identity{0,0,0,one};
std::size_t checks{},allocation_failures{},allocation_extent{};
void check(bool b,const char* why){++checks;if(!b)throw std::runtime_error(why);}
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
template<class T,class... V>T value(std::variant<V...> result){
 if(auto* p=std::get_if<T>(&result))return std::move(*p);
 throw std::runtime_error(std::visit([](const auto& e){return describe(e);},result));
}
std::vector<std::uint8_t> read(const char* path,std::size_t size){
 std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f)&&f.tellg()==static_cast<std::streamoff>(size),"Resource extent");
 std::vector<std::uint8_t> bytes(size);f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(bytes.data()),static_cast<std::streamsize>(size))),"Resource read");return bytes;
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string_view(argv[1])=="--source-files"){
  auto actor=value<na::Actor>(na::Actor::load(argv[2]));
  for(const auto& path:actor.source_files())std::cout<<path.string()<<'\n';return 0;
 }
 check(argc==10,"Expected recipe constants/SHA caps/SHA table35/SHA table31/SHA");
 auto actor=value<na::Actor>(na::Actor::load(argv[1]));check(actor.part_count()==7&&actor.joint_count()==19,"Actual hunter extent");
 const auto constants=value<pt::Constants>(pt::Constants::decode(read(argv[2],88),{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));
 const auto bytes=read(argv[4],42);check(picosha2::hash256_hex_string(bytes)==argv[5],"Caps identity");std::array<std::uint8_t,42> caps{};std::copy(bytes.begin(),bytes.end(),caps.begin());
 auto table35=value<tbl::Table>(tbl::Table::decode(read(argv[6],792),argv[7]));auto table31=value<tbl::Table>(tbl::Table::decode(read(argv[8],704),argv[9]));
 auto seed=mh4u::test_dispatch_actor_fixture::adjustment_seed();mh4u::test_notifications_actor_fixture::notification_inputs(seed);
 mh4u::test_manager_actor_fixture::manager_inputs(seed);mh4u::test_contact_actor_fixture::contact_inputs(seed);
 mh4u::test_model_actor_fixture::model_inputs(seed);
 seed.shared_fields.pbw.field808=0x80a5a5a3;seed.shared_fields.pw.field97c=0x7777;seed.shared_fields.pw.field97e=0x8888;
 seed.controller.player.pw.fieldb0=0x1234cafe;
 auto& pending=seed.layers[0].animation.layer;pending.selector=50;pending.flags|=2;pending.weight=1;pending.frame=54;pending.prepared_frame=54;pending.requested_frame=54;
 auto player=value<na::Player>(na::Player::from_rest(actor,seed,constants,mh4u::test_update_actor_fixture::banks()));
 auto before=mh4u::test_update_actor_checks::save(player);
 wl::Manager world;world.head=wl::Absent{};ad::Parameters admission;admission.prefix={{},one,{&world,1}};
 admission.virtual120=ad::Virtual120::uplayer_b3dd60;mh4u::test_dispatch_actor_fixture::preserve_input(admission);
 admission.reference=mh4u::player_reference::Points{0,0,0,1,0,one};admission.global_c8c=19;admission.global_c90=0;
 const std::string caps_sha=argv[5];
 mh4u::test_update_actor_fixture::Parameters parameters(caps,caps_sha);mh4u::player_state1::Tables tables{table35,table31};
 std::array<us::IndexedRow,1> rows{{{0x238,{}}}};us::PlayerRegistry registry;registry.indexed_rows=rows;parameters.context.players=&registry;
 mh4u::motion_initialization::Templates templates{jp::Bits3{},jp::Bits3{},identity,jp::Bits4{}};
 na::PlayerUpdateDispatchContext context{admission,parameters.context,tables,templates};
 na::PlayerModelContext model{jp::Bits3{one,one,one},jp::Null{},mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0},identity,bridge::OrientationCallback::b05328,bridge::OutputCallbacks::standard_95b490_95b940};
 auto objects=mh4u::test_dispatch_actor_fixture::dispatch_collection();objects.data[0].field530=1;objects.data[0].position_bits[1]=0;
 auto queue=mh4u::test_notifications_actor_fixture::queue_seed();auto manager=mh4u::test_manager_actor_fixture::manager_seed();
 constexpr direction::Bindings bindings{direction::Virtual84::uplayer_b085bc,direction::Virtual144::uplayer_b47aa0};
 auto out=value<na::PlayerUpdateEventsOutput>(player.prepare_and_update_events(objects,queue,context,bindings,
  mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{manager,&player},model));
 check(out.modeled.effects.animation.has_value(),"Model frame not reached");const auto& effects=*out.modeled.effects.animation;
 check(effects.pending_found&&effects.initializer_calls>0&&effects.joint_samples>0,"Actual LMT action/track resolvers not consumed");
 check(player.state().frame.animation.layers[0].animation.layer.selector==50,"Source selector lost");
 check(player.state().frame.animation.joints!=before.state.frame.animation.joints,"Actual animation never changed pose");
 check(player.completed_steps()==1&&out.modeled.contacted.managed.positioned.frame.tick==1,"Actual resource frame repeated");
 check(out.modeled.contacted.managed.positioned.frame.samples>=effects.model_samples+effects.joint_samples,"Frame sample instrumentation lost resource work");
 for(unsigned i=0;i<6;++i)check(mh4u::test_player_actor_state::layer_equal(player.state().inactive[i],before.state.inactive[i]),"Actual resource frame touched inactive slot");
 check(player.shared_fields().pbw.field808==0x80a5a5a5,"Attribute decision did not publish PBW808");
 check(player.state().controller.player.pw.fieldb0==0xcafe,"Caller did not truncate PWb0");
 check(player.state().frame.animation.layers[0].animation.layer.masks[0]==1,"Actual slot50 event is not active");
 check(player.shared_fields().pw.field97c==10&&player.shared_fields().pw.field97e==0,"Real LMT event prefix did not reach persistent fields");
 const auto pristine_objects=[](){auto c=mh4u::test_dispatch_actor_fixture::dispatch_collection();c.data[0].field530=1;c.data[0].position_bits[1]=0;return c;};
 const auto make=[&](const na::PlayerInitial& initial){return value<na::Player>(na::Player::from_rest(player,initial));};
 const mh4u::test_update_actor_checks::Checks assertions{check};
 // Original helper requires equality to one. Use a nonlocal path for the
 // foreign publication, then fail only at the new phase and retry both owners.
 auto foreign_seed=seed;foreign_seed.controller.player.selection_pbw.field9=3;
 auto caller=make(foreign_seed),peer=make(seed);auto coll=pristine_objects();auto q=mh4u::test_notifications_actor_fixture::queue_seed();auto m=mh4u::test_manager_actor_fixture::manager_seed();
 const auto caller_before=mh4u::test_update_actor_checks::save(caller),peer_before=mh4u::test_update_actor_checks::save(peer);
 const auto old_coll=coll;const auto old_q=q;const auto old_m=m;
 rows[0].selector1.reset();
 auto rejected=value<na::PlayerError>(caller.prepare_and_update_events(coll,q,context,bindings,mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{m,&peer},model));
 check(rejected.driver&&rejected.driver->phase==pa::Phase::update_events,"Unknown row did not fail at the reached new stage");
 assertions.unchanged(caller,caller_before);assertions.unchanged(peer,peer_before);check(coll==old_coll&&q==old_q&&m==old_m,"Late event failure published external owners");
 rows[0].selector1=0x238;
 auto retried=value<na::PlayerUpdateEventsOutput>(caller.prepare_and_update_events(coll,q,context,bindings,mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{m,&peer},model));
 check(retried.modeled.contacted.managed.foreign_frame.has_value(),"Selected foreign Player never rendered");
 check(caller.completed_steps()==1&&peer.completed_steps()==0,"Retry repeated model or advanced foreign clock");
 check(caller.shared_fields().pw.field97c==10,"Retry lost real event prefix");
 // A source-gated model still consumes the retained event binding. This
 // diagnostic mask reaches the third prefix read in the real resource.
 auto gated_seed=seed;gated_seed.position_fields.fieldd64=8;
 auto& layer=gated_seed.layers[0].animation.layer;layer.flags&=~2U;layer.masks[0]=7;
 const auto owned=std::get<rq::Resource>(player.banks().banks[0]).identity;
 layer.bindings[0]=rq::Binding{owned,0,50,0};
 auto gated=make(gated_seed);auto gc=pristine_objects();auto gq=mh4u::test_notifications_actor_fixture::queue_seed();auto gm=mh4u::test_manager_actor_fixture::manager_seed();
 auto gout=value<na::PlayerUpdateEventsOutput>(gated.prepare_and_update_events(gc,gq,context,bindings,mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{gm,&gated},{}));
 check(gout.modeled.effects.early_return&&gated.completed_steps()==0,"Gated model unexpectedly advanced");
 check(gated.shared_fields().pw.field97c==10&&gated.shared_fields().pw.field97e==0,"Retained owned prefix was not resolved after model gate");
 auto foreign_binding=gated_seed;foreign_binding.layers[0].animation.layer.bindings[0]->resource_identity=0;
 auto invalid=make(foreign_binding);auto ic=pristine_objects();auto iq=mh4u::test_notifications_actor_fixture::queue_seed();auto im=mh4u::test_manager_actor_fixture::manager_seed();const auto saved_invalid=mh4u::test_update_actor_checks::save(invalid);const auto old_ic=ic;const auto old_iq=iq;const auto old_im=im;
 auto ie=value<na::PlayerError>(invalid.prepare_and_update_events(ic,iq,context,bindings,mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{im,&invalid},{}));
 check(ie.driver&&ie.driver->phase==pa::Phase::update_events,"Foreign binding was silently accepted");assertions.unchanged(invalid,saved_invalid);check(ic==old_ic&&iq==old_iq&&im==old_im,"Foreign prefix error published candidates");
 // Allocation coverage is bounded to this measured complete operation,
 // including source event resolution and both actual geometry renders.
 auto measured=make(foreign_seed),target=make(seed);auto mc=pristine_objects();auto mq=mh4u::test_notifications_actor_fixture::queue_seed();auto mm=mh4u::test_manager_actor_fixture::manager_seed();
 std::optional<na::PlayerResult<na::PlayerUpdateEventsOutput>> measured_result;
 {allocation_probe::Scope scope;measured_result.emplace(measured.prepare_and_update_events(mc,mq,context,bindings,mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{mm,&target},model));allocation_extent=allocation_probe::calls;}
 check(value<na::PlayerUpdateEventsOutput>(std::move(*measured_result)).modeled.contacted.managed.foreign_frame.has_value(),"Allocation trace omitted foreign render");
 for(std::size_t ordinal=1;ordinal<=allocation_extent;++ordinal){
  auto subject=make(foreign_seed),foreign=make(seed);auto objects0=pristine_objects();auto queue0=mh4u::test_notifications_actor_fixture::queue_seed();auto manager0=mh4u::test_manager_actor_fixture::manager_seed();
  const auto saved=mh4u::test_update_actor_checks::save(subject),saved_foreign=mh4u::test_update_actor_checks::save(foreign);const auto objects_before=objects0;const auto queue_before=queue0;const auto manager_before=manager0;
  std::optional<na::PlayerResult<na::PlayerUpdateEventsOutput>> failure;
  {allocation_probe::Scope scope(ordinal);failure.emplace(subject.prepare_and_update_events(objects0,queue0,context,bindings,mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{manager0,&foreign},model));}
  (void)value<na::PlayerError>(std::move(*failure));++allocation_failures;assertions.unchanged(subject,saved);assertions.unchanged(foreign,saved_foreign);
  check(objects0==objects_before&&queue0==queue_before&&manager0==manager_before,"Allocation failure published external candidates");
  (void)value<na::PlayerUpdateEventsOutput>(subject.prepare_and_update_events(objects0,queue0,context,bindings,mh4u::player_update_post_dispatch::Virtual10c::uplayer_b25b68,{manager0,&foreign},model));
  check(subject.shared_fields().pw.field97c==10&&subject.completed_steps()==1,"Allocation retry lost event or repeated model");
 }
 check(allocation_failures==allocation_extent&&allocation_extent>0,"Allocation sweep incomplete");
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"allocation_failures\":"<<allocation_failures<<",\"allocation_extent\":"<<allocation_extent<<",\"real_prefix\":10,\"output_joints\":"<<effects.output_joints<<"}\n";
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
