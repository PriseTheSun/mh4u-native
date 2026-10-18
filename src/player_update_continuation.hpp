#pragma once
#include "player_update_admission.hpp"
#include "player_update_statistics.hpp"

namespace mh4u::player_update_continuation {
struct Context {
    const player_update_admission::Parameters& admission;
    const player_update_statistics::Context& statistics;
    const player_transition::Constants& constants;
};
enum class FirstReturn {state8,local_positive_timer};
enum class SecondReturn {disabled,local_ordered_values,local_zero_flag};
struct EarlyReturns {FirstReturn first;SecondReturn second;};
struct Output {
    // One candidate owns all canonical state, layers and mutable collection.
    // prepared.effects retains the evidence of the admission prefix.
    player_update_admission::Output prepared;
    player_update_statistics::Effects statistics;
    EarlyReturns returns;
};
enum class Phase {admission,statistics,first_helper,second_helper,caller};
enum class ErrorCode {dependency,unknown_field,unsupported_first_helper,unsupported_second_helper,capacity};
using Cause=std::variant<std::monostate,player_update_admission::Error,player_update_statistics::Error>;
struct Error {Phase phase;ErrorCode code;std::string_view field;Cause cause;};
using Result=std::variant<Output,Error>;
// Original b1407c entry through the checkpoint BEFORE b143f8 callsb66a20.
// Executes all parameter queries and admits the source early returns of
// b2f09c/b42108. Their remaining bodies are explicit unsupported paths.
// No caller-selected helper return, publication, frame clock or first dispatch.
[[nodiscard]] Result apply(const player_controller_step::State&,
    std::span<const motion_pending::Layer>,const player_fields::Fields&,
    const player_object_query::Manager&,const Context&,motion_command::Limits = {});
} // namespace mh4u::player_update_continuation
