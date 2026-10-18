#include "player_update_contact.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace mh4u::player_update_contact {
namespace sq=spatial_query;
Result apply(const player_controller_step::State& state,const player_state1::Fields& action_fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
    const player_fields::Fields& fields,const player_position_bridge::Fields& position,
    const player_object_query::Manager& objects,const Context& context,
    const notification_queue::State& queue,motion_command::Limits limits){
    try{
        auto previous=player_update_manager::apply(state,action_fields,layers,active_count,
            fields,objects,context.preceding,queue,limits);
        if(const auto* error=std::get_if<player_update_manager::Error>(&previous))
            return Error{Phase::manager,*error};
        auto completed=std::get<player_update_manager::Output>(std::move(previous));
        auto& dispatched=completed.positioned.directed.notified.status.adjusted.dispatched;
        auto& candidate=dispatched.prepared.prepared;
        const auto& dispatch_context=context.preceding.preceding.preceding.dispatch;
        const auto& admission=dispatch_context.preparation.admission;
        auto spatial=dispatch_context.resources.spatial;
        const sq::HeightResult* earlier_height=nullptr;
        for(std::size_t i=0;i<dispatched.dispatch_count;++i){
            const auto& effects=dispatched.dispatches[i].effects;
            if(effects.spatial&&effects.spatial->height){
                if(earlier_height)return Error{Phase::grid,sq::Error{
                    sq::ErrorCode::unsupported_route,"dispatch multiple height queries",{}}};
                earlier_height=&*effects.spatial->height;
            }
        }
        // Query results are cumulative; a skipped dispatch retains an explicit
        // incoming overlay. Validate and own it before borrowing it for contact.
        const std::span<const sq::GridUpdate> prior=earlier_height?
            std::span<const sq::GridUpdate>{earlier_height->query.grid_updates}:spatial.previous_grid_updates;
        std::vector<sq::GridUpdate> grid_updates;
        if(!prior.empty()){
            if(!spatial.scene)return Error{Phase::grid,sq::Error{
                sq::ErrorCode::unknown_operand,"spatial Scene",{}}};
            auto plan=sq::plan_updates(*spatial.scene,prior,{},spatial.query_limits);
            if(const auto* error=std::get_if<sq::Error>(&plan))return Error{Phase::grid,*error};
            grid_updates=std::get<std::vector<sq::GridUpdate>>(std::move(plan));
        }
        spatial.previous_grid_updates=grid_updates;
        auto manager=context.preceding.manager;
        if(auto* view=std::get_if<player_manager_update::ManagerView>(&manager);view&&completed.manager)
            view->state=&*completed.manager;
        const player_contact_update::Context next{admission.prefix.delta_bits,manager,
            admission.reference,spatial,admission.global_c8c,admission.global_c90,
            context.null_reference_template,{}};
        auto contact=player_contact_update::apply(candidate.state.player,candidate.fields,position,next);
        if(const auto* error=std::get_if<player_contact_update::Error>(&contact))
            return Error{Phase::contact,*error};
        auto applied=std::get<player_contact_update::Output>(std::move(contact));
        if(applied.effects.height)grid_updates=applied.effects.height->query.grid_updates;
        candidate.state.player=std::move(applied.player);
        candidate.fields=std::move(applied.fields);
        return Output{std::move(completed),std::move(applied.position),
            std::move(applied.effects),std::move(grid_updates)};
    }catch(const std::bad_alloc&){
        return Error{Phase::grid,sq::Error{sq::ErrorCode::resource_limit,"contact candidate allocation",{}}};
    }catch(const std::length_error&){
        return Error{Phase::grid,sq::Error{sq::ErrorCode::resource_limit,"contact candidate allocation",{}}};
    }
}
}
