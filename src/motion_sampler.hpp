#pragma once
#include "motion_binding.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace mh4u::motion_sampler {
struct KeyBuffer {
    motion_binding::BufferReference base;
    std::span<const std::byte> bytes;
};
// Non-owning view supplied by the immutable, identity-verified resource owner.
// The resource must outlive sample(). No second LMT interpretation occurs here.
struct TrackView {
    motion_binding::TrackReference identity;
    std::uint32_t metadata{};
    std::array<std::uint32_t,4> reference_bits{};
    std::optional<KeyBuffer> keys;
    std::optional<std::array<std::uint32_t,8>> extremes_bits;
};
struct Limits {std::size_t key_steps=1000000;}; // Caller work budget, no allocation.
enum class ErrorCode {resource_identity,unresolved_track,unsupported_codec,
    unknown_key_data,unknown_extremes,cursor_extent,key_extent,resource_limit,
    floating_environment,nonfinite_operand,numeric_overflow,degenerate_quaternion};
struct Error {
    ErrorCode code;
    std::string_view message;
    std::uint64_t byte_offset{};
};
struct Sample {
    std::array<std::uint32_t,4> value_bits{}; // s0..s3, raw binary32; no normalization beyond source codec.
    motion_binding::Channel channel;
};
using Result=std::variant<Sample,Error>;
// Original 0xaea0e4 dispatch, cursor search and admitted real decoders.
// Frames are raw binary32 source frames, not seconds. track/frame/key/weight
// have exactly the single representation owned by motion_binding::Channel.
// Null track returns [0,0,0,1]; codecs1/2 copy reference_bits unchanged. Both
// leave the cursor untouched and do not inspect keys, extremes or frame bits.
// Compressed paths require disjoint original Channel/key storage and bounded
// resource spans. Backward search is not silently clamped or repaired.
// Errors publish no partially advanced channel. No gameplay clock or fallback.
[[nodiscard]] Result sample(const motion_binding::Channel&,const TrackView* resolved_track,
                            std::uint32_t query_frame_bits,Limits={});
} // namespace mh4u::motion_sampler
