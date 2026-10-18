#include "native_actor.hpp"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace n=mh4u::native_actor;
std::size_t checks=0,frames=0,clips=0,transitions=0;
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
template<class T>T get(n::Result<T> result){if(auto* e=std::get_if<n::Error>(&result))throw std::runtime_error(e->message);return std::get<T>(std::move(result));}
void equal(const n::Frame& a,const n::Frame& b){
 check(a.source_frame==b.source_frame&&a.samples==b.samples&&a.palettes==b.palettes,"Frame metadata differs after seeking");
 check(a.tick==b.tick&&a.event_masks==b.event_masks&&a.ended==b.ended,"Clock or event state differs after seeking");
 check(a.scene.global_transforms==b.scene.global_transforms,"Joint matrices differ after seeking");
 const auto& x=a.scene.mesh;const auto& y=b.scene.mesh;check(x.source_positions==y.source_positions&&x.triangles.size()==y.triangles.size(),"Geometry counts differ");
 for(std::size_t i=0;i<x.triangles.size();++i){const auto& p=x.triangles[i];const auto& q=y.triangles[i];
  for(auto values:{std::pair{p.position.x,q.position.x},{p.position.y,q.position.y},{p.position.z,q.position.z},{p.normal.x,q.normal.x},{p.normal.y,q.normal.y},{p.normal.z,q.normal.z},{p.uv.x,q.uv.x},{p.uv.y,q.uv.y}})
   check(std::bit_cast<std::uint32_t>(values.first)==std::bit_cast<std::uint32_t>(values.second),"Vertex bits differ after seek/copy/rejected input");
  check(std::abs(std::hypot(p.normal.x,p.normal.y,p.normal.z)-1)<1e-6f,"Normal is not finite unit length");
 }
 check(a.scene.draws.size()==b.scene.draws.size(),"Draw counts differ");
 for(std::size_t i=0;i<a.scene.draws.size();++i){auto p=a.scene.draws[i],q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Draw selection differs");}
}
void run(const char* path,std::size_t expected_clips,std::size_t vertices,std::size_t triangles){
 auto pristine=get(n::Actor::load(path));auto actor=pristine;const auto scene=pristine.presentation();
 check(scene->nodes.empty()&&scene->skins.empty()&&scene->instances.empty(),"Native presentation accidentally contains glTF skeleton/instances");
 for(const auto& p:scene->primitives)check(p.vertices.empty()&&p.indices.empty(),"Native presentation must not own converted geometry");
 check(scene->animations.size()==expected_clips,"Selected action count changed");
 auto rest=get(actor.evaluate(-1,0,false));check(rest.scene.mesh.source_positions==vertices&&rest.scene.mesh.triangles.size()/3==triangles,"Real geometry counts changed");
 auto copied=actor;get(actor.evaluate(0,0.05f,false));equal(rest,get(copied.evaluate(-1,0,false)));
 for(std::size_t clip=0;clip<scene->animations.size();++clip){++clips;const float d=scene->animations[clip].duration;
  for(float t:{0.f,d*.5f,d,d*.25f,0.f,d*2.f+.1f})for(bool loop:{false,true}){
   auto fresh=pristine;const auto expected=get(fresh.evaluate(static_cast<int>(clip),t,loop));
   const auto actual=get(actor.evaluate(static_cast<int>(clip),t,loop));equal(actual,expected);++frames;
   check(actual.scene.mesh.source_positions==vertices&&actual.scene.mesh.triangles.size()/3==triangles,"Animation changed geometry topology");
  }
 }
 for(auto [clip,time]:{std::pair{-2,0.f},{int(expected_clips),0.f},{0,-1.f},{0,std::numeric_limits<float>::infinity()},{0,std::numeric_limits<float>::quiet_NaN()}}){auto failed=actor.evaluate(clip,time,false);check(std::holds_alternative<n::Error>(failed)&&std::get<n::Error>(failed).code==n::ErrorCode::time,"Invalid frame accepted");}
 auto fresh=pristine;equal(get(actor.evaluate(0,.5f,false)),get(fresh.evaluate(0,.5f,false)));
 auto failed=actor.evaluate(0,100000.f,false);check(std::holds_alternative<n::Error>(failed)&&std::get<n::Error>(failed).code==n::ErrorCode::capacity,"Unbounded replay accepted");
 equal(get(actor.evaluate(0,.5f,false)),get(fresh.evaluate(0,.5f,false)));
 auto copied_clock=actor;equal(get(actor.evaluate(0,.6f,false)),get(copied_clock.evaluate(0,.6f,false)));
 equal(get(actor.evaluate(-1,0,false)),rest);
 check(actor.supports_transitions(),"Pinned command snapshots not loaded");
 bool changed_pose=false;
 for(std::size_t clip=0;clip<expected_clips;++clip){
  auto moving=pristine;const auto next=static_cast<int>((clip+1)%expected_clips);
  get(moving.evaluate(static_cast<int>(clip),scene->animations[clip].duration*.4f,true));
  auto twin=moving;
  const auto changed=get(moving.change_action(next,true));
  equal(changed,get(twin.change_action(next,true)));equal(changed,get(moving.evaluate(next,0,true)));
  auto isolated=pristine;const auto direct=get(isolated.evaluate(next,0,true));
  changed_pose|=changed.scene.global_transforms!=direct.scene.global_transforms;
  for(float invalid:{-1.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
   check(std::holds_alternative<n::Error>(moving.change_action(next,true,invalid)),"Invalid transition accepted");
   equal(changed,get(moving.evaluate(next,0,true)));
  }
  for(float t:{.05f,.15f})equal(get(moving.evaluate(next,t,true)),get(twin.evaluate(next,t,true)));
  // Seeking backward deliberately returns to an isolated action, removing
  // transition history. This is distinct from selecting another action.
  equal(get(moving.evaluate(next,0,true)),direct);
  get(moving.change_action(static_cast<int>(clip),true));
  equal(get(moving.seek(next,.1f,true)),get(isolated.seek(next,.1f,true)));
  ++transitions;
 }
 check(changed_pose,"Action changes unexpectedly reset every previous pose");
}
int main(int argc,char**argv){try{check(argc==3,"Expected em062 and hunter-seven native recipes");run(argv[1],28,1516,1564);run(argv[2],44,4507,5043);std::cout<<"{\"status\":\"passed\",\"clips\":"<<clips<<",\"frames\":"<<frames<<",\"transitions\":"<<transitions<<",\"checks\":"<<checks<<"}\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
