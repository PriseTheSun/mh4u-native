#include "player_preview.hpp"
#include <new>
#include <stdexcept>

namespace mh4u::native_actor {
namespace {
namespace jp=joint_pose;
constexpr std::uint32_t one=0x3f800000;
constexpr jp::Bits4 identity{0,0,0,one};
PlayerError capacity(){return{PlayerErrorCode::capacity,"Inspection seed allocation exceeded capacity",{}, {}};}
PlayerInitial seed(std::size_t joints){
    PlayerInitial initial{};initial.snapshots.resize(joints);
    initial.model.rotation=initial.model.rotation_delta=identity;
    initial.model.scale=initial.model.null_scale={one,one,one,0};
    for(auto& layer:initial.layers){
        layer.animation.tracks.joints.resize(joints);
        layer.animation.layer.speed=1;layer.animation.layer.flags=2;
        layer.animation.prediction_rotation=identity;
    }
    // Named diagnostic gate values, not an inference of the game's startup state.
    initial.controller.player.global.field2b=2;initial.controller.player.pbw.field30=3;
    return initial;
}
PlayerStep parameters(){
    PlayerStep step;step.delta_bits=one;
    step.templates={jp::Bits3{},jp::Bits3{},identity,jp::Bits4{}};
    step.restoration_scale=jp::Bits3{one,one,one};step.parent=jp::Null{};
    step.reference_basis=motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};
    step.reference_translation=identity;return step;
}
PlayerResult<PlayerOutput> capture(const Player& player){
    auto result=player.capture();if(auto* e=std::get_if<PlayerError>(&result))return *e;
    return PlayerOutput{std::get<Frame>(std::move(result)),{}, {}};
}
}
PlayerResult<PlayerPreview> PlayerPreview::from_actor(const Actor& actor,const player_transition::Constants& constants){return create(actor,constants,std::nullopt);}
PlayerResult<PlayerPreview> PlayerPreview::from_actor_with_policy(const Actor& actor,const player_transition::Constants& constants,const PolicyPreviewSeed& policy){return create(actor,constants,policy);}
PlayerResult<PlayerPreview> PlayerPreview::create(const Actor& actor,const player_transition::Constants& constants,std::optional<PolicyPreviewSeed> policy){
    try{
        // This policy is intentionally limited to the already verified hunter rig.
        PlayerBanks banks{}; // Every other bank is unknown, not absent or aliased.
        banks[0]=MotionSource{"c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"};
        auto initial=seed(19);
        if(policy){initial.controller.player=policy->player;initial.shared_fields=policy->fields;initial.state1_fields=policy->state1_fields;}
        auto result=Player::from_rest(actor,initial,constants,banks);
        if(auto* e=std::get_if<PlayerError>(&result))return *e;
        if(actor.part_count()!=7)return PlayerError{PlayerErrorCode::initial_state,"Player preview requires the explicit seven-piece, 19-joint hunter profile",{}, {}};
        return PlayerPreview{std::get<Player>(std::move(result)),std::move(policy)};
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerOutput> PlayerPreview::capture()const{return native_actor::capture(player_);}
PlayerResult<PlayerOutput> PlayerPreview::step(){return player_.step(parameters());}
PlayerResult<PlayerState1Output> PlayerPreview::dispatch_state1(const player_state1::Tables& tables,const PlayerState1Context& context){return player_.dispatch_state1(tables,parameters().templates,context);}
PlayerResult<PlayerOutput> PlayerPreview::request(std::uint32_t state,std::int32_t interpolation){
    if(state!=1&&state!=3)return PlayerError{PlayerErrorCode::initial_state,"Inspection request supports only explicit states 1 and 3",{}, {}};
    return player_.apply(player_selection::CommonArguments{state,interpolation,0,0,1,0},parameters().templates);
}
PlayerResult<PlayerOutput> PlayerPreview::reset(){
    try{
        auto initial=seed(player_.state().frame.animation.joints.size());
        if(policy_seed_){initial.controller.player=policy_seed_->player;initial.shared_fields=policy_seed_->fields;initial.state1_fields=policy_seed_->state1_fields;}
        auto result=Player::from_rest(player_,initial);
        if(auto* e=std::get_if<PlayerError>(&result))return *e;
        auto fresh=std::get<Player>(std::move(result));auto output=native_actor::capture(fresh);
        if(std::holds_alternative<PlayerError>(output))return output;
        player_=std::move(fresh);return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
}
