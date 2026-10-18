#include "player_update_status.hpp"
#include <utility>

namespace mh4u::player_update_status {
Result apply(const player_controller_step::State& state,const player_state1::Fields& action_fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const player_update_post_dispatch::Context& context,motion_command::Limits limits){
    auto adjusted=player_update_post_dispatch::apply(state,action_fields,layers,active_count,
        fields,objects,context,limits);
    if(const auto* error=std::get_if<player_update_post_dispatch::Error>(&adjusted))
        return Error{Phase::adjustment,*error};
    Output out{std::get<player_update_post_dispatch::Output>(std::move(adjusted)),{}};
    auto& candidate=out.adjusted.dispatched.prepared.prepared;
    const player_object_query::Manager current_objects=candidate.collection?
        player_object_query::Manager{&*candidate.collection}:objects;
    auto status=player_status_update::apply(candidate.state.player,candidate.fields,current_objects,
        context.dispatch.preparation.admission.prefix.delta_bits);
    if(const auto* error=std::get_if<player_status_update::Error>(&status))
        return Error{Phase::status,*error};
    auto updated=std::get<player_status_update::Output>(std::move(status));
    candidate.state.player.pw.field1f4=updated.field1f4;
    candidate.fields=std::move(updated.fields);
    out.effects=std::move(updated.effects);
    return out;
}
}
