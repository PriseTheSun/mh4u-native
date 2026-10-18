#pragma once
#include <cstdint>
#include <string_view>
#include <variant>

namespace mh4u::binary32_arithmetic {
using Word=std::uint32_t;
enum class ErrorCode {unsupported_environment};
struct Error {ErrorCode code;std::string_view reason;};
using Result=std::variant<Word,Error>;
// Scalar results of the retained ARM11 VFP add/sub/div expressions, FPSCR0.
// Raw NaN payload/sign/priority and invalid-operation default NaN are explicit.
// Native binary32 arithmetic requires RNE and gradual underflow; no FP flags,
// traps, machine state or implicit clock are exposed. All operand words remain
// caller owned. Infinities, overflow and NaN results are valid numeric outputs.
[[nodiscard]] Result add(Word left,Word right);
[[nodiscard]] Result sub(Word left,Word right);
[[nodiscard]] Result div(Word left,Word right);
} // namespace mh4u::binary32_arithmetic
