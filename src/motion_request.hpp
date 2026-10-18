#pragma once
#include "motion_events.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace mh4u::motion_request {
// Numeric counterpart of model+0x240+layer*0x130. Bindings are logical source
// identities, never ARM addresses; failure preserves existing bindings.
struct Binding {
    std::uint64_t resource_identity{};
    std::uint32_t bank{},slot{},group{};
    bool operator==(const Binding&) const=default;
};
struct Layer {
    std::uint16_t selector{0xffff},flags{},field08{};
    std::uint32_t attributes{};
    float requested_frame{},offset{},interpolation{},remaining_interpolation{},weight{};
    float frame{},previous_frame{},end_frame{},field58{},speed{},field60{};
    std::array<std::optional<Binding>,4> bindings{};
    std::array<std::uint32_t,4> masks{};
    // Canonical numeric fields shared by preparation, application and957f80.
    // Appended to preserve existing aggregate ordering; not an ARM memory layout.
    float prepared_frame{},prepared_next_frame{}; // +64/+68.
    float previous_step{}; // +18; +14 is already represented by offset above.
    std::array<std::uint32_t,2> interpolation_curve_bits{}; // +6c/+70, cd5db4 inputs.
    std::array<std::uint32_t,4> previous_masks{}; // +118..+124.
};
struct Action {
    std::uint32_t frame_count{};
    std::int32_t loop_frame_raw{};
    // All four source group descriptors must exist, including zero-count groups.
    // A null action+0x34 is not four empty groups in the original setter.
    bool event_descriptors_present{};
    std::array<std::span<const motion_events::Record>,4> groups{};
};
struct Unknown {};
struct Absent {};
struct Resource {
    // Nonzero stable identity supplied by the immutable resource owner. This
    // prevents old bindings from silently following a later bank replacement.
    std::uint64_t identity{};
    // nullopt means the resource's +0x64 table pointer is explicitly null.
    // Within a present table, a null slot is an explicitly empty action.
    std::optional<std::span<const std::optional<Action>>> slots;
};
using Bank=std::variant<Unknown,Absent,Resource>;
struct Model {
    bool skeleton_present{}; // Explicit original model+0xf0 null/non-null fact.
    std::array<Bank,16> banks{};
};
struct Request {
    std::uint32_t layer{},selector{},attributes{};
    float interpolation{},start_frame{},speed{};
};
struct Output {Layer layer;bool accepted{};};
enum class ErrorCode {unknown_bank,resource_identity,table_extent,nonfinite_input,floating_environment,
    frame_domain,event_descriptors,event_accumulator};
struct Error {ErrorCode code;std::string_view message;};
using Result=std::variant<Output,Error>;
// Full numeric 0x9575dc + real getter 0xcd7540; finite binary32 profile.
// No track binder, skeleton sampling, state selection or original pointers.
// Caller owns descriptors and must bind them to verified source resources.
Result apply(const Layer&,const Model&,const Request&) noexcept;
} // namespace mh4u::motion_request
