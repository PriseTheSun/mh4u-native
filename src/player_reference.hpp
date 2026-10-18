#pragma once
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::player_reference {
// Original object fields +40/+48 and +60/+68. Physical/camera identity and
// the producer of these coordinates remain outside this numeric consumer.
struct Points { float first_x,first_z,second_x,second_z; };
struct Unknown {bool operator==(const Unknown&)const=default;};
struct Absent {bool operator==(const Absent&)const=default;};
// External manager+1c fact. Unknown is never an implicitly null object.
using Reference=std::variant<Unknown,Absent,Points>;
struct State {
    std::uint32_t flags; // pw+f0
    std::optional<std::uint8_t> stored_mode; // pw+150; unknown until consumed
    std::optional<std::uint32_t> stored_heading; // pw+14c; full word, NOT truncated here
    std::optional<Points> points; // nullopt means manager+1c was explicitly null
};
enum class ErrorCode { nonfinite_input,nonfinite_difference,unsupported_floating_environment,unknown_field };
struct Error { ErrorCode code;std::string_view message; };
using Result=std::variant<std::uint32_t,Error>;
using StateResult=std::variant<State,Error>;
// Temporary, lazy projection of borrowed caller facts. Stored mode/heading
// remain owned by the caller. flags4000 with mode unknown or1 does not consume
// the external reference; heading() owns the eventual stored-field validation.
[[nodiscard]] StateResult resolve_state(std::uint32_t flags,
    std::optional<std::uint8_t> stored_mode,std::optional<std::uint32_t> stored_heading,
    const Reference&) noexcept;
// Original b1061c -> 568b54 -> 5687b8. Stored/null branches ignore points and
// floating state. The vector branch uses the exact original 33-entry table,
// separately rounded binary32 operations and round-to-nearest arithmetic.
// No runtime, callbacks, memory mutation or continuous atan2 substitution.
Result heading(const State&) noexcept;
// Complete numeric b3ca88: heading then low16(heading-0x4000+angle0).
// Returns the new full pw+b0 word without modifying the caller's state.
[[nodiscard]] Result orient(const State&,std::uint16_t angle0) noexcept;
} // namespace mh4u::player_reference
