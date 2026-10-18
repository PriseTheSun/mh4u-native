#pragma once
#include "player_update_dispatch.hpp"
#include "motion_speed.hpp"

namespace mh4u::player_update_post_dispatch {
enum class Virtual10c {uplayer_b25b68};
enum class Phase {dispatch,virtual_call,rate,primary_speed,secondary_speed,policy,eligibility,storage};
enum class ErrorCode {dependency,unknown_field,unsupported_virtual,layer_extent,capacity};
using Cause=std::variant<std::monostate,player_update_dispatch::Error,binary32_arithmetic::Error,
    motion_speed::Error,player_policy_table::Error,player_selection::Error,motion_command::Error>;
struct Error {Phase phase;ErrorCode code;std::string_view field;Cause cause;std::size_t record{};};
template<class T>using Result=std::variant<T,Error>;
struct PolicyQuery {std::uint32_t kind{},action{};bool matched{};std::size_t records{};};
struct Effects {
    bool virtual_returned{},primary_rescaled{};
    std::uint32_t base_bits{},primary_bits{},secondary_bits{};
    std::array<PolicyQuery,3> policy_queries{};
    std::uint8_t query_count{};
    std::optional<std::uint32_t> eligibility_selector;
    std::optional<bool> eligible;
};
// Full b2a75c; source arguments remain full-width. No48-row interpreter cap
// is applied. Missing reached carrier fields fail; early hits stay lazy.
[[nodiscard]] Result<PolicyQuery> policy_has_action(const player_policy_table::State&,
    std::uint32_t kind,std::uint32_t action);
// Full b345b8, composing d048c4 and the closed b3444c mask predicate.
[[nodiscard]] Result<bool> eligible(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,std::uint32_t selector);
struct PostOutput {
    player_controller_step::State state;
    std::vector<motion_pending::Layer> layers;
    player_fields::Fields fields;
    Effects effects;
};
// Standalone original interval b1441c through BEFORE b145dc. The virtual is an
// explicit concrete-class binding. Native layer storage must contain0 and1;
// active_count is not read by either speed helper. Does not advance clocks.
[[nodiscard]] Result<PostOutput> post_phase(const player_controller_step::State&,
    std::span<const motion_pending::Layer>,const player_fields::Fields&,
    std::optional<Virtual10c>,motion_command::Limits = {});
struct Context {const player_update_dispatch::Context& dispatch;std::optional<Virtual10c> virtual10c;};
struct Output {player_update_dispatch::Output dispatched;Effects effects;};
// Composed original b1407c through BEFORE b145dc, using the dispatch candidate
// in place and publishing nothing on failure. Collection and spatial effects
// from dispatch still belong to this same transaction. Not a complete frame.
[[nodiscard]] Result<Output> apply(const player_controller_step::State&,
    const player_state1::Fields&,std::span<const motion_pending::Layer>,std::uint32_t active_count,
    const player_fields::Fields&,const player_object_query::Manager&,const Context&,
    motion_command::Limits = {});
}
