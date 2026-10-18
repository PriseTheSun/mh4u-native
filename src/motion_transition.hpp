#pragma once
#include "joint_update.hpp"
#include "motion_pending.hpp"
#include "motion_transform_math.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_transition {
struct LayerView {const motion_request::Layer& layer;};
struct Parameters {
    joint_pose::Bits4 model_rotation;
    joint_pose::Bits3 model_translation;
    std::uint32_t accumulated_flags{}; // O+23c.
};
struct Limits {std::size_t layers=8,joints=256;}; // Allocation/work policy.
enum class Component {none,model_rt,rotation,translation,scale};
enum class ErrorCode {resource_limit,snapshot_extent,metadata_extent,layer_extent,arithmetic};
struct Error {
    ErrorCode code;std::string_view message;
    std::size_t joint{},layer{};Component component{Component::none};
    std::optional<motion_transform_math::Error> arithmetic;
};
struct Output {
    std::vector<joint_pose::Joint> joints;
    joint_pose::Matrix local_rt;
    std::uint32_t accumulated_flags{};
    bool blend_layer_found{};
    std::size_t blended_joints{};
};
using Result=std::variant<Output,Error>;
// Complete95b490 including6d385c. Canonical joints/layers/snapshots remain
// borrowed and unchanged. RT is built before scanning layer.flags&100;
// absence clears accumulated_flags. Otherwise each snapshot's owner selects
// layer.field60 independently of that layer's flags/selector/attributes.
// Sentinel owner, joint flags&41 and +/-zero weight skip in that order.
// Snapshot/metadata/owner extents are checked only where read by the original.
// Arithmetic uses the shared finite-consumed binary32 domain; raw copies
// preserve bits. Errors publish no partial joint state or model matrix.
// No attachment, hierarchical output, clock, sampler, scheduler or gameplay.
[[nodiscard]] Result apply(std::span<const joint_pose::Joint>,
    std::span<const joint_update::Metadata>,std::span<const motion_pending::Snapshot>,
    std::span<const LayerView>,const Parameters&,Limits={});
} // namespace mh4u::motion_transition
