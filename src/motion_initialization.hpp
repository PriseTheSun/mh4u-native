#pragma once
#include "motion_binding.hpp"
#include "motion_sampler.hpp"
#include <array>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace mh4u::motion_initialization {
using Bits4=std::array<std::uint32_t,4>;
using Bits3=std::array<std::uint32_t,3>;
struct State {
    motion_request::Layer layer;
    motion_binding::Tracks tracks;
    // Original offsets relative to the selected model layer, not extra clocks.
    Bits4 prediction_rotation; // +20, read only by attributes&10000.
    Bits4 prediction_translation; // +30, only XYZ is consumed.
    Bits4 previous_rotation; // +78.
    Bits4 base_rotation; // +88.
    Bits4 previous_translation; // +a8; written W is +0.
    Bits4 base_translation; // +b8; written W is +0.
};
struct Templates {
    std::optional<Bits3> base_translation_10e62c0;
    std::optional<Bits3> previous_translation_10e6370;
    std::optional<Bits4> identity_10e6560;
    std::optional<Bits4> inverse_fallback_10e6570;
};
struct Parameters {
    std::uint32_t model_delta_time_bits; // model+14; source unit not inferred.
    Templates templates;
};
// The adapter owns diagnostics and views for the duration of initialize().
// Missing resolution of a PRESENT identity is an error, never a null channel.
struct ResolutionError {std::uint32_t code{};std::string message;};
using Resolution=std::variant<motion_sampler::TrackView,ResolutionError>;
struct Resolver {
    const void* context{};
    Resolution (*resolve)(const void*,const motion_binding::TrackReference&){};
};
struct Limits {motion_binding::Limits binding;motion_sampler::Limits sampling;};
enum class ErrorCode {binding,sampling,unresolved_track,missing_template,
    floating_environment,nonfinite_operand,numeric_overflow};
struct Error {
    ErrorCode code;
    std::string_view message;
    std::optional<motion_binding::TrackReference> track;
    std::optional<ResolutionError> resolution;
    std::optional<motion_binding::Error> binding;
    std::optional<motion_sampler::Error> sampling;
};
struct Output {State state;bool action_present{};std::uint32_t samples{};};
using Result=std::variant<Output,Error>;
// Original 95aa90 through return: prepare_tracks owns the prefix and Channel;
// motion_sampler owns all sampling/cursor updates. At most six samples are
// requested sequentially. Inputs and borrowed resources are never mutated.
// Frames/attributes/events remain owned by Layer. This does not consume the
// caller's pending flags, run the animation clock, or infer scheduler timing.
// Finite arithmetic operands/results, nearest-even and gradual underflow are
// required only when consumed. Raw copies preserve every bit. No quaternion
// normalization, host FMA, hidden templates or invented samples are applied.
[[nodiscard]] Result initialize(const State&,std::span<const std::uint8_t> bone_to_joint,
    const motion_binding::Action*,const Parameters&,Resolver,Limits={});
} // namespace mh4u::motion_initialization
