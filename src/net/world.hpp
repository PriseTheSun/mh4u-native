#pragma once
#include "content.hpp"
#include "wire.hpp"
#include "world_config.hpp"
#include "locomotion.hpp"
#include <chrono>
#include <memory>

namespace mh4u::net {
inline constexpr std::uint16_t world_protocol_version=2;
inline constexpr int world_tick_ms=50,world_hold_ms=250;
inline constexpr std::uint16_t world_initial_selection=0;
struct MotionInput { int x{},z{},turn{}; };
void validate_motion(MotionInput);
void write_motion(Writer&,MotionInput);
MotionInput read_motion(Reader&);
struct WorldActor {
    locomotion::State state;
    MotionInput input;
    std::uint64_t accepted{},applied{};
    std::chrono::steady_clock::time_point received{},rate_window{};
    unsigned rate_count{};
    bool blocked{},expired{};
};
struct WorldParticipant {
    std::uint32_t id{};std::string name;
    std::uint64_t selection_acknowledged{},accepted{},applied{};
    locomotion::State state;
    bool blocked{},expired{};
};
struct WorldSnapshot {
    std::uint64_t session{},tick{},revision{};
    Digest digest{};
    std::uint16_t selection{};
    std::vector<WorldParticipant> participants;
};
Writer write_world_snapshot(const WorldSnapshot&);
WorldSnapshot read_world_snapshot(Reader&,std::size_t clip_count);
struct NetworkWorld {
    WorldConfig config;
    WorldIdentity identity;
    locomotion::World geometry;
    locomotion::State spawn;
    explicit NetworkWorld(const std::filesystem::path&);
    WorldActor join() const;
    void accept(WorldActor&,std::uint64_t,MotionInput,std::chrono::steady_clock::time_point) const;
    void tick(WorldActor&,std::chrono::steady_clock::time_point) const;
};
}
