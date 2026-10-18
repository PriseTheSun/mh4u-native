#pragma once
#include "binary32_arithmetic.hpp"
#include "player_controller_step.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::player_direction_refresh {
using Word=std::uint32_t;
enum class Virtual84 {uplayer_b085bc};
enum class Virtual144 {uplayer_b47aa0};
struct Bindings {
    std::optional<Virtual84> virtual84;
    std::optional<Virtual144> virtual144;
};
enum class Call : std::uint8_t {byte6f9,state_action,virtual144,virtual84};
struct Effects {
    Word rate{},step{};
    std::uint16_t difference{};
    bool snapped{};
    std::array<Call,5> calls{};
    std::uint8_t call_count{};
};
enum class Phase {byte6f9,virtual144,delta,rate_conversion,scale,step_conversion,virtual84};
enum class ErrorCode {unknown_field,unknown_binding,unsupported_binding,arithmetic};
struct Error {
    Phase phase;ErrorCode code;std::string_view field;
    std::optional<binary32_arithmetic::Error> arithmetic;
};
struct Output {player_controller_step::State state;Effects effects;};
using Result=std::variant<Output,Error>;
// Complete b6d2a4 with the concrete, stable uPlayer virtual identities.
// Delta is the caller's single Player+14 snapshot. Only canonical PBW24 changes;
// PWb0 retains its full word. No clock, input-device or physical meaning follows.
// virtual144 is consumed only when PBW6f9==1; virtual84 is always consumed.
// b085bc's Player244 load has a dead result: no duplicate field or native read
// requirement is invented. Effects preserve helper order, not instruction reads
// or FPSCR flags. Raw NaN/Inf/conversion results follow binary32_arithmetic.
// The borrowed state/bindings remain stable during the call. Errors publish no
// state and every successful result preserves all other canonical fields.
[[nodiscard]] Result apply(const player_controller_step::State&,
    std::optional<Word> delta_bits,const Bindings&) noexcept;
} // namespace mh4u::player_direction_refresh
