#include "player_input_filter.hpp"

namespace mh4u::player_input_filter {
std::variant<bool,Error> query_manager(const Manager& manager,ManagerQuery query) noexcept {
    if(!manager.ad24)return Error{ErrorCode::unknown_field,"contextAD24"};
    const auto value=*manager.ad24;
    return value>=(query==ManagerQuery::pre_action?3:4) && value<=7;
}
namespace {
constexpr std::size_t base=player_input_storage::first;
// VCMPE/BGT and VCMPE/BLE, under the nontrapping gradual binary32
// profile: NaNs are unordered and neither signed zero is positive.
constexpr bool ordered_positive(std::uint32_t bits) noexcept {
    return bits!=0 && bits<=0x7f800000U;
}
struct Evaluation {
    Block block;const Facts& facts;const Context& context;
    Effects effects{};std::optional<Error> error{};
    template<class T>T read(std::optional<T> value,std::string_view field) noexcept {
        if(value)return *value;
        if(!error)error=Error{ErrorCode::unknown_field,field};
        // Evaluation remains private after failure; finish never publishes
        // speculative writes or effects from an unknown input.
        return {};
    }
    void zero(std::size_t offset,std::size_t width) noexcept {
        for(std::size_t i=0;i<width;++i)block[offset-base+i]=0;
    }
    template<class T>void store(std::size_t offset,T value) noexcept {
        for(std::size_t i=0;i<sizeof(T);++i)block[offset-base+i]=std::uint8_t(value>>(8*i));
    }
    std::uint32_t word3cc() noexcept {
        std::uint32_t value=0;
        for(unsigned i=0;i<4;++i)value|=std::uint32_t(read(block[0x3cc-base+i],"PBW3cc"))<<(8*i);
        return value;
    }
    bool local() noexcept {
        player_transition::GlobalFields global{};player_transition::PbwFields pbw{};
        global.field2b=read(facts.global2b,"global2b");
        if(global.field2b==2){
            pbw.field30=read(facts.pbw30,"PBW30");
            if(pbw.field30!=3)pbw.field33=read(facts.pbw33,"PBW33");
        }else{
            global.field2f=read(facts.global2f,"global2f");
            pbw.field33=read(facts.pbw33,"PBW33");
        }
        return player_transition::local_control(global,pbw);
    }
    bool external() noexcept {
        if(read(facts.global2b,"global2b")==2){
            if(std::holds_alternative<Absent>(context.mode2))return false;
            const auto* object=std::get_if<Mode2Object>(&context.mode2);
            if(!object){if(!error)error=Error{ErrorCode::unknown_field,"edbe90 presence"};return false;}
            if(read(object->field984c,"edbe90+984c")!=0)return true;
            return read(object->field982f,"edbe90+982f")!=0;
        }
        if(read(facts.globald72,"globald72")!=0)return true;
        // Both LDRSB instructions precede the OR, even when +20 is nonzero.
        const auto first=read(context.object20,"fb5edc+20");
        const auto second=read(context.object21,"fb5edc+21");
        return first!=0 || second!=0;
    }
    bool static_gate() noexcept {
        if(read(context.byte108b9b8,"108b9b8")==1)return true;
        return read(context.byte1083850,"1083850")!=0;
    }
    bool manager_gate() noexcept {
        const auto id=read(context.manager.ad50,"contextAD50");
        if(id==100){
            const auto state=read(context.manager.ad20,"contextAD20");
            return state==8 || state==13 || state==18;
        }
        const auto flags=read(context.manager.ad64,"contextAD64");
        if((flags&0x40000000U)==0 || id!=1450)return false;
        return read(context.manager.ad20,"contextAD20")>19;
    }
    bool low_bit() noexcept {return (read(context.byte109bd29,"109bd29")&1U)!=0;}
    bool suppress() noexcept {
        if(static_gate())return true;
        if(manager_gate())return true;
        if(read(facts.pw9dc,"PW9dc")!=0)return true;
        if(read(facts.pw9de,"PW9de")!=0)return true;
        if(read(facts.pw9dd,"PW9dd")!=0)return true;
        const auto state=read(facts.pbw371,"PBW371");
        if(state==0){const auto action=read(facts.pbw372,"PBW372");if(action==151 || action==152)return true;}
        else if(state==12){if(read(facts.pbw372,"PBW372")==11)return true;}
        if(low_bit())return true;
        return (read(facts.pwe8,"PWe8")&0x200U)!=0;
    }
    void suppress_writes() noexcept {
        // Preserve the ten source stores, including their interleaved widths.
        zero(0x3a8,2);zero(0x3aa,2);zero(0x3a0,4);zero(0x3ac,2);zero(0x3ae,2);zero(0x3a4,4);
        for(const auto offset:{0x3b0,0x3b2,0x3b4,0x3b6})zero(offset,2);
        if(manager_gate())zero(0x3c4,2);
        zero(0x452,1);zero(0x454,2);zero(0x458,4);zero(0x45c,2);zero(0x45e,2);
        const auto bits=word3cc();store<std::uint32_t>(0x3cc,bits&~0x800U);store<std::uint32_t>(0x3cc,bits&~0xa00U);
        for(unsigned i=0;i<8;++i){
            if(i==6 && (static_gate() || low_bit())){effects.lane6_preserved=true;continue;}
            zero(0x406+i,1);zero(0x410+4*i,4);
        }
        for(const auto offset:{0x3d4,0x3d8,0x3dc,0x3e0})zero(offset,4);
    }
    void clear() noexcept {zero(base,player_input_storage::size);effects.cleared=true;}
    void restore() noexcept {
        const auto& pad=context.pad;
        store<std::uint16_t>(0x3ba,read(pad.field31e,"pad31e"));
        store<std::uint16_t>(0x3b8,read(pad.field31c,"pad31c"));
        store<std::uint16_t>(0x3bc,read(pad.field320,"pad320"));
        store<std::uint16_t>(0x3c0,read(pad.field32c,"pad32c"));
        store<std::uint16_t>(0x3c2,read(pad.field32e,"pad32e"));
        store<std::uint16_t>(0x3c4,read(pad.field338,"pad338"));
        store<std::uint16_t>(0x3c6,read(pad.field33a,"pad33a"));
        effects.restored=true;
    }
    BlockResult finish() const noexcept {
        if(error)return *error;
        return BlockOutput{block,effects};
    }
    BlockResult run() noexcept {
        effects.local=local();if(!effects.local)return finish();
        if(ordered_positive(read(facts.pw13c_bits,"PW13c")) || external()){clear();return finish();}
        const auto queried=query_manager(context.manager,ManagerQuery::restore_input);
        if(const auto* failure=std::get_if<Error>(&queried)){if(!error)error=*failure;return finish();}
        if(std::get<bool>(queried) || ordered_positive(read(facts.pbw464_bits,"PBW464"))){clear();restore();}
        effects.suppressed=suppress();if(effects.suppressed)suppress_writes();
        return finish();
    }
};
}
BlockResult apply_block(const Block& block,const Facts& facts,const Context& context) noexcept {
    return Evaluation{block,facts,context}.run();
}
Facts project_facts(const player_controller_step::PlayerFields& p,const player_fields::Fields& f) noexcept {
    return {p.global.field2b,p.global.field2f,p.global.fieldd72,p.pbw.field30,p.pbw.field33,p.pbw.field371,
        p.pbw.field372,f.pw.field13c_bits,f.pbw.field464_bits,p.pw.fielde8,f.pw.field9dc,f.pw.field9de,f.pw.field9dd};
}
Result apply(const player_controller_step::PlayerFields& p,const player_fields::Fields& f,const Context& context) noexcept {
    auto result=apply_block(player_input_block::project(p,f),project_facts(p,f),context);
    if(const auto* e=std::get_if<Error>(&result))return *e;
    const auto& output=std::get<BlockOutput>(result);
    auto published=player_input_block::publish(output.block,p,f);
    if(const auto* e=std::get_if<player_input_block::Error>(&published))return Error{ErrorCode::projection,"input block",*e};
    auto state=std::get<player_input_block::Output>(published);
    return Output{state.player,state.fields,output.effects};
}
} // namespace mh4u::player_input_filter
