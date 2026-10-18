#pragma once
#include "player_controller_step.hpp"
#include "player_state1.hpp"
#include "player_update_contact.hpp"
#include "player_animation_types.hpp"
#include "player_position_bridge.hpp"
#include "player_update_events.hpp"
#include <array>
#include <optional>
#include <variant>

namespace mh4u::player_animation {
struct State {
    model_animation_frame::State frame; // Exactly two active layers, canonical J/B/A.
    std::array<motion_pending::Layer,6> inactive; // Physical slots2..7, never active in this profile.
    player_controller_step::State controller; // Excludes shared layer1.weight.
    model_animation_output::Hierarchy hierarchy; // No second pose, model TRS, calculation flags or snapshots.
};
struct WrapperCommand {player_selection::Wrapper wrapper;player_selection::WrapperArguments arguments;};
using Command=std::variant<player_selection::CommonArguments,WrapperCommand>;
struct CommandContext {
    const motion_request::Model& model;
    const motion_command::Globals& globals;
    const player_transition::Constants& constants;
    motion_command::Sampling sampling;
    player_selection::AreaContext area{}; // Borrowed for this call only.
};
struct State1Context {
    const player_state1::Tables& tables;
    const motion_request::Model& model;
    const motion_command::Globals& globals;
    motion_command::Sampling sampling;
    player_state1::ContinuationContext continuation{};
};


struct CommandEffects {
    std::optional<player_selection::AnimationRequest> selection_request;
    std::array<motion_command::ObservedRequest,2> requests;
    std::uint8_t request_count{};
    std::optional<player_selection::TransitionEffects> transition;
    std::size_t queries{},samples{};
};

struct Output {State state;std::optional<CommandEffects> command;std::optional<FrameEffects> frame;};
struct State1Output {State state;player_state1::Fields fields;player_state1::Effects effects;player_fields::Fields shared_fields;};
struct UpdateDispatchEffects {
    player_update_admission::Effects admission;
    player_update_statistics::Effects statistics;
    player_update_continuation::EarlyReturns returns;
    std::array<player_update_dispatch::Dispatch,2> dispatches{};
    std::uint8_t dispatch_count{};
};
struct UpdateDispatchOutput {
    State state;
    player_state1::Fields action_fields;
    player_fields::Fields shared_fields;
    std::optional<player_object_query::Collection> collection;
    UpdateDispatchEffects effects;
};
struct UpdateAdjustmentOutput {UpdateDispatchOutput dispatched;player_update_post_dispatch::Effects effects;};
struct UpdateStatusOutput {UpdateAdjustmentOutput adjusted;player_status_update::Effects effects;};
struct UpdateNotificationsOutput {
    UpdateStatusOutput status;
    notification_queue::State queue;
    player_status_notifications::Effects effects;
};
struct UpdateDirectionOutput {
    UpdateNotificationsOutput notified;
    player_update_direction::MaterialEffects material;
    player_direction_refresh::Effects effects;
};
struct UpdatePositionOutput {UpdateDirectionOutput directed;player_position_timer::Effects effects;};
struct UpdateManagerOutput {
    UpdatePositionOutput positioned;
    std::optional<player_manager_update::ManagerState> manager;
    std::optional<player_manager_update::ForeignPatch> foreign;
    player_manager_update::Effects effects;
};
struct UpdateContactOutput {
    UpdateManagerOutput managed;
    player_position_bridge::Fields position;
    player_contact_update::Effects effects;
    std::vector<spatial_query::GridUpdate> grid_updates;
};
struct UpdateModelContext {
    const player_update_contact::Context& preceding;
    const FrameResources& frame;
    std::optional<player_position_bridge::OrientationCallback> orientation;
    std::optional<player_position_bridge::OutputCallbacks> publication;
};
struct UpdateModelOutput {UpdateContactOutput contacted;player_position_bridge::Effects effects;};
struct UpdateEventsContext {const UpdateModelContext& preceding;};
struct UpdateEventsOutput {UpdateModelOutput modeled;player_update_events::Effects effects;};
enum class DomainCode {active_layers,joint_extent,resource_limit};
struct DomainError {DomainCode code;std::string_view message;};
enum class Phase {domain,controller,animation,transition,hierarchy,state1,update_dispatch,update_adjustment,update_status,update_notifications,update_direction,update_position,update_manager,update_contact,update_model,update_events};
struct Error {
    Phase phase;
    std::variant<DomainError,player_controller_step::Error,model_animation_frame::Error,
                 motion_transition::Error,joint_output::Error,player_state1::Error,player_update_dispatch::Error,player_update_post_dispatch::Error,player_update_status::Error,player_update_notifications::Error,player_update_direction::Error,player_update_position::Error,player_update_manager::Error,player_update_contact::Error,player_position_bridge::Error,player_update_events::Error> cause;
};
using Result=std::variant<Output,Error>;
using UpdateDispatchResult=std::variant<UpdateDispatchOutput,Error>;
using UpdateAdjustmentResult=std::variant<UpdateAdjustmentOutput,Error>;
using UpdateStatusResult=std::variant<UpdateStatusOutput,Error>;
using UpdateNotificationsResult=std::variant<UpdateNotificationsOutput,Error>;
using UpdateDirectionResult=std::variant<UpdateDirectionOutput,Error>;
using UpdatePositionResult=std::variant<UpdatePositionOutput,Error>;
using UpdateManagerResult=std::variant<UpdateManagerOutput,Error>;
using UpdateContactResult=std::variant<UpdateContactOutput,Error>;
using UpdateModelResult=std::variant<UpdateModelOutput,Error>;
using UpdateEventsResult=std::variant<UpdateEventsOutput,Error>;
using State1Result=std::variant<State1Output,Error>;
// Explicit 2-active/8-storage profile, initialized by the caller. The driver
// does not infer bootstrap, a physical control mapping, a selector or a bank.
// Every operation is transactional; borrowed resources must outlive the call.
// Resolver side effects remain caller-owned. apply executes only controller;
// step executes9573b8 ->95b490 ->95b940; tick executes apply then step once.
// Inactive slots survive bit-exactly. Temporary projections are bounded and
// never retained as a second persistent pose, layer or snapshot owner.
[[nodiscard]] Result apply(const State&,const Command&,const CommandContext&,Limits={});
// Explicit state1 dispatch only: no policy evaluation, pending binding or tick.
// Additional fields have their caller's sole persistent owner, like controller.
[[nodiscard]] State1Result dispatch_state1(const State&,const player_state1::Fields&,
    const State1Context&,Limits={});
// Original update through BEFORE b1441c on the same bounded 2+6 owner.
// No animation step; external Collection/grid effects are candidates only.
[[nodiscard]] UpdateDispatchResult dispatch_update(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_dispatch::Context&,Limits={});
// Original composition through BEFORE b145dc, using the same candidate and
// canonical motion fields; no implicit clock or external publication.
[[nodiscard]] UpdateAdjustmentResult adjust_update(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_post_dispatch::Context&,Limits={});
// Same candidate through BEFORE b145e4 after b22164 returns. Earlier object
// mutations are visible to the status scan; event emission remains unsupported.
[[nodiscard]] UpdateStatusResult update_status(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_post_dispatch::Context&,Limits={});
// Extend the same candidate through b41804, BEFORE b145f0. The world queue
// stays explicit and borrowed; publication belongs to the persistent caller.
[[nodiscard]] UpdateNotificationsResult update_notifications(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_post_dispatch::Context&,const notification_queue::State&,Limits={});
// Through b6d2a4 return, BEFORE b14610, with concrete direction bindings.
[[nodiscard]] UpdateDirectionResult update_direction(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_direction::Context&,const notification_queue::State&,Limits={});
// Through the position/timer phase, BEFORE b146c4. Canonical shared PBW
// position is retained separately from the later model-position bridge.
[[nodiscard]] UpdatePositionResult update_position(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_direction::Context&,const notification_queue::State&,Limits={});

// Through b11f3c and b24c44, BEFORE b146d8. External manager/Player patches
// are unpublished; persistent owners decide the joint render/commit boundary.
[[nodiscard]] UpdateManagerResult update_manager(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_object_query::Manager&,
    const player_update_manager::Context&,const notification_queue::State&,Limits={});

// Through contact/integration and b3c244, BEFORE b14700. All position, grid,
// manager and foreign Player writes remain part of one unpublished candidate.
[[nodiscard]] UpdateContactResult update_contact(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_position_bridge::Fields&,
    const player_object_query::Manager&,const player_update_contact::Context&,
    const notification_queue::State&,Limits={});

// Extend contact exactly once through b07c08 -> b06f8c -> b07bb8, BEFORE
// b14724. Frame/controller/shared/placement/hierarchy have the same candidate
// owners; the six inactive slots and all preceding external patches survive.
// Delta comes only from preceding admission.prefix.delta_bits; initialization
// templates come only from dispatch.resources.sampling. The bridge retains
// its concrete-callback, finite-arithmetic and active-route limitations.
[[nodiscard]] UpdateModelResult update_model(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_position_bridge::Fields&,
    const player_object_query::Manager&,const UpdateModelContext&,
    const notification_queue::State&,Limits={});

// Extend the same model candidate through BEFORE b148f0. The registry and
// work budget come from preceding statistics; the prefix uses the frame's
// existing event resolver. No alternative resources or helper-result inputs.
[[nodiscard]] UpdateEventsResult update_events(const State&,const player_state1::Fields&,
    const player_fields::Fields&,const player_position_bridge::Fields&,
    const player_object_query::Manager&,const UpdateEventsContext&,
    const notification_queue::State&,Limits={});

[[nodiscard]] Result step(const State&,const FrameContext&,Limits={});
[[nodiscard]] Result tick(const State&,const Command&,const CommandContext&,const FrameContext&,Limits={});
} // namespace mh4u::player_animation
