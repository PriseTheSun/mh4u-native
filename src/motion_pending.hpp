#pragma once
#include "joint_pose.hpp"
#include "motion_initialization.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_pending {
using Bits4=motion_initialization::Bits4;
struct Layer {
    motion_initialization::State animation;
    Bits4 rotation_98{},translation_c8{}; // Additional fields reset by 9577ec.
};
struct Snapshot {
    std::uint32_t layer_index=0xffffffffU,flags{};
    Bits4 rotation{},translation{},scale{}; // Both T/S fourth words preserved unless copied.
    bool operator==(const Snapshot&)const=default;
};
struct State {
    // Lengths are the explicit active C/N counts. Unused original allocation
    // capacity is not inferred. J, B and per-layer A are distinct owners.
    std::vector<Layer> layers;
    std::vector<joint_pose::Joint> joints;
    std::vector<Snapshot> snapshots;
    std::uint32_t calculation_flags{},accumulated_flags{}; // model+238/+23c.
};
struct ActionResolutionError {std::uint32_t code{};std::string message;};
using ActionResolution=std::variant<std::optional<motion_binding::Action>,ActionResolutionError>;
struct ActionResolver {
    const void* context{};
    // Resolve exactly this bank/slot. Unknown source is an error; absent bank,
    // explicitly null action table or empty slot is nullopt. Resource owner
    // and all returned views must outlive apply(). No LMT parsing here.
    ActionResolution (*resolve)(const void*,std::uint32_t bank,std::uint32_t slot){};
};
struct Parameters {
    joint_pose::ModelReference model_reference;
    motion_initialization::Parameters initialization;
    std::optional<joint_pose::Bits3> scale_10e6380;
};
struct Limits {
    std::size_t layers=8,joints=256; // Copy/work policy; not general format maxima.
    motion_initialization::Limits initialization;
    joint_pose::Limits restoration;
};
enum class ErrorCode {resource_limit,snapshot_extent,binding_extent,unknown_model,
    missing_template,unresolved_action,action_identity,initialization,restoration};
struct Error {
    ErrorCode code;std::string_view message;
    std::size_t layer{},joint{};
    std::optional<ActionResolutionError> resolution;
    std::optional<motion_initialization::Error> initialization;
    std::optional<joint_pose::Error> restoration;
};
struct Output {
    State state;
    bool pending_found{},restored_joints{};
    std::uint32_t initializer_calls{};
};
using Result=std::variant<Output,Error>;
// Full 9577ec, composing canonical 95aa90 and 95c7dc owners. Does not advance
// clocks, sample joint poses, dispatch gameplay or infer resource bank policy.
// All copied storage is bounded before allocation; errors publish no partial
// state. Pure bit comparisons/copies preserve raw nonfinite fields. Consumed
// arithmetic domains are owned by initialization/restoration/sampling.
[[nodiscard]] Result apply(const State&,std::span<const std::uint8_t> bone_to_joint,
    const Parameters&,ActionResolver,motion_initialization::Resolver,Limits={});
} // namespace mh4u::motion_pending
