#include "player_update_dispatch.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace mh4u::player_update_dispatch {
namespace {
Error capacity() {return {Phase::storage,ErrorCode::capacity,"Dispatch candidate storage",{}};}
}
Result apply(const player_controller_step::State& state,const player_state1::Fields& action_fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const Context& context,motion_command::Limits limits) {
    try {
        namespace continuation=player_update_continuation;
        namespace action=player_state1;
        auto prepared=continuation::apply(state,layers,fields,objects,context.preparation,limits);
        if(const auto* e=std::get_if<continuation::Error>(&prepared))
            return Error{Phase::preparation,e->code==continuation::ErrorCode::capacity?
                ErrorCode::capacity:ErrorCode::dependency,e->field,*e};
        Output out{std::get<continuation::Output>(std::move(prepared)),action_fields,{},0};
        auto& candidate=out.prepared.prepared;
        const auto& admission=context.preparation.admission;
        const auto& resources=context.resources;
        const auto dispatch=[&](Phase phase)->std::optional<Error> {
            const auto& pbw=candidate.state.player.pbw;
            Dispatch observation{pbw.field371,pbw.field372,pbw.fields5[0],{}};
            const player_object_query::Manager current_objects=candidate.collection?
                player_object_query::Manager{&*candidate.collection}:objects;
            const action::ContinuationContext facts{&context.preparation.constants,&candidate.fields,
                admission.reference,{admission.global_c8c,resources.area_mapping,resources.area_categories},
                admission.prefix.delta_bits,admission.prefix.filter.manager.ad50,current_objects,
                resources.spatial};
            auto result=action::apply(candidate.state,out.action_fields,candidate.layers,active_count,
                resources.tables,resources.model,resources.globals,resources.sampling,limits,facts);
            if(const auto* e=std::get_if<action::Error>(&result))
                return Error{phase,e->code==action::ErrorCode::capacity?
                    ErrorCode::capacity:ErrorCode::dependency,e->message,*e};
            auto updated=std::get<action::Output>(std::move(result));
            candidate.state=std::move(updated.state);candidate.fields=std::move(updated.shared_fields);
            candidate.layers=std::move(updated.layers);out.action_fields=std::move(updated.fields);
            observation.effects=std::move(updated.effects);
            out.dispatches[out.dispatch_count++]=std::move(observation);
            return {};
        };
        if(auto error=dispatch(Phase::first_dispatch))return *error;
        if(candidate.state.player.selection_pbw.field376!=0) {
            // Source r9=1 was established unconditionally before this region.
            // No old PW01 read and no third dispatch, even if376 remains set.
            candidate.fields.pw.field01=1;
            if(auto error=dispatch(Phase::second_dispatch))return *error;
        }
        return out;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
} // namespace mh4u::player_update_dispatch
