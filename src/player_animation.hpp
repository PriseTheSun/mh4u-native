#pragma once
#include "player_controller_step.hpp"
#include "player_state1.hpp"
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
enum class DomainCode {active_layers,joint_extent,resource_limit};
struct DomainError {DomainCode code;std::string_view message;};
enum class Phase {domain,controller,animation,transition,hierarchy,state1};
struct Error {
    Phase phase;
    std::variant<DomainError,player_controller_step::Error,model_animation_frame::Error,
                 motion_transition::Error,joint_output::Error,player_state1::Error> cause;
};
using Result=std::variant<Output,Error>;
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
[[nodiscard]] Result step(const State&,const FrameContext&,Limits={});
[[nodiscard]] Result tick(const State&,const Command&,const CommandContext&,const FrameContext&,Limits={});
} // namespace mh4u::player_animation
