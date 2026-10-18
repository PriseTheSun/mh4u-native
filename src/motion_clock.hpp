#pragma once
#include "motion_pending.hpp"
#include "motion_transform_math.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_clock {
struct Unknown {};
struct Null {};
using SnapshotTable=std::variant<Unknown,Null,std::span<const motion_pending::Snapshot>>;
using OwnedSnapshotTable=std::variant<Unknown,Null,std::vector<motion_pending::Snapshot>>;
struct Snapshots {
    std::uint32_t joint_count{}; // Original model+220, independent of pointer presence.
    SnapshotTable table;
};
struct EventGroup {
    motion_request::Binding identity;
    std::span<const motion_events::Record> records; // Borrowed from the canonical resource owner.
    // Copied from the same Resource::event_groups result, whose prefix is a
    // value. Omission is unknown; the clock only consumes records, while a
    // later prefix consumer requires this fact from the same bound resource.
    std::optional<std::array<std::uint16_t,32>> prefix{};
};
struct ResolutionError {std::uint32_t code{};std::string message;};
using EventResolution=std::variant<EventGroup,ResolutionError>;
struct EventResolver {
    const void* context{};
    EventResolution (*resolve)(const void*,const motion_request::Binding&){};
};
struct Parameters {
    std::uint32_t delta_time_bits{}; // Original model+14, no FPS/unit inferred.
    std::optional<motion_initialization::Bits4> inverse_fallback_10e6570;
};
struct Limits {
    std::size_t layers=8,snapshots=256,bindings_per_layer=256;
    std::size_t loop_iterations=4096,event_records=250000,event_work=1000000;
};
enum class ErrorCode {resource_limit,floating_environment,nonfinite_operand,numeric_overflow,
    frame_domain,nonprogressing_loop,missing_template,unknown_snapshots,null_snapshots,
    snapshot_extent,unresolved_events,event_identity,event_accumulator,arithmetic};
struct Error {
    ErrorCode code;std::string_view message;
    std::size_t layer{},group{};
    std::optional<motion_request::Binding> binding;
    std::optional<ResolutionError> resolution;
    std::optional<motion_transform_math::Error> arithmetic;
};
struct Output {
    std::vector<motion_pending::Layer> layers;
    OwnedSnapshotTable snapshots;
    std::size_t loop_iterations{},event_queries{},event_records_checked{},cleared_snapshots{};
};
using Result=std::variant<Output,Error>;
// Original957f80 through return, including cd5db4, event masks and transition
// owner clearing. Complete canonical layers are copied transactionally; channels,
// prepared frames, and pending scratch fields are preserved when not written.
// Snapshot presence/extents are consumed only when an expired transition with
// nonzero joint_count traverses them. Null event bindings produce zero; present
// bindings require exact source identity. No parsing, scheduler, pose sampling,
// gameplay callbacks or original FPS policy. Finite consumed binary32 arithmetic;
// raw copies preserve all bits. Loop/event work bounds reject without publication.
[[nodiscard]] Result advance(std::span<const motion_pending::Layer>,const Snapshots&,
    const Parameters&,EventResolver,Limits={});
} // namespace mh4u::motion_clock
