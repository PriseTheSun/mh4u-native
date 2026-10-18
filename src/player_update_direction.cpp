#include "player_update_direction.hpp"
#include <utility>

namespace mh4u::player_update_direction {
namespace {
std::uint8_t ordinal(std::uint32_t mask) noexcept {
    if((mask&0x01000000U)==0)return 0xff;
    switch((mask>>22)&3U){
    case 1:return 1;
    case 2:return 0;
    case 3:return 2;
    default:return 0xff;
    }
}
std::variant<MaterialEffects,MaterialError> material_phase(
    const player_controller_step::PlayerFields& player,const player_fields::Fields& fields){
    if(player.pbw.field31!=13)return MaterialEffects{};
    // b29f7c reads previous first; unknown is distinct from zero/current.
    if(!fields.pw.field1fc)return MaterialError{MaterialErrorCode::unknown_field,"PW1fc",{}};
    const auto previous=ordinal(*fields.pw.field1fc),current=ordinal(player.pw.field1f4);
    if(previous==current)return MaterialEffects{MaterialPath::unchanged_ordinal,previous,current,{}};
    if(current==0xff)return MaterialError{MaterialErrorCode::unsupported_resource_change,"b43250 resources",std::uint8_t{0xff}};
    const auto bank=static_cast<std::uint8_t>(current==0?0:current+1);
    if(!fields.pw.field9e8)return MaterialError{MaterialErrorCode::unknown_field,"PW9e8",bank};
    if(*fields.pw.field9e8==bank)return MaterialEffects{MaterialPath::already_selected,previous,current,bank};
    return MaterialError{MaterialErrorCode::unsupported_resource_change,"b29f7c resource/material tables",bank};
}
}
Result apply(const player_controller_step::State& state,const player_state1::Fields& action_fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const Context& context,const notification_queue::State& queue,motion_command::Limits limits){
    auto notified=player_update_notifications::apply(state,action_fields,layers,active_count,
        fields,objects,context.preceding,queue,limits);
    if(const auto* error=std::get_if<player_update_notifications::Error>(&notified))
        return Error{Phase::notifications,*error};
    auto completed=std::get<player_update_notifications::Output>(std::move(notified));
    auto& candidate=completed.status.adjusted.dispatched.prepared.prepared;
    auto material=material_phase(candidate.state.player,candidate.fields);
    if(const auto* error=std::get_if<MaterialError>(&material))return Error{Phase::material,*error};
    auto direction=player_direction_refresh::apply(candidate.state,
        context.preceding.dispatch.preparation.admission.prefix.delta_bits,context.direction);
    if(const auto* error=std::get_if<player_direction_refresh::Error>(&direction))return Error{Phase::direction,*error};
    auto directed=std::get<player_direction_refresh::Output>(std::move(direction));
    candidate.state=std::move(directed.state);
    return Output{std::move(completed),std::get<MaterialEffects>(std::move(material)),std::move(directed.effects)};
}
}
