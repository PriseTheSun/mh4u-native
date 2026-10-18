#pragma once
#include "player_input_filter.hpp"
#include "player_update_accumulator.hpp"
#include "player_update_begin.hpp"
#include "player_world_lookup.hpp"

namespace mh4u::player_update_prefix {
struct Context {
    player_input_filter::Context filter;
    std::optional<std::uint32_t> delta_bits;
    player_world_lookup::Context world;
};
struct Output {
    player_controller_step::PlayerFields player;
    player_fields::Fields fields;
    // Absent means the outer d04924 gate skipped b33638 completely.
    std::optional<player_input_filter::Effects> filter;
    player_world_lookup::Output lookup;
};
enum class Phase {filter,accumulator,begin,lookup};
struct Error {
    Phase phase;
    std::variant<player_input_filter::Error,player_update_accumulator::Error,
        player_update_begin::Error,player_world_lookup::Error> cause;
};
using Result=std::variant<Output,Error>;
// b1407c through b14100, stopping before b14104: local gate/filter, accumulator,
// begin, area lookup and PW5a8 publication. Later caller operations and scheduler
// remain outside this prefix. Consumes explicit immutable snapshots and commits
// no partial result on failure; it does not advance an animation clock.
[[nodiscard]] Result apply(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const Context&);
} // namespace mh4u::player_update_prefix
