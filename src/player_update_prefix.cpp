#include "player_update_prefix.hpp"

namespace mh4u::player_update_prefix {
Result apply(const player_controller_step::PlayerFields& player,
    const player_fields::Fields& fields,const Context& context) {
    auto p=player;auto f=fields;
    std::optional<player_input_filter::Effects> filter;
    if(player_transition::local_control(p.global,p.pbw)) {
        auto result=player_input_filter::apply(p,f,context.filter);
        if(const auto* error=std::get_if<player_input_filter::Error>(&result))return Error{Phase::filter,*error};
        const auto& out=std::get<player_input_filter::Output>(result);
        p=out.player;f=out.fields;filter=out.effects;
    }
    auto accumulated=player_update_accumulator::apply(f,context.delta_bits);
    if(const auto* error=std::get_if<player_update_accumulator::Error>(&accumulated))return Error{Phase::accumulator,*error};
    f=std::get<player_update_accumulator::Output>(accumulated).fields;
    auto begun=player_update_begin::apply(p,f);
    if(const auto* error=std::get_if<player_update_begin::Error>(&begun))return Error{Phase::begin,*error};
    const auto& begin=std::get<player_update_begin::Output>(begun);
    auto queried=player_world_lookup::apply(begin.player,begin.fields,context.world);
    if(const auto* error=std::get_if<player_world_lookup::Error>(&queried))return Error{Phase::lookup,*error};
    auto out=std::get<player_world_lookup::PlayerOutput>(std::move(queried));
    return Output{begin.player,std::move(out.fields),filter,std::move(out.lookup)};
}
} // namespace mh4u::player_update_prefix
