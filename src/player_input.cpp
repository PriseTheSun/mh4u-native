#include "player_input.hpp"
#include "player_selection_predicates.hpp"

namespace mh4u::player_input {
namespace {
struct Sector { std::uint32_t id;std::uint16_t lower,upper;std::uint8_t channel; };
constexpr std::array<Sector,8> sectors{{
    {0x400,0xe000,0x2000,0},{0x800,0x6000,0xa000,0},
    {0x1000,0xa000,0xe000,0},{0x2000,0x2000,0x6000,0},
    {4,0xe000,0x2000,1},{8,0x6000,0xa000,1},
    {0x10,0xa000,0xe000,1},{0x20,0x2000,0x6000,1}
}};
}
std::string_view error_name(ErrorCode code) noexcept {
    switch(code) {
    case ErrorCode::unsupported_mode:return "unsupported_mode";
    case ErrorCode::unsupported_reference:return "unsupported_reference";
    case ErrorCode::invalid_channel:return "invalid_channel";
    case ErrorCode::reference_failure:return "reference_failure";
    case ErrorCode::unknown_gate:return "unknown_gate";
    }
    return "unknown";
}
Result<bool> in_arc(const Channels& channels,const Arc& arc,const ArcContext& context) noexcept {
    const auto& scope=context.scope;const auto* reference=context.reference;
    if(scope.mode==1) {
        if(!context.gate)return Error{ErrorCode::unsupported_mode,"Mode 1 requires the explicit uPlayer magnitude gate"};
        const auto result=gated_magnitude_class(channels,arc.channel,*context.gate);
        if(const auto* error=std::get_if<Error>(&result))return *error;
        if(std::get<std::uint32_t>(result)==0)return false;
    }
    std::uint16_t delta=0;
    if(scope.mode!=1&&(scope.reference_flag!=0||context.force_reference)) {
        if(!reference)return Error{ErrorCode::unsupported_reference,"Reference-dependent source state requires an explicit context"};
        if(!(reference->source.flags&0x4000U)||context.force_reference) {
            const auto resolved=player_reference::heading(reference->source);
            if(const auto* error=std::get_if<player_reference::Error>(&resolved))
                return Error{ErrorCode::reference_failure,error->message,error->code};
            delta=static_cast<std::uint16_t>(reference->orientation-std::get<std::uint32_t>(resolved));
        }
    }
    if(arc.channel>=channels.angles.size())return Error{ErrorCode::invalid_channel,"Only source channels 0/1 are supported"};
    if(scope.mode!=1&&channels.magnitudes[arc.channel]<50)return false;
    const auto offset=static_cast<std::uint16_t>(static_cast<std::uint32_t>(channels.angles[arc.channel])-delta-arc.lower);
    const auto width=static_cast<std::uint16_t>(arc.upper-arc.lower);
    return offset<=width;
}
Result<bool> in_sector_with_context(const Channels& channels,std::uint32_t id,const ArcContext& context) noexcept {
    for(const auto& sector:sectors)if(sector.id==id)
        return in_arc(channels,Arc{sector.channel,sector.lower,sector.upper},context);
    return false; // Source returns before reading player state or calling a gate.
}
Result<bool> in_sector(const Channels& channels,std::uint32_t id,const SectorScope& scope) noexcept {
    return in_sector_with_context(channels,id,ArcContext{scope});
}
Result<bool> in_sector(const Channels& channels,std::uint32_t id,const SectorScope& scope,const ReferenceContext& reference) noexcept {
    return in_sector_with_context(channels,id,ArcContext{scope,&reference});
}
Result<std::uint32_t> magnitude_class(const Channels& channels,std::uint32_t channel) noexcept {
    if(channel>=channels.magnitudes.size())return Error{ErrorCode::invalid_channel,"Only source channels 0/1 are supported"};
    const auto magnitude=channels.magnitudes[channel];
    if(magnitude>=110)return std::uint32_t{3};
    return std::uint32_t{magnitude>=50?1U:0U};
}
Result<std::uint32_t> gated_magnitude_class(const Channels& channels,std::uint32_t channel,const MagnitudeGate& gate) noexcept {
    if(!gate.type)return Error{ErrorCode::unknown_gate,"uPlayer type byte is unknown"};
    const auto type=*gate.type;
    if(type==4||type==5||type==6||type==15) {
        if(!gate.field6f9)return Error{ErrorCode::unknown_gate,"pbw+6f9 is unknown"};
        if(player_selection::flag6f9(*gate.field6f9)) {
            if(!gate.field50d)return Error{ErrorCode::unknown_gate,"pw+50d is unknown"};
            if(*gate.field50d==1)return std::uint32_t{0};
        }
    }
    return magnitude_class(channels,channel);
}
} // namespace mh4u::player_input
