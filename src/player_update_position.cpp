#include "player_update_position.hpp"
#include <utility>

namespace mh4u::player_update_position {
Result apply(const player_controller_step::State& state,const player_state1::Fields& action_fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const player_update_direction::Context& context,const notification_queue::State& queue,motion_command::Limits limits){
    auto directed=player_update_direction::apply(state,action_fields,layers,active_count,
        fields,objects,context,queue,limits);
    if(const auto* error=std::get_if<player_update_direction::Error>(&directed))
        return Error{Phase::direction,*error};
    auto completed=std::get<player_update_direction::Output>(std::move(directed));
    auto& candidate=completed.notified.status.adjusted.dispatched.prepared.prepared;
    auto position=player_position_timer::apply(candidate.fields,
        context.preceding.dispatch.preparation.admission.prefix.delta_bits);
    if(const auto* error=std::get_if<player_position_timer::Error>(&position))
        return Error{Phase::position,*error};
    auto positioned=std::get<player_position_timer::Output>(std::move(position));
    candidate.fields=std::move(positioned.fields);
    return Output{std::move(completed),std::move(positioned.effects)};
}
}
