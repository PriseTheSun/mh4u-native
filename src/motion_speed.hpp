#pragma once
#include "binary32_arithmetic.hpp"
#include "motion_request.hpp"
#include <cstdint>
#include <variant>

namespace mh4u::motion_command {struct Fields;}
namespace mh4u::motion_speed {
// Canonical Layer fields only; the caller selects layer0 or layer1. No storage,
// clock, track sampling, pointer emulation or secondary copy of model+d68.
enum class Phase {ratio,scaled,interpolation,remaining};
struct Error {Phase phase;binary32_arithmetic::Error cause;};
struct Output {motion_request::Layer layer;bool rescaled{};};
using Result=std::variant<Output,Error>;
// Complete numeric b05f84, with ordered remaining>0 then new speed!=+/-0.
// Other words, including unconsumed NaNs, stay raw. The arithmetic branch uses
// FPSCR0 binary32_arithmetic, permits NaN/Inf results, and fails transactionally
// on an unsupported host environment (the original first store is not published).
[[nodiscard]] Result primary(const motion_request::Layer&,
    const motion_command::Fields&,std::uint32_t speed_bits);
// b061f0 for a valid native layer object. Only raw speed is written; the original
// wrapped/null address check has no counterpart in a C++ reference.
[[nodiscard]] motion_request::Layer secondary(const motion_request::Layer&,
    std::uint32_t speed_bits) noexcept;
} // namespace mh4u::motion_speed
