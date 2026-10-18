#pragma once
#include "player_update_continuation.hpp"
#include "player_state1.hpp"

namespace mh4u::player_update_dispatch {
// Only new borrowed resources. Shared reference/delta/manager/area-mode facts
// come from preparation; object queries use its updated Collection candidate.
struct Resources {
    const player_state1::Tables& tables;
    const motion_request::Model& model;
    const motion_command::Globals& globals;
    motion_command::Sampling sampling;
    std::span<const player_selection::AreaMappingRecord> area_mapping{};
    std::span<const std::optional<std::uint8_t>> area_categories{};
    player_spatial_acquisition::Context spatial{};
};
struct Context {
    player_update_continuation::Context preparation;
    const Resources& resources;
};
struct Dispatch {
    std::uint8_t state{};
    std::uint16_t action{};
    std::uint8_t substate{};
    player_state1::Effects effects;
};
struct Output {
    player_update_continuation::Output prepared;
    player_state1::Fields action_fields;
    std::array<Dispatch,2> dispatches{};
    std::uint8_t dispatch_count{};
};
enum class Phase {preparation,first_dispatch,second_dispatch,storage};
enum class ErrorCode {dependency,capacity};
using Cause=std::variant<std::monostate,player_update_continuation::Error,player_state1::Error>;
struct Error {Phase phase;ErrorCode code;std::string_view field;Cause cause;};
using Result=std::variant<Output,Error>;
// b1407c through BEFORE b1441c. Dispatches b66a20 once, then exactly once
// more when its resulting PBW376 is nonzero, with PW01=1 before that call.
// The second call reads the first call's candidate; its PBW376 is preserved.
// State1's recovered handlers and source state3 no-op share the existing owner.
// No virtual+10c, animation clock, publication or full frame is implied.
// Returned spatial updates belong to the same transaction as Player/Collection.
// The recovered handlers admit at most one spatial query in a successful
// result: ID2 spatial transitions leave this dispatch profile; ID2->ID9 uses
// the other branch. Extending those handlers requires revisiting grid replay.
[[nodiscard]] Result apply(const player_controller_step::State&,
    const player_state1::Fields&,std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const player_fields::Fields&,const player_object_query::Manager&,
    const Context&,motion_command::Limits = {});
} // namespace mh4u::player_update_dispatch
