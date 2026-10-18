#include "item_pop_update.hpp"
#include "source_rng.hpp"
#include <algorithm>
#include <bit>
#include <new>
#include <stdexcept>

namespace mh4u::item_pop_update {
namespace {
namespace wl=player_world_lookup;
namespace fp=binary32_arithmetic;
struct Failure {Error error;};
struct Update {
    const wl::Manager& expected;
    const Context& context;
    Output out;
    std::size_t work{};
    std::optional<std::size_t> current;
    [[noreturn]] void fail(ErrorCode code,std::string_view field)const {
        throw Failure{{code,field,current,{},{},{}}};
    }
    template<class T>T read(std::optional<T> value,std::string_view field)const {
        if(!value)fail(ErrorCode::unknown_field,field);
        return *value;
    }
    Word arithmetic(fp::Result result,std::string_view field)const {
        if(const auto* e=std::get_if<fp::Error>(&result))
            throw Failure{{ErrorCode::arithmetic,field,current,*e,{},{}}};
        return std::get<Word>(result);
    }
    Word delta()const{return read(context.clock.field5c_bits,"clock5c");}
    static bool nan(Word bits){return (bits&0x7fffffffU)>0x7f800000U;}
    static bool positive(Word bits){return !nan(bits)&&(bits>>31)==0&&(bits&0x7fffffffU)!=0;}
    static bool ordered_nonpositive(Word bits){return !nan(bits)&&!positive(bits);}
    void absent(std::optional<wl::AuxiliaryPresence> value,std::string_view field)const {
        if(read(value,field)!=wl::AuxiliaryPresence::absent)
            fail(ErrorCode::unsupported_auxiliary,field);
    }
    std::optional<std::size_t> link(const wl::Link& value,std::string_view field)const {
        if(std::holds_alternative<wl::Absent>(value))return {};
        const auto* index=std::get_if<wl::Index>(&value);
        if(!index)fail(ErrorCode::unknown_field,field);
        if(index->value>=wl::Manager::capacity)fail(ErrorCode::invalid_link,field);
        return index->value;
    }
    void state1(wl::Record& r) {
        const auto fade=read(r.field58_bits,"record58");
        if(std::bit_cast<std::int32_t>(fade)<0x3f800000){
            const auto duration=read(r.field5c_bits,"record5c");
            if(!positive(duration))r.field58_bits=0x3f800000;
            else{
                const auto ratio=arithmetic(fp::div(delta(),duration),"delta/record5c");
                const auto sum=arithmetic(fp::add(ratio,fade),"ratio+fade");
                // Both original clamps compare raw words as signed integers.
                r.field58_bits=std::bit_cast<std::int32_t>(sum)<=0x3f800000?sum:Word(0x3f800000);
            }
        }
        const auto kind=read(r.field2e,"record2e");
        if(kind>=5&&kind<=8)fail(ErrorCode::unsupported_kind,"record2e/state1");
        if(read(r.field79,"record79")!=0)fail(ErrorCode::unsupported_spatial,"record79");
        const auto life=read(r.field1c_bits,"record1c");
        const auto expire=[&]{
            const auto duration=read(r.field5c_bits,"record5c");
            r.field19=2;r.field1c_bits=duration;
        };
        if(positive(life)){
            const auto left=arithmetic(fp::sub(life,delta()),"record1c-delta");
            r.field1c_bits=left;
            // BHI keeps positive OR unordered results; only ordered<=0 expires.
            if(ordered_nonpositive(left))expire();
        }
        if(read(r.field30,"record30")==0)expire();
    }
    void state2(wl::Record& r) {
        const auto life=read(r.field1c_bits,"record1c");
        const auto left=arithmetic(fp::sub(life,delta()),"record1c-delta");
        r.field1c_bits=left;
        if(positive(left)){
            const auto duration=read(r.field5c_bits,"record5c");
            absent(r.auxiliary14,"record14");
            r.field58_bits=arithmetic(fp::div(left,duration),"record1c/record5c");
            return;
        }
        const auto kind=read(r.field2e,"record2e");r.field58_bits=0;
        if(kind==2){r.field1c_bits=0x4628c000;r.field19=3;return;}
        if(kind==1){
            const auto flags=read(r.field56,"record56");
            if(flags&0x1c){
                r.field1c_bits=(flags&0x20)?Word(0x46a8c000):Word(0x4628c000);
                r.field19=5;return;
            }
        }
        if(read(r.field88,"record88")!=0)fail(ErrorCode::unsupported_callback,"record88");
        r.field19=4;
    }
    void charge() {
        if(work==context.limits.records)fail(ErrorCode::resource_limit,"update work");
        ++work;
    }
    void manager()const {
        if(auto e=item_pop_secondary::validate_manager(expected,context.environment.singleton_manager))
            throw Failure{{ErrorCode::dependency,e->field,current,{},*e,{}}};
    }
    const item_pop_resources::Resource& resource(item_pop_resources::Kind kind)const {
        const auto& binding=kind==item_pop_resources::Kind::set?context.environment.set:context.environment.list;
        auto result=item_pop_secondary::resolve_resource(binding,kind);
        if(const auto* e=std::get_if<item_pop_secondary::Error>(&result))
            throw Failure{{ErrorCode::dependency,e->field,current,{},*e,{}}};
        return *std::get<const item_pop_resources::Resource*>(result);
    }
    template<class T>T require(item_pop_resources::Result<T> result)const {
        if(const auto* e=std::get_if<item_pop_resources::Error>(&result))
            throw Failure{{ErrorCode::resource_failure,"resource",current,{},{},*e}};
        return std::get<T>(std::move(result));
    }
    bool renewal_wait(wl::Record& r) {
        const auto life=read(r.field1c_bits,"record1c");
        const auto dt=delta();
        const auto left=arithmetic(fp::sub(life,dt),"record1c-delta");
        r.field1c_bits=left;
        return !ordered_nonpositive(left); // BHI keeps positive OR unordered.
    }
    void state3(wl::Record& r) {
        if(renewal_wait(r))return;
        const auto seed=read(out.global.field20,"global20");
        out.global.field20=source_rng::advance_seed(seed);++out.effects.rng_calls[0];
        if(*out.global.field20%100>=50){r.field1c_bits=0x4628c000;return;}
        auto allowance=context.limits;allowance.records-=work;
        auto result=item_pop_secondary::initialize(r,out.global,expected,context.environment,allowance);
        if(const auto* e=std::get_if<item_pop_secondary::Error>(&result))
            throw Failure{{ErrorCode::secondary_failure,e->field,current,{},*e,{}}};
        auto renewed=std::get<item_pop_secondary::Output>(std::move(result));
        work+=renewed.work;out.effects.rng_calls[1]+=renewed.rng_calls;
        r=std::move(renewed.record);out.global=std::move(renewed.global);
        if(renewed.initialized){r.field19=1;++out.effects.secondary_renewed;}
        else{r.field19=4;++out.effects.secondary_failed;}
    }
    void state4(wl::Record& r) {
        if(read(r.field4,"record4")==0)return; // Loaded singleton/links are dead here.
        manager();
        // Snapshot both source links before any aliasing writes, as5b9d84 does.
        const auto original_next=r.next,original_previous=r.previous;
        const auto head=link(out.manager.head,"head");
        const auto next=link(original_next,"next"),previous=link(original_previous,"previous");
        if(head==current)out.manager.head=original_next;
        if(next){out.manager.records[*next].previous=original_previous;r.next=wl::Absent{};}
        if(previous){out.manager.records[*previous].next=original_next;r.previous=wl::Absent{};}
        r.field4=0;
        absent(r.auxiliary10,"record10");absent(r.auxiliary14,"record14");
        r.auxiliary10=wl::AuxiliaryPresence::absent;r.auxiliary14=wl::AuxiliaryPresence::absent;
        out.manager.field4a20=read(out.manager.field4a20,"manager4a20")-Word(1);
        ++out.effects.removed;
    }
    static bool equal_float(Word a,Word b) {
        if(nan(a)||nan(b))return false;
        return a==b||((a&0x7fffffffU)==0&&(b&0x7fffffffU)==0);
    }
    static std::uint16_t signed_byte(std::uint8_t b) {
        return static_cast<std::uint16_t>(b<128?Word(b):Word(b)|0xff00U);
    }
    void state5(wl::Record& r) {
        if(renewal_wait(r))return;
        const auto group=read(r.field2f,"record2f");manager();
        const auto& set=resource(item_pop_resources::Kind::set);
        // Invalid source group leaves caller stack outputs uninitialized; the
        // immutable resource owner reports that boundary, never an empty group.
        const auto primary=require(set.group(group)).primary;
        for(const auto& p:primary){
            charge();const auto& list=resource(item_pop_resources::Kind::list);
            if(!require(list.member(0,p.member_word&255)))continue;
            if(p.bytes1c[4]!=read(r.field56,"record56"))continue;
            bool matches=true;
            for(unsigned k=0;k<3;++k)if(!equal_float(p.position_bits[k],read(r.fields34[k],"record34/38/3c"))){matches=false;break;}
            if(!matches)continue;
            r.field30=signed_byte(p.bytes1c[0]);r.field32=signed_byte(p.bytes1c[1]);
        }
        r.field19=1;++out.effects.primary_renewed; // Last matching entry wins.
    }
    void record(wl::Record& r) {
        if(read(r.update_binding,"record.virtual18")!=wl::RecordUpdateBinding::item_pop_5aa4cc)
            fail(ErrorCode::unsupported_virtual,"record.virtual18");
        absent(r.auxiliary10,"record10");absent(r.auxiliary14,"record14");
        const auto transform=read(r.field63,"record63");
        if(transform==1||transform==2)fail(ErrorCode::unsupported_transform,"record63");
        const auto state=read(r.field19,"record19");
        switch(state){
        case 0:{
            const auto timer=read(r.field20_bits,"record20");
            const auto left=arithmetic(fp::sub(timer,delta()),"record20-delta");
            r.field20_bits=left;
            if(positive(left))return;
            if(read(r.field2e,"record2e")==7)fail(ErrorCode::unsupported_kind,"record2e/state0");
            r.field20_bits=0;r.field19=1;
            state1(r);return;
        }
        case 1:state1(r);return;
        case 2:state2(r);return;
        case 3:state3(r);return;
        case 4:state4(r);return;
        case 5:state5(r);return;
        case 6:r.field58_bits=0;return;
        default:return;
        }
    }
    Output run() {
        if(read(context.clock.field50,"clock50")==0&&read(context.clock.field57,"clock57")!=0){
            out.clock_gated=true;return std::move(out);
        }
        if(read(context.category_e250,"category_e250")!=0)
            fail(ErrorCode::unsupported_category_helper,"category_e250");
        auto next=link(out.manager.head,"head");
        std::array<bool,wl::Manager::capacity> seen{};
        while(next){
            current=*next;
            if(seen[*current])fail(ErrorCode::cycle,"next");
            seen[*current]=true;charge();++out.visited;
            auto& item=out.manager.records[*current];record(item);
            next=link(item.next,"next"); // Source reads this after the virtual call.
        }
        return std::move(out);
    }
};
} // namespace
Result apply(const wl::Manager& manager,const player_transition::GlobalFields& global,const Context& context) {
    try{
        // Representation policy includes candidate identities and a temporary
        // secondary result; resource payloads remain owned by immutable Resource.
        std::size_t remaining=context.limits.representation_bytes;
        constexpr auto fixed=sizeof(Output)+sizeof(item_pop_secondary::Output)+4*65;
        if(fixed>remaining)return Error{ErrorCode::resource_limit,"update output",{},{},{},{}};
        remaining-=fixed;
        std::size_t largest_identity=0;
        for(const auto& r:manager.records){
            std::size_t identity=0;
            if(r.field28)if(const auto* ref=std::get_if<wl::ResourceReference>(&*r.field28))identity=ref->sha256.size();
            // Reserve every potentially renewed identity and its terminator.
            const auto payload=std::max<std::size_t>(identity,64);
            if(payload>=remaining)return Error{ErrorCode::resource_limit,"record28 identity",{},{},{},{}};
            remaining-=payload+1;largest_identity=std::max(largest_identity,identity);
        }
        // Secondary initialization temporarily copies one existing record.
        if(largest_identity>remaining)return Error{ErrorCode::resource_limit,"secondary identity",{},{},{},{}};
        return Update{manager,context,Output{manager,global,0,false,{}},0,{}}.run();
    }catch(const Failure& failure){return failure.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::capacity,"manager copy",{},{},{},{}};}
    catch(const std::length_error&){return Error{ErrorCode::capacity,"manager copy",{},{},{},{}};}
}
} // namespace mh4u::item_pop_update
