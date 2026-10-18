#pragma once
#include "motion_transform_math.hpp"
#include "player_fields.hpp"
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::player_update_accumulator {
struct Output {player_fields::Fields fields;};
enum class ErrorCode {unknown_field,arithmetic};
struct Error {
    ErrorCode code;
    std::string_view field;
    std::optional<motion_transform_math::Error> cause;
};
using Result=std::variant<Output,Error>;
// Complete b09ccc for disjoint canonical storage and explicit player+14 bits.
// Adds PBW+54 and player+14 once in binary32, then compares the SUM'S RAW BITS
// as signed32 against 0x4b189680; the selected overwrite is positive zero.
// Finite consumed operands/result, nearest-even, gradual underflow. Errors
// publish no fields, including an overflow that original code later clears.
// No unit, initial value, pacing or implicit invocation is inferred.
[[nodiscard]] Result apply(const player_fields::Fields&,
    std::optional<std::uint32_t> delta_bits);
} // namespace mh4u::player_update_accumulator
