#pragma once
#include "model_animation_frame.hpp"
#include "motion_transition.hpp"
#include "joint_output.hpp"

namespace mh4u::model_animation_output {
// Output metadata has no second persistent joint pose or model transform.
struct JointOutput {
    joint_update::Metadata metadata;
    joint_pose::Matrix matrix;
    std::uint32_t depth{};
    joint_output::Callback callback=joint_output::Callback::unknown;
    joint_output::Owner owner=joint_output::Owner::unknown;
    bool operator==(const JointOutput&)const=default;
};
struct Hierarchy {
    joint_pose::Matrix local_rt,world;
    std::uint32_t flags4{},flags104{},flags230{},attachment_index{};
    std::uint16_t attachment_mode_high16{}; // high half of model+38 only; no Euler storage.
    std::uint8_t dirty{};
    std::uint32_t fieldc4c{};
    std::vector<JointOutput> joints;
    joint_output::Order order=joint_pose::Unknown{};
};
struct Limits {
    std::size_t layers=8,joints=256,snapshots=256,order=256;
    motion_transition::Limits transition;
    joint_output::Limits output;
};
struct Effects {
    bool blend_layer_found{},attachment_cleared{};
    std::optional<joint_output::SelectedCallback> output_callback;
    std::size_t blended_joints{},output_joints{};
};
struct Output {
    std::vector<joint_pose::Joint> joints;
    std::uint32_t accumulated_flags{};
    Hierarchy hierarchy;
    Effects effects;
};
enum class DomainCode {joint_extent,resource_limit};
struct DomainError {DomainCode code;std::string_view message;};
enum class Phase {domain,transition,hierarchy};
struct Error {Phase phase;std::variant<DomainError,motion_transition::Error,joint_output::Error> cause;};
using Result=std::variant<Output,Error>;
// 95b490 ->95b940, independently of9573b8. The caller may apply the original
// placement wrapper between the frame and this publication. Canonical frame
// state/resources are borrowed; only the changed poses/flags/hierarchy return.
// All copies are bounded first. Failure publishes no partial output.
[[nodiscard]] Result apply(const model_animation_frame::State&,const Hierarchy&,
    const joint_output::Parameters&,Limits={});
}
