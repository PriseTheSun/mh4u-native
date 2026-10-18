#pragma once
#include "player_input.hpp"
#include <array>
#include <optional>
#include <span>
#include <variant>

namespace mh4u::player_policy_predicate {
struct Predicate {
    std::uint16_t word;
    std::array<std::uint16_t,3> arguments;
};
// The caller owns the 22-byte policy record. Only the predicate fields are
// interpreted here; state/action, timing, context and priority belong to the
// policy interpreter, which must not be replaced by a first-true scan.
Predicate decode(std::span<const std::uint8_t,22>) noexcept;
struct Context {
    player_input::Channels channels;
    player_input::MagnitudeGate magnitude_gate;
    std::optional<std::uint16_t> field3a8,field3ac,field3b8,field3bc;
    // Canonical words beginning at pbw+3d4. b2ac30's base+3e4 is index4 in
    // this same view, avoiding independently supplied aliases of source bytes.
    std::span<const std::optional<std::uint32_t>> flags3d4;
    std::optional<std::uint8_t> reference_flag;
    std::optional<player_input::ReferenceContext> reference;
    std::optional<std::uint32_t> field24,field154;
    std::optional<std::array<std::uint16_t,2>> direction_bounds; // runtime100c250
};
enum class ErrorCode {unknown_field,flag_extent,input_failure,reference_failure};
struct Error {
    ErrorCode code;
    std::string_view field;
    std::optional<player_input::Error> input;
    std::optional<player_reference::Error> reference;
};
using Result=std::variant<bool,Error>;
// Complete b24354 dispatcher, including concrete uPlayer virtual+98 and its
// numeric helper chains. This reads an explicit source context; it does not
// map physical input, change latches, choose actions, or execute the table loop.
Result evaluate(const Predicate&,const Context&) noexcept;
} // namespace mh4u::player_policy_predicate
