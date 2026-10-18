#pragma once
#include "joint_pose.hpp"
#include "motion_initialization.hpp"
#include "motion_transform_math.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::joint_update {
struct Metadata {
    std::uint32_t flags94{},mapping98{0xff};
    bool operator==(const Metadata&)const=default;
};
struct LayerView {
    const motion_request::Layer& layer;
    const motion_binding::Tracks& tracks;
    std::uint32_t prepared_frame_bits{}; // Original L+64, already prepared.
};
enum class Leaf {rotation_translation,rotation_translation_scale};
struct Parameters {
    Leaf leaf{Leaf::rotation_translation};
    std::uint32_t flags238{};
    joint_pose::ModelReference model;
    joint_pose::Templates templates;
};
struct Limits {
    std::size_t layers=8,joints=256,bindings_per_layer=256;
    motion_sampler::Limits sampling;
};
enum class ErrorCode {resource_limit,metadata_extent,unknown_model,null_model,
    unknown_table,null_table,table_extent,binding_extent,mirror_extent,
    missing_template,unresolved_track,sampling,matrix,arithmetic,unsupported_leaf};
struct Error {
    ErrorCode code;std::string_view message;
    std::size_t layer{},joint{},channel{};
    std::optional<motion_binding::TrackReference> track;
    std::optional<motion_initialization::ResolutionError> resolution;
    std::optional<motion_sampler::Error> sampling;
    std::optional<joint_pose::Error> matrix;
    std::optional<motion_transform_math::Error> arithmetic;
};
struct Output {
    std::vector<joint_pose::Joint> joints;
    std::vector<motion_binding::Tracks> tracks; // Same layer order/identities.
    std::size_t samples{},updated_joints{};
};
using Result=std::variant<Output,Error>;
// Complete tail-entry leaves 95df38 and 95e6ec, including multiple layers,
// additive mixing, overwrite attribute80000 and mapped mirror attribute100.
// Does not execute the preceding95b790 preparation/95cab0, clock or transition
// snapshots. Layer fields and metadata are read-only; canonical Channel owns
// all cursor mutations. Resource/Joint/Channel storage is originally disjoint.
// rModel/table pointer facts are consumed even with zero layers or joints;
// a null table is allowed until an actual absent-Q matrix fallback consumes it.
// Raw copies preserve bits, arithmetic has each owner's finite/environment
// domain. Errors publish neither partial joints nor partially advanced cursors.
[[nodiscard]] Result apply_layers(std::span<const joint_pose::Joint>,
    std::span<const Metadata>,std::span<const LayerView>,const Parameters&,
    motion_initialization::Resolver,Limits={});
} // namespace mh4u::joint_update
