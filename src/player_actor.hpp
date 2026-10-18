#pragma once
#include "native_actor.hpp"
#include "player_animation.hpp"
#include "player_policy_interpreter.hpp"
#include "player_position_bridge.hpp"
#include "player_update_begin.hpp"
#include "player_update_accumulator.hpp"
#include "player_input_filter.hpp"
#include "player_update_prefix.hpp"
#include "player_update_admission.hpp"
#include "player_update_continuation.hpp"
#include "item_pop_update.hpp"
#include <array>
#include <optional>
#include <string>
#include <variant>

namespace mh4u::native_actor {
class Player;
// Exclusive per-call identity binding for manager+e18; no native Player pointer
// is retained. Unknown/null source links remain distinct from a present Player.
using PlayerManagerLink=std::variant<player_manager_update::Unknown,player_manager_update::Absent,Player*>;
struct PlayerManagerContext {player_manager_update::ManagerState& manager;PlayerManagerLink player;};
// Stable source selection, resolved once against Actor's immutable LMT owner.
// A digest is not a display label or truncated resource token.
struct MotionSource {std::string sha256;};
using PlayerBank=std::variant<motion_request::Unknown,motion_request::Absent,MotionSource>;
using PlayerBanks=std::array<PlayerBank,16>;
struct PlayerInitial {
    std::array<motion_pending::Layer,8> layers{};
    player_controller_step::State controller{};
    motion_application::Model model{};
    std::vector<motion_pending::Snapshot> snapshots;
    std::uint32_t calculation_flags{},accumulated_flags{};
    player_fields::Fields shared_fields;
    player_state1::Fields state1_fields;
    player_position_bridge::Fields position_fields;
};
// Borrowed per-call facts. No parent views, area views or caller buffers persist.
struct PlayerStep {
    std::uint32_t delta_bits{};
    motion_initialization::Templates templates;
    std::optional<joint_pose::Bits3> restoration_scale;
    joint_output::ParentReference parent=joint_pose::Unknown{};
    std::optional<motion_transform_math::Basis> reference_basis;
    std::optional<joint_pose::Bits4> reference_translation;
};
// Frame resources for the composed update. Its delta and initialization
// templates are owned exclusively by PlayerUpdateDispatchContext.
struct PlayerModelContext {
    std::optional<joint_pose::Bits3> restoration_scale;
    joint_output::ParentReference parent=joint_pose::Unknown{};
    std::optional<motion_transform_math::Basis> reference_basis;
    std::optional<joint_pose::Bits4> reference_translation;
    std::optional<player_position_bridge::OrientationCallback> orientation;
    std::optional<player_position_bridge::OutputCallbacks> publication;
};
// Optional patches: omission preserves the live field, while an explicit zero
// is a known zero. These source channels/words imply no physical device mapping.
struct PlayerPolicyInput {
    std::optional<player_input::Channels> channels;
    std::optional<std::uint32_t> field3d4;
    std::array<std::optional<std::uint32_t>,19> flags3d8{};
    std::array<std::optional<std::uint16_t>,4> masks{}; // 3a8,3ac,3b8,3bc
};
struct PlayerPolicyContext {
    std::optional<std::uint32_t> delta_time_bits;
    player_reference::Reference reference=player_reference::Unknown{};
    std::optional<std::uint8_t> context_ad24;
    std::optional<std::uint32_t> global_c8c;
    std::optional<std::array<std::uint16_t,2>> direction_bounds;
};
enum class PlayerErrorCode {initial_state,bank,driver,render,capacity,policy,position,update_begin,input_filter,update_accumulator,update_prefix,item_pop_update,update_admission,update_continuation,grid_publication,manager_identity};
enum class GridPublicationCode {missing_scene,query_profile,invalid_slot,duplicate_slot,source_binding,resource_limit};
struct GridPublicationError {GridPublicationCode code;std::optional<std::uint32_t> slot;};
struct PlayerError {
    PlayerErrorCode code;std::string message;
    std::optional<player_animation::Error> driver;
    std::optional<Error> render;
    std::optional<player_policy_interpreter::Error> policy{};
    std::optional<player_position_bridge::Error> position{};
    std::optional<player_update_begin::Error> update_begin{};
    std::optional<player_input_filter::Error> input_filter{};
    std::optional<player_update_accumulator::Error> update_accumulator{};
    std::optional<player_update_prefix::Error> update_prefix{};
    std::optional<item_pop_update::Error> item_pop_update{};
    std::optional<player_update_admission::Error> update_admission{};
    std::optional<player_update_continuation::Error> update_continuation{};
    std::optional<GridPublicationError> grid_publication{};
};
template<class T>using PlayerResult=std::variant<T,PlayerError>;
struct PlayerOutput {
    Frame frame;
    std::optional<player_animation::CommandEffects> command;
    std::optional<player_animation::FrameEffects> animation;
};
struct PlayerPolicyOutput {
    Frame frame;
    bool returned{};
    std::array<std::uint8_t,48> visited{};
    std::size_t visited_count{};
    std::optional<player_selection::TransitionEffects> transition;
};
struct PlayerState1Output {Frame frame;player_state1::Effects effects;};
struct PlayerPositionOutput {Frame frame;player_position_bridge::Effects effects;};
struct PlayerInputFilterOutput {Frame frame;player_input_filter::Effects effects;};
struct PlayerPreparationOutput {
    Frame frame;
    std::optional<player_input_filter::Effects> filter;
    player_world_lookup::Output lookup;
};
struct PlayerItemPopOutput {
    Frame frame;
    std::size_t visited{};
    bool clock_gated{};
    item_pop_update::Effects effects;
};
struct PlayerAdmissionOutput {Frame frame;player_update_admission::Effects effects;};
struct PlayerDispatchOutput {
    Frame frame;
    player_update_admission::Effects admission;
    player_update_statistics::Effects statistics;
    player_update_continuation::EarlyReturns returns;
};
struct PlayerUpdateDispatchContext {
    const player_update_admission::Parameters& admission;
    const player_update_statistics::Context& statistics;
    const player_state1::Tables& tables;
    const motion_initialization::Templates& templates;
    std::span<const player_selection::AreaMappingRecord> area_mapping{};
    std::span<const std::optional<std::uint8_t>> area_categories{};
    spatial_query::Scene* spatial_scene=nullptr; // Unknown; borrowed exclusively for this call.
    std::optional<std::uint32_t> area_enable_mask{};
    spatial_query::Limits query_limits{};
};
struct PlayerUpdateDispatchOutput {Frame frame;player_animation::UpdateDispatchEffects effects;};
struct PlayerUpdateAdjustmentOutput {Frame frame;player_animation::UpdateDispatchEffects dispatch;player_update_post_dispatch::Effects effects;};
struct PlayerUpdateStatusOutput {
    Frame frame;
    player_animation::UpdateDispatchEffects dispatch;
    player_update_post_dispatch::Effects adjustment;
    player_status_update::Effects effects;
};
struct PlayerUpdateNotificationsOutput {
    Frame frame;
    player_animation::UpdateDispatchEffects dispatch;
    player_update_post_dispatch::Effects adjustment;
    player_status_update::Effects status;
    player_status_notifications::Effects effects;
};
struct PlayerUpdateDirectionOutput {
    Frame frame;
    player_animation::UpdateDispatchEffects dispatch;
    player_update_post_dispatch::Effects adjustment;
    player_status_update::Effects status;
    player_status_notifications::Effects notifications;
    player_update_direction::MaterialEffects material;
    player_direction_refresh::Effects effects;
};
struct PlayerUpdatePositionOutput {
    Frame frame;
    player_animation::UpdateDispatchEffects dispatch;
    player_update_post_dispatch::Effects adjustment;
    player_status_update::Effects status;
    player_status_notifications::Effects notifications;
    player_update_direction::MaterialEffects material;
    player_direction_refresh::Effects direction;
    player_position_timer::Effects effects;
};
struct PlayerUpdateManagerOutput {
    PlayerUpdatePositionOutput positioned;
    player_manager_update::Effects effects;
    std::optional<Frame> foreign_frame;
};
struct PlayerUpdateContactOutput {
    PlayerUpdateManagerOutput managed;
    player_contact_update::Effects effects;
};
struct PlayerUpdateModelOutput {
    PlayerUpdateContactOutput contacted;
    player_position_bridge::Effects effects;
};
struct PlayerUpdateEventsOutput {
    PlayerUpdateModelOutput modeled;
    player_update_events::Effects effects;
};
struct PlayerState1Context {
    player_reference::Reference reference=player_reference::Unknown{};
    player_selection::AreaContext area{};
    std::optional<std::uint32_t> delta_time_bits{},context_ad50{};
    player_object_query::Manager manager=player_object_query::Unknown{};
    player_spatial_acquisition::Context spatial{};
};
class Player {
public:
    // Explicit rest-seeded profile: MOD-owned poses/hierarchy, caller-owned
    // eight layer states, controller, model TRS, snapshots, flags and bank map.
    // This is not uPlayer construction or a default gameplay/bootstrap policy.
    static PlayerResult<Player> from_rest(const Actor&,const PlayerInitial&,
        const player_transition::Constants&,const PlayerBanks&,player_animation::Limits={});
    // Reuse immutable ownership/banks/constants; the supplied seed is still explicit.
    static PlayerResult<Player> from_rest(const Player&,const PlayerInitial&);
    std::shared_ptr<const Scene> presentation()const noexcept;
    std::span<const std::filesystem::path> source_files()const noexcept;
    const player_animation::State& state()const noexcept{return state_;}
    const player_fields::Fields& shared_fields()const noexcept{return shared_fields_;}
    const player_state1::Fields& state1_fields()const noexcept{return state1_fields_;}
    const player_position_bridge::Fields& position_fields()const noexcept{return position_fields_;}
    const motion_request::Model& banks()const noexcept{return banks_;}
    std::uint64_t completed_steps()const noexcept{return completed_steps_;}
    PlayerResult<Frame> capture()const;
    // Explicit b3b3a0 only, before a caller-selected action sequence. No clock
    // tick or b1407c/b48230 entry-gate inference. Commit follows rendering;
    // dispatch_state1 and tick do not reset this cache automatically.
    PlayerResult<Frame> begin_update();
    // Explicit source helpers preceding b3b3a0 in the local update branch.
    // Each call commits after rendering, advances no clock and applies no
    // policy. The caller selects sequencing; b1407c remains incomplete.
    PlayerResult<PlayerInputFilterOutput> filter_input(const player_input_filter::Context&);
    PlayerResult<Frame> update_accumulator(std::optional<std::uint32_t> delta_bits);
    // Original b1407c prefix through b14100, as one native transaction.
    // Explicit snapshots; neither the remaining caller nor the scheduler runs.
    // All four stages commit together only after the shared renderer succeeds.
    PlayerResult<PlayerPreparationOutput> prepare_update(const player_update_prefix::Context&);
    // b1407c entry through the checkpoint beforeb142d0. The supplied objects
    // explicitly bind this call's object singleton. Player and Collection
    // publish together after rendering; constants come from this Player owner.
    // No full-frame/scheduler or animation-clock completion is implied.
    PlayerResult<PlayerAdmissionOutput> prepare_to_admission(player_object_query::Collection&,
        const player_update_admission::Parameters&);
    // Extend that same transaction to the checkpoint BEFORE b143f8 dispatch.
    // Admission, resource queries and source early-return gates publish only
    // after rendering. This does not execute b66a20 or complete the scheduler.
    PlayerResult<PlayerDispatchOutput> prepare_to_dispatch(player_object_query::Collection&,
        const player_update_admission::Parameters&,const player_update_statistics::Context&);
    // Original update through BEFORE b1441c, including at most two supported
    // dispatches. Player, objects and any grid stamps publish together after
    // render; Scene is exclusively borrowed and only returned grid slots change.
    // The current source profile admits at most one actual height query, not a
    // game-wide capacity limit. Cached-height reuse is not another query.
    // No clock tick, scheduler completion or automatic invocation is implied.
    PlayerResult<PlayerUpdateDispatchOutput> prepare_and_dispatch(player_object_query::Collection&,
        const PlayerUpdateDispatchContext&);
    // Same transaction extended through BEFORE b145dc. The concrete virtual
    // binding is explicit; omission remains Unknown. Original speed/policy
    // fields are consumed on the dispatch candidate before one render/commit.
    PlayerResult<PlayerUpdateAdjustmentOutput> prepare_and_adjust(player_object_query::Collection&,
        const PlayerUpdateDispatchContext&,std::optional<player_update_post_dispatch::Virtual10c>);
    // Extend the same candidate through b22164's return, BEFORE b145e4.
    // Status sees the candidate Collection and sole entry delta. Unsupported
    // event emission and other late errors publish none of the three owners.
    PlayerResult<PlayerUpdateStatusOutput> prepare_and_update_status(player_object_query::Collection&,
        const PlayerUpdateDispatchContext&,std::optional<player_update_post_dispatch::Virtual10c>);
    // Through b41804 return, BEFORE b145f0. The single world-owned queue is
    // borrowed exclusively for this call and commits with Player/Collection/
    // Scene after rendering. Earlier checkpoints do not consume this queue.
    PlayerResult<PlayerUpdateNotificationsOutput> prepare_and_update_notifications(player_object_query::Collection&,
        notification_queue::State&,const PlayerUpdateDispatchContext&,
        std::optional<player_update_post_dispatch::Virtual10c>);
    // Through direction refresh, BEFORE b14610. Concrete virtual identities
    // are explicit and errors preserve every owner from the preceding stages.
    PlayerResult<PlayerUpdateDirectionOutput> prepare_and_update_direction(player_object_query::Collection&,
        notification_queue::State&,const PlayerUpdateDispatchContext&,
        const player_direction_refresh::Bindings&,std::optional<player_update_post_dispatch::Virtual10c>);
    // Through position/timer, BEFORE b146c4. Rendering and all four mutable
    // owners share the same publication boundary as the preceding checkpoints.
    PlayerResult<PlayerUpdatePositionOutput> prepare_and_update_position(player_object_query::Collection&,
        notification_queue::State&,const PlayerUpdateDispatchContext&,
        const player_direction_refresh::Bindings&,std::optional<player_update_post_dispatch::Virtual10c>);
    // Through b11f3c/b24c44, BEFORE b146d8. A reached foreign Player patch
    // prepares that Player's render too; every owner publishes only after both
    // renders succeed. A self binding uses the same preceding candidate.
    PlayerResult<PlayerUpdateManagerOutput> prepare_and_update_manager(player_object_query::Collection&,
        notification_queue::State&,const PlayerUpdateDispatchContext&,
        const player_direction_refresh::Bindings&,std::optional<player_update_post_dispatch::Virtual10c>,
        const PlayerManagerContext&);
    // Through b3150c/b3c244, BEFORE b14700. Position and sequential grid
    // candidates publish with the caller, foreign Player, manager and queue.
    // The null-reference template is consumed only on its source branch.
    PlayerResult<PlayerUpdateContactOutput> prepare_and_update_contact(player_object_query::Collection&,
        notification_queue::State&,const PlayerUpdateDispatchContext&,
        const player_direction_refresh::Bindings&,std::optional<player_update_post_dispatch::Virtual10c>,
        const PlayerManagerContext&,std::optional<spatial_query::Bits3> null_reference_template = {});
    // Through b07c08/b06f8c/b07bb8, BEFORE b14724. The contact-adjusted
    // candidate advances the model once with admission's delta and the same
    // templates. All reached owners publish after caller and foreign renders.
    PlayerResult<PlayerUpdateModelOutput> prepare_and_update_model(player_object_query::Collection&,
        notification_queue::State&,const PlayerUpdateDispatchContext&,
        const player_direction_refresh::Bindings&,std::optional<player_update_post_dispatch::Virtual10c>,
        const PlayerManagerContext&,const PlayerModelContext&,
        std::optional<spatial_query::Bits3> null_reference_template = {});
    // Through the attribute queries and current event-prefix scan, BEFORE
    // b148f0. Uses the same statistics registry and model resource resolver;
    // caller and foreign renders still precede publication of every owner.
    PlayerResult<PlayerUpdateEventsOutput> prepare_and_update_events(player_object_query::Collection&,
        notification_queue::State&,const PlayerUpdateDispatchContext&,
        const player_direction_refresh::Bindings&,std::optional<player_update_post_dispatch::Virtual10c>,
        const PlayerManagerContext&,const PlayerModelContext&,
        std::optional<spatial_query::Bits3> null_reference_template = {});
    // Explicit lifecycle update using this Player's sole canonical global/RNG.
    // Manager remains caller-owned. Both owners commit only after rendering,
    // with nonthrowing publication; failure preserves both. No implicit tick,
    // preparation, seed reset, singleton binding or resource default.
    PlayerResult<PlayerItemPopOutput> update_item_pop(player_world_lookup::Manager&,
        const item_pop_update::Context&);
    // b0d0e4 only. No animation request, clock tick or automatic table reinstall.
    // Controller, layer1 weight and extra fields commit only after rendering.
    PlayerResult<PlayerPolicyOutput> apply_policy(const PlayerPolicyInput&,const PlayerPolicyContext&);
    // Explicit dispatch; tables/templates are borrowed for this call. No clock
    // tick or implicit policy evaluation. Commit follows successful rendering.
    PlayerResult<PlayerState1Output> dispatch_state1(const player_state1::Tables&,
        const motion_initialization::Templates&,const PlayerState1Context& = {});
    PlayerResult<PlayerOutput> apply(const player_animation::Command&,
        const motion_initialization::Templates&,const player_selection::AreaContext& = {});
    PlayerResult<PlayerOutput> step(const PlayerStep&);
    // Original placement wrapper around the same frame/output owners. Callback
    // identities and additional fields must be explicit; no diagnostic defaults.
    // PBW/controller/model/pose changes commit together only after skinning.
    PlayerResult<PlayerPositionOutput> step_position(const PlayerStep&,
        std::optional<player_position_bridge::OrientationCallback> = {},
        std::optional<player_position_bridge::OutputCallbacks> = {});
    PlayerResult<PlayerOutput> tick(const player_animation::Command&,const PlayerStep&,
        const player_selection::AreaContext& = {});
private:
    struct Resources;
    // Public checkpoints share the same resource binding, grid planning,
    // rendering and publication implementation. No generic callback is exposed.
    template<class Output>PlayerResult<Output> prepare_dispatch(player_object_query::Collection&,
        const PlayerUpdateDispatchContext&,std::optional<player_update_post_dispatch::Virtual10c>,
        notification_queue::State* = nullptr,const player_direction_refresh::Bindings* = nullptr,
        const PlayerManagerContext* = nullptr,std::optional<spatial_query::Bits3> = {},
        const PlayerModelContext* = nullptr);
    static PlayerResult<player_animation::State> rest_state(const Actor::Data&,const PlayerInitial&,player_animation::Limits);
    Player(std::shared_ptr<const Actor::Data>,player_animation::State,
        player_transition::Constants,motion_request::Model,player_animation::Limits,
        player_fields::Fields,player_state1::Fields,player_position_bridge::Fields);
    PlayerResult<PlayerOutput> execute(const player_animation::Command*,
        const motion_initialization::Templates&,const PlayerStep*,const player_selection::AreaContext&);
    PlayerResult<Frame> render(const player_animation::State&,std::uint64_t,std::size_t,std::uint64_t)const;
    std::shared_ptr<const Actor::Data> data_;
    player_animation::State state_; // Sole persistent layer/pose/snapshot/output owner.
    player_transition::Constants constants_;
    player_fields::Fields shared_fields_; // Sole owner of shared extras, including raw PBW position/cost.
    player_state1::Fields state1_fields_; // Sole live owner of pw+50; no inferred countdown.
    player_position_bridge::Fields position_fields_; // Extra placement facts, no duplicated PBW/model.
    motion_request::Model banks_; // Views borrow immutable Data resources.
    player_animation::Limits limits_;
    std::uint64_t completed_steps_{},revision_=1;
    std::size_t samples_{};
};
}
