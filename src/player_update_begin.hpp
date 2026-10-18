#pragma once
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include <string_view>
#include <variant>

namespace mh4u::player_update_begin {
struct Output {
    player_controller_step::PlayerFields player;
    player_fields::Fields fields;
};
enum class ErrorCode {unknown_field};
struct Error {ErrorCode code;std::string_view field;};
using Result=std::variant<Output,Error>;
// Complete b3b3a0 for the canonical disjoint Player/PBW/PW owners. Copies raw
// current XYZ/area, clears the source update flags and invalidates the height
// cache without consuming or changing its stored height. No float arithmetic.
// Explicit operation: caller gates/order belong to b1407c and b48230, which
// are not implemented here. Never invoke implicitly per action or query.
// An unknown consumed source returns an error without publishing any stores;
// overwritten destinations need not have been known before this operation.
[[nodiscard]] Result apply(const player_controller_step::PlayerFields&,
    const player_fields::Fields&) noexcept;
} // namespace mh4u::player_update_begin
