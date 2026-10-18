#include "player_update_manager.hpp"
#include <utility>

namespace mh4u::player_update_manager {
Result apply(const player_controller_step::State& state,const player_state1::Fields& action_fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const Context& context,const notification_queue::State& queue,motion_command::Limits limits){
    auto previous=player_update_position::apply(state,action_fields,layers,active_count,
        fields,objects,context.preceding,queue,limits);
    if(const auto* error=std::get_if<player_update_position::Error>(&previous))
        return Error{Phase::position,*error};
    auto completed=std::get<player_update_position::Output>(std::move(previous));
    auto& candidate=completed.directed.notified.status.adjusted.dispatched.prepared.prepared;
    auto manager=context.manager;
    if(auto* view=std::get_if<player_manager_update::ManagerView>(&manager)){
        if(auto* foreign=std::get_if<player_manager_update::ForeignPlayer>(&view->player)){
            // Rebase input-owner identities to the one preceding candidate.
            // Partial aliases remain partial for the leaf's lazy validation.
            const bool same_player=foreign->player==&state.player;
            const bool same_fields=foreign->fields==&fields;
            if(same_player&&same_fields)view->player=player_manager_update::Caller{};
            else{
                if(same_player)foreign->player=&candidate.state.player;
                if(same_fields)foreign->fields=&candidate.fields;
            }
        }
    }
    const player_object_query::Manager candidate_objects=candidate.collection?
        player_object_query::Manager{&*candidate.collection}:objects;
    const auto& admission=context.preceding.preceding.dispatch.preparation.admission;
    auto next=player_manager_update::apply(candidate.state.player,candidate.fields,
        {candidate.layers,candidate_objects,manager,admission.reference,context.preceding.direction.virtual84});
    if(const auto* error=std::get_if<player_manager_update::Error>(&next))
        return Error{Phase::manager,*error};
    auto applied=std::get<player_manager_update::Output>(std::move(next));
    candidate.state.player=std::move(applied.player);
    candidate.fields=std::move(applied.fields);
    return Output{std::move(completed),std::move(applied.manager),std::move(applied.foreign),std::move(applied.effects)};
}
}
