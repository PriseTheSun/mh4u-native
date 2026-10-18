#pragma once
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include "player_reference.hpp"
#include <array>
#include <optional>
#include <span>
#include <variant>

namespace mh4u::player_object_query {
struct Unknown {};
struct Absent {};
// Fields of the subobject reached through object+e28, not gameplay names.
// Presence of an Object declares that subobject; unknown fields stay unknown.
struct Object {
    std::optional<std::uint8_t> field140,field34c,field290,field1c0,field14c;
    std::optional<std::uint32_t> field1b4;
    std::optional<std::array<std::uint32_t,3>> position_bits; // D+198/19c/1a0
};
using Slot=std::variant<Unknown,Absent,Object>;
using Manager=std::variant<Unknown,std::span<const Slot>>; // Exactly 32 physical slots.
enum class ErrorCode {unknown_field,manager_extent,unsupported_availability,
    unsupported_position,reference};
struct Error {
    ErrorCode code;std::string_view field;
    std::optional<std::size_t> slot;
    std::optional<player_reference::Error> reference;
};
struct Output {
    bool matched{};
    std::uint8_t slots_examined{},object_queries{},direction_queries{};
};
using Result=std::variant<Output,Error>;
// b34268 -> 6823a8,911e48,91a2a8,91f214 with the retained local availability
// profile. Borrowed views are synchronous, and never stored in Player.
// Unknown is never an empty manager or a failed direction check.
[[nodiscard]] Result query(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const Manager&) noexcept;
}
