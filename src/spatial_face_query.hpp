#pragma once
#include "motion_transform_math.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>

namespace mh4u::spatial_face_query {
using Word=std::uint32_t;
using Bits3=std::array<Word,3>;
using Bits4=std::array<Word,4>;
// Values borrowed from the resource owner. The source normal is authoritative;
// this leaf neither derives nor normalizes it. Vertex order is preserved.
struct Face {Bits3 source_normal;std::array<Bits3,3> vertices;};
struct Vertical {
    std::optional<Bits3> origin;
    std::optional<Word> direction_y;
};
struct Replacement {
    Bits4 position,opposed_direction,plane;
    Word t_bits{};
    std::uint8_t flag{}; // Original context.byteD8 is written as zero on acceptance.
    bool operator==(const Replacement&)const=default;
};
enum class ErrorCode {unknown_operand,nonfinite_operand,numeric_overflow,floating_environment,unsupported_arithmetic};
struct Error {
    ErrorCode code;std::string_view field;
    std::optional<motion_transform_math::Error> arithmetic;
};
template<class T>using Result=std::variant<T,Error>;
// Original17fec8 only: direct resource carrier, byte32==0, vertical type0x15,
// untransformed source geometry. All consumed arithmetic must remain finite,
// binary32 nearest-even with gradual underflow and no contraction.
// No replacement means a proven miss; it never publishes partial output.
// Origin is consumed only after orientation/parallel gates. maximum_t is
// consumed only after t>=0 and the height calculation. Equality accepts.
// The caller retains prior state on miss/error, and copies the selected face
// identity/carrier only on acceptance. Opaque carrier bytes are not interpreted.
[[nodiscard]] Result<std::optional<Replacement>> intersect(
    const Face&,const Vertical&,std::optional<Word> maximum_t);
} // namespace mh4u::spatial_face_query
