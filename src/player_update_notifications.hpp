#pragma once
#include "player_update_status.hpp"
#include "player_status_notifications.hpp"

namespace mh4u::player_update_notifications {
enum class Phase {status,notifications};
struct Error {
    Phase phase;
    std::variant<player_update_status::Error,player_status_notifications::Error> cause;
};
struct Output {
    player_update_status::Output status;
    notification_queue::State queue;
    player_status_notifications::Effects effects;
};
using Result=std::variant<Output,Error>;
// Original b1407c through BEFORE b145f0, after b41804 returns. The queue is
// an explicit world-owned snapshot, shared between Players, not a per-Player
// default. Later errors discard this candidate and all preceding state.
// This source profile fixes disjoint queue/Player/stack bindings. b22164 saves
// caller s16=+0 at callerSP-40, supplying the retained local byte of b41804.
// No previous-mask copy, queue initialization, consumer or scheduler is inferred.
[[nodiscard]] Result apply(const player_controller_step::State&,
    const player_state1::Fields&,std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_post_dispatch::Context&,const notification_queue::State&,
    motion_command::Limits = {});
}
