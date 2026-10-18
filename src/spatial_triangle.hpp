#pragma once
#include "motion_transform_math.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::spatial_triangle {
using Bits3=std::array<std::uint32_t,3>;
using Bits4=std::array<std::uint32_t,4>;
struct Triangle {std::array<Bits3,3> vertices;};
struct Vertical {
    // Unknown origin is not consumed when the plane/direction test rejects.
    std::optional<Bits3> origin;
    std::optional<std::uint32_t> direction_y;
};
struct Hit {
    Bits4 position,normal; // W words are +0; normal is not the source face normal.
    std::uint32_t scalar0_bits{},t_bits{};
    std::uint8_t flag{};
    bool operator==(const Hit&)const=default;
};
struct Output {bool intersects{};std::optional<Hit> hit;};
enum class ErrorCode {unknown_operand,nonfinite_operand,numeric_overflow,floating_environment,unsupported_arithmetic};
struct Error {
    ErrorCode code;std::string_view field;
    std::optional<motion_transform_math::Error> arithmetic;
};
template<class T>using Result=std::variant<T,Error>;
// Finite binary32 cc94f0 -> 1eb400 -> bdcf50, exact instruction association.
// No maximum t, manager enumeration, transforms, collision policy or SBC parser.
// write_hit=false still performs the numerical query; only the caller's final
// output record is absent. A miss never publishes a Hit.
[[nodiscard]] Result<Output> intersect(const Triangle&,const Vertical&,bool write_hit=true);
// 193c18..193c44 only: true means ACCEPT (source dot <= 0). This does not
// implement the other carrier branches, and does not normalize source_normal.
[[nodiscard]] Result<bool> normal_gate(const Bits3& source_normal,const Bits3& test_vector);
} // namespace mh4u::spatial_triangle
