#pragma once
#include "player_controller_step.hpp"
#include "player_input.hpp"
#include "player_fields.hpp"
#include "player_state1_dynamics.hpp"
#include "player_object_query.hpp"
#include "player_spatial_acquisition.hpp"
#include <optional>

namespace mh4u::player_state1 {
namespace pcs=player_controller_step;
namespace mc=motion_command;
namespace tbl=player_policy_table;
namespace pt=player_transition;
namespace ps=player_selection;
namespace pr=player_reference;
// Per-call projection of externally owned facts. The stored reference fields
// remain in the policy/shared fields owner; no second persistent copy is kept.
// Constants and reference facts are demanded only on paths that consume them.
struct ContinuationContext {
    const pt::Constants* constants=nullptr;
    const player_fields::Fields* shared_fields=nullptr;
    pr::Reference reference=pr::Unknown{};
    ps::AreaContext area{};
    std::optional<std::uint32_t> delta_time_bits{},context_ad50{};
    player_object_query::Manager manager=player_object_query::Unknown{};
    player_spatial_acquisition::Context spatial{};
};
// Additional storage only. The handler's timer is not the policy owner's
// timer140/144 and has no inferred clock or decrement in this operation.
struct Fields {
    std::optional<std::uint32_t> field50_bits;
    bool operator==(const Fields&)const=default;
};
// Explicit immutable source bindings; these are source identities, not runtime
// addresses. Unknown/Absent remain distinct and are assigned without a read,
// exactly as the installer does. The caller retains file/digest provenance.
struct Tables {
    tbl::TableReference f2efb8=tbl::Unknown{};
    tbl::TableReference f2f2d0=tbl::Unknown{};
};
enum class TableSlot {f2efb8,f2f2d0};
struct Installation {TableSlot slot;bool before_command;};
struct Effects {
    std::optional<mc::Command> command;
    std::array<mc::ObservedRequest,2> requests{};
    std::uint8_t request_count{};
    std::optional<Installation> installation;
    std::size_t queries{},samples{};
    std::optional<ps::AnimationRequest> selection_request;
    std::optional<ps::TransitionEffects> transition;
    std::uint8_t magnitude_queries{}; // Repeated source virtual+98 calls, not cached.
    std::uint8_t cost_calls{},cost_updates{},position_updates{},manager_queries{};
    std::optional<player_object_query::Output> object_query;
    std::optional<player_spatial_acquisition::Effects> spatial;
};
struct Output {
    pcs::State state;
    Fields fields;
    std::vector<motion_pending::Layer> layers;
    Effects effects;
    player_fields::Fields shared_fields;
};
enum class ErrorCode {unsupported_state,unsupported_action,
    layer_extent,capacity,motion,unknown_field,nonfinite_operand,input,reference,
    transition,controller,dynamics,object_query,spatial_acquisition};
using Cause=std::variant<std::monostate,mc::Error,player_input::Error,pr::Error,
    ps::Error,pcs::Error,player_state1_dynamics::Error,player_object_query::Error,
    player_spatial_acquisition::Error>;
struct Error {
    ErrorCode code;
    std::string_view message;
    Cause cause;
};
using Result=std::variant<Output,Error>;
// b66a20 -> b6a424 selected state1 handlers. Entry substate0 composes the
// real motion-command owner and the post-request tail, including setter refusal
// and if_changed skipping the setter. Continuations1/4/9 compose the original
// gates, heading, direct transition and selection wrapper. ID2 also composes
// cost, raw position, cached/uncached spatial acquisition and manager contexts.
// Returned spatial grid updates remain part of the combined transaction.
// Original no-op substates preserve all but byte376.
// No input mapping, pose tick, scheduler or fabricated helper return.
// All layer copies are bounded before allocation; errors publish no partial
// controller/timer/layer state. Resolver effects are owned by the caller.
[[nodiscard]] Result apply(const pcs::State&,const Fields&,
    std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const Tables&,const motion_request::Model&,const mc::Globals&,
    const mc::Sampling& = {},mc::Limits = {},const ContinuationContext& = {});
}
