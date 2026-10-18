#pragma once
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>

namespace mh4u::root_motion {
using Vector = std::array<float,3>;
using Quaternion = std::array<float,4>; // x,y,z,w; inputs are never silently normalized.

struct Model {
    Vector position{};
    Quaternion rotation{};
    Vector scale{};
    Vector null_scale{};
};
struct Layer {
    Vector previous_translation{};
    Quaternion previous_rotation{};
    std::uint32_t attr{};
    std::uint16_t internal_flags=4,previous_flags=1;
    // Read by the original routine but do not weight its first contribution.
    float blend=1,translation_weight=1,rotation_weight=1;
};
struct Sample { Vector translation{}; Quaternion rotation{}; };
// Required caller parameters. Initializer evidence is recorded separately;
// callers still supply these explicitly, with no hidden identity/zero default.
struct ResetTemplates { Vector position{}; Quaternion rotation{}; };
struct Output {
    Model model;
    Layer layer;
    Sample sampled; // After the source mirror operation, before accumulation.
    Vector delta_position{};
    Quaternion delta_rotation{};
};
enum class ErrorCode {
    unsupported_attributes,unsupported_internal_flags,nonfinite_input,
    invalid_quaternion,nonfinite_result,unsupported_advance_scope,
    unsupported_clock_domain,invalid_loop_interval,loop_limit,
    unsupported_floating_environment,unsupported_pre_update_scope,
    missing_sample,unexpected_sample,sample_frame_mismatch,duplicate_sample,
    missing_event_sample,unexpected_event_sample,event_request_mismatch,duplicate_event_sample
};
class Error : public std::runtime_error {
public:
    Error(ErrorCode code,const char* message):std::runtime_error(message),code_(code){}
    ErrorCode code() const noexcept { return code_; }
private:
    ErrorCode code_;
};
inline constexpr std::uint32_t supported_attributes=0x0004a1b5;

// Row-major basis using the same raw-quaternion polynomial as motion
// application. Validates finite, nonzero float32 quaternion norm; never
// normalizes. Presentation may reuse this convention, but this is not proof
// of the original model/world matrix construction.
using RotationBasis = std::array<std::array<double,3>,3>;
RotationBasis rotation_basis_raw(const Quaternion&);

// Contract: exactly one active layer, internal_flags=4/previous_flags=1.
// Derivation: shipped routine 0x95cab0; 52 isolated ARM fixtures. No clocks,
// gameplay policy, parent transforms, events or multi-layer blending. The
// selected internal flags exclude the next-sample prediction side effect.
// Throws tagged Error before returning any state on unsupported/nonfinite
// input or a quaternion whose float32 squared norm is zero/nonfinite.
// Arithmetic uses double intermediates and finite float32 outputs; it is not
// a bit-exact VFP/FPSCR implementation. See docs/root-motion-runtime.md.
Output apply_one_layer(const Model&,const Layer&,const Sample&,const ResetTemplates&);

struct AdvanceState {
    float frame{},previous_frame{},previous_step{};
    Vector previous_translation{};
    Quaternion previous_rotation{};
    std::uint16_t internal_flags{},previous_flags{};
    std::array<std::uint32_t,4> event_masks{},previous_event_masks{};
    std::uint32_t model_attributes{};
};
struct AdvanceParameters {
    float delta_multiplier{},speed{},frame_end{},loop_start{};
    Vector base_translation{};
    Quaternion base_rotation{};
    std::uint32_t attr{},attribute_override{};
};
// Required evidence boundary. original_joint_count is model+0x220, copied
// from the MOD joint count; it is unrelated to event callbacks. This numeric
// profile excludes the original joint-processing path. No scope defaults.
enum class PreUpdate { unverified,declared_unchanged };
struct AdvanceScope {
    PreUpdate pre_update;
    std::uint32_t original_joint_count;
    std::array<bool,4> event_sources_present;
};
struct AdvanceLimits { std::uint32_t max_loop_iterations; };
struct EventRange {
    std::int32_t begin{},end{};
    bool operator==(const EventRange&) const = default;
};
// Original signed kernel requests, not geometric intervals: reverse wrapping
// can produce begin>end; frame_end=0 produces -1. Never normalize downstream.
struct EventPlan {
    bool preserve_masks{};
    std::array<EventRange,2> ranges{};
    std::uint32_t count{};
};
struct AdvanceOutput {
    AdvanceState state;
    float progress{};
    std::uint32_t loop_iterations{};
    EventPlan event_plan;
};
inline constexpr std::uint32_t supported_advance_attributes=0x14;
inline constexpr std::uint16_t supported_advance_flags=0x205;

// Isolated 0x9573b8/0x957f80 profile: one active layer, unchanged pre-update,
// zero original joints and no event sources, no blend transition. End/loop are nonnegative
// integer-valued float32 inputs; all int32 conversions are checked. Clock
// operations round separately to float32 under FE_TONEAREST; quaternion
// products use double intermediates, round per loop and are not normalized.
// Nonpositive reverse intervals, nonprogressing wraps, budget exhaustion and
// uncovered flags/scope fail with Error; input objects remain unchanged.
// This does not sample/apply motion, assign gameplay timing units or enable
// actions. Scope and loop budget are mandatory, without default arguments.
AdvanceOutput advance_one_layer(const AdvanceState&,const AdvanceParameters&,
                                const AdvanceScope&,const AdvanceLimits&);

struct PreUpdateState {
    Model model;
    AdvanceState clock;
    Sample sampled;
    Vector delta_position;
    Quaternion delta_rotation;
    float sample_frame,next_frame;
};
struct PreUpdateWeights { float blend,translation_weight,rotation_weight; };
struct TimedSample { float frame;Sample value; };
struct PreUpdateScope {
    bool layer_active;
    std::uint32_t original_joint_count;
    std::array<bool,4> event_sources_present;
};
struct PreUpdateOutput { PreUpdateState state;bool applied; };
struct UpdatedAdvanceOutput {
    PreUpdateState state;
    bool applied;
    float progress;
    std::uint32_t loop_iterations;
};
// Real 0x95b790 subset. Gate model_attributes&0x20000000 clear is a verified
// return without mutation; active gate requires internal4/previous1 and one
// explicit sample at clamp(frame,0,end). An absent/unexpected/wrong-frame
// sample fails; this API never substitutes missing channels or default poses.
// Standalone pre-update reads the supplied attribute word as-is. Composition
// merges model attributes first, applies pre-update, then advances the clock.
// Scope requires one active layer, zero original joints and no event sources. Prediction,
// attr0x800 helper and transitions are excluded. Active-gate composition is
// NOT closed under repetition: its next flags normally require prediction.
PreUpdateOutput pre_update_one_layer(const PreUpdateState&,const AdvanceParameters&,
    const PreUpdateWeights&,const std::optional<TimedSample>&,const ResetTemplates&,const PreUpdateScope&);
UpdatedAdvanceOutput pre_update_and_advance_one_layer(const PreUpdateState&,const AdvanceParameters&,
    const PreUpdateWeights&,const std::optional<TimedSample>&,const ResetTemplates&,const PreUpdateScope&,const AdvanceLimits&);

// Repeated one-layer path, including source prediction side effects M+0x20/30.
// The prior prediction is required even when a tick preserves it. This is a
// sampled relative transform, not a velocity or a gameplay movement command.
struct TickState { PreUpdateState pre;Sample prediction_delta; };
enum class PredictionBranch { gate_clear,preserve,current_to_next,current_to_end,previous_to_end };
struct SampleRequests {
    std::array<float,3> frames;
    std::uint32_t count;
    float current_frame,next_frame;
    PredictionBranch prediction;
};
struct PredictionOutput { TickState state;bool applied,prediction_written; };
struct TickOutput {
    TickState state;
    bool applied,prediction_written;
    float progress;
    std::uint32_t loop_iterations;
};
// Event-table queries are independent of the excluded original joint path.
// Gameplay consumption remains outside this clock. Legacy calls exclude tables.
enum class EventSourcePolicy { excluded,masks_only };
struct EventRequest {
    std::uint32_t group{},range_ordinal{};
    EventRange range;
    bool operator==(const EventRequest&) const = default;
};
struct EventSample { EventRequest request;std::uint32_t mask; };
class PendingTick {
public:
    std::span<const EventRequest> requests() const { return {requests_.data(),count_}; }
    // Publishes a complete state only when every exact request has one answer.
    // Reordering answers is allowed; missing/extra/duplicate answers fail.
    TickOutput finish(std::span<const EventSample>) const;
private:
    PendingTick(TickOutput, const EventPlan&,const std::array<bool,4>&);
    TickOutput output_;
    std::array<EventRequest,8> requests_{};
    std::uint32_t count_{};
    friend PendingTick plan_tick_with_events(const TickState&,const AdvanceParameters&,const PreUpdateWeights&,
        std::span<const TimedSample>,const ResetTemplates&,const PreUpdateScope&,const AdvanceLimits&);
};
// Plans only the source's sample requests. Duplicate frame requests collapse
// once, preserving current/end/previous call order. Gate-clear needs none.
// The caller owns sample generation and must provide exactly these frames.
// This profile accepts attr mask0x14, flags mask0x205 and one active layer with
// zero original joints. Tables require explicit masks_only policy; the default
// excludes them. No channels, initialization or timing units inferred.
SampleRequests sample_requests_one_layer(const PreUpdateState&,const AdvanceParameters&,const PreUpdateScope&,
    EventSourcePolicy = EventSourcePolicy::excluded);
// Standalone pre-update reads the supplied model attribute word unchanged.
PredictionOutput pre_update_with_prediction_one_layer(const TickState&,const AdvanceParameters&,const PreUpdateWeights&,
    std::span<const TimedSample>,const ResetTemplates&,const PreUpdateScope&);
// Runs attribute merge -> actual pre-update/application/prediction -> clock.
// Sample frames must be finite/distinct and exactly match the plan; unused,
// missing or duplicate samples fail. Prior prediction remains unchanged in
// the preserve/gate-clear branches. No normalization of prediction results.
// No ARM runtime dependency; accuracy is bounded only by retained fixtures.
TickOutput tick_one_layer(const TickState&,const AdvanceParameters&,const PreUpdateWeights&,
    std::span<const TimedSample>,const ResetTemplates&,const PreUpdateScope&,const AdvanceLimits&);

// Same arithmetic profile as tick_one_layer, with explicit presence for four
// event tables. Zero original joints remain mandatory. Returned
// queries are raw signed32 kernel arguments; the table owner must not clamp or
// reorder them. Freeze preserves both mask arrays and requests no table reads.
PendingTick plan_tick_with_events(const TickState&,const AdvanceParameters&,const PreUpdateWeights&,
    std::span<const TimedSample>,const ResetTemplates&,const PreUpdateScope&,const AdvanceLimits&);

}
