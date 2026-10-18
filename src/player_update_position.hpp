#pragma once
#include "player_update_direction.hpp"
#include "player_position_timer.hpp"

namespace mh4u::player_update_position {
enum class Phase {direction,position};
struct Error {
    Phase phase;
    std::variant<player_update_direction::Error,player_position_timer::Error> cause;
};
struct Output {
    player_update_direction::Output directed;
    player_position_timer::Effects effects;
};
using Result=std::variant<Output,Error>;
// Original b1407c through BEFORE b146c4. Position/timer reads the canonical
// shared fields produced by preceding stages and the sole Player14 delta.
// A skipped timer leaves unknown saved position/rates untouched. Later errors
// discard the whole candidate; no model-position bridge or scheduler is implied.
[[nodiscard]] Result apply(const player_controller_step::State&,
    const player_state1::Fields&,std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_direction::Context&,const notification_queue::State&,motion_command::Limits = {});
}
