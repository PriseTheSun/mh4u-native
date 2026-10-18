#pragma once
#include "joint_pose.hpp"
#include "joint_update.hpp"
#include "motion_transform_math.hpp"
#include <optional>
#include <span>
#include <variant>
#include <vector>

namespace mh4u::joint_output {
using Matrix=joint_pose::Matrix;
using Bits3=joint_pose::Bits3;
using Bits4=joint_pose::Bits4;
using Unknown=joint_pose::Unknown;
using Null=joint_pose::Null;
// These are explicit resolved pointer facts, never inferred from unset storage.
enum class Callback {unknown,null,unsupported};
enum class Owner {unknown,null,self,other};
struct Joint {
    joint_pose::Joint pose;
    joint_update::Metadata metadata;
    Matrix matrix; // J+8..47; no inverse bind applied.
    std::uint32_t depth{}; // J+90.
    Callback callback=Callback::unknown; // J+88.
    Owner owner=Owner::unknown; // J+a0; consumed only for non-root dirty joints.
    bool operator==(const Joint&)const=default;
};
using Order=std::variant<Unknown,Null,std::vector<std::uint8_t>>;
using Mapping=std::variant<Unknown,Null,std::span<const std::uint8_t>>;
using ParentJoints=std::variant<Unknown,Null,std::span<const Matrix>>;
enum class Getter {unknown,uplayer_d02258,unsupported};
struct Parent {
    std::uint32_t flags4{};
    Matrix world;
    Getter getter=Getter::unknown;
    Mapping mapping=Unknown{};
    ParentJoints joints=Unknown{};
};
using ParentReference=std::variant<Unknown,Null,Parent>;
struct State {
    Matrix local_rt; // O+70, supplied by the preceding transition stage.
    Matrix world; // O+b0.
    Bits3 scale; // O+60 XYZ.
    std::uint32_t flags4{},flags104{},flags230{},flags238{};
    std::uint32_t attachment_index{}; // O+34, interpreted as signed only for negative test.
    std::uint32_t attachment_mode{}; // O+38; upper halfword selects basis policy.
    std::uint8_t dirty{};
    std::uint32_t fieldc4c{};
    std::vector<Joint> joints; // Single canonical pose/metadata owner.
    Order order=Unknown{};
};
struct Parameters {
    joint_pose::ModelReference model=Unknown{}; // Only pointer presence is consumed.
    ParentReference parent=Unknown{};
    std::optional<motion_transform_math::Basis> reference_basis; // 10e6300..632f.
    std::optional<Bits4> reference_translation; // 10e6330..633f.
};
struct Limits {std::size_t joints=256,order=256,parent_joints=256,mapping=256;};
enum class ErrorCode {resource_limit,unknown_model,unknown_parent,unknown_getter,
    unsupported_getter,unknown_mapping,null_mapping,mapping_extent,
    unknown_parent_joints,null_parent_joints,parent_extent,unknown_order,null_order,
    order_extent,unknown_owner,null_owner,foreign_owner,unknown_callback,
    unsupported_callback,unsupported_selected_callback,cycle,missing_template,
    floating_environment,nonfinite_operand,numeric_overflow,numeric_domain};
struct Error {ErrorCode code;std::string_view message;std::size_t joint_index{};};
enum class SelectedCallback {null,normal_9608d8,unsupported_95a7bc,unsupported_95ba60,unsupported_95c24c};
struct Output {
    State state;
    bool attachment_cleared{}; // Caller clears O+30 only when true.
    std::optional<SelectedCallback> selected; // Absent when rModel pointer is null.
    std::size_t updated_joints{};
};
using Result=std::variant<Output,Error>;
// Full 95b940, including 1a63fc/6d3948 and normal/null 95de18 traversal.
// Reads borrowed parent tables synchronously. No returned borrowed storage.
// Original storage is assumed owned and disjoint; same-owner acyclic rebuild
// only. Order is literal (duplicates and skipped parents permitted) when clean.
// Unsupported callbacks fail only if invoked. Unknown unconsumed fields remain
// unknown. Finite consumed arithmetic, nearest-even, gradual underflow; raw
// copies and IEEE equality preserve NaNs/Inf/zero semantics without FP flags.
// Output is transactional; the caller's state is never modified on error.
[[nodiscard]] Result apply(const State&,const Parameters&,Limits={});
} // namespace mh4u::joint_output
