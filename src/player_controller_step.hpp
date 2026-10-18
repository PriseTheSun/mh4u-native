#pragma once
#include "motion_command.hpp"
#include "player_selection.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <variant>

namespace mh4u::player_controller_step {
namespace ps=player_selection;
namespace pt=player_transition;
namespace mc=motion_command;
// Canonical storage excludes transition.player.field3b8: those same four
// source bytes belong to borrowed layers[1].animation.layer.weight.
struct PlayerFields {
    std::array<std::uint32_t,3> fieldsd10{};
    pt::PbwFields pbw;
    pt::PwFields pw;
    pt::GlobalFields global;
    ps::PbwFields selection_pbw;
    ps::PwFields selection_pw;
    bool operator==(const PlayerFields&) const=default;
};
struct State {PlayerFields player;mc::Fields motion;};
struct Output {
    State state;
    std::vector<motion_pending::Layer> layers;
    // Router command can exist even when if_changed emits no setter requests.
    std::optional<ps::AnimationRequest> selection_request;
    std::array<mc::ObservedRequest,2> animation_requests{};
    std::uint8_t animation_request_count{};
    std::optional<ps::TransitionEffects> transition;
    std::size_t queries{},samples{};
};
enum class Phase {storage,selection,animation_command,transition};
struct Error {Phase phase;std::variant<ps::Error,mc::Error> cause;};
using Result=std::variant<Output,Error>;
// Read-only projection for integrations that need the selection owner's view.
// field3b8 is derived from the authoritative layer1 weight bits on every call.
using SelectionView=std::variant<ps::State,Error>;
SelectionView selection_view(const State&,std::span<const motion_pending::Layer>) noexcept;
// Inverse projection into the same canonical owners. A short layer extent
// fails before any controller or layer store; the layer1 weight keeps raw bits.
[[nodiscard]] std::optional<Error> store_selection(State&,std::span<motion_pending::Layer>,
    const ps::State&) noexcept;
// Full numeric composition: prefix -> real command semantics -> continuation.
// Uses explicit immutable resources and global snapshots, with no callbacks,
// binder, clock, device mapping or inferred selector-to-resource assignment.
// Errors publish no partial combined state. Valid original setter failure
// still continues into the state transition, as the source does.
// The caller owns layer storage and active_count independently. Output layers
// replace that same storage after success; no second persistent animation state
// or stale copy of the shared weight is kept in this controller.
[[nodiscard]] Result apply_common(const State&,std::span<const motion_pending::Layer>,
    std::uint32_t active_count,const ps::CommonArguments&,
    const motion_request::Model&,const mc::Globals&,const pt::Constants&,
    const mc::Sampling& = {},mc::Limits = {},const ps::AreaContext& = {});
[[nodiscard]] Result apply_wrapper(const State&,std::span<const motion_pending::Layer>,
    std::uint32_t active_count,ps::Wrapper,const ps::WrapperArguments&,
    const motion_request::Model&,const mc::Globals&,const pt::Constants&,
    const mc::Sampling& = {},mc::Limits = {},const ps::AreaContext& = {});
} // namespace mh4u::player_controller_step
