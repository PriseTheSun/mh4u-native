#pragma once
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include "player_state1_dynamics.hpp"
#include "spatial_query.hpp"

namespace mh4u::player_spatial_acquisition {
using Word=std::uint32_t;
struct Context {
    // Borrowed for this call only. Null means Unknown, not an empty world.
    const spatial_query::Scene* scene=nullptr;
    std::optional<Word> area_enable_mask; // *fcf45c +18, not the query area ID.
    spatial_query::Limits query_limits{};
};
struct Request {Word override_height{},offset_bits{};};
struct Effects {
    bool cache_used{},cache_written{};
    std::optional<spatial_query::HeightResult> height;
    std::optional<Word> attribute_mask,wrapper_flags;
    std::optional<spatial_query::Bits3> current_point,previous_point;
};
struct Output {player_fields::Fields fields;bool returned{};Effects effects;};
enum class ErrorCode {unknown_field,nonfinite_operand,arithmetic,rotation,query,capacity};
struct Error {
    ErrorCode code;std::string_view field;
    std::variant<std::monostate,motion_transform_math::Error,
        player_state1_dynamics::Error,spatial_query::Error> cause;
};
using Result=std::variant<Output,Error>;
// Complete b40b88, composing 2ba230's zero-extra-mask profile and 2bb178.
// Cache and grid updates are returned together, with no external mutation.
// A caller must commit grid updates only after its entire transaction succeeds.
// The Scene owner's supported geometry routes and finite arithmetic profile
// remain explicit; no default world, previous position or filter is invented.
[[nodiscard]] Result apply(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const Request&,const Context& = {});
} // namespace mh4u::player_spatial_acquisition
