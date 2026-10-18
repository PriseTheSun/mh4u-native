#pragma once
#include "joint_pose.hpp"
#include "joint_update.hpp"
#include "model_resource.hpp"
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::joint_bootstrap {
struct Output {
    std::vector<joint_pose::Joint> joints;
    std::vector<joint_pose::Matrix> matrices;
    std::vector<joint_update::Metadata> metadata;
    std::vector<std::uint32_t> depth90,parameter8c;
    std::vector<std::uint8_t> order;
    // The original metadata initializer sets every callback J+88 to null and
    // owner J+a0 to the model itself. These are facts, not borrowed pointers.
    bool callbacks_null=true,owners_self=true;
};
struct Limits {std::size_t joints=255;};
enum class ErrorCode {resource_limit,extent,parent_extent,parent_cycle,missing_scale,matrix};
struct Error {ErrorCode code;std::string_view message;std::size_t joint{};
    std::optional<joint_pose::Error> matrix;};
using Result=std::variant<Output,Error>;
// Source960f7c..9613b0 plus original95b260 order construction, with preallocated
// ownership supplied by the caller. Not the allocator/resource/GPU prefix960b50.
// records and A are parsed source facts; prior_mapping98 preserves bits17..31
// which neither95fcac nor this metadata tail initializes. No zero default.
// Raw translation/scale/matrix copies preserve bits; matrix-to-Q uses the
// canonical finite arithmetic contract without normalization or matrix repair.
// Parents must form a bounded forest. Original invalid-parent/cycle paths have
// no safe bounds; they are rejected here. Count256 is rejected because source
// M+220 is assigned count&255 before this tail. No gameplay/bootstrap policy.
[[nodiscard]] Result initialize(std::span<const model_resource::JointRecord>,
    std::span<const joint_pose::Matrix>,std::span<const std::uint32_t> prior_mapping98,
    const std::optional<joint_pose::Bits3>& scale_template,Limits={});
}
