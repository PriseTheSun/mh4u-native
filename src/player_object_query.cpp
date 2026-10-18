#include "player_object_query.hpp"
#include <bit>

namespace mh4u::player_object_query {
namespace {
namespace pr=player_reference;
class Query {
    const player_controller_step::PlayerFields& p;
    const player_fields::Fields& fields;
    std::optional<Error> error;
    std::optional<std::size_t> slot;
    Output out;
    void fail(ErrorCode code,std::string_view field,std::optional<pr::Error> cause={}){
        if(!error)error=Error{code,field,slot,cause};
    }
    template<class T>T read(const std::optional<T>& value,std::string_view name){
        if(!value){fail(ErrorCode::unknown_field,name);return T{};}return *value;
    }
    bool accepted(const Object& object){
        ++out.object_queries;
        // 91a2a8 repeats the source enabled gate after 911e48.
        if(read(object.field140,"D+140")==0||error)return false;
        if(!(read(object.field1b4,"D+1b4")&1U)||error)return false;
        if(read(object.field34c,"D+34c")!=1||error)return false;
        const auto mask=read(object.field290,"D+290");
        // ARM register-shift is zero at >=32; an eight-bit mask also makes
        // indices8..31 fail. Avoid undefined C++ shifts at all byte indices.
        if(error||p.pbw.field33>=8||!(mask&(1U<<p.pbw.field33)))return false;
        if(p.global.field5d!=0){fail(ErrorCode::unsupported_availability,"682c4c network context");return false;}
        if(static_cast<int>(std::bit_cast<std::int8_t>(p.global.field2f))!=p.pbw.field33)return false;
        if(p.pbw.field371==8)return false;
        const auto area=read(object.field14c,"D+14c");
        const auto player_area=read(p.selection_pbw.field9,"pbw+9");
        if(error||area!=player_area)return false;
        const auto kind=read(object.field1c0,"D+1c0");
        if(error||kind==83||kind==111)return false;
        if(kind==24||kind==110){fail(ErrorCode::unsupported_position,"930294 object position");return false;}
        const auto position=read(object.position_bits,"D+198/19c/1a0");
        if(error)return false;
        const auto x=read(fields.pbw.field10_bits,"pbw+10");
        const auto z=read(fields.pbw.field18_bits,"pbw+18");
        if(error)return false;
        ++out.direction_queries;
        const auto heading=pr::heading({0,{},{},pr::Points{
            std::bit_cast<float>(x),std::bit_cast<float>(z),
            std::bit_cast<float>(position[0]),std::bit_cast<float>(position[2])}});
        if(const auto* e=std::get_if<pr::Error>(&heading)){fail(ErrorCode::reference,e->message,*e);return false;}
        auto direction=std::get<std::uint32_t>(heading);
        if(kind==46&&area==1){
            direction=(direction&0x8000U)?0xc000U:0x4000U;
            const auto difference=(direction-p.pbw.field24)&0xffffU;
            return difference-0x38e4U<=0x8e39U;
        }
        const auto difference=(direction-p.pbw.field24)&0xffffU;
        return difference-0x2000U<=0xc000U;
    }
public:
    Query(const player_controller_step::PlayerFields& player,const player_fields::Fields& extra):p(player),fields(extra){}
    Result run(const Manager& manager){
        if(p.selection_pw.field9a8!=0)return out;
        const auto* entries=std::get_if<std::span<const Slot>>(&manager);
        if(!entries)return Error{ErrorCode::unknown_field,"manager slots",{}, {}};
        if(entries->size()!=32)return Error{ErrorCode::manager_extent,"Manager has 32 physical slots",{}, {}};
        for(std::size_t i=0;i<entries->size();++i){
            slot=i;++out.slots_examined;
            const auto& entry=(*entries)[i];
            if(std::holds_alternative<Unknown>(entry))return Error{ErrorCode::unknown_field,"manager slot",slot,{}};
            if(std::holds_alternative<Absent>(entry))continue;
            const auto& object=std::get<Object>(entry);
            const auto enabled=read(object.field140,"D+140");
            if(error)return *error;
            if(enabled&&accepted(object)){out.matched=true;return out;}
            if(error)return *error;
        }
        return out;
    }
};
}
Result query(const player_controller_step::PlayerFields& p,const player_fields::Fields& fields,const Manager& manager)noexcept{
    return Query(p,fields).run(manager);
}
}
