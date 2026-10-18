#pragma once
#include "motion_command.hpp"
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::player_policy_timing {
// Borrow the canonical first animation layer and command fields. No duplicate
// clock state, pointer emulation, rate conversion or persistent projection.
struct Context {
    const motion_request::Layer& layer;
    const motion_command::Fields& fields;
    std::optional<std::uint32_t> delta_time_bits; // model+14; consumed lazily.
};
struct Query {std::uint32_t mode,lower_bits,upper_bits;};
enum class ErrorCode {floating_environment,nonfinite_operand,numeric_overflow,unknown_delta};
struct Error {ErrorCode code;std::string_view field;};
using Result=std::variant<bool,Error>;
// Complete b09da8 -> b064fc with null optional fraction output, the call used
// by b0d0e4. All modes0..12 and default returns, finite consumed arithmetic,
// FPSCR0 equivalent rounding; no table iteration, context gate or action change.
[[nodiscard]] Result query(const Context&,const Query&) noexcept;
}
