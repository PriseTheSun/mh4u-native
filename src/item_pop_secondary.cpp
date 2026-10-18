#include "item_pop_secondary.hpp"
#include "source_rng.hpp"
#include <new>
#include <stdexcept>

namespace mh4u::item_pop_secondary {
namespace wl=player_world_lookup;
namespace ip=item_pop_resources;
std::optional<Error> validate_manager(const wl::Manager& expected,const Binding<wl::Manager>& binding) {
    if(std::holds_alternative<wl::Unknown>(binding))return Error{ErrorCode::unknown_field,"singleton_manager",{}, {}};
    if(std::holds_alternative<wl::Absent>(binding))return Error{ErrorCode::null_binding,"singleton_manager",{}, {}};
    const auto* ptr=std::get<const wl::Manager*>(binding);
    if(!ptr)return Error{ErrorCode::null_binding,"singleton_manager",{}, {}};
    if(ptr!=&expected)return Error{ErrorCode::foreign_manager,"singleton_manager",{}, {}};
    return {};
}
Result<const Resource*> resolve_resource(const Binding<Resource>& binding,ip::Kind kind) {
    const auto field=kind==ip::Kind::set?"manager14/Set":"manager18/List";
    if(std::holds_alternative<wl::Unknown>(binding))return Error{ErrorCode::unknown_field,field,{}, {}};
    if(std::holds_alternative<wl::Absent>(binding))return Error{ErrorCode::null_binding,field,{}, {}};
    const auto* ptr=std::get<const Resource*>(binding);
    if(!ptr)return Error{ErrorCode::null_binding,field,{}, {}};
    if(ptr->kind()!=kind)return Error{ErrorCode::wrong_kind,field,{}, {}};
    return ptr;
}
namespace {
struct Failure {Error error;};
struct Initialize {
    const wl::Manager& expected;
    const Environment& environment;
    Limits limits;
    Output out;
    [[noreturn]] void fail(ErrorCode code,std::string_view field)const{throw Failure{{code,field,{},{}}};}
    template<class T>T read(std::optional<T> value,std::string_view field)const {
        if(!value)fail(ErrorCode::unknown_field,field);
        return *value;
    }
    template<class T>T require(ip::Result<T> result)const {
        if(const auto* e=std::get_if<ip::Error>(&result))throw Failure{{ErrorCode::resource_failure,"resource",*e,{}}};
        return std::get<T>(std::move(result));
    }
    const Resource& resource(const Binding<Resource>& binding,ip::Kind kind)const {
        auto result=resolve_resource(binding,kind);
        if(const auto* e=std::get_if<Error>(&result))throw Failure{*e};
        return *std::get<const Resource*>(result);
    }
    void charge() {
        if(out.work==limits.records)fail(ErrorCode::resource_limit,"secondary work");
        ++out.work;
    }
    Word random() {
        out.global.field22=source_rng::advance_seed(out.global.field22);
        ++out.rng_calls;return out.global.field22;
    }
    static std::uint16_t signed_byte(std::uint8_t b){return static_cast<std::uint16_t>(b<128?Word(b):Word(b)|0xff00U);}
    static Location location(const auto& record){return {record.source_offset,record.buffer_offset};}
    Output run() {
        auto& r=out.record;
        const auto group_id=read(r.field2f,"record2f");
        const auto ordinal=read(r.field57,"record57");
        const auto mapped=player_selection::remap_area(group_id,environment.area);
        if(const auto* e=std::get_if<player_selection::Error>(&mapped))throw Failure{{ErrorCode::selection_failure,"area remap",{},*e}};
        const auto group=std::get<Word>(mapped);out.selection.remapped_group=group;
        if(auto e=validate_manager(expected,environment.singleton_manager))throw Failure{*e};
        const auto& set=resource(environment.set,ip::Kind::set);
        out.selection.set_sha256=std::string(set.sha256());
        if(group>=set.group_count())return std::move(out);
        const auto g=require(set.group(group));
        if(ordinal>=g.positions.size()||ordinal>=g.choices.size())
            fail(ErrorCode::unsupported_descriptor,"Remapped ordinal escapes admitted descriptor table");
        const auto positions=g.positions[ordinal].records;
        const auto choices=g.choices[ordinal].records;
        if(positions.empty()||choices.empty())return std::move(out);
        const auto pi=random()%static_cast<Word>(positions.size());
        out.selection.position_index=pi;out.selection.position=location(positions[pi]);
        Word total=0;for(const auto& c:choices){charge();total+=c.weight;}
        if(total==0)return std::move(out);
        const Word remainder=random()%total;
        Word cumulative=0;std::size_t ci=0;
        for(;ci<choices.size();++ci){charge();cumulative+=choices[ci].weight;if(cumulative>=remainder)break;}
        // The final cumulative value is total modulo2^32, and total>remainder.
        const auto& choice=choices[ci];out.selection.choice_index=static_cast<Word>(ci);
        out.selection.choice=location(choice);
        const auto& list=resource(environment.list,ip::Kind::list);
        const auto member=require(list.member(1,choice.member));
        if(!member){++out.missing_members;return std::move(out);}
        const auto& p=positions[pi];r.field1c_bits=0x4628c000;
        for(unsigned k=0;k<3;++k)r.fields34[k]=p.position_bits[k];
        r.field4c_bits=p.radius_bits;r.field50_bits=p.height_bits;
        r.fields40={Word(p.words14[0]),Word(p.words14[1]),Word(0)};
        r.field56=p.bytes18[0];r.field28=wl::Field28{member->reference};
        r.field30=signed_byte(choice.counts[0]);r.field32=signed_byte(choice.counts[1]);
        r.field54=choice.flags[0];r.field55=choice.flags[1];
        if(read(r.auxiliary10,"record10")!=wl::AuxiliaryPresence::absent)
            fail(ErrorCode::unsupported_auxiliary,"record10");
        out.initialized=true;return std::move(out);
    }
};
}
Result<Output> initialize(const wl::Record& record,const player_transition::GlobalFields& global,
    const wl::Manager& expected,const Environment& environment,Limits limits) {
    try{
        // Bound record/string copies before allocation; source resources stay shared.
        std::size_t remaining=limits.representation_bytes;
        constexpr std::size_t fixed=sizeof(Output)+3*65;
        if(fixed>remaining)return Error{ErrorCode::resource_limit,"secondary output",{}, {}};
        remaining-=fixed;
        if(record.field28)if(const auto* ref=std::get_if<wl::ResourceReference>(&*record.field28))
            if(ref->sha256.size()>=remaining)return Error{ErrorCode::resource_limit,"record28 identity",{}, {}};
        return Initialize{expected,environment,limits,Output{record,global,{},{},{},{},{}}}.run();
    }catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"secondary allocation",{}, {}};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"secondary capacity",{}, {}};}
}
} // namespace mh4u::item_pop_secondary
