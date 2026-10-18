#pragma once
#include "player_transition.hpp"
#include <cstdint>
#include <array>
#include <span>
#include <optional>
#include <string_view>
#include <utility>
#include <variant>

namespace mh4u::player_selection {
namespace pt=player_transition;
// Additional numeric fields only; shared fields have exactly one owner in pt.
// f32 storage is represented as its original bits, including unchanged NaNs.
struct PbwFields {
    std::uint8_t field370{},field376{},field6fa{};
    // Added source facts for pl_state0. Absence is unknown, never zero.
    std::optional<std::uint8_t> field6f9{};
    std::optional<std::uint32_t> field358{};
    std::optional<std::uint8_t> field9{};
    std::array<std::optional<std::uint16_t>,20> fields2fc{};
    bool operator==(const PbwFields&) const=default;
};
struct PwFields {
    std::uint32_t field170{}; std::uint8_t field176{};
    std::uint32_t field1b0{}; std::uint16_t field5b2{},field5b4{};
    std::uint8_t field594{},field9a8{}; std::uint32_t fielda10{};
    std::optional<std::uint8_t> field7fc{};
    std::optional<std::uint32_t> field800{};
    bool operator==(const PwFields&) const=default;
};
struct State {
    pt::State transition; PbwFields pbw; PwFields pw;
    bool operator==(const State&) const=default;
};
// Borrowed facts from the two external lookup tables. Indices are source IDs,
// not display categories. Missing records/bytes remain unknown when consumed.
struct AreaMappingRecord {std::optional<std::uint8_t> mapped,gate;};
struct AreaContext {
    std::optional<std::uint32_t> mode_c8c;
    std::span<const AreaMappingRecord> mapping; // +e1cc/+e1c7 +4*source ID
    std::span<const std::optional<std::uint8_t>> categories; // +15c +24*mapped ID
};
struct CommonArguments {
    std::uint32_t pl_state{};
    std::int32_t interpolation{},frame{};
    std::uint32_t flags{},request_mode{},extra_bits{};
};
struct WrapperArguments {
    std::uint32_t pl_state{};
    std::int32_t interpolation{},frame{};
    std::uint32_t extra_bits{};
};
enum class Wrapper {b1d114,b23b74};
struct AnimationRequest {
    // A request at b27c1c, not a completed animation setter or glTF action.
    std::uint32_t selector{},mode{},interpolation_bits{},frame_bits{};
    bool operator==(const AnimationRequest&) const=default;
};
struct TransitionEffects {
    pt::Request arguments;
    std::uint32_t effective_flags{};
    std::optional<pt::ObservedDispatchCall> returned_dispatch_call;
};
struct Completed {State state;std::optional<TransitionEffects> transition;};
enum class ErrorCode {unsupported_pl_state,invalid_wrapper,unsupported_float_environment,transition_rejected,
    unknown_selection_context,unsupported_selection_context};
struct Error {
    ErrorCode code;std::string_view message;
    std::optional<pt::ErrorCode> transition_code;
};
template<class T>using Result=std::variant<T,Error>;
std::string_view error_name(ErrorCode) noexcept;
// d048c4: scans all20 source halfwords in order, returning on first equality.
// An unknown halfword before that equality is an error, not a missing ID.
[[nodiscard]] Result<bool> contains_id(const PbwFields&,std::uint16_t);
// Complete69d568 for byte source IDs used by the original callers. This
// consumes mode+c8c before any conditional map byte; categories are never read.
// IDs>=6 in mode13 still require the mapped byte before returning9. No view
// or mapping state is retained, and unknown consumed facts stay typed errors.
[[nodiscard]] Result<std::uint32_t> remap_area(std::optional<std::uint8_t> source_id,const AreaContext&);
class PendingAnimation;
using BeginResult=std::variant<Completed,PendingAnimation,Error>;
class PendingAnimation {
public:
    const State& before_request() const noexcept{return state_;}
    const AnimationRequest& request() const noexcept{return request_;}
private:
    enum class Continuation {state1,state0_zero,state0_ten};
    PendingAnimation(State state,AnimationRequest request,std::uint32_t flags,Continuation continuation):
        state_(std::move(state)),request_(request),flags_(flags),continuation_(continuation){}
    State state_;AnimationRequest request_;std::uint32_t flags_{};Continuation continuation_;
    friend BeginResult begin_common(const State&,const CommonArguments&,const AreaContext&);
    friend Result<Completed> resume(const PendingAnimation&,const State&,const pt::Constants&);
};
// Exact full-width pl_state 0, 1 or 3. Three returns after prefix; other
// admitted branches stop before b27c1c. State0 requires only the context facts
// consumed along its branch; specialized type paths remain errors. AreaContext
// is consumed synchronously and is never retained by PendingAnimation.
// No callback is invoked and no continuation is run.
BeginResult begin_common(const State&,const CommonArguments&,const AreaContext& = {});
// These source wrappers fix flags=0 and request mode 0 or 2 respectively.
BeginResult begin_wrapper(const State&,Wrapper,const WrapperArguments&,const AreaContext& = {});
// Caller must supply the state AFTER the request has returned. There is no
// default to before_request(). The selector8 branch reads post_request.pw594.
// This pure continuation preserves effects from that supplied state, validates
// the transition owner's profile and publishes a complete result or an error.
Result<Completed> resume(const PendingAnimation&,const State& post_request,const pt::Constants&);
// Original b235f8: candidate.pbw376=1, then b1c050, then pbw6fa=0.
// No selection or animation request is made. Uses the supplied current state,
// preserves the transition owner's typed failure and publishes only on success.
[[nodiscard]] Result<Completed> direct_transition(const State&,const pt::Request&,const pt::Constants&);
} // namespace mh4u::player_selection
