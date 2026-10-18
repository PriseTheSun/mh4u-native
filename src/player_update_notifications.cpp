#include "player_update_notifications.hpp"
#include <utility>

namespace mh4u::player_update_notifications {
Result apply(const player_controller_step::State& state,const player_state1::Fields& action_fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const player_update_post_dispatch::Context& context,const notification_queue::State& queue,
    motion_command::Limits limits){
    auto status=player_update_status::apply(state,action_fields,layers,active_count,fields,objects,context,limits);
    if(const auto* error=std::get_if<player_update_status::Error>(&status))
        return Error{Phase::status,*error};
    auto completed=std::get<player_update_status::Output>(std::move(status));
    const auto& candidate=completed.adjusted.dispatched.prepared.prepared;
    auto notifications=player_status_notifications::apply(candidate.state.player,candidate.fields,queue,std::uint8_t{0});
    if(const auto* error=std::get_if<player_status_notifications::Error>(&notifications))
        return Error{Phase::notifications,*error};
    auto notified=std::get<player_status_notifications::Output>(std::move(notifications));
    return Output{std::move(completed),std::move(notified.queue),std::move(notified.effects)};
}
}
