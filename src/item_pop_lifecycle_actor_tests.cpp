// Persistent Player publication test: expected lifecycle states come only from
// the retained original fixture. No implicit scheduler or bootstrap is inferred.
#include "player_actor.hpp"
#include "test_support/player_actor_state.hpp"
#include "test_support/item_pop_lifecycle_fixture.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
namespace na=mh4u::native_actor;namespace pa=mh4u::player_animation;
namespace pt=mh4u::player_transition;namespace jp=mh4u::joint_pose;
namespace wl=mh4u::player_world_lookup;namespace ib=mh4u::item_pop_builder;
namespace update=mh4u::item_pop_update;
using U=std::uint32_t;
constexpr U one=0x3f800000;constexpr jp::Bits4 identity{0,0,0,one};
std::size_t checks{},players{},updates{},queries{},negatives{};
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
U bits(float v){return std::bit_cast<U>(v);}
template<class T,class...V>T value(std::variant<V...> result){
 if(auto* v=std::get_if<T>(&result))return std::move(*v);
 if constexpr((std::is_same_v<V,na::PlayerError>||...))throw std::runtime_error(std::get<na::PlayerError>(result).message);
 throw std::runtime_error("Unexpected typed result");
}
na::PlayerInitial seed(const pt::GlobalFields& global,const wl::Request& query){
 na::PlayerInitial s{};s.controller.player.global=global;
 s.snapshots.resize(19);s.model.rotation=s.model.rotation_delta=identity;s.model.scale=s.model.null_scale={one,one,one,0};
 for(auto& l:s.layers){l.animation.tracks.joints.resize(19);l.animation.layer.speed=1;l.animation.layer.flags=2;l.animation.prediction_rotation=identity;}
 s.layers[0].animation.layer.frame=5.25F;s.layers[0].animation.layer.prepared_frame=6.5F;s.layers[0].animation.layer.end_frame=30;
 s.layers[1].animation.layer.weight=.375F;s.controller.motion.fieldsd48[4]=one;s.controller.motion.fieldsd48[5]=bits(30);
 for(U i=2;i<8;++i){s.layers[i].animation.layer.frame=std::bit_cast<float>(0x7fc00001U+i);s.layers[i].rotation_98={i,0xffffffff,0x80000000,0x7f800000};}
 check(query.area&&*query.area<256,"Oracle Player area must be a byte");
 s.controller.player.selection_pbw.field9=std::uint8_t(*query.area);
 s.controller.player.selection_pbw.field6fa=0x91;s.controller.player.pw.fielde8=0x80000004;
 auto& f=s.shared_fields;
 f.pbw.field10_bits=query.point[0];f.pbw.field14_bits=query.point[1];f.pbw.field18_bits=query.point[2];
 f.pbw.field54_bits=0;f.pbw.field6e8_bits=0x7fc12345;f.pbw.field6ec_bits=0x7f800000;f.pbw.field6f0_bits=0xff800000;
 f.pw.field13c_bits=one;f.pw.field1f8=0x80001000;f.pw.field5a8=0xa5a5a5a5;
 f.pw.field6a4_bits=0x7fc56789;f.pw.field6a8=0xff;
 s.state1_fields.field50_bits=0x7fc12345;s.position_fields.fieldd30_bits=0xff812345;
 return s;
}
na::PlayerBanks banks(){na::PlayerBanks b{};b[0]=na::MotionSource{"c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"};return b;}
void same_frame(const na::Frame& a,const na::Frame& b){
 check(a.tick==b.tick&&a.samples==b.samples&&bits(a.source_frame)==bits(b.source_frame)&&a.event_masks==b.event_masks&&a.ended==b.ended,"Preparation advanced source clock");
 check(a.scene.global_transforms==b.scene.global_transforms&&a.palettes==b.palettes,"Preparation changed hierarchy or palettes");
 check(a.scene.mesh.triangles.size()==b.scene.mesh.triangles.size()&&a.scene.draws.size()==b.scene.draws.size(),"Preparation changed geometry extent");
 for(std::size_t i=0;i<a.scene.mesh.triangles.size();++i){const auto& p=a.scene.mesh.triangles[i];const auto& q=b.scene.mesh.triangles[i];
  check(std::array{bits(p.position.x),bits(p.position.y),bits(p.position.z),bits(p.normal.x),bits(p.normal.y),bits(p.normal.z),bits(p.uv.x),bits(p.uv.y)}==std::array{bits(q.position.x),bits(q.position.y),bits(q.position.z),bits(q.normal.x),bits(q.normal.y),bits(q.normal.z),bits(q.uv.x),bits(q.uv.y)},"Preparation changed skinned vertex bits");}
 for(std::size_t i=0;i<a.scene.draws.size();++i){const auto& p=a.scene.draws[i];const auto& q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Preparation changed draw identity");}
}
struct Saved {pa::State state;mh4u::player_fields::Fields fields;mh4u::player_state1::Fields state1;mh4u::player_position_bridge::Fields position;std::uint64_t steps;};
Saved save(const na::Player& p){return {p.state(),p.shared_fields(),p.state1_fields(),p.position_fields(),p.completed_steps()};}
void unchanged(const na::Player& p,const Saved& s){check(mh4u::test_player_actor_state::state_equal(p.state(),s.state)&&p.shared_fields()==s.fields&&p.state1_fields()==s.state1&&p.position_fields()==s.position&&p.completed_steps()==s.steps,"Failed transaction changed Player");}
void success_owners(const na::Player& p,const Saved& saved,const pt::GlobalFields& global){
 auto expected=saved.state;expected.controller.player.global=global;
 check(mh4u::test_player_actor_state::state_equal(p.state(),expected),"Lifecycle changed animation/controller outside canonical globals");
 check(p.shared_fields()==saved.fields&&p.state1_fields()==saved.state1&&p.position_fields()==saved.position&&p.completed_steps()==saved.steps,"Lifecycle changed another persistent owner");
}
void equal_lookup(const wl::Output& actual,const wl::Output& original){check(actual.returned==original.returned&&actual.match==original.match&&actual.visited==original.visited,"Player lookup differs from original query");}

namespace fx=mh4u::test_support::item_pop_lifecycle_fixture;
namespace f=mh4u::test_support::item_pop_manager;
namespace previous=mh4u::test_support::item_pop_update_fixture;
pt::GlobalFields observed_global(const fx::Step& step,bool after){
 const auto raw=after?step.global_after:step.global_before;
 return fx::global(raw,step.facts[7]?std::optional<U>(f::read(raw,0x20,2)):std::nullopt);
}
update::Effects effects(const fx::Step& step){
 update::Effects out{};
 for(const auto& call:step.calls){
  if(call.pc==0x2b555c){check(call.args[1]<2,"Original RNG channel domain");++out.rng_calls[call.args[1]];}
  if(call.pc==0x5b9d84)++out.removed;
  if(call.pc==0x23dd60)++out.primary_renewed;
  if(call.pc==0x5b928c){if(call.result)++out.secondary_renewed;else ++out.secondary_failed;}
 }
 return out;
}
wl::Request point(const fx::Query& q){wl::Request out{};out.area=q.area;for(U j=0;j<3;++j)out.point[j]=q.point[j];return out;}
const fx::Query& final_query(const fx::Case& c){check(!c.steps.back().queries.empty(),"Selected original scenario has no final query");return c.steps.back().queries.front();}
void sequence(const fx::Case& c,const na::Actor& actor,const pt::Constants& constants,
 const mh4u::item_pop_resources::Resource& set,const mh4u::item_pop_resources::Resource& list){
 const auto built=value<ib::Built>(ib::build_fresh(&set,&list,c.builder));
 check(built.manager==f::decode_manager(c.built,list,f::BindingProfile::constructor_observed),"Native builder differs from original");
 check(built.global==fx::global(c.global_after,f::read(c.global_after,0x20,2)),"Builder lost channel0 or changed global outside original");
 // Only declared first-call diagnostic mutations are applied. Later states
 // come exclusively from the persistent native operation, not source reloads.
 f::Bytes first=f::encode_manager(built.manager,c.built,list);
 const auto& initial_step=c.steps.front();check(initial_step.external_mutations.empty(),"Selected sequence has external mutation");
 for(const auto& mutation:initial_step.mutations)f::put(first,0x1c+0x94*mutation.slot+mutation.offset,mutation.value,mutation.width);
 check(std::equal(first.begin(),first.end(),initial_step.before.begin()),"Source diagnostic prefix continuity");
 auto manager=f::decode_manager(first,list,f::BindingProfile::constructor_observed);
 const auto& q=final_query(c);const auto request=point(q);
 auto initial=seed(observed_global(initial_step,false),request);
 auto player=value<na::Player>(na::Player::from_rest(actor,initial,constants,banks()));++players;
 const auto frame=value<na::Frame>(player.capture());const auto presentation=player.presentation();
 for(std::size_t i=0;i<c.steps.size();++i){const auto& step=c.steps[i];
  if(i)check(step.mutations.empty()&&step.external_mutations.empty(),"Persistent sequence cannot reseed or reload state");
  check(manager==f::decode_manager(step.before,list,f::BindingProfile::constructor_observed),"Sequential Manager pre-state differs from original");
  check(player.state().controller.player.global==observed_global(step,false),"Sequential canonical RNG pre-state differs from original");
  const auto before=save(player);const auto context=fx::context(step,manager,set,list);
  const auto out=value<na::PlayerItemPopOutput>(player.update_item_pop(manager,context));
  check(manager==f::decode_manager(step.after,list,f::BindingProfile::constructor_observed),"Atomic external Manager publication differs from original");
  check(out.effects==effects(step),"Lifecycle effects differ from original calls");
  const auto visits=std::count_if(step.calls.begin(),step.calls.end(),[](const auto& call){return call.pc==0x5aa4cc;});
  check(out.visited==std::size_t(visits)&&!out.clock_gated,"Record visits/clock gate differ from original");
  success_owners(player,before,observed_global(step,true));same_frame(out.frame,frame);++updates;
  for(const auto& original:step.queries){
   if(original.area!=q.area||original.point!=q.point)continue;
   const auto manager_before=manager;const auto global_before=player.state().controller.player.global;
   mh4u::player_update_prefix::Context context_query{{},U(0),{&manager,original.mask}};
   const auto queried=value<na::PlayerPreparationOutput>(player.prepare_update(context_query));
   equal_lookup(queried.lookup,previous::expected_lookup(original,list));
   check(player.shared_fields().pw.field5a8==(original.returned==0xffff?0:original.returned),"Lifecycle lookup publication differs from original");
   check(manager==manager_before&&player.state().controller.player.global==global_before,"Preparation mutated external Manager or RNG");
   same_frame(queried.frame,frame);++queries;
  }
  check(player.completed_steps()==0&&player.presentation()==presentation,"Lifecycle replaced Player resources or advanced animation");
 }
}
struct RoundGuard {int previous=std::fegetround();~RoundGuard(){std::fesetround(previous);}};
void failures(const fx::Case& renewal,const fx::Case& removal,const na::Actor& actor,const pt::Constants& constants,
 const mh4u::item_pop_resources::Resource& set,const mh4u::item_pop_resources::Resource& list){
 // This original call advances channel0 and then channel1 twice, before the
 // following linked record is reached. Typed failure must publish neither.
 const auto& step=renewal.steps.at(1);const auto expected_effects=effects(step);
 check(expected_effects.rng_calls==std::array<std::size_t,2>{1,2},"Negative fixture requires observed two-channel renewal");
 const auto request=point(final_query(renewal));
 for(unsigned which=0;which<3;++which){
  auto manager=f::decode_manager(step.before,list,f::BindingProfile::constructor_observed);
  auto globals=observed_global(step,false);if(which==0)globals.field20.reset();
  if(which==2){
   const auto last=std::find_if(step.calls.rbegin(),step.calls.rend(),[](const auto& c){return c.pc==0x5aa4cc;});
   check(last!=step.calls.rend(),"No last record in source");
   const auto index=std::get<wl::Index>(f::decode_link(last->args[0])).value;
   check(std::count_if(step.calls.begin(),step.calls.end(),[&](const auto& c){return c.pc==0x2b555c&&c.instruction<last->instruction;})==3,"Source draws do not precede late failure point");
   manager.records[index].field63.reset();
  }
  auto player=value<na::Player>(na::Player::from_rest(actor,seed(globals,request),constants,banks()));
  auto context=fx::context(step,manager,set,list);const auto foreign=manager;
  if(which==1)context.environment.singleton_manager=&foreign;
  const auto before=save(player);const auto manager_before=manager;const auto frame=value<na::Frame>(player.capture());
  const auto error=value<na::PlayerError>(player.update_item_pop(manager,context));
  check(error.code==na::PlayerErrorCode::item_pop_update&&error.item_pop_update.has_value(),"Lost lifecycle error cause");
  const auto& cause=*error.item_pop_update;
  if(which==1)check(cause.secondary&&cause.secondary->code==mh4u::item_pop_secondary::ErrorCode::foreign_manager,"Foreign singleton collapsed into same Manager");
  else check(cause.code==update::ErrorCode::unknown_field&&cause.field==(which==0?"global20":"record63"),"Wrong unknown-field failure");
  check(manager==manager_before,"Failed lifecycle published Manager");unchanged(player,before);same_frame(value<na::Frame>(player.capture()),frame);++negatives;
 }
 // Removal is integer-only, so an unsupported rendering environment fails
 // after a complete removal candidate, without needing a callback or stub.
 const auto& rs=removal.steps.front();auto manager=f::decode_manager(rs.before,list,f::BindingProfile::constructor_observed);
 auto player=value<na::Player>(na::Player::from_rest(actor,seed(observed_global(rs,false),point(final_query(removal))),constants,banks()));
 const auto before=save(player);const auto manager_before=manager;const auto frame=value<na::Frame>(player.capture());
 auto context=fx::context(rs,manager,set,list);
 {
  RoundGuard restore;check(restore.previous==FE_TONEAREST&&std::fesetround(FE_DOWNWARD)==0,"Rounding setup failed");
  const auto error=value<na::PlayerError>(player.update_item_pop(manager,context));
  check(error.code==na::PlayerErrorCode::render&&error.render.has_value(),"Removal did not reach failing renderer");
  check(manager==manager_before,"Render failure published removed Manager");unchanged(player,before);
 }
 check(std::fegetround()==FE_TONEAREST,"Rounding not restored");same_frame(value<na::Frame>(player.capture()),frame);++negatives;
 const auto retry=value<na::PlayerItemPopOutput>(player.update_item_pop(manager,context));
 check(manager==f::decode_manager(rs.after,list,f::BindingProfile::constructor_observed)&&retry.effects.removed==1,"Corrected render retry failed");
 success_owners(player,before,observed_global(rs,true));same_frame(retry.frame,frame);
}
void run(const std::filesystem::path& recipe,const pt::Constants& constants,const std::filesystem::path& path,
 const std::string& sha,const std::filesystem::path& root){
 auto fixture=fx::load(path,sha);const auto raw=fixture.bytes;
 f::Resources resources{root,{},{}};auto actor=value<na::Actor>(na::Actor::load(recipe));
 check(actor.part_count()==7&&actor.joint_count()==19,"Expected seven-piece19joint source");
 const std::array<std::pair<std::size_t,const char*>,6> selected{{
  {48,"real-cycle-2-3-refresh"},{49,"real-cycle-2-5-rescan"},{50,"real-cycle-2-4-remove"},
  {53,"real-state3-percent-skip-unknown-singleton"},{55,"real-state3-wait-unknown-context"},{56,"real-state5-wait-unknown-context"}}};
 for(const auto& [index,name]:selected){const auto& c=fixture.cases.at(index);check(c.name==name,"Original scenario identity");
  const auto& set=resources.get(c.sources[0].path,c.sources[0].sha,c.sources[0].size);
  const auto& list=resources.get(c.sources[1].path,c.sources[1].sha,c.sources[1].size);sequence(c,actor,constants,set,list);
 }
 const auto& renewal=fixture.cases.at(48);const auto& removal=fixture.cases.at(0);check(removal.name=="real-remove-head","Original removal identity");
 for(std::size_t i=0;i<2;++i)check(renewal.sources[i].sha==removal.sources[i].sha,"Negative source identity mismatch");
 const auto& set=resources.get(renewal.sources[0].path,renewal.sources[0].sha,renewal.sources[0].size);
 const auto& list=resources.get(renewal.sources[1].path,renewal.sources[1].sha,renewal.sources[1].size);
 failures(renewal,removal,actor,constants,set,list);
 check(players==6&&updates==12&&queries>0&&negatives==4,"Integration coverage incomplete");
 check(resources.unchanged()>0&&fixture.bytes==raw,"Consumed source mutated");
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=value<na::Actor>(na::Actor::load(argv[2]));for(const auto& path:actor.source_files())std::cout<<path.generic_string()<<'\n';return 0;}
 check(argc==7,"Expected recipe constants constants-SHA fixture fixture-SHA root");
 std::ifstream file(argv[2],std::ios::binary|std::ios::ate);check(bool(file)&&file.tellg()==88,"Input extent");std::array<std::uint8_t,88> bytes{};file.seekg(0);
 check(bool(file.read(reinterpret_cast<char*>(bytes.data()),88))&&file.peek()==std::char_traits<char>::eof(),"Input read");
 auto constants=value<pt::Constants>(pt::Constants::decode(bytes,{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));run(argv[1],constants,argv[4],argv[5],argv[6]);
 std::cout<<"{\"status\":\"passed\",\"persistent_players\":"<<players<<",\"updates\":"<<updates<<",\"queries\":"<<queries<<",\"typed_controls\":"<<negatives<<",\"checks\":"<<checks<<",\"fixture_checks\":"<<f::checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

