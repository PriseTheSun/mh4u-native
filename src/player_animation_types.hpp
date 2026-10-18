#pragma once
#include "motion_command.hpp"
#include "model_animation_output.hpp"

namespace mh4u::player_animation {
// Shared declarations owned here, independent of the update/placement pipeline.
struct FrameContext {
    std::span<const std::uint8_t> bone_to_joint;
    motion_pending::Parameters pending; // Sole step delta; no internal timer/rate.
    model_animation_frame::Resolvers resolvers;
    joint_output::Parameters output;
};
struct Limits {
    std::size_t joints=256,snapshots=256,bindings=2048,order=256;
    motion_command::Limits command;
    model_animation_frame::Limits frame;
    motion_transition::Limits transition;
    joint_output::Limits output;
};
struct FrameEffects {
    bool pending_found{},restored_joints{},blend_layer_found{},attachment_cleared{};
    std::optional<joint_update::Leaf> joint_leaf;
    std::optional<joint_output::SelectedCallback> output_callback;
    std::size_t initializer_calls{},prepared_layers{},model_samples{},joint_samples{},
        loop_iterations{},event_queries{},blended_joints{},output_joints{};
};
// Borrowed frame resources only: delta and initialization templates retain
// their preceding update owners. Model presence is shared by restoration and
// hierarchy publication, rather than accepted as two conflicting facts.
struct FrameResources {
    std::span<const std::uint8_t> bone_to_joint;
    joint_pose::ModelReference model_reference=joint_pose::Unknown{};
    std::optional<joint_pose::Bits3> restoration_scale;
    model_animation_frame::Resolvers resolvers;
    joint_output::ParentReference parent=joint_pose::Unknown{};
    std::optional<motion_transform_math::Basis> reference_basis;
    std::optional<joint_pose::Bits4> reference_translation;
};
} // namespace mh4u::player_animation
