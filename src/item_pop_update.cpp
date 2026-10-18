#include "item_pop_update.hpp"
#include <bit>
#include <new>
#include <stdexcept>

namespace mh4u::item_pop_update {
namespace {
namespace wl=player_world_lookup;
namespace fp=binary32_arithmetic;
struct Failure {Error error;};
struct Update {
    const Context& context;
    Output out;
    std::optional<std::size_t> current;
    [[noreturn]] void fail(ErrorCode code,std::string_view field)const {
        throw Failure{{code,field,current,{}}};
    }
    template<class T>T read(std::optional<T> value,std::string_view field)const {
        if(!value)fail(ErrorCode::unknown_field,field);
        return *value;
    }
    Word arithmetic(fp::Result result,std::string_view field)const {
        if(const auto* e=std::get_if<fp::Error>(&result))
            throw Failure{{ErrorCode::arithmetic,field,current,*e}};
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
        case 3:case 4:case 5:fail(ErrorCode::unsupported_state,"record19");
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
            seen[*current]=true;++out.visited;
            auto& item=out.manager.records[*current];record(item);
            next=link(item.next,"next"); // Source reads this after the virtual call.
        }
        return std::move(out);
    }
};
} // namespace
Result apply(const wl::Manager& manager,const Context& context) {
    try{return Update{context,Output{manager,0,false},{}}.run();}
    catch(const Failure& failure){return failure.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::capacity,"manager copy",{},{}};}
    catch(const std::length_error&){return Error{ErrorCode::capacity,"manager copy",{},{}};}
}
} // namespace mh4u::item_pop_update
