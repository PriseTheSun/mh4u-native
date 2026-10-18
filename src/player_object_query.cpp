#include "player_object_query.hpp"
#include <bit>

namespace mh4u::player_object_query {
namespace {
namespace pr=player_reference;
using Player=player_controller_step::PlayerFields;
using Fields=player_fields::Fields;
// This view owns only failure context. Persistent facts stay in Collection.
struct Access {
    std::optional<Error> error;
    std::optional<std::size_t> slot,object,data;
    void fail(ErrorCode code,std::string_view field,std::optional<pr::Error> cause={}) noexcept {
        if(!error)error=Error{code,field,slot,cause,object,data};
    }
    template<class T>T read(const std::optional<T>& value,std::string_view name) noexcept {
        if(!value){fail(ErrorCode::unknown_field,name);return T{};}return *value;
    }
    const Collection* collection(const Manager& manager) noexcept {
        const auto result=resolve_manager(manager);
        if(const auto* e=std::get_if<Error>(&result)){error=*e;return nullptr;}
        return std::get<const Collection*>(result);
    }
    const DataFields* lookup(const Collection& c,std::size_t index) noexcept {
        slot=index;object.reset();data.reset();
        const auto result=resolve_slot(c,index);
        if(const auto* e=std::get_if<Error>(&result)){error=*e;return nullptr;}
        const auto& found=std::get<std::optional<ResolvedObject>>(result);if(!found)return nullptr;
        object=found->index.value;data=found->data.index.value;return found->data.fields;
    }
    bool available(const Player& p,const DataFields& d) noexcept {
        const auto result=availability(p,d);
        if(const auto* e=std::get_if<Error>(&result)){fail(e->code,e->field,e->reference);return false;}
        return std::get<bool>(result);
    }
    std::optional<std::uint32_t> heading(const Fields& fields,const DataFields& d) noexcept {
        // Both paths consume only X/Z. An unknown unused Y stays unknown.
        const auto x=read(fields.pbw.field10_bits,"pbw+10");
        const auto z=read(fields.pbw.field18_bits,"pbw+18");
        const auto dx=read(d.position_bits[0],"D+198");
        const auto dz=read(d.position_bits[2],"D+1a0");
        if(error)return {};
        const auto result=pr::heading({0,{},{},pr::Points{
            std::bit_cast<float>(x),std::bit_cast<float>(z),
            std::bit_cast<float>(dx),std::bit_cast<float>(dz)}});
        if(const auto* e=std::get_if<pr::Error>(&result)){fail(ErrorCode::reference,e->message,*e);return {};}
        return std::get<std::uint32_t>(result);
    }
};
// Byte mask intersection equals the ARM register shift for every byte index:
// 8..31 cannot intersect; >=32 yields zero. No undefined C++ shift occurs.
std::uint8_t mask(std::uint8_t index) noexcept {
    return index<8?static_cast<std::uint8_t>(1U<<index):std::uint8_t{0};
}
class Query:private Access {
    const Player& p;const Fields& fields;Output out;
    bool accepted(const DataFields& d) noexcept {
        ++out.object_queries;
        if(read(d.field140,"D+140")==0||error)return false;
        if(!(read(d.field1b4,"D+1b4")&1U)||error)return false;
        if(read(d.field34c,"D+34c")!=1||error)return false;
        const auto bits=read(d.field290,"D+290");
        if(error||!(bits&mask(p.pbw.field33)))return false;
        // Original91a2a8 checks the mask BEFORE shared91f214.
        if(!available(p,d))return false;
        const auto area=read(d.field14c,"D+14c");
        const auto kind=read(d.field1c0,"D+1c0");
        if(error||kind==83||kind==111)return false;
        if(kind==24||kind==110){fail(ErrorCode::unsupported_position,"930294 object position");return false;}
        ++out.direction_queries;const auto direction=heading(fields,d);if(!direction)return false;
        if(kind==46&&area==1){
            const auto adjusted=(*direction&0x8000U)?0xc000U:0x4000U;
            const auto difference=(adjusted-p.pbw.field24)&0xffffU;
            return difference-0x38e4U<=0x8e39U;
        }
        const auto difference=(*direction-p.pbw.field24)&0xffffU;
        return difference-0x2000U<=0xc000U;
    }
public:
    Query(const Player& player,const Fields& extra):p(player),fields(extra){}
    Result run(const Manager& manager) noexcept {
        if(p.selection_pw.field9a8!=0)return out;
        const auto* c=collection(manager);if(error)return *error;
        for(std::size_t i=0;i<c->slots.size();++i){
            ++out.slots_examined;const auto* d=lookup(*c,i);if(error)return *error;if(!d)continue;
            const auto enabled=read(d->field140,"D+140");if(error)return *error;
            if(enabled&&accepted(*d)){out.matched=true;return out;}
            if(error)return *error;
        }
        return out;
    }
};
class Scan:private Access {
    const Player& p;const Fields& fields;
    bool accepted(DataFields& d,ScanEffects& effects) noexcept {
        ++effects.object_queries;
        if(read(d.field140,"D+140")==0||error)return false;
        const auto active=read(d.field1b4,"D+1b4");
        const auto bit=mask(p.pbw.field33); // Loaded even for inactive D1b4.
        if(error||!(active&1U))return false;
        ++effects.availability_queries;
        // Original9368c4 calls91f214 BEFORE the D290 mask gate.
        if(!available(p,d))return false;
        const auto bits=read(d.field290,"D+290");
        if(error||!(bits&bit))return false;
        const auto prior=read(d.field349,"D+349");
        if(error||(prior&bit))return false;
        d.field349=static_cast<std::uint8_t>(prior|bit);++effects.mask_stores;
        ++effects.direction_queries;const auto direction=heading(fields,d);if(!direction)return false;
        return ((*direction-p.pbw.field24+0x3555U)&0xffffU)>0x6aaaU;
    }
public:
    Scan(const Player& player,const Fields& extra):p(player),fields(extra){}
    ScanResult run(const Manager& manager) noexcept {
        const auto* source=collection(manager);if(error)return *error;
        ScanOutput out{*source,false,{},{},{}};
        for(std::size_t i=0;i<out.collection.slots.size();++i){
            ++out.effects.slots_examined;
            const auto* resolved=lookup(out.collection,i);if(error)return *error;if(!resolved)continue;
            ++out.effects.enabled_queries;
            const auto enabled=read(resolved->field140,"D+140");if(error)return *error;
            if(enabled&&accepted(out.collection.data[*data],out.effects)){
                out.matched=true;out.matched_object=ObjectIndex{static_cast<std::uint32_t>(*object)};
                out.matched_data=DataIndex{static_cast<std::uint32_t>(*data)};return out;
            }
            if(error)return *error;
        }
        return out;
    }
};
class StatusScan:private Access {
    const Player& p;
public:
    explicit StatusScan(const Player& player):p(player){}
    std::variant<StatusScanOutput,Error> run(const Manager& manager) noexcept {
        const auto* source=collection(manager);if(error)return *error;
        StatusScanOutput out{};
        for(std::size_t i=0;i<source->slots.size();++i){
            ++out.slots_examined;
            const auto* d=lookup(*source,i);if(error)return *error;if(!d)continue;
            ++out.enabled_queries;
            const auto enabled=read(d->field140,"D+140");if(error)return *error;
            if(!enabled)continue;
            const auto flags=read(d->field1b4,"D+1b4");if(error)return *error;
            if(!(flags&1U))continue;
            ++out.status_queries;
            const auto status=read(d->field530,"D+530");if(error)return *error;
            if(status!=1)continue;
            ++out.availability_queries;
            const bool found=available(p,*d);if(error)return *error;
            if(found){out.matched=true;return out;}
        }
        return out;
    }
};
}
std::variant<const Collection*,Error> resolve_manager(const Manager& manager) noexcept {
    if(std::holds_alternative<Unknown>(manager))return Error{ErrorCode::unknown_field,"manager singleton",{},{}};
    const auto* ptr=std::get_if<const Collection*>(&manager);
    if(!ptr||!*ptr)return Error{ErrorCode::absent_manager,"manager singleton",{},{}};
    return *ptr;
}
std::variant<ResolvedData,Error> resolve(const Collection& c,ObjectIndex index) noexcept {
    if(index.value>=c.objects.size())return Error{ErrorCode::invalid_index,"manager Object index",{},{},index.value,{}};
    const auto& link=c.objects[index.value].data;
    if(std::holds_alternative<Unknown>(link))return Error{ErrorCode::unknown_field,"Object+e28",{},{},index.value,{}};
    if(std::holds_alternative<Absent>(link))return Error{ErrorCode::absent_data,"Object+e28",{},{},index.value,{}};
    const auto data=std::get<DataIndex>(link);
    if(data.value>=c.data.size())return Error{ErrorCode::invalid_index,"Object Data index",{},{},index.value,data.value};
    return ResolvedData{data,&c.data[data.value]};
}
std::variant<std::optional<ResolvedObject>,Error> resolve_slot(const Collection& c,std::size_t slot) noexcept {
    if(slot>=c.slots.size())return Error{ErrorCode::invalid_index,"manager slot index",slot,{}};
    const auto& entry=c.slots[slot];
    if(std::holds_alternative<Unknown>(entry))return Error{ErrorCode::unknown_field,"manager slot",slot,{}};
    if(std::holds_alternative<Absent>(entry))return std::optional<ResolvedObject>{};
    const auto index=std::get<ObjectIndex>(entry);const auto result=resolve(c,index);
    if(const auto* e=std::get_if<Error>(&result)){auto out=*e;out.slot=slot;return out;}
    return std::optional<ResolvedObject>{{index,std::get<ResolvedData>(result)}};
}
std::variant<bool,Error> availability(const Player& p,const DataFields& d) noexcept {
    if(p.global.field5d!=0)return Error{ErrorCode::unsupported_availability,"682c4c network context",{},{}};
    if(static_cast<int>(std::bit_cast<std::int8_t>(p.global.field2f))!=p.pbw.field33)return false;
    if(p.pbw.field371==8)return false;
    if(!d.field14c)return Error{ErrorCode::unknown_field,"D+14c",{},{}};
    if(!p.selection_pbw.field9)return Error{ErrorCode::unknown_field,"pbw+9",{},{}};
    return *d.field14c==*p.selection_pbw.field9;
}
Result query(const Player& p,const Fields& fields,const Manager& manager) noexcept {return Query(p,fields).run(manager);}
ScanResult scan(const Player& p,const Fields& fields,const Manager& manager) noexcept {return Scan(p,fields).run(manager);}
std::variant<StatusScanOutput,Error> scan_status(const Player& p,const Manager& manager) noexcept {
    return StatusScan(p).run(manager);
}
} // namespace mh4u::player_object_query
