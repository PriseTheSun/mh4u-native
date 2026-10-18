#pragma once
#include "notification_queue.hpp"
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include <string_view>

namespace mh4u::player_status_notifications {
struct Attempt {
    std::uint16_t id{};
    bool permitted{};
    // Absent means permission denied, not a fabricated queue return value.
    std::optional<notification_queue::Effects> queue{};
    std::optional<notification_queue::PoolIndex> inserted{};
};
struct Effects {
    std::array<Attempt,3> attempts{};
    std::uint8_t count{};
    std::uint8_t second_byte{}; // Observed local byte after the completed route.
};
enum class Phase {edges,rising,falling,tail,mode1};
enum class ErrorCode {unknown_field,queue};
struct Error {
    Phase phase;ErrorCode code;std::string_view field;
    std::optional<std::uint8_t> attempt{};
    std::optional<notification_queue::Error> queue{};
};
struct Output {notification_queue::State queue;Effects effects{};};
using Result=std::variant<Output,Error>;
struct Mode1Output {notification_queue::State queue;Attempt attempt;};
using Mode1Result=std::variant<Mode1Output,Error>;
// b0af48 mode1 ->5a9fb8. The canonical local_control gate owns permission;
// denied requests do not consume any queue fact. value narrows to low16.
[[nodiscard]] Mode1Result mode1(const player_controller_step::PlayerFields&,
    const notification_queue::State&,std::uint32_t value) noexcept;
// Complete b41804 with its explicit inherited second stack byte. No generic
// zero default: only a separately proved caller may supply derived zero.
// Current/previous masks remain at their existing owners; no writes to them.
// Attempts execute sequentially even after null insertion/cleanup results.
// Entire queue candidate returned only on complete success. No incidental R0
// bool, display meaning, input scheduler or world queue startup is inferred.
[[nodiscard]] Result apply(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const notification_queue::State&,
    std::optional<std::uint8_t> prior_second_byte) noexcept;
}
