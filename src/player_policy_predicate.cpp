#include "player_policy_predicate.hpp"

namespace mh4u::player_policy_predicate {
Predicate decode(std::span<const std::uint8_t,22> bytes) noexcept {
    const auto half=[&](std::size_t n){return static_cast<std::uint16_t>(bytes[n]|(std::uint16_t{bytes[n+1]}<<8));};
    return {half(6),{half(8),half(10),half(12)}};
}
namespace {
class Evaluation {
    const Context& c;
public:
    std::optional<Error> error;
    explicit Evaluation(const Context& context):c(context){}
    template<class T> T field(const std::optional<T>& value,std::string_view name) {
        if(error)return T{};
        if(!value){error=Error{ErrorCode::unknown_field,name,{},{}};return T{};}
        return *value;
    }
    template<class T> T input(player_input::Result<T> result) {
        if(error)return T{};
        if(const auto* e=std::get_if<player_input::Error>(&result)) {
            error=Error{ErrorCode::input_failure,"player_input",*e,{}};return T{};
        }
        return std::get<T>(result);
    }
    std::uint32_t magnitude(unsigned channel) {
        if(error)return 0;
        return input(player_input::gated_magnitude_class(c.channels,channel,c.magnitude_gate));
    }
    bool mask(const std::optional<std::uint16_t>& value,std::uint16_t bits,std::string_view name) {
        return (field(value,name)&bits)!=0;
    }
    bool flag(std::uint16_t id,std::size_t base) {
        if(error)return false;
        const auto index=base+static_cast<std::size_t>(id>>5);
        if(index>=c.flags3d4.size()) {error=Error{ErrorCode::flag_extent,"pbw+3d4 indexed word",{},{}};return false;}
        return (field(c.flags3d4[index],"pbw+3d4 indexed word")&(1U<<(id&31)))!=0;
    }
    std::uint32_t heading() {
        const auto ref=field(c.reference,"reference context");
        if(error)return 0;
        const auto result=player_reference::heading(ref.source);
        if(const auto* e=std::get_if<player_reference::Error>(&result)) {
            error=Error{ErrorCode::reference_failure,"reference heading",{},*e};return 0;
        }
        return std::get<std::uint32_t>(result);
    }
    bool arc(unsigned channel,std::uint16_t low,std::uint16_t high,bool gated) {
        if(error)return false;
        // Ungated b3ad00 compares magnitude directly and never reads reference.
        if(!gated)return input(player_input::in_arc(c.channels,{channel,low,high},{{0,0}}));
        return input(player_input::in_arc(c.channels,{channel,low,high},{{1,0},nullptr,&c.magnitude_gate}));
    }
    bool directional_arc(unsigned channel,std::uint16_t low,std::uint16_t high) {
        if(!magnitude(channel)||error)return false;
        const auto reference=heading();
        const auto orientation=field(c.field24,"pbw+24");
        return !error&&static_cast<std::uint16_t>(c.channels.angles[channel]-(orientation-reference)-low)<=static_cast<std::uint16_t>(high-low);
    }
    bool pressed_arc(unsigned channel,std::uint16_t low,std::uint16_t high) {
        return mask(c.field3bc,channel?0x3c:0x3c00,"pbw+3bc")&&arc(channel,low,high,true);
    }
    bool sector(std::uint32_t id,std::uint32_t mode) {
        if(error)return false;
        // Unknown IDs are rejected by the source before consuming context.
        if(id!=0x400&&id!=0x800&&id!=0x1000&&id!=0x2000&&id!=4&&id!=8&&id!=16&&id!=32)return false;
        const auto reference_flag=mode==1?std::uint8_t{0}:field(c.reference_flag,"pbw+451");
        if(error)return false;
        return input(player_input::in_sector_with_context(c.channels,id,player_input::ArcContext{{mode,reference_flag},c.reference?&*c.reference:nullptr,&c.magnitude_gate}));
    }
    bool corrected_sector(std::uint16_t id) {
        const auto ref=field(c.reference,"reference context");
        if(error)return false;
        const auto mode=field(ref.source.stored_mode,"pw+150");
        if(error)return false;
        const auto orientation=mode==1?field(c.field154,"pw+154"):ref.orientation;
        const auto h=heading();
        const auto bounds=field(c.direction_bounds,"100c250 runtime bounds");
        if(error)return false;
        const auto delta=static_cast<std::uint16_t>(static_cast<std::uint16_t>(orientation)-h);
        // Conditional CMP preserves a half-open interval: lower included,
        // upper excluded. It is not the modular interval used by arc helpers.
        if(delta>=bounds[0]&&delta<bounds[1]) {
            if(id==0x400)id=0x800;else if(id==0x800)id=0x400;
        }
        return sector(id,0);
    }
    bool run(const Predicate& p) {
        const auto a=p.arguments[0],b=p.arguments[1],d=p.arguments[2];
        switch(p.word&0x3fffU) {
        case 0:return true;
        case 1:return pressed_arc(1,a,b);
        case 2:return pressed_arc(0,a,b);
        case 3:return mask(c.field3ac,a,"pbw+3ac");
        case 4:return magnitude(0)&&mask(c.field3ac,a,"pbw+3ac");
        case 5:return mask(c.field3bc,a,"pbw+3bc");
        case 6:return !magnitude(0)&&mask(c.field3ac,a,"pbw+3ac");
        case 7:return magnitude(0)&&mask(c.field3a8,a,"pbw+3a8");
        case 8:return magnitude(0)!=0;
        case 9:return magnitude(0)>=3;
        case 10:return mask(c.field3a8,a,"pbw+3a8")&&mask(c.field3ac,b,"pbw+3ac");
        case 11:return arc(0,a,b,true)&&mask(c.field3ac,d,"pbw+3ac");
        case 12:return magnitude(0)==0;
        case 13:return magnitude(1)==0;
        case 14:return arc(1,a,b,true);
        case 15:return arc(0,a,b,true);
        case 16:return !arc(1,a,b,true);
        case 17:return !arc(0,a,b,true);
        case 18:return mask(c.field3a8,a,"pbw+3a8");
        case 19:return pressed_arc(1,a,b)&&mask(c.field3b8,d,"pbw+3b8");
        case 20:return magnitude(1)!=0;
        case 21:return magnitude(1)>=3;
        case 22:return flag(a,0);
        case 23:return magnitude(0)&&flag(a,0);
        case 24:return magnitude(0)&&!flag(a,0);
        case 25:return flag(a,4);
        case 26:return flag(a,4)&&sector(d,0);
        case 27:return flag(a,0)&&arc(0,b,d,true);
        case 28:return flag(a,0)&&!magnitude(0);
        case 29:return magnitude(0)&&flag(a,4);
        case 30:return flag(a,0)&&arc(0,b,d,false);
        case 31:return false;
        case 32:return directional_arc(0,a,b);
        case 33:return sector(a,b);
        case 34:return (flag(a,0)&&sector(b,0))||corrected_sector(a);
        case 35:return corrected_sector(a);
        case 36:return flag(a,0)&&corrected_sector(b);
        case 37: {
            if(!flag(a,4)||error)return false;
            const auto reference_flag=field(c.reference_flag,"pbw+451");
            if(error)return false;
            return input(player_input::in_arc(c.channels,{0,b,d},{{0,reference_flag},c.reference?&*c.reference:nullptr}));
        }
        case 38:return flag(a,4)&&!magnitude(0);
        default:return false;
        }
    }
};
}
Result evaluate(const Predicate& p,const Context& c) noexcept {
    Evaluation evaluation(c);const auto value=evaluation.run(p);
    if(evaluation.error)return *evaluation.error;
    return value;
}
} // namespace mh4u::player_policy_predicate
