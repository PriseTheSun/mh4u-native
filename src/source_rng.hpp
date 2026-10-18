#pragma once
#include <cstdint>
namespace mh4u::source_rng {
// Shipped2b555c literal2b55a0; not a configurable generator variant.
inline constexpr std::uint32_t source_multiplier=0x2015aea7U;
// Complete numeric update of one selected u16 seed. The caller owns its
// channel/state (GlobalFields.field20 for channel0, field22 for channel1).
// Zero substitutes1 locally before computation; the returned value is both
// the original return value and its store. No seed, clock or scheduler retained.
[[nodiscard]] std::uint16_t advance_seed(std::uint16_t seed) noexcept;
} // namespace mh4u::source_rng
