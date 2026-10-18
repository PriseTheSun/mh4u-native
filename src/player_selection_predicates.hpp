#pragma once
#include <cstdint>

namespace mh4u::player_selection {
// b07e80 returns one only for the exact byte value1. Callers establish
// knownness before using this predicate; direct byte tests can differ.
[[nodiscard]] inline constexpr bool flag6f9(std::uint8_t value) noexcept {
    return value==1;
}
} // namespace mh4u::player_selection
