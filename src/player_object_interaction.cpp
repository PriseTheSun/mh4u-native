#include "player_object_interaction.hpp"
#include <bit>

namespace mh4u::player_object_interaction {
namespace {
namespace fp=binary32_arithmetic;
using Word=std::uint32_t;
using Player=player_controller_step::PlayerFields;
using Fields=player_fields::Fields;
// Ordered VCMPE predicates, including unordered and both zero encodings.
// Comparing the sign/magnitude words avoids consuming host FP exception state.
bool nan(Word v) noexcept{return (v&0x7fffffffU)>0x7f800000U;}
bool less_equal(Word a,Word b) noexcept {
    if(nan(a)||nan(b))return false;
    if(((a|b)&0x7fffffffU)==0)return true;
    if((a^b)&0x80000000U)return (a&0x80000000U)!=0;
    return (a&0x80000000U)?a>=b:a<=b;
}
struct Access {
    std::optional<Error> error;
    std::optional<std::size_t> slot,object,data,index;
    void fail(ErrorCode code,std::string_view field) noexcept {
        if(!error)error=Error{code,field,slot,object,data,index,{},{}};
    }
    void fail(const oq::Error& cause) noexcept {
        ErrorCode code=ErrorCode::object_query;
        if(cause.code==oq::ErrorCode::unknown_field)code=ErrorCode::unknown_field;
        else if(cause.code==oq::ErrorCode::absent_manager||cause.code==oq::ErrorCode::absent_data)code=ErrorCode::null_binding;
        else if(cause.code==oq::ErrorCode::invalid_index)code=ErrorCode::invalid_index;
        if(!error)error=Error{code,cause.field,cause.slot,cause.object,cause.data,{},{},cause};
    }
    template<class T>T read(const std::optional<T>& value,std::string_view name) noexcept {
        if(!value){fail(ErrorCode::unknown_field,name);return T{};}return *value;
    }
    template<class I>bool known(const Link<I>& link,std::string_view name) noexcept {
        if(std::holds_alternative<oq::Unknown>(link)){fail(ErrorCode::unknown_field,name);return false;}return true;
    }
    template<class T,class I>const T* dereference(const Link<I>& link,std::span<const T> values,std::string_view name) noexcept {
        if(!known(link,name))return nullptr;
        if(std::holds_alternative<oq::Absent>(link)){fail(ErrorCode::null_binding,name);return nullptr;}
        index=std::get<I>(link).value;
        if(*index>=values.size()){fail(ErrorCode::invalid_index,name);return nullptr;}
        return &values[*index];
    }
    Word operation(fp::Result(*fn)(Word,Word),Word a,Word b,std::string_view name) noexcept {
        if(error)return 0;
        const auto result=fn(a,b);
        if(const auto* cause=std::get_if<fp::Error>(&result)){
            error=Error{ErrorCode::arithmetic,name,slot,object,data,index,*cause,{}};return 0;
        }
        return std::get<Word>(result);
    }
};
class Query:private Access {
    const Player& p;const Fields& fields;const Resources& resources;
    Output out;
    const Registry* registry() noexcept {
        if(std::holds_alternative<oq::Unknown>(resources)){fail(ErrorCode::unknown_field,"provider singleton f3a020");return nullptr;}
        const auto* binding=std::get_if<const Registry*>(&resources);
        if(!binding||!*binding){fail(ErrorCode::null_binding,"provider singleton f3a020");return nullptr;}
        return *binding;
    }
    const Record* provider(const oq::DataFields& d,const Registry*& r) noexcept {
        ++out.effects.provider_queries;
        const auto kind=read(d.field1c0,"D+1c0");if(error)return nullptr;
        // D797e is a dead load/argument, intentionally outside this projection.
        r=registry();if(error)return nullptr;
        index=kind;if(kind>=r->table.size()){fail(ErrorCode::invalid_index,"provider table kind");return nullptr;}
        const auto* entry=dereference(r->table[kind],r->entries,"provider table entry");if(error)return nullptr;
        return dereference(entry->record,r->records,"provider entry record");
    }
    bool category(const oq::DataFields& d,std::uint8_t value) noexcept {
        ++out.effects.category_queries;
        if(read(d.field1ca,"D+1ca")!=10||error)return false;
        return read(d.field1cb,"D+1cb")==value&&!error;
    }
    Word radius(const oq::DataFields& d,std::array<Word,3>& position) noexcept {
        const Registry* r{};const auto* first=provider(d,r);if(error)return 0;
        // Loading R+4c preserves a known-null pointer until its actual +30
        // dereference. Source copies XYZ and resolves937f94 before that load.
        const auto a=first->field4c;if(!known(a,"provider R+4c"))return 0;
        position[0]=read(d.position_bits[0],"D+198");if(error)return 0;
        position[1]=read(d.position_bits[1],"D+19c");if(error)return 0;
        position[2]=read(d.position_bits[2],"D+1a0");if(error)return 0;
        const auto* second=provider(d,r);if(error)return 0;
        Word factor{};
        if(std::holds_alternative<oq::Unknown>(d.field580)){fail(ErrorCode::unknown_field,"D+580");return 0;}
        if(const auto* override_index=std::get_if<oq::OverrideIndex>(&d.field580)){
            index=override_index->value;
            if(*index>=r->overrides.size()){fail(ErrorCode::invalid_index,"D+580 override index");return 0;}
            factor=read(r->overrides[*index].field_e4_bits,"override+e4");
        }else{
            const auto* parameter=dereference(second->fieldb4,r->parameters,"provider R+b4");if(error)return 0;
            factor=read(parameter->field80_bits,"provider parameter+80");
        }
        if(error)return 0;
        const auto* a_parameter=dereference(a,r->parameters,"provider R+4c");if(error)return 0;
        const auto scale=read(a_parameter->field30_bits,"provider parameter+30");if(error)return 0;
        auto result=operation(fp::mul,factor,scale,"radius product");if(error)return 0;
        const auto multiplier=read(d.field1bc_bits,"D+1bc");if(error)return 0;
        if(Word(multiplier+0xc0e66666U)>0x00e00000U)result=operation(fp::mul,result,multiplier,"D+1bc radius product");
        return result;
    }
    bool sphere(const std::array<Word,3>& position,Word radius_value) noexcept {
        ++out.effects.sphere_queries;
        // Source loads PlayerX before squaring radius; XYZ object words were
        // copied by the caller. Distances use separate VFP2 multiply/add.
        const auto px=read(fields.pbw.field10_bits,"pbw+10");if(error)return false;
        const auto radius_squared=operation(fp::mul,radius_value,radius_value,"radius squared");if(error)return false;
        const auto py=read(fields.pbw.field14_bits,"pbw+14");if(error)return false;
        const auto pz=read(fields.pbw.field18_bits,"pbw+18");if(error)return false;
        const auto dx=operation(fp::sub,px,position[0],"sphere dx");if(error)return false;
        const auto dy=operation(fp::sub,py,position[1],"sphere dy");if(error)return false;
        const auto dz=operation(fp::sub,pz,position[2],"sphere dz");if(error)return false;
        auto sum=operation(fp::mul,dx,dx,"sphere dx squared");if(error)return false;
        const auto yy=operation(fp::mul,dy,dy,"sphere dy squared");if(error)return false;
        sum=operation(fp::add,sum,yy,"sphere xy squared");if(error)return false;
        const auto zz=operation(fp::mul,dz,dz,"sphere dz squared");if(error)return false;
        sum=operation(fp::add,sum,zz,"sphere xyz squared");
        return !error&&less_equal(sum,radius_squared);
    }
    bool accepted(const oq::DataFields& d) noexcept {
        ++out.effects.enabled_queries;
        if(read(d.field140,"D+140")==0||error)return false;
        if(!(read(d.field1b4,"D+1b4")&1U)||error)return false;
        const auto area=read(d.field14c,"D+14c");if(error)return false;
        if(area!=read(p.selection_pbw.field9,"pbw+9")||error)return false;
        const auto gate=read(d.field78b8_bits,"D+78b8");if(error||less_equal(gate,0))return false;
        if(read(d.field78e5,"D+78e5")!=0||error)return false;
        if(read(d.field278,"D+278")!=1||error)return false;
        if(read(d.field27c,"D+27c")!=p.pbw.field33||error)return false;
        if(!category(d,187)){
            if(error||!category(d,191))return false;
        }
        std::array<Word,3> position{};const auto value=radius(d,position);if(error)return false;
        return sphere(position,value);
    }
public:
    Query(const Player& player,const Fields& extra,const Resources& providers):p(player),fields(extra),resources(providers){}
    Result run(const oq::Manager& manager) noexcept {
        const auto collection=oq::resolve_manager(manager);
        if(const auto* e=std::get_if<oq::Error>(&collection)){fail(*e);return *error;}
        const auto& c=*std::get<const oq::Collection*>(collection);
        for(std::size_t i=0;i<c.slots.size();++i){
            ++out.effects.slots_examined;slot=i;object.reset();data.reset();index.reset();
            const auto resolved=oq::resolve_slot(c,i);
            if(const auto* e=std::get_if<oq::Error>(&resolved)){fail(*e);return *error;}
            const auto& item=std::get<std::optional<oq::ResolvedObject>>(resolved);if(!item)continue;
            object=item->index.value;data=item->data.index.value;
            if(accepted(*item->data.fields)){out.selected=item->index;return out;}
            if(error)return *error;
        }
        return out;
    }
};
}
Result query(const Player& p,const Fields& fields,const oq::Manager& manager,const Resources& resources) noexcept {
    return Query(p,fields,resources).run(manager);
}
} // namespace mh4u::player_object_interaction
