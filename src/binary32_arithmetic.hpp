#pragma once
#include <cstdint>
#include <string_view>
#include <variant>

namespace mh4u::binary32_arithmetic {
using Word=std::uint32_t;
enum class ErrorCode {unsupported_environment};
struct Error {ErrorCode code;std::string_view reason;};
using Result=std::variant<Word,Error>;
// Scalar results of the retained ARM11 VFP add/sub/mul/div expressions, FPSCR0.
// Raw NaN payload/sign/priority and invalid-operation default NaN are explicit.
// Native binary32 arithmetic requires RNE and gradual underflow; no FP flags,
// traps, machine state or implicit clock are exposed. All operand words remain
// caller owned. Infinities, overflow and NaN results are valid numeric outputs.
[[nodiscard]] Result add(Word left,Word right);
[[nodiscard]] Result sub(Word left,Word right);
[[nodiscard]] Result mul(Word left,Word right);
[[nodiscard]] Result div(Word left,Word right);
// VCVT with FPSCR0: signed/unsigned words to binary32, and binary32 to
// saturated integer words with round-toward-zero. Signed results retain their
// two's-complement encoding; NaN converts to zero. No FP flags are exposed.
[[nodiscard]] Result from_signed_word(Word);
[[nodiscard]] Result from_unsigned_word(Word);
[[nodiscard]] Result to_signed_word_rtz(Word);
[[nodiscard]] Result to_unsigned_word_rtz(Word);
} // namespace mh4u::binary32_arithmetic
