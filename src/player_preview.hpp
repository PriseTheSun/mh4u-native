#pragma once
#include "player_actor.hpp"

namespace mh4u::native_actor {
// Explicit inspection policy over Player, not recovered uPlayer construction,
// gameplay input mapping, or source scheduling. No request runs implicitly.
// A caller-selected diagnostic reset recipe, not inferred constructor facts.
// It owns no animation/pose state. Its table retains immutable source ownership.
struct PolicyPreviewSeed {
    player_controller_step::PlayerFields player;
    player_fields::Fields fields;
    player_state1::Fields state1_fields;
};
class PlayerPreview {
public:
    static PlayerResult<PlayerPreview> from_actor(const Actor&,const player_transition::Constants&);
    static PlayerResult<PlayerPreview> from_actor_with_policy(const Actor&,
        const player_transition::Constants&,const PolicyPreviewSeed&);
    PlayerResult<PlayerPolicyOutput> apply_policy(const PlayerPolicyInput& input,const PlayerPolicyContext& context){
        return player_.apply_policy(input,context);
    }
    PlayerResult<PlayerOutput> reset();
    PlayerResult<PlayerState1Output> dispatch_state1(const player_state1::Tables&,const PlayerState1Context& = {});
    PlayerResult<PlayerOutput> step();
    PlayerResult<PlayerOutput> request(std::uint32_t pl_state,std::int32_t interpolation=4);
    PlayerResult<PlayerOutput> capture()const;
    std::shared_ptr<const Scene> presentation()const noexcept{return player_.presentation();}
    const Player& player()const noexcept{return player_;}
private:
    static PlayerResult<PlayerPreview> create(const Actor&,const player_transition::Constants&,
        std::optional<PolicyPreviewSeed>);
    explicit PlayerPreview(Player player,std::optional<PolicyPreviewSeed> seed):player_(std::move(player)),policy_seed_(std::move(seed)){}
    Player player_; // Only the canonical live owner; no retained Actor or saved pose.
    std::optional<PolicyPreviewSeed> policy_seed_; // Immutable reset recipe, never live policy state.
};
}
