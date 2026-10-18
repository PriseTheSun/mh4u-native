#pragma once
#include "player_update_post_dispatch.hpp"
#include "player_status_update.hpp"

namespace mh4u::player_update_status {
enum class Phase {adjustment,status};
struct Error {
    Phase phase;
    std::variant<player_update_post_dispatch::Error,player_status_update::Error> cause;
};
struct Output {
    player_update_post_dispatch::Output adjusted;
    player_status_update::Effects effects;
};
using Result=std::variant<Output,Error>;
// Original b1407c through BEFORE b145e4: the returned b22164 consumes the
// same canonical candidate, current Collection and entry delta. A reached
// event emission is still an explicit failure with no partial publication.
// Manager is the same f32004 binding used throughout the caller. No extra
// snapshot, standalone helper clock, animation step or scheduler is implied.
[[nodiscard]] Result apply(const player_controller_step::State&,
    const player_state1::Fields&,std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_post_dispatch::Context&,motion_command::Limits = {});
}
