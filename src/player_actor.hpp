#pragma once
#include "native_actor.hpp"
#include "player_animation.hpp"
#include "player_policy_interpreter.hpp"
#include "player_position_bridge.hpp"
#include "player_update_begin.hpp"
#include "player_update_accumulator.hpp"
#include "player_input_filter.hpp"
#include <array>
#include <optional>
#include <string>
#include <variant>

namespace mh4u::native_actor {
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
enum class PlayerErrorCode {initial_state,bank,driver,render,capacity,policy,position,update_begin,input_filter,update_accumulator};
struct PlayerError {
    PlayerErrorCode code;std::string message;
    std::optional<player_animation::Error> driver;
    std::optional<Error> render;
    std::optional<player_policy_interpreter::Error> policy{};
    std::optional<player_position_bridge::Error> position{};
    std::optional<player_update_begin::Error> update_begin{};
    std::optional<player_input_filter::Error> input_filter{};
    std::optional<player_update_accumulator::Error> update_accumulator{};
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
