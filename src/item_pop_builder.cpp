#include "item_pop_builder.hpp"
#include <new>
#include <stdexcept>

namespace mh4u::item_pop_builder {
namespace {
namespace wl=player_world_lookup;
namespace ip=item_pop_resources;
struct Failure {Error error;};
struct Builder {
    const Context& context;
    Limits limits;
    Built out;
    std::size_t work{};
    std::optional<Word> group_index,record_index;
    [[noreturn]] void fail(ErrorCode code,std::string_view message)const {
        throw Failure{Error{code,message,group_index,record_index,{},{},{}}};
    }
    template<class T>T require(ip::Result<T> result)const {
        if(const auto* e=std::get_if<ip::Error>(&result))
            throw Failure{Error{ErrorCode::resource_failure,"Resource lookup rejected",
                group_index,record_index,*e,{},{}}};
        return std::get<T>(std::move(result));
    }
    void charge(std::size_t n) {
        if(n>limits.records-work)fail(ErrorCode::resource_limit,"Builder work policy exceeded");
        work+=n;
    }
    void reset(wl::Record& r)const {
        r.next=wl::Absent{};r.previous=wl::Absent{};
        r.field4=0;r.field19=0;r.field1a=0;r.field2e=0;r.field2f=0;
        r.field54=0;r.field55=0;r.field56=0;r.field60=0;r.field61=0;
        r.field62=0;r.field63=0;r.field78=0;r.field79=0;
        r.field57=255;r.field64=255;
        r.field30=0;r.field32=0;r.field2c=65535;
        r.field1c_bits=0xbf800000;r.field20_bits=0;r.field24=0;
        r.field28=wl::Field28{Word(0)};
        for(unsigned k=0;k<3;++k){
            r.fields34[k]=context.position_template[k];r.fields40[k]=0;
            r.fields68[k]=context.auxiliary_template[k];
            r.fields7c[k]=context.auxiliary_template[k];
        }
        r.field4c_bits=0;r.field50_bits=0;r.field58_bits=0x3f800000;
        r.field5c_bits=0x41a00000;r.field74=0;r.field88=0;
        r.fields8c={Word(0),Word(0)}; // Source literals e069a0/e069a4.
    }
    std::optional<std::size_t> allocate() {
        // After fresh reset, even a failed initializer consumes the next slot.
        if(out.allocated==wl::Manager::capacity){++out.capacity_skips;return {};}
        const auto index=out.allocated++;
        auto& r=out.manager.records[index];reset(r);r.field4=1;
        const Word counter=*out.manager.field4a24;
        r.field2c=static_cast<std::uint16_t>(counter&~Word(0x8000));
        out.manager.field4a24=counter+1;
        return index;
    }
    void link(std::size_t index) {
        auto& r=out.manager.records[index];
        if(const auto* head=std::get_if<wl::Index>(&out.manager.head))
            out.manager.records[head->value].previous=wl::Index{index};
        r.next=out.manager.head;r.previous=wl::Absent{};
        out.manager.head=wl::Index{index};++*out.manager.field4a20;
    }
    static std::uint16_t signed_byte(std::uint8_t b) {
        return static_cast<std::uint16_t>(b<128?Word(b):Word(b)|0xff00U);
    }
    static item_pop_secondary::Location location(const auto& record){return {record.source_offset,record.buffer_offset};}
    bool secondary(const Resource& set,const Resource& list,std::size_t index) {
        auto allowance=limits;allowance.records-=work;
        item_pop_secondary::Environment environment{&out.manager,&set,&list,context.area};
        auto result=item_pop_secondary::initialize(out.manager.records[index],out.global,out.manager,environment,allowance);
        if(const auto* e=std::get_if<item_pop_secondary::Error>(&result)){
            // Translate the existing builder contract, preserving the producing cause.
            auto code=ErrorCode::secondary_failure;
            switch(e->code){
            case item_pop_secondary::ErrorCode::resource_limit:code=ErrorCode::resource_limit;break;
            case item_pop_secondary::ErrorCode::resource_failure:code=ErrorCode::resource_failure;break;
            case item_pop_secondary::ErrorCode::selection_failure:code=ErrorCode::selection_failure;break;
            case item_pop_secondary::ErrorCode::unsupported_descriptor:code=ErrorCode::unsupported_descriptor;break;
            default:break;
            }
            throw Failure{Error{code,e->field,group_index,record_index,e->resource,e->selection,*e}};
        }
        auto initialized=std::get<item_pop_secondary::Output>(std::move(result));
        work+=initialized.work;out.rng_calls+=initialized.rng_calls;
        out.missing_members+=initialized.missing_members;
        out.manager.records[index]=std::move(initialized.record);out.global=std::move(initialized.global);
        out.sources[index]->secondary=std::move(initialized.selection);
        return initialized.initialized;
    }
    Built run(const Resource* set,const Resource* list) {
        // Includes both per-slot identities, one temporary Member and the
        // terminating bytes. No source/resource storage is copied into Built.
        constexpr auto output_budget=sizeof(Built)+sizeof(item_pop_secondary::Output)+(wl::Manager::capacity*2+4)*65;
        if(output_budget>limits.representation_bytes)
            fail(ErrorCode::resource_limit,"Builder output exceeds representation policy");
        out.global=context.global;out.manager.head=wl::Absent{};
        out.manager.field4a20=0;out.manager.field4a24=context.id_counter;
        for(auto& r:out.manager.records){
            // Native fresh construction establishes the class and absent
            // auxiliaries, matching5ab458. They are not stores of5ba7b4/reset.
            r.update_binding=wl::RecordUpdateBinding::item_pop_5aa4cc;
            r.auxiliary10=wl::AuxiliaryPresence::absent;
            r.auxiliary14=wl::AuxiliaryPresence::absent;
            reset(r);
        }
        if(!set)return std::move(out);
        if(!list)return std::move(out);
        if(set->kind()!=ip::Kind::set)fail(ErrorCode::wrong_kind,"Expected Set resource");
        if(list->kind()!=ip::Kind::list)fail(ErrorCode::wrong_kind,"Expected List resource");
        charge(set->group_count());
        for(Word g=0;g<set->group_count();++g){
            group_index=g;record_index.reset();const Word source_group=g&255;
            const auto group=require(set->group(source_group));
            for(std::size_t i=0;i<group.primary.size();++i){
                record_index=static_cast<Word>(i);charge(1);
                const auto& p=group.primary[i];
                const auto member=require(list->member(0,p.member_word&255));
                if(!member){++out.missing_members;continue;}
                const auto allocated=allocate();if(!allocated)continue;
                const auto index=*allocated;auto& r=out.manager.records[index];
                out.sources[index]=SlotSource{g,source_group,*record_index,PrimarySource{location(p),std::string(set->sha256())},{}};
                r.field2e=1;r.field2f=static_cast<std::uint8_t>(g);
                r.field30=signed_byte(p.bytes1c[0]);r.field32=signed_byte(p.bytes1c[1]);
                for(unsigned k=0;k<3;++k)r.fields34[k]=p.position_bits[k];
                r.fields40={Word(p.words18[0]),Word(p.words18[1]),Word(0)};
                r.field4c_bits=p.radius_bits;r.field50_bits=p.height_bits;
                r.field54=p.bytes1c[2];r.field55=p.bytes1c[3];r.field56=p.bytes1c[4];
                r.field28=wl::Field28{member->reference};link(index);
            }
            for(Word i=0;i<group.secondary_count;++i){
                record_index=i;charge(1);const auto allocated=allocate();if(!allocated)continue;
                const auto index=*allocated;auto& r=out.manager.records[index];
                out.sources[index]=SlotSource{g,source_group,i,{},{}};
                r.field2e=2;r.field2f=static_cast<std::uint8_t>(g);r.field57=static_cast<std::uint8_t>(i);
                if(secondary(*set,*list,index))link(index);else ++out.failed_secondary;
            }
        }
        return std::move(out);
    }
};
} // namespace
Result build_fresh(const Resource* set,const Resource* list,const Context& context,Limits limits) {
    try{return Builder{context,limits,{},{},{},{}}.run(set,list);}
    catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate builder output",{},{},{},{},{}};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Builder output exceeds host capacity",{},{},{},{},{}};}
}
} // namespace mh4u::item_pop_builder
