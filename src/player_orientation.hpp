#pragma once
#include "motion_transform_math.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
namespace mh4u::player_orientation {
using Angles=std::array<std::uint16_t,3>;
using Quaternion=motion_transform_math::Bits4;
enum class ErrorCode {unknown_mode,unsupported_mode,arithmetic};
struct Error {ErrorCode code;std::string_view message;std::optional<motion_transform_math::Error> arithmetic;};
using Result=std::variant<Quaternion,Error>;
// b05350 ->6d4080 with explicit already-narrowed source angles, modes0..5,
// and the concrete b05328 virtual80 (returns0). No model mutation or host trig.
// Unknown mode differs from out-of-profile mode; neither means mode0.
// Ordered finite binary32, nearest-even and gradual underflow. No final
// normalization: source matrix-to-Q and W-sign canonicalization are retained.
[[nodiscard]] Result calculate(const Angles&,std::optional<std::uint16_t> euler_mode);
}
