#pragma once
#include "player_selection.hpp"
#include "player_fields.hpp"
#include "player_policy_predicate.hpp"
#include "player_policy_timing.hpp"
#include <array>
#include <optional>
#include <variant>

namespace mh4u::player_policy_interpreter {
using Reference=player_reference::Reference;
struct Context {
    const motion_request::Layer& layer; // canonical first layer, read-only here
    const motion_command::Fields& motion;
    std::optional<std::uint32_t> delta_time_bits;
    const player_transition::Constants& constants;
    Reference reference=player_reference::Unknown{}; // manager+1c
    std::optional<std::uint8_t> context_ad24;
    std::optional<std::uint32_t> global_c8c;
    std::optional<std::array<std::uint16_t,2>> direction_bounds;
};
enum class ErrorCode {unknown_field,unknown_table,floating_environment,nonfinite_operand,
    numeric_overflow,predicate,timing,input,reference,transition,unsupported_context,
    unsupported_special_action,unsupported_selection,unsupported_suffix};
using Cause=std::variant<std::monostate,player_policy_predicate::Error,player_policy_timing::Error,
    player_input::Error,player_reference::Error,player_transition::Error>;
struct Error {ErrorCode code;std::string_view field;std::optional<std::size_t> record;Cause cause;};
struct Output {
    player_selection::State state;player_fields::Fields fields;bool returned{};
    std::array<std::uint8_t,48> visited{};std::size_t visited_count{};
    std::optional<player_selection::TransitionEffects> transition;
};
using Result=std::variant<Output,Error>;
// Ordered b0d0e4, with full table visitation and native predicate/timing/transition
// owners. Unsupported consumed contexts/actions fail transactionally. No ARM
// runtime, physical input mapping, scheduler or implicit field initialization.
// The selection state is the existing temporary controller projection (including
// authoritative layer1 weight), never a second persistent Player state.
[[nodiscard]] Result apply(const player_selection::State&,const player_fields::Fields&,const Context&);
}
