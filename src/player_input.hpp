#pragma once
#include "player_reference.hpp"
#include <array>
#include <cstdint>
#include <string_view>
#include <variant>

namespace mh4u::player_input {
// Two source-state channels: pbw+0x3c0/+0x3c2 angles and +0x3c4/+0x3c6
// magnitudes. Device identity, units, producers and movement names are unknown.
struct Channels {
    std::array<std::uint16_t,2> angles;
    std::array<std::uint16_t,2> magnitudes;
};
struct SectorScope {
    std::uint32_t mode; // Exact original r2 bits; only value 1 selects the virtual gate.
    std::uint8_t reference_flag; // Original pbw+0x451. This profile requires zero.
};
enum class ErrorCode { unsupported_mode,unsupported_reference,invalid_channel,reference_failure,unknown_gate };
struct Error {
    ErrorCode code;
    std::string_view message;
    std::optional<player_reference::ErrorCode> reference_reason{};
};
template<class T>using Result=std::variant<T,Error>;
std::string_view error_name(ErrorCode) noexcept;

// Original 0xb23f94 -> 0xb2ab7c subset: exact ID dispatch, magnitude>=50,
// inclusive modular-u16 angular intervals, no reference-angle adjustment.
// Unknown IDs return false BEFORE consulting channels or scope, as the source
// does. For recognized IDs, mode 1 and nonzero reference_flag fail explicitly.
// Other mode values take the verified ordinary branch. No gate is simulated,
// no channel is mapped to a physical device, and overlapping sectors remain
// overlapping; caller priority is outside this API. Inputs are never modified.
Result<bool> in_sector(const Channels&,std::uint32_t id,const SectorScope&) noexcept;

struct ReferenceContext {
    std::uint32_t orientation; // pw+b0, subtraction wraps before reduction to u16.
    player_reference::State source;
};
// Same dispatcher, with the ordinary (non-mode1) reference gate reproduced.
// Unknown IDs still return first. A nonzero reference flag consults flags;
// bit0x4000 suppresses adjustment. Otherwise heading is resolved BEFORE the
// magnitude test. Context fields that the source ignores are not validated.
Result<bool> in_sector(const Channels&,std::uint32_t id,const SectorScope&,const ReferenceContext&) noexcept;

// Original 0xb0a41c leaf only: magnitude 0..49 -> 0, 50..109 -> 1, 110..65535 -> 3.
// Accepts source channels 0/1. This DOES NOT implement virtual+0x98/0xb417a4,
// whose external gates may return 0 regardless of this classification.
Result<std::uint32_t> magnitude_class(const Channels&,std::uint32_t channel) noexcept;

// Concrete uPlayer virtual+98 (b417a4). Missing fields remain unknown and are
// required only in source evaluation order. Suppression occurs before the leaf
// reads its channel. These fields are raw bytes, not boolean approximations.
struct MagnitudeGate {
    std::optional<std::uint8_t> type,field6f9,field50d;
};
Result<std::uint32_t> gated_magnitude_class(const Channels&,std::uint32_t channel,const MagnitudeGate&) noexcept;
struct Arc {std::uint32_t channel;std::uint16_t lower,upper;};
struct ArcContext {
    SectorScope scope;
    const ReferenceContext* reference=nullptr;
    const MagnitudeGate* gate=nullptr;
    bool force_reference=false;
};
// b2ab7c ordinary/reference route or b24b60 when mode==1. The arc is inclusive
// and modular. Mode1 ignores reference inputs; other modes preserve the source
// order (reference before magnitude). No direction priority is inferred.
Result<bool> in_arc(const Channels&,const Arc&,const ArcContext&) noexcept;
Result<bool> in_sector_with_context(const Channels&,std::uint32_t id,const ArcContext&) noexcept;
} // namespace mh4u::player_input
