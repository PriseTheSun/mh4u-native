#pragma once
#include "player_controller_step.hpp"
#include "player_state1.hpp"
#include "player_update_direction.hpp"
#include "model_animation_output.hpp"
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
struct FrameContext {
    std::span<const std::uint8_t> bone_to_joint;
    motion_pending::Parameters pending; // Sole step delta; no internal timer/rate.
    model_animation_frame::Resolvers resolvers;
    joint_output::Parameters output;
};
struct Limits {
    std::size_t joints=256,snapshots=256,bindings=2048,order=256;
    motion_command::Limits command;
    model_animation_frame::Limits frame;
    motion_transition::Limits transition;
    joint_output::Limits output;
};
struct CommandEffects {
    std::optional<player_selection::AnimationRequest> selection_request;
    std::array<motion_command::ObservedRequest,2> requests;
    std::uint8_t request_count{};
    std::optional<player_selection::TransitionEffects> transition;
    std::size_t queries{},samples{};
};
struct FrameEffects {
    bool pending_found{},restored_joints{},blend_layer_found{},attachment_cleared{};
    std::optional<joint_update::Leaf> joint_leaf;
    std::optional<joint_output::SelectedCallback> output_callback;
    std::size_t initializer_calls{},prepared_layers{},model_samples{},joint_samples{},
        loop_iterations{},event_queries{},blended_joints{},output_joints{};
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
enum class DomainCode {active_layers,joint_extent,resource_limit};
struct DomainError {DomainCode code;std::string_view message;};
enum class Phase {domain,controller,animation,transition,hierarchy,state1,update_dispatch,update_adjustment,update_status,update_notifications,update_direction};
struct Error {
    Phase phase;
    std::variant<DomainError,player_controller_step::Error,model_animation_frame::Error,
                 motion_transition::Error,joint_output::Error,player_state1::Error,player_update_dispatch::Error,player_update_post_dispatch::Error,player_update_status::Error,player_update_notifications::Error,player_update_direction::Error> cause;
};
using Result=std::variant<Output,Error>;
using UpdateDispatchResult=std::variant<UpdateDispatchOutput,Error>;
using UpdateAdjustmentResult=std::variant<UpdateAdjustmentOutput,Error>;
using UpdateStatusResult=std::variant<UpdateStatusOutput,Error>;
using UpdateNotificationsResult=std::variant<UpdateNotificationsOutput,Error>;
using UpdateDirectionResult=std::variant<UpdateDirectionOutput,Error>;
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

[[nodiscard]] Result step(const State&,const FrameContext&,Limits={});
[[nodiscard]] Result tick(const State&,const Command&,const CommandContext&,const FrameContext&,Limits={});
} // namespace mh4u::player_animation
