#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::joint_pose {
using Bits3=std::array<std::uint32_t,3>;
using Bits4=std::array<std::uint32_t,4>;
using Matrix=std::array<std::uint32_t,16>; // Source memory order, not a glTF convention.
struct Joint {
    Bits3 reference_translation; // Original J+48; preserved.
    Bits4 rotation; // J+58.
    Bits4 scale; // J+68, including padding+74.
    Bits4 translation; // J+78, including padding+84.
    bool operator==(const Joint&)const=default;
};
struct Unknown {};
struct Null {};
using MatrixTable=std::variant<Unknown,Null,std::span<const Matrix>>;
struct PresentModel {MatrixTable matrices;};
using ModelReference=std::variant<Unknown,Null,PresentModel>;
struct Templates {
    std::optional<Bits3> translation; // 10e6370.
    std::optional<Bits4> rotation; // 10e6560.
    std::optional<Bits3> scale; // 10e6380.
};
struct Limits {std::size_t joints=256;}; // Caller resource policy, not a format maximum.
enum class ErrorCode {resource_limit,unknown_model,null_model,unknown_table,
    null_table,table_extent,missing_template,floating_environment,
    nonfinite_operand,numeric_overflow,numeric_domain};
struct Error {ErrorCode code;std::string_view message;std::size_t joint_index{};};
struct Restored {std::vector<Joint> joints;};
using Result=std::variant<Restored,Error>;
// Canonical source matrix extraction shared by 95c7dc and joint-update leaves.
// Same finite consumed-field/environment contract; no normalization or repair.
[[nodiscard]] std::variant<Bits4,Error> matrix_rotation(const Matrix&);
// Original 95c7dc through return. J has a single owner; transition snapshots
// belong to the pending consumer and are not aliases of these transforms.
// The caller guarantees owned, disjoint original J/resource/template storage.
// A present model's table may be explicitly null when there are zero joints;
// an unknown table is still an error because the original reads rModel+6c.
// With flags&10000000, model/table are unconsumed. Zero joints consume no
// templates and no float environment. Copies preserve all raw bits; arithmetic
// requires finite consumed operands/results, nearest-even and gradual underflow.
// No quaternion normalization, matrix repair, clock or gameplay inference.
// Returned state is transactional; borrowed matrix storage must outlive this call.
[[nodiscard]] Result restore(std::span<const Joint>,std::uint32_t flags238,
                            const ModelReference&,const Templates&,Limits={});
} // namespace mh4u::joint_pose
