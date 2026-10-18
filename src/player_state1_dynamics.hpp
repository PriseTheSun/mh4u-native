#pragma once
#include "player_fields.hpp"
#include "player_selection.hpp"
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::player_controller_step {struct PlayerFields;}
namespace mh4u::player_state1_dynamics {
enum class ErrorCode {unknown_field,nonfinite_operand,numeric_overflow,unsupported_floating_environment};
struct Error {
    ErrorCode code;std::string_view field;
    std::optional<player_selection::Error> selection;
};
struct Output {
    player_fields::Fields fields;
    // True when source stores execute, including writes of unchanged bits.
    bool updated{};
};
using Result=std::variant<Output,Error>;
// b464fc(-0.5) -> b2a58c(mode0). Source gates are evaluated lazily;
// only pbw+354 can be written. The repeated local-control helper is preserved.
[[nodiscard]] Result cost(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,std::optional<std::uint32_t> delta_bits,
    std::optional<std::uint32_t> context_ad50);
// b2a318(2.5), using source table e36e98 and separate binary32 operations.
// Writes all3 position words10/14/18. No renderer/world-space interpretation.
[[nodiscard]] Result advance(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,std::optional<std::uint32_t> delta_bits);
} // namespace mh4u::player_state1_dynamics
