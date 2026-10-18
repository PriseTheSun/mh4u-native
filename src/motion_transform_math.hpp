#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::motion_transform_math {
using Bits3=std::array<std::uint32_t,3>;
using Bits4=std::array<std::uint32_t,4>;
using Matrix=std::array<std::uint32_t,16>; // Source row-major memory; no glTF convention.
using Basis=std::array<std::uint32_t,12>;
using Palette=std::array<std::uint32_t,12>;
enum class ErrorCode {floating_environment,nonfinite_operand,numeric_overflow,degenerate_quaternion,missing_template};
struct Error {ErrorCode code;std::string_view message;};
template<class T>using Result=std::variant<T,Error>;
// Shared instruction orders in 95df38/95e6ec and 958884. Finite consumed
// operands/results, nearest-even and gradual underflow; every operation rounds
// separately, without FMA. No clamping or normalization of the inputs.
[[nodiscard]] Result<std::uint32_t> multiply(std::uint32_t,std::uint32_t);
[[nodiscard]] Result<std::uint32_t> add(std::uint32_t,std::uint32_t);
// One rounded DIV; zero denominators/nonfinite results fail explicitly.
[[nodiscard]] Result<std::uint32_t> divide(std::uint32_t,std::uint32_t);
[[nodiscard]] Result<Bits3> lerp(const Bits3& current,const Bits3& incoming,std::uint32_t weight);
[[nodiscard]] Result<Bits3> add_scaled(const Bits3& current,const Bits3& incoming,std::uint32_t weight);
[[nodiscard]] Result<Bits4> nlerp(const Bits4& current,const Bits4& incoming,std::uint32_t weight);
[[nodiscard]] Result<Bits4> multiply_rotation(const Bits4& delta,const Bits4& current);
// Sum X²,Y²,Z²,W² in source order. Inverse uses reciprocal norm and
// VNMUL xyz; norm<=0 copies the explicit fallback, preserving raw bits.
[[nodiscard]] Result<Bits4> inverse_rotation(const Bits4&,const std::optional<Bits4>& fallback);
// Source95dde0: sqrt(norm), reciprocal, then multiply each component.
// Zero/underflowed norm is a typed error, never a fabricated quaternion.
[[nodiscard]] Result<Bits4> normalize_rotation(const Bits4&);
// 95e6ec additive scale: current * ((incoming-template)*weight+template).
[[nodiscard]] Result<Bits3> scale_additive(const Bits3& current,const Bits3& incoming,
                                         const Bits3& reference,std::uint32_t weight);
// 6d385c and9608d8 local RT, without normalization. Translation is copied raw.
[[nodiscard]] Result<Matrix> rt_matrix(const Bits4& rotation,const Bits3& translation);
// Ordered four-term row/column dot products, with separate VMUL/VMLA rounding.
[[nodiscard]] Result<Matrix> matrix_product(const Matrix& left,const Matrix& right);
// Only first three rows, including their fourth components, consume scale.
[[nodiscard]] Result<Matrix> scale_rows(const Matrix&,const Bits3& scale);
// 6d3948 attachment mode0 replaces basis,1 normalizes above source threshold,
// 2 keeps row norms on diagonal,others copy unchanged. Last row stays raw.
[[nodiscard]] Result<Matrix> attachment_basis(const Matrix&,std::uint32_t mode,
                                             const std::optional<Basis>& reference);
// 959d00: first three columns of B*W, transposed into3rows of4,
// adding context[c] only to the last component of each output row.
// W column3 is not read; all16 B values are consumed. No affine shortcut.
[[nodiscard]] Result<Palette> palette_product(const Matrix& source,const Matrix& world,
                                             const Bits3& context);
} // namespace mh4u::motion_transform_math
