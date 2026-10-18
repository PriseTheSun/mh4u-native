#pragma once
#include <array>
#include <cstdint>
#include <variant>

namespace mh4u::input_filter {

// Numeric source fields, not physical device axes or a mutable calibration state.
struct Axes {
    std::int16_t first{},second{};
    bool operator==(const Axes&) const = default;
};
struct Limits {
    std::int16_t inner{},outer{};
    bool operator==(const Limits&) const = default;
};
struct Config {
    std::uint8_t source_mode{}; // 0 -> C+0/+6; 1 -> C+2/+8; others unsupported.
    std::array<Limits,2> limits{};
    bool operator==(const Config&) const = default;
};
enum class Error {unsupported_mode,invalid_limits,unsafe_arithmetic,unsupported_rounding};
using FilterResult=std::variant<Axes,Error>;
struct Normalized {
    std::array<float,2> ratios{}; // 4858f8, before multiplication by 32767.
    std::array<std::int32_t,2> words{}; // 5b8328 numeric stores, truncation toward zero.
    bool operator==(const Normalized&) const = default;
};
using NormalizeResult=std::variant<Normalized,Error>;
enum class Phase {filter,normalize};
struct Failure {
    Error reason{};
    Phase phase{};
    std::uint8_t pair{};
    bool operator==(const Failure&) const = default;
};
struct Sample {
    std::array<Axes,2> filtered{};
    std::array<Normalized,2> normalized{};
    bool operator==(const Sample&) const = default;
};
using ComposeResult=std::variant<Sample,Failure>;

// Profile: selected limits 0 <= inner < outer (signed16). Integer operations
// must fit their original signed32 intermediates. The root branch additionally
// requires 0 < n <= 131071; early returns preserve their larger valid domain.
// Unknown modes and unsafe inputs are rejected; no setter/clamp is invented.
[[nodiscard]] FilterResult filter(const Config& config,Axes input) noexcept;

// Uses only the selected limits. Nearest-even float32 division, then float32
// multiplication by 32767, then signed32 truncation. Negative values are NOT
// clamped to -1. Mode2's normalizer is excluded with its unimplemented filter.
[[nodiscard]] NormalizeResult normalize(const Config& config,Axes filtered) noexcept;

// Each pair uses its own filter config. BOTH pairs then use configs[1] for
// normalization, matching 4858f8's fixed C1 lookup. Transactional value result:
// no partial sample on failure, allocations, retained references, or input writes.
// No acquisition, dirty setter, scheduler, button handling, or device mapping.
[[nodiscard]] ComposeResult compose(const std::array<Config,2>& configs,
                                    const std::array<Axes,2>& input) noexcept;
}
