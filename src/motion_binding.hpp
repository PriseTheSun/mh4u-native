#pragma once
#include "motion_request.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_binding {
struct TrackReference {
    std::uint64_t resource_identity{};
    std::uint32_t bank{},slot{},ordinal{};
    bool operator==(const TrackReference&) const=default;
};
struct BufferReference {
    std::uint64_t resource_identity{},byte_offset{};
    bool operator==(const BufferReference&) const=default;
};
// Numeric fields use raw binary32 bits. The prefix only compares weights with
// zero and exact +1; it neither samples keys nor performs weight arithmetic.
struct Channel {
    std::optional<TrackReference> track;
    std::uint32_t frame_bits{};
    std::optional<BufferReference> key_data;
    std::uint32_t weight_bits{};
    bool operator==(const Channel&) const=default;
};
struct Joint {
    std::array<Channel,3> channels; // Rotation, translation, scale.
    std::uint32_t flags{};
    bool operator==(const Joint&) const=default;
};
struct Tracks {
    Channel translation,rotation;
    std::vector<Joint> joints;
    bool operator==(const Tracks&) const=default;
};
struct Track {
    std::uint32_t metadata{},weight_bits{};
    std::optional<std::uint64_t> key_data_offset; // Null is explicitly absent.
};
struct Action {
    std::uint64_t resource_identity{};
    std::uint32_t bank{},slot{};
    // Shared parsed facts: no second interpretation of frame/event headers.
    const motion_request::Action& header;
    std::uint32_t flags{};
    std::span<const Track> tracks;
};
struct Limits {std::size_t tracks=4096,joints=256;}; // Caller resource policy.
enum class ErrorCode {resource_limit,resource_identity,event_descriptors,
    floating_environment,bone_map_extent,joint_extent};
struct Error {ErrorCode code;std::string_view message;std::size_t track_ordinal{};};
struct Prepared {
    motion_request::Layer layer;
    Tracks tracks;
    bool action_present{};
};
using Result=std::variant<Prepared,Error>;
// Original 0x95aa90 entry through, but not including, 0x95acd8, and its real
// 0xae923c helper. A null action returns unchanged. This is only preparation:
// the remaining original routine must derive base/previous transforms before
// playback. No complete binder, pending-bit consumption or pose is implied.
// For each mapped joint the resource owner guarantees non-null, disjoint
// original joint and binding storage. Raw ARM pointer overflow/aliasing is not
// represented. Map value 255 skips a joint; other values index tracks.joints.
Result prepare_tracks(const motion_request::Layer&,const Tracks&,
    std::span<const std::uint8_t> bone_to_joint,const Action*,Limits={});
} // namespace mh4u::motion_binding
