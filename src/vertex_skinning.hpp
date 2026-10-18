#pragma once
#include "model_resource.hpp"
#include "motion_transform_math.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace mh4u::vertex_skinning {
using Bits3=motion_transform_math::Bits3;
using Palette=motion_transform_math::Palette;
using View3=std::array<std::uint32_t,9>; // Three rows, no translation.
inline constexpr std::uint32_t weight_scale_bits=0x3b808000U; // PICA literal370100, exactly representable in binary32.
struct Output {
    Bits3 position_model,normal_skin,normal_view,normal_unit;
    bool operator==(const Output&)const=default;
};
struct Limits {std::size_t palette_entries=24;}; // VS18 upload cap72 rows; caller CPU work bound.
enum class Stage {input,blend,position,normal_skin,normal_view,normal_length,normal_unit};
enum class ErrorCode {unsupported_influences,palette_limit,invalid_palette_index,
    floating_environment,nonfinite_operand,numeric_overflow,degenerate_normal};
struct Error {
    ErrorCode code;std::string_view message;Stage stage=Stage::input;
    std::optional<std::size_t> influence;
    std::size_t component{};std::uint8_t palette_index{};
};
using Result=std::variant<Output,Error>;
// Source-derived VS18, no morph: raw position and signed normal XYZ; exactly2/4
// influences in source order. Every index/matrix is consumed, even weight0.
// k*w stays unnormalized. CPU profile: nearest-even binary32, gradual underflow,
// separate rounded MUL then ADD for MAD and left-to-right dots. RSQ is explicitly
// sqrtf, reciprocal, multiply; this does not claim PICA MAD/DP/RSQ bit equivalence.
// Finite operands/intermediates; zero/underflowed normal norm is a typed error.
// View rows explicit; no camera/projection, Q conversion, lighting or UV work.
// Borrowed inputs remain unchanged and failures publish no partial Output.
[[nodiscard]] Result skin(const model_resource::Vertex&,std::span<const Palette>,
                          const View3&,Limits={});
} // namespace mh4u::vertex_skinning
