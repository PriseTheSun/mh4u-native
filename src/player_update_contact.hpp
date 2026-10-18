#pragma once
#include "player_update_manager.hpp"
#include "player_contact_update.hpp"

namespace mh4u::player_update_contact {
struct Context {
    const player_update_manager::Context& preceding;
    std::optional<spatial_query::Bits3> null_reference_template;
};
enum class Phase {manager,grid,contact};
struct Error {
    Phase phase;
    std::variant<player_update_manager::Error,spatial_query::Error,player_contact_update::Error> cause;
};
struct Output {
    player_update_manager::Output managed;
    player_position_bridge::Fields position;
    player_contact_update::Effects effects;
    // Complete grid candidate across dispatch and contact. Earlier effects
    // retain their observations; this plan alone is eligible for publication.
    std::vector<spatial_query::GridUpdate> grid_updates;
};
using Result=std::variant<Output,Error>;
// Original b1407c through BEFORE b14700. The source delta, reference, manager
// and spatial resources come from the same preceding context. Each reached
// query observes preceding grid changes without mutating the borrowed Scene.
// Player.d2c retains its existing placement owner. Opaque attribute tail words
// remain Unknown. Reference points are explicitly independent snapshots; this
// interface does not infer dynamically aliased original-memory references.
[[nodiscard]] Result apply(const player_controller_step::State&,
    const player_state1::Fields&,std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const player_fields::Fields&,const player_position_bridge::Fields&,
    const player_object_query::Manager&,const Context&,const notification_queue::State&,
    motion_command::Limits = {});
}
