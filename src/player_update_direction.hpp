#pragma once
#include "player_update_notifications.hpp"
#include "player_direction_refresh.hpp"

namespace mh4u::player_update_direction {
enum class MaterialPath {not_called,unchanged_ordinal,already_selected};
struct MaterialEffects {
    MaterialPath path=MaterialPath::not_called;
    std::optional<std::uint8_t> previous_ordinal,current_ordinal,selected_bank;
};
enum class MaterialErrorCode {unknown_field,unsupported_resource_change};
struct MaterialError {
    MaterialErrorCode code;std::string_view field;
    std::optional<std::uint8_t> selected_bank;
};
enum class Phase {notifications,material,direction};
struct Error {
    Phase phase;
    std::variant<player_update_notifications::Error,MaterialError,player_direction_refresh::Error> cause;
};
struct Context {
    const player_update_post_dispatch::Context& preceding;
    player_direction_refresh::Bindings direction;
};
struct Output {
    player_update_notifications::Output notified;
    MaterialEffects material;
    player_direction_refresh::Effects effects;
};
using Result=std::variant<Output,Error>;
// Original b1407c through BEFORE b14610, after b6d2a4 returns. The candidate
// from notifications supplies every direction field and the sole entry delta.
// Type13's b29f7c early returns are executed only when their exact predicates
// hold. A resource-changing path is explicitly unsupported until its complete
// material/resource owners are available; no call is replaced with a no-op.
// Any later error discards the earlier Player/Collection/Scene/queue candidate.
[[nodiscard]] Result apply(const player_controller_step::State&,
    const player_state1::Fields&,std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const player_fields::Fields&,const player_object_query::Manager&,
    const Context&,const notification_queue::State&,motion_command::Limits = {});
}
