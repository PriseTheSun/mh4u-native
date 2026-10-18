#include "item_pop_builder.hpp"
#include "source_rng.hpp"
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
        throw Failure{Error{code,message,group_index,record_index,{},{}}};
    }
    template<class T>T require(ip::Result<T> result)const {
        if(const auto* e=std::get_if<ip::Error>(&result))
            throw Failure{Error{ErrorCode::resource_failure,"Resource lookup rejected",
                group_index,record_index,*e,{}}};
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
    Word random() {
        out.global.field22=source_rng::advance_seed(out.global.field22);
        ++out.rng_calls;return out.global.field22;
    }
    static std::uint16_t signed_byte(std::uint8_t b) {
        return static_cast<std::uint16_t>(b<128?Word(b):Word(b)|0xff00U);
    }
    static Word read(std::span<const std::byte> raw,unsigned offset,unsigned size) {
        Word v=0;for(unsigned k=0;k<size;++k)v|=Word(std::to_integer<std::uint8_t>(raw[offset+k]))<<(8*k);
        return v;
    }
    static Location location(const auto& record){return {record.source_offset,record.buffer_offset};}
    bool secondary(const Resource& set,const Resource& list,std::size_t index) {
        auto& r=out.manager.records[index];auto& source=*out.sources[index];
        const auto mapped=player_selection::remap_area(r.field2f,context.area);
        if(const auto* e=std::get_if<player_selection::Error>(&mapped))
            throw Failure{Error{ErrorCode::selection_failure,"Area remap rejected",
                group_index,record_index,{},*e}};
        const auto group=std::get<Word>(mapped);source.remapped_group=group;
        if(group>=set.group_count())return false; // Both original getters return null.
        const auto g=require(set.group(group));const auto ordinal=*r.field57;
        // Original getters do not check this bound. Do not convert an unknown
        // read past an admitted descriptor table into the source's null branch.
        if(ordinal>=g.positions.size()||ordinal>=g.choices.size())
            fail(ErrorCode::unsupported_descriptor,"Remapped ordinal escapes admitted descriptor table");
        const auto positions=g.positions[ordinal].records;
        const auto choices=g.choices[ordinal].records;
        if(positions.empty()||choices.empty())return false;
        const auto pi=random()%static_cast<Word>(positions.size());source.position_index=pi;
        source.position=location(positions[pi]);
        Word total=0;for(const auto& c:choices){charge(1);total+=read(c.raw,0,1);}
        if(total==0)return false;
        const Word remainder=random()%total;
        Word cumulative=0;std::size_t ci=0;
        for(;ci<choices.size();++ci){charge(1);cumulative+=read(choices[ci].raw,0,1);if(cumulative>=remainder)break;}
        // With an immutable snapshot and total!=0, the last cumulative value
        // equals total modulo2^32 and must exceed remainder, even after wrap.
        const auto& choice=choices[ci];source.choice_index=static_cast<Word>(ci);
        source.choice=location(choice);
        const auto member=require(list.member(1,read(choice.raw,1,1)));
        if(!member){++out.missing_members;return false;}
        const auto& p=positions[pi];r.field1c_bits=0x4628c000;
        for(unsigned k=0;k<3;++k)r.fields34[k]=read(p.raw,4*k,4);
        r.field4c_bits=read(p.raw,12,4);r.field50_bits=read(p.raw,16,4);
        r.fields40={read(p.raw,20,2),read(p.raw,22,2),Word(0)};
        r.field56=static_cast<std::uint8_t>(read(p.raw,24,1));
        r.field28=wl::Field28{member->reference};
        r.field30=signed_byte(static_cast<std::uint8_t>(read(choice.raw,2,1)));
        r.field32=signed_byte(static_cast<std::uint8_t>(read(choice.raw,3,1)));
        r.field54=static_cast<std::uint8_t>(read(choice.raw,4,1));
        r.field55=static_cast<std::uint8_t>(read(choice.raw,5,1));return true;
    }
    Built run(const Resource* set,const Resource* list) {
        // Includes both per-slot identities, one temporary Member and the
        // terminating bytes. No source/resource storage is copied into Built.
        constexpr auto output_budget=sizeof(Built)+(wl::Manager::capacity*2+1)*65;
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
                out.sources[index]=SlotSource{g,source_group,*record_index,{},{},{},location(p),{},{},std::string(set->sha256())};
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
                out.sources[index]=SlotSource{g,source_group,i,{},{},{},{},{},{},std::string(set->sha256())};
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
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate builder output",{},{},{},{}};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Builder output exceeds host capacity",{},{},{},{}};}
}
} // namespace mh4u::item_pop_builder
