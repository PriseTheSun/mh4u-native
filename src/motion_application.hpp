#pragma once
#include "motion_pending.hpp"
#include "motion_transform_math.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_application {
using Bits3=motion_initialization::Bits3;
using Bits4=motion_initialization::Bits4;
struct Model {
    Bits4 position{},rotation{},scale{},null_scale{}; // +40,+50,+60,+bd0.
    Bits4 translation_delta{},rotation_delta{}; // +be0,+bf0.
    bool operator==(const Model&)const=default;
};
struct Limits {
    std::size_t layers=8,bindings_per_layer=256; // Copy/work policy.
    motion_sampler::Limits sampling;
};
enum class Component {none,translation,rotation,model};
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
    Model model;
    // Canonical pending owner retains mi::State and scratch +98/+c8 together.
    std::vector<motion_pending::Layer> layers;
    std::size_t sampled_layers{},contributing_layers{},samples{},prediction_updates{};
};
using Result=std::variant<Output,Error>;
// Complete numeric95cab0. Input layer count is model+234; slots preserve order.
// Uses already prepared L+64/+68, not a second clock. First-layer attributes
// select final model application even if that layer did not contribute.
// Templates have explicit presence; raw copied fields retain their bit patterns.
// Finite consumed arithmetic, nearest-even and gradual underflow are required.
// Borrowed inputs/resources are immutable; failure publishes no partial state.
// Resolver side effects are caller-owned and cannot be rolled back here.
[[nodiscard]] Result apply(const Model&,std::span<const motion_pending::Layer>,
    const motion_initialization::Templates&,motion_initialization::Resolver,Limits={});
} // namespace mh4u::motion_application
