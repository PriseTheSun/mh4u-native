#pragma once
// Shared test-only snapshots/assertions for the persistent dispatch checkpoints.
// Raw floating state and all rendered vertices are compared at their owners.
#include "player_actor.hpp"
#include "player_actor_state.hpp"
#include "spatial_query.hpp"
#include <bit>
namespace mh4u::test_update_actor_checks {
namespace na=native_actor;namespace pa=player_animation;namespace sq=spatial_query;
inline std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
struct Saved {pa::State state;mh4u::player_fields::Fields fields;mh4u::player_state1::Fields action;mh4u::player_position_bridge::Fields position;std::uint64_t steps;};
inline Saved save(const na::Player& p){return {p.state(),p.shared_fields(),p.state1_fields(),p.position_fields(),p.completed_steps()};}
struct Checks {
 void (*check)(bool,const char*);
void unchanged(const na::Player& p,const Saved& s)const{check(mh4u::test_player_actor_state::state_equal(p.state(),s.state)&&p.shared_fields()==s.fields&&p.state1_fields()==s.action&&p.position_fields()==s.position&&p.completed_steps()==s.steps,"Failed checkpoint published Player prefix");}
void pose_equal(const na::Player& p,const Saved& s)const{
 const auto& a=p.state();const auto& b=s.state;
 check(a.frame.model==b.frame.model&&a.frame.animation.joints==b.frame.animation.joints&&a.frame.animation.snapshots==b.frame.animation.snapshots&&mh4u::test_player_actor_state::hierarchy_equal(a.hierarchy,b.hierarchy),"Checkpoint changed canonical pose");
 for(std::size_t i=0;i<6;++i)check(mh4u::test_player_actor_state::layer_equal(a.inactive[i],b.inactive[i]),"Checkpoint changed inactive layer");
 check(p.completed_steps()==s.steps,"Checkpoint advanced clock");
}
void frame_equal(const na::Frame& a,const na::Frame& b)const{
 check(a.tick==b.tick&&bits(a.source_frame)==bits(b.source_frame)&&a.event_masks==b.event_masks&&a.ended==b.ended,"Frame clock changed");
 check(a.scene.global_transforms==b.scene.global_transforms&&a.palettes==b.palettes,"Pose/palettes changed");
 check(a.scene.mesh.triangles.size()==b.scene.mesh.triangles.size()&&a.scene.draws.size()==b.scene.draws.size(),"Geometry extent changed");
 for(std::size_t i=0;i<a.scene.mesh.triangles.size();++i){const auto& p=a.scene.mesh.triangles[i];const auto& q=b.scene.mesh.triangles[i];
  check(std::array{bits(p.position.x),bits(p.position.y),bits(p.position.z),bits(p.normal.x),bits(p.normal.y),bits(p.normal.z),bits(p.uv.x),bits(p.uv.y)}==std::array{bits(q.position.x),bits(q.position.y),bits(q.position.z),bits(q.normal.x),bits(q.normal.y),bits(q.normal.z),bits(q.uv.x),bits(q.uv.y)},"Checkpoint changed rendered geometry");}
 for(std::size_t i=0;i<a.scene.draws.size();++i){const auto& p=a.scene.draws[i];const auto& q=b.scene.draws[i];check(p.first==q.first&&p.count==q.count&&p.material==q.material&&p.primitive==q.primitive,"Draw identity changed");}
}
void frame_unchanged(const na::Frame& a,const na::Frame& b)const{frame_equal(a,b);check(a.samples==b.samples,"Failed checkpoint changed observed sample count");}
void grids_equal(const sq::Scene& a,const sq::Scene& b)const{
 check(a.grid_states.size()==b.grid_states.size()&&a.instances.size()==b.instances.size(),"Scene extent changed");
 for(std::size_t i=0;i<a.grid_states.size();++i){const auto& x=a.grid_states[i];const auto& y=b.grid_states[i];check(x.resource.sha256()==y.resource.sha256()&&x.group==y.group&&x.counter==y.counter&&x.depth==y.depth&&x.pair_marks==y.pair_marks,"Failed checkpoint published grid");}
}
};
} // namespace mh4u::test_update_actor_checks
