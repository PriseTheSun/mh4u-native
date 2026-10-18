#pragma once
#include "motion_initialization.hpp"
#include "motion_transform_math.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_query {
using Bits4=motion_initialization::Bits4;
// Borrow canonical state. Only the two model-channel cursors can change;
// joints, root-motion clocks and initialization vectors are not copied here.
struct LayerView {
    const motion_request::Layer& layer;
    const motion_binding::Channel& translation;
    const motion_binding::Channel& rotation;
};
struct Channels {
    motion_binding::Channel translation,rotation;
    bool operator==(const Channels&)const=default;
};
struct OutputSelection {bool translation=true,rotation=true;};
struct Limits {
    std::size_t layers=8; // Caller work/allocation policy, not a format maximum.
    motion_sampler::Limits sampling;
};
enum class Component {none,translation,rotation};
enum class ErrorCode {resource_limit,missing_template,unresolved_track,sampling,arithmetic};
struct Error {
    ErrorCode code;std::string_view message;
    std::size_t layer{};Component component=Component::none;
    std::optional<motion_binding::TrackReference> track;
    std::optional<motion_initialization::ResolutionError> resolution;
    std::optional<motion_sampler::Error> sampling;
    std::optional<motion_transform_math::Error> arithmetic;
};
struct Output {
    // Same count/order as input, including skipped/inactive layers. The caller
    // applies these channels to their original owners only after success.
    std::vector<Channels> channels;
    std::optional<Bits4> translation,rotation;
    std::size_t accepted_layers{},samples{};
};
using Result=std::variant<Output,Error>;
// Complete 958884. Explicit output absence suppresses only final writes:
// all accepted layers are sampled and blended even with neither output.
// base_translation_10e62c0 and identity_10e6560 are read at entry, even when
// layers/output are empty. Other initialization templates are unconsumed.
// Source Layer and Channel values remain canonical and borrowed. Resource
// views/resolvers must outlive the call. Errors publish no changed cursor.
// Callback side effects are caller-owned and cannot be rolled back here.
// No motion clock, joint pose, bank placement or gameplay policy is inferred.
[[nodiscard]] Result query(std::span<const LayerView>,std::uint32_t frame_bits,
    const motion_initialization::Templates&,OutputSelection,
    motion_initialization::Resolver,Limits={});
} // namespace mh4u::motion_query
