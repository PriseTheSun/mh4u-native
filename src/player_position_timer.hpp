#pragma once
#include "binary32_arithmetic.hpp"
#include "player_fields.hpp"
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::player_position_timer {
using Word=std::uint32_t;
enum class Phase {increment,decrement,entry,restore,x_multiply,x_add,y_multiply,y_add,z_multiply,z_add};
enum class ErrorCode {unknown_field,arithmetic};
struct Error {
    ErrorCode code;Phase phase;std::string_view field;
    std::optional<binary32_arithmetic::Error> cause;
};
struct TimerOutput {Word value_bits;Word returned;};
using TimerResult=std::variant<TimerOutput,Error>;
using WordResult=std::variant<Word,Error>;
// Complete b04c24: timer + player14 (operand order matters for NaNs).
[[nodiscard]] WordResult increment(std::optional<Word> timer,std::optional<Word> delta);
// Complete b069fc. BLE first clamps nonpositive/unordered timer to +0, r0=1.
// Otherwise subtract delta; BHI preserves a positive OR unordered result with
// r0=0, and clamps all other results to +0 with r0=1. The original early load
// of delta is semantically dead on the clamp branch; unknown delta is allowed.
[[nodiscard]] TimerResult decrement(std::optional<Word> timer,std::optional<Word> delta);
struct Effects {
    bool timer_called{};
    std::optional<Word> timer_returned;
    bool position_integrated{},position_restored{};
};
struct Output {player_fields::Fields fields;Effects effects;};
using Result=std::variant<Output,Error>;
// Standalone b14610..before b146c4 with source s16=+0 and canonical disjoint
// PW/PBW storage. Entry BLS skips ordered nonpositive timers unchanged. The
// remaining paths call decrement and either restore raw PW90/94/98 plus W=0,
// or perform nonfused VMLA on PBW10/14/18 using PW a0/a4/a8 and player14.
// Borrowed inputs remain unchanged on error; no preceding caller, clock,
// renderer, scheduler or later helpers execute. NaN/Inf results are retained.
[[nodiscard]] Result apply(const player_fields::Fields&,std::optional<Word> delta);
} // namespace mh4u::player_position_timer
