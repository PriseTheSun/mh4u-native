#include "world.hpp"
#include <cmath>
#include <algorithm>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace mh4u::net {
namespace {
std::array<std::filesystem::path,4> scenes(const WorldConfig& config) { return {config.stage,config.actor,config.floor,config.walls}; }
locomotion::Parameters parameters(const WorldConfig& config) {
    locomotion::Parameters result;result.radius=config.radius;result.height=config.height;result.slope_min_y=config.min_normal_y;
    result.max_step_height=0;result.skin=0.001;result.max_displacement=4096;result.max_contacts=8;
    return result;
}
}
void validate_motion(MotionInput input) {
    if(input.x < -1000||input.x>1000||input.z < -1000||input.z>1000||input.turn < -1000||input.turn>1000||
       input.x*input.x+input.z*input.z>1000000) throw std::runtime_error("motion_range");
}
void write_motion(Writer& writer,MotionInput input) {
    validate_motion(input);
    for(int value:{input.x,input.z,input.turn}) writer.number(static_cast<std::uint16_t>(value),2);
}
MotionInput read_motion(Reader& reader) {
    const auto signed_value=[&]{const auto v=static_cast<int>(reader.number(2));return v>=32768?v-65536:v;};
    MotionInput result{signed_value(),signed_value(),signed_value()};validate_motion(result);return result;
}
NetworkWorld::NetworkWorld(const std::filesystem::path& path)
    :config(load_world_config(path)),identity(identify_world(path,scenes(config))),
     geometry(collision::load_static_gltf(config.floor),collision::load_static_gltf(config.walls),parameters(config)),
     spawn(geometry.create_state(config.spawn.x,config.spawn.z,config.heading)) {
    if(std::abs(spawn.position.y-config.spawn.y)>geometry.parameters().skin) throw std::runtime_error("world_spawn_height");
    // Startup consistency only, not a watcher or protection against a hostile
    // concurrent writer. The loaded geometry then remains privately owned.
    const auto checked=identify_world(path,scenes(config));
    const auto parsed_again=load_world_config(path);
    if(checked.document!=identity.document||parsed_again!=config) throw std::runtime_error("world_changed_during_load");
    if(identity.content.clips.empty()) throw std::runtime_error("world_requires_initial_clip_0");
}
WorldActor NetworkWorld::join() const {WorldActor actor;actor.state=spawn;return actor;}
void NetworkWorld::accept(WorldActor& actor,std::uint64_t sequence,MotionInput input,std::chrono::steady_clock::time_point now) const {
    validate_motion(input);
    if(actor.accepted==std::numeric_limits<std::uint64_t>::max()||sequence!=actor.accepted+1) throw std::runtime_error("motion_sequence");
    if(actor.rate_window==std::chrono::steady_clock::time_point{}||now-actor.rate_window>=std::chrono::seconds(1)) {actor.rate_window=now;actor.rate_count=0;}
    if(++actor.rate_count>40) throw std::runtime_error("motion_rate_limit");
    // One replaceable current intention, never an unbounded per-client queue.
    actor.accepted=sequence;actor.input=input;actor.received=now;
}
void NetworkWorld::tick(WorldActor& actor,std::chrono::steady_clock::time_point now) const {
    actor.expired=actor.received==std::chrono::steady_clock::time_point{}||now-actor.received>=std::chrono::milliseconds(world_hold_ms);
    const MotionInput input=actor.expired?MotionInput{}:actor.input;
    const double seconds=world_tick_ms/1000.0;
    const auto moved=geometry.step(actor.state,{config.speed*seconds*input.x/1000.0,config.speed*seconds*input.z/1000.0,
                                               std::numbers::pi*seconds*input.turn/1000.0});
    actor.state=moved.state;actor.applied=actor.accepted;actor.blocked=moved.blocked;
}
Writer write_world_snapshot(const WorldSnapshot& state) {
    Writer body;body.number(state.session,8);body.raw(state.digest);body.number(state.tick,8);
    body.number(state.revision,8);body.number(state.selection,2);body.number(state.participants.size(),1);
    for(const auto& p:state.participants) {
        body.number(p.id,4);body.text(p.name);body.number(p.selection_acknowledged,8);body.number(p.accepted,8);body.number(p.applied,8);
        body.real(p.state.position.x);body.real(p.state.position.y);body.real(p.state.position.z);body.real(p.state.yaw);
        body.number((p.blocked?1:0)|(p.expired?2:0),1);
    }
    return body;
}
WorldSnapshot read_world_snapshot(Reader& body,std::size_t clips) {
    WorldSnapshot result;result.session=body.number(8);const auto digest=body.raw(32);std::copy(digest.begin(),digest.end(),result.digest.begin());
    result.tick=body.number(8);result.revision=body.number(8);result.selection=static_cast<std::uint16_t>(body.number(2));
    if(!result.revision||(result.selection!=0xffff&&result.selection>=clips))throw std::runtime_error("invalid_world_state");
    const auto count=body.number(1);if(count>4)throw std::runtime_error("invalid_state_count");
    std::uint32_t previous=0;
    for(std::uint64_t i=0;i<count;++i) {
        WorldParticipant p;p.id=static_cast<std::uint32_t>(body.number(4));p.name=body.text();p.selection_acknowledged=body.number(8);p.accepted=body.number(8);p.applied=body.number(8);
        p.state.position={body.real(),body.real(),body.real()};p.state.yaw=body.real();const auto flags=body.number(1);
        if(p.id<=previous||!valid_name(p.name)||p.applied>p.accepted||flags>3||std::abs(p.state.position.x)>1e6||std::abs(p.state.position.y)>1e6||std::abs(p.state.position.z)>1e6||std::abs(p.state.yaw)>std::numbers::pi)
            throw std::runtime_error("invalid_world_participant");
        p.blocked=flags&1;p.expired=flags&2;previous=p.id;result.participants.push_back(std::move(p));
    }
    body.finish();return result;
}

}
