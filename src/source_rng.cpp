#include "source_rng.hpp"
namespace mh4u::source_rng {
std::uint16_t advance_seed(std::uint16_t seed) noexcept {
    const auto product=std::uint32_t{seed?seed:std::uint16_t{1}}*176U;
    // UMLAL has the source literal already in its low accumulator word.
    const auto wide=std::uint64_t{source_multiplier}+std::uint64_t{product}*source_multiplier;
    const auto quotient=static_cast<std::uint32_t>(wide>>32)>>13;
    return static_cast<std::uint16_t>(product+quotient*173U);
}
} // namespace mh4u::source_rng
