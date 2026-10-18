#pragma once
#include "player_object_query.hpp"
#include "player_position_timer.hpp"
#include "player_status_predicates.hpp"

namespace mh4u::player_status_update {
using Word=std::uint32_t;
enum class Phase {flags,object_scan,timer,event,secondary_flags};
enum class ErrorCode {unknown_field,dependency,unsupported_event};
using Cause=std::variant<std::monostate,binary32_arithmetic::Error,player_selection::Error,
    player_object_query::Error,player_position_timer::Error>;
struct Error {Phase phase;ErrorCode code;std::string_view field;Cause cause;};
struct Effects {
    bool local{},status224{},timer_incremented{},timer_cleared{};
    std::array<std::uint16_t,4> list_queries{};
    std::uint8_t query_count{};
    std::optional<player_object_query::StatusScanOutput> scan;
};
struct Output {
    Word field1f4{}; // Replaces the canonical player.pw.field1f4 after success.
    player_fields::Fields fields;
    Effects effects;
};
using Result=std::variant<Output,Error>;
// Original b22164, composing the concrete gates and read-only object scan.
// Threshold crossing that reaches c08238 is an explicit unsupported event:
// no partial candidate is published. Other paths return original numeric
// results, including signed raw-word timer/ratio tests and unordered floats.
// This leaf is not implicitly scheduled and does not advance the caller.
// Manager is the f32004 singleton snapshot; PW/PBW and all graph links retain
// their bindings throughout this synchronous call. Results describe semantic
// state, not original read/store traces or processor exception flags.
[[nodiscard]] Result apply(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const player_object_query::Manager&,
    std::optional<Word> delta_bits);
}
