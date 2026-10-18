#pragma once
#include <array>
#include <cstdint>
#include <variant>

namespace mh4u::input_quantization {

// Numeric sPad fields after acquisition/calibration, not physical device axes.
struct PairInput {std::int32_t first{},second{};};
struct Pair {
    std::int8_t first{},second{};
    std::uint16_t degrees{},angle{},magnitude{};
    std::uint8_t sector{};
    bool operator==(const Pair&) const = default;
};
struct Sample {
    std::array<Pair,2> pairs{};
    std::uint16_t direction_bits{};
    bool operator==(const Sample&) const = default;
};
enum class Error {unsupported_rounding};
using Result=std::variant<Sample,Error>;

// Profile of 0x6947f8..0x6949b8: enabled numeric block, threshold 30,
// original source constants, nearest-even float32. No scheduler/gates/buttons.
// Full signed32 domain: division truncates toward zero, then low byte wraps.
// No allocation, retained references, input mutation, or rounding-mode changes.
// Floating-point exception status flags are not part of the output contract.
[[nodiscard]] Result convert(const std::array<PairInput,2>& input) noexcept;

}
