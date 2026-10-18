#pragma once
#include "player_update_prefix.hpp"
#include "player_object_interaction.hpp"
#include "player_policy_interpreter.hpp"

namespace mh4u::player_update_admission {
enum class Virtual120 {uplayer_b3dd60};
struct Parameters {
    player_update_prefix::Context prefix;
    std::optional<Virtual120> virtual120;
    player_object_interaction::Resources resources=player_object_query::Unknown{};
    std::optional<std::uint32_t> global_c8c;
    player_reference::Reference reference=player_reference::Unknown{};
    std::optional<std::array<std::uint16_t,2>> direction_bounds;
    std::optional<std::uint32_t> global_c90{}; // Same external global as c8c; b07b90/b3c244.
};
struct Context {const Parameters& parameters;const player_transition::Constants& constants;};
struct PolicyEffects {
    bool returned{};
    std::array<std::uint8_t,48> visited{};
    std::size_t visited_count{};
};
struct Effects {
    std::optional<player_input_filter::Effects> filter;
    player_world_lookup::Output lookup;
    std::optional<player_object_query::ScanEffects> scan;
    std::optional<player_object_interaction::Output> interaction;
    bool virtual120_returned{},local_override{},cleared_target{};
    std::optional<bool> pre_policy_returned;
    std::optional<PolicyEffects> policy;
    std::optional<player_selection::TransitionEffects> transition;
};
struct Output {
    player_controller_step::State state;
    std::vector<motion_pending::Layer> layers;
    player_fields::Fields fields;
    // Present only when the mutating686fc0 executed, including ordinaryfalse.
    // Slots and root/D identities stay unchanged; only candidate fields change.
    std::optional<player_object_query::Collection> collection;
    Effects effects;
};
enum class Phase {prefix,scan,dispatch,caller,interaction,clear_target,pre_policy,policy,transition,projection,storage};
enum class ErrorCode {dependency,unknown_field,unsupported_virtual,unsupported_pre_policy,capacity};
using Cause=std::variant<std::monostate,player_update_prefix::Error,player_object_query::Error,
    player_object_interaction::Error,player_input_filter::Error,player_policy_interpreter::Error,
    player_selection::Error,player_controller_step::Error>;
struct Error {Phase phase;ErrorCode code;std::string_view field;Cause cause;};
using Result=std::variant<Output,Error>;
// Original b1407c entry through the checkpoint immediately beforeb142d0.
// Includes the supported returned b17f78 branches and real policy/direct
// transition owners. Reached unclosed branches are typed errors, not false.
// All fields/objects/layers form one candidate; no animation clock is advanced.
// The prefix delta and managerAD24 are reused by policy/pre-policy rather than
// accepted as independently conflicting facts. This is not a complete frame.
// Aggregate layer/binding copies use the existing command storage policy,
// checked before allocation. Limits are host budgets, not source format maxima.
[[nodiscard]] Result apply(const player_controller_step::State&,
    std::span<const motion_pending::Layer>,const player_fields::Fields&,
    const player_object_query::Manager&,const Context&,motion_command::Limits = {});
} // namespace mh4u::player_update_admission
