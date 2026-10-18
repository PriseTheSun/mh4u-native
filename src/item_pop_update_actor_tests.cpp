// Composition only: real resource build/update observations feed the existing
// Player preparation API. Explicit diagnostic Player seed; no scheduler claim.
#include "player_actor.hpp"
#include "item_pop_builder.hpp"
#include "item_pop_update.hpp"
#include "test_support/player_actor_state.hpp"
#include "test_support/item_pop_update_fixture.hpp"
#include <array>
#include <bit>
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
std::size_t checks{},pairs{};
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
 check(a.tick==b.tick&&bits(a.source_frame)==bits(b.source_frame)&&a.event_masks==b.event_masks&&a.ended==b.ended,"Preparation advanced source clock");
 check(a.scene.global_transforms==b.scene.global_transforms&&a.palettes==b.palettes,"Preparation changed hierarchy or palettes");
 check(a.scene.mesh.triangles.size()==b.scene.mesh.triangles.size()&&a.scene.draws.size()==b.scene.draws.size(),"Preparation changed geometry extent");
 for(std::size_t i=0;i<a.scene.mesh.triangles.size();++i){const auto& p=a.scene.mesh.triangles[i];const auto& q=b.scene.mesh.triangles[i];
  check(std::array{bits(p.position.x),bits(p.position.y),bits(p.position.z),bits(p.normal.x),bits(p.normal.y),bits(p.normal.z),bits(p.uv.x),bits(p.uv.y)}==std::array{bits(q.position.x),bits(q.position.y),bits(q.position.z),bits(q.normal.x),bits(q.normal.y),bits(q.normal.z),bits(q.uv.x),bits(q.uv.y)},"Preparation changed skinned vertex bits");}
 for(std::size_t i=0;i<a.scene.draws.size();++i){const auto& p=a.scene.draws[i];const auto& q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Preparation changed draw identity");}
}
struct Saved {pa::State state;mh4u::player_fields::Fields fields;mh4u::player_state1::Fields state1;mh4u::player_position_bridge::Fields position;std::uint64_t steps;};
Saved save(const na::Player& p){return {p.state(),p.shared_fields(),p.state1_fields(),p.position_fields(),p.completed_steps()};}
void unchanged(const na::Player& p,const Saved& s){check(mh4u::test_player_actor_state::state_equal(p.state(),s.state)&&p.shared_fields()==s.fields&&p.state1_fields()==s.state1&&p.position_fields()==s.position&&p.completed_steps()==s.steps,"Failed transaction changed Player");}
void equal_lookup(const wl::Output& actual,const wl::Output& original){check(actual.returned==original.returned&&actual.match==original.match&&actual.visited==original.visited,"Player lookup differs from original query");}
// The fixture owner supplies original observations. This routine never derives
// expected lookup results from the native Manager or from a selected record.
void query_pair(const na::Actor& actor,const pt::Constants& constants,
 const ib::Built& built,const wl::Manager& after_update,const wl::Request& query,
 U area_mask,const wl::Output& expected_before,const wl::Output& expected_after){
 auto initial=seed(built.global,query);
 auto player=value<na::Player>(na::Player::from_rest(actor,initial,constants,banks()));
 const auto baseline=value<na::Frame>(player.capture());const auto presentation=player.presentation();
 const auto before_manager=built.manager,after_manager=after_update;
 mh4u::player_update_prefix::Context context{{},U(0),{&built.manager,area_mask}};
 const auto pre=value<na::PlayerPreparationOutput>(player.prepare_update(context));equal_lookup(pre.lookup,expected_before);
 check(player.shared_fields().pw.field5a8==(expected_before.returned==0xffff?0:expected_before.returned),"Pre-update PW5a8 publication");
 same_frame(pre.frame,baseline);
 // The same Player survives the explicit caller-owned Manager replacement.
 context.world.manager=&after_update;
 const auto post=value<na::PlayerPreparationOutput>(player.prepare_update(context));equal_lookup(post.lookup,expected_after);
 check(player.shared_fields().pw.field5a8==(expected_after.returned==0xffff?0:expected_after.returned),"Post-update PW5a8 publication");
 check(player.state().controller.player.global==built.global&&player.completed_steps()==0&&player.presentation()==presentation,"Preparation duplicated or advanced canonical RNG/animation owner");
 check(built.manager==before_manager&&after_update==after_manager,"Player mutated borrowed Manager");same_frame(post.frame,baseline);
 // A late missing world must roll back earlier prefix stores. The committed
 // external Manager stays committed; corrected retry does not call update.
 const auto saved=save(player);context.world.manager=nullptr;context.delta_bits=one;
 const auto failed=value<na::PlayerError>(player.prepare_update(context));
 check(failed.code==na::PlayerErrorCode::update_prefix&&failed.update_prefix&&failed.update_prefix->phase==mh4u::player_update_prefix::Phase::lookup,"Missing world lost typed late phase");
 unchanged(player,saved);check(player.shared_fields().pbw.field54_bits==0,"Failed prefix published accumulator candidate");check(after_update==after_manager,"Player failure rolled back an external Manager transaction");
 context.world.manager=&after_update;const auto retried=value<na::PlayerPreparationOutput>(player.prepare_update(context));equal_lookup(retried.lookup,expected_after);check(player.shared_fields().pbw.field54_bits==one,"Corrected retry did not commit prefix once");same_frame(retried.frame,baseline);
 ++pairs;
}
void failed_update(const na::Actor& actor,const pt::Constants& constants,
 const ib::Built& built,const wl::Request& query,const update::Context& context){
 auto player=value<na::Player>(na::Player::from_rest(actor,seed(built.global,query),constants,banks()));
 const auto saved=save(player);const auto baseline=value<na::Frame>(player.capture());
 auto candidate=built.manager;const auto head=std::get<wl::Index>(candidate.head).value;
 const auto second=std::get<wl::Index>(candidate.records[head].next).value;
 candidate.records[second].field63.reset();const auto before=candidate;
 const auto result=update::apply(candidate,context);const auto error=value<update::Error>(result);
 check(error.code==update::ErrorCode::unknown_field&&error.field=="record63"&&error.index==second,"Late update error cause/index lost");
 check(candidate==before,"Failed update published earlier record writes");
 unchanged(player,saved);same_frame(value<na::Frame>(player.capture()),baseline);
}
void run(const std::filesystem::path& recipe,const pt::Constants& constants,
 const std::filesystem::path& fixture_path,const std::string& fixture_sha,const std::filesystem::path& root){
 namespace fx=mh4u::test_support::item_pop_update_fixture;
 namespace f=mh4u::test_support::item_pop_manager;
 auto fixture=fx::load(fixture_path,fixture_sha);const auto bytes_before=fixture.bytes;
 f::Resources resources{root,{},{}};
 const auto& set=resources.get(fixture.sources[0].path,fixture.sources[0].sha,fixture.sources[0].size);
 const auto& list=resources.get(fixture.sources[1].path,fixture.sources[1].sha,fixture.sources[1].size);
 const auto& c=fixture.cases.at(0);
 check(c.index==0&&c.name=="real-seed0"&&c.set_present&&c.list_present,"Unexpected original integration scenario");
 check(c.steps.size()==1&&c.steps[0].mutations.empty(),"Integration scenario requires unmodified original continuity");
 const auto& step=c.steps[0];
 check(std::equal(c.built.begin(),c.built.end(),step.before.begin()),"Original builder/update continuity");
 const auto built=value<ib::Built>(ib::build_fresh(&set,&list,c.builder));
 check(built.manager==f::decode_manager(c.built,list,c.binding),"Native builder differs from original Manager");
 auto expected_global=c.builder.global;expected_global.field22=std::uint16_t(f::read(c.global_after,0x22,2));
 check(built.global==expected_global,"Native builder global owner differs from original");
 f::Bytes global(c.global_before.begin(),c.global_before.end());f::put(global,0x22,built.global.field22,2);
 check(std::equal(global.begin(),global.end(),c.global_after.begin()),"Original global changed outside canonical RNG owner");
 const auto before=built.manager;
 const auto updated=value<update::Output>(update::apply(built.manager,step.context));
 check(updated.manager==f::decode_manager(step.after,list,c.binding),"Native update differs from original Manager");
 check(updated.visited==step.record_calls.size()&&!updated.clock_gated,"Original update dispatch coverage differs");
 check(built.manager==before,"Native update mutated borrowed pre-state");
 auto actor=value<na::Actor>(na::Actor::load(recipe));
 check(c.before_queries.size()==43&&step.queries.size()==43,"Original integration query coverage");
 for(std::size_t i=0;i<c.before_queries.size();++i){const auto& pre=c.before_queries[i];const auto& post=step.queries[i];
  check(pre.source_slot==post.source_slot&&pre.area==post.area&&pre.point==post.point&&pre.mask==post.mask,"Original query point identity changed");
  const auto expected_pre=fx::expected_lookup(pre,list),expected_post=fx::expected_lookup(post,list);
  check(!expected_pre.match&&expected_pre.returned==0xffff&&expected_post.match.has_value(),"Original activation observation missing");
  wl::Request request{};request.area=pre.area;for(U j=0;j<3;++j)request.point[j]=pre.point[j];
  query_pair(actor,constants,built,updated.manager,request,pre.mask,expected_pre,expected_post);
  if(i==0)failed_update(actor,constants,built,request,step.context);
 }
 check(pairs==43,"Incomplete persistent Player coverage");
 check(fixture.bytes==bytes_before,"Original fixture mutated");
 check(resources.unchanged()==fixture.sources[0].size+fixture.sources[1].size,"Resource extent/readback changed");
}
}
int main(int argc,char** argv){try{
 if(argc==3&&std::string(argv[1])=="--source-files"){auto actor=value<na::Actor>(na::Actor::load(argv[2]));for(const auto& path:actor.source_files())std::cout<<path.generic_string()<<'\n';return 0;}
 check(argc==7,"Expected recipe constants constants-SHA fixture fixture-SHA root");
 std::ifstream file(argv[2],std::ios::binary|std::ios::ate);check(bool(file)&&file.tellg()==88,"Input extent");
 std::array<std::uint8_t,88> bytes{};file.seekg(0);check(bool(file.read(reinterpret_cast<char*>(bytes.data()),88))&&file.peek()==std::char_traits<char>::eof(),"Input read");
 auto constants=value<pt::Constants>(pt::Constants::decode(bytes,{argv[3],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"}));
 run(argv[1],constants,argv[4],argv[5],argv[6]);
 std::cout<<"{\"status\":\"passed\",\"players\":"<<pairs<<",\"original_misses\":"<<pairs<<",\"original_hits\":"<<pairs<<",\"checks\":"<<checks<<",\"fixture_checks\":"<<mh4u::test_support::item_pop_manager::checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

