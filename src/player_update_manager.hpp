#pragma once
#include "player_update_position.hpp"
#include "player_manager_update.hpp"

namespace mh4u::player_update_manager {
struct Context {
    const player_update_direction::Context& preceding;
    player_manager_update::Manager manager;
};
enum class Phase {position,manager};
struct Error {
    Phase phase;
    std::variant<player_update_position::Error,player_manager_update::Error> cause;
};
struct Output {
    player_update_position::Output positioned;
    std::optional<player_manager_update::ManagerState> manager;
    std::optional<player_manager_update::ForeignPatch> foreign;
    player_manager_update::Effects effects;
};
using Result=std::variant<Output,Error>;
// Original b1407c through BEFORE b146d8. The two following calls consume
// canonical candidate fields/objects and the same reference/virtual binding
// used by preceding phases. External manager and Player changes remain
// unpublished candidates. No new startup, scheduler or model bridge is implied.
[[nodiscard]] Result apply(const player_controller_step::State&,
    const player_state1::Fields&,std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const player_fields::Fields&,const player_object_query::Manager&,
    const Context&,const notification_queue::State&,motion_command::Limits = {});
}
