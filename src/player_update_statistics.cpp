#include "player_update_statistics.hpp"
#include <algorithm>
#include <bit>

namespace mh4u::player_update_statistics {
namespace {
namespace pf=player_fields;
namespace pq=player_parameter_query;
namespace pt=player_transition;
namespace ps=player_selection;
namespace fp=binary32_arithmetic;
constexpr Word one=0x3f800000U;
constexpr std::int32_t signed_word(Word v) noexcept {return std::bit_cast<std::int32_t>(v);}
constexpr bool nan(Word v) noexcept {return (v&0x7fffffffU)>0x7f800000U;}
// ARM VCMPE ordered comparisons, without host NaN exceptions or payload loss.
constexpr bool less(Word a,Word b) noexcept {
    if(nan(a)||nan(b)||((a|b)&0x7fffffffU)==0)return false;
    if((a^b)&0x80000000U)return (a&0x80000000U)!=0;
    return a&0x80000000U?a>b:a<b;
}
constexpr bool equal(Word a,Word b) noexcept {
    return !nan(a)&&!nan(b)&&(a==b||((a|b)&0x7fffffffU)==0);
}
struct Engine {
    const player_controller_step::PlayerFields& player;
    const player_input_filter::Manager& manager;
    const Context& context;
    Output out;
    Phase phase=Phase::first_query;
    [[noreturn]] void fail(ErrorCode code,std::string_view field,Cause cause={}) const {
        throw Error{phase,code,field,cause};
    }
    template<class T>T need(const std::optional<T>& value,std::string_view field) const {
        if(!value)fail(ErrorCode::unknown_field,field);
        return *value;
    }
    Word number(fp::Result value) const {
        if(auto e=std::get_if<fp::Error>(&value))fail(ErrorCode::dependency,"binary32",*e);
        return std::get<Word>(value);
    }
    Word from(std::int32_t value) const {return number(fp::from_signed_word(std::bit_cast<Word>(value)));}
    Word add(Word a,Word b) const {return number(fp::add(a,b));}
    Word sub(Word a,Word b) const {return number(fp::sub(a,b));}
    Word mul(Word a,Word b) const {return number(fp::mul(a,b));}
    Word div(Word a,Word b) const {return number(fp::div(a,b));}
    bool type(std::uint8_t id) const {
        return std::find(player.pbw.fields2f8.begin(),player.pbw.fields2f8.end(),id)!=player.pbw.fields2f8.end();
    }
    bool skill(std::uint16_t id) const {
        const auto found=ps::contains_id(player.selection_pbw,id);
        if(auto e=std::get_if<ps::Error>(&found))fail(ErrorCode::dependency,"PBW2fc",*e);
        return std::get<bool>(found);
    }
    Word query(Word selector) {
        auto result=pq::apply(out.fields,selector,context.parameters);
        if(auto e=std::get_if<pq::Error>(&result))fail(ErrorCode::dependency,"2f8e04",*e);
        const auto& value=std::get<pq::Output>(result);
        if(out.effects.query_count==out.effects.queries.size())fail(ErrorCode::capacity,"Parameter observations");
        out.effects.queries[out.effects.query_count++]={selector,value.value,value.effects};
        out.fields=value.fields;
        return from(value.value);
    }
    std::optional<PlayerView> find_player() {
        ++out.effects.registry_searches;
        if(need(player.global.field5a,"Global5a")==1)return {};
        if(std::holds_alternative<Unknown>(context.players))fail(ErrorCode::unknown_field,"Player registry");
        if(std::holds_alternative<Absent>(context.players))fail(ErrorCode::absent_reference,"Player registry");
        const auto* registry=std::get<const PlayerRegistry*>(context.players);
        if(!registry)fail(ErrorCode::absent_reference,"Player registry");
        const auto count=need(registry->count,"Player registry count");
        for(std::int32_t i=0;i<count;++i) {
            const auto index=static_cast<std::size_t>(i);
            if(index>=context.registry_work_limit)fail(ErrorCode::capacity,"Player registry work budget");
            if(index==4)fail(ErrorCode::unsupported_registry_overlap,"Registry slot4 aliases count");
            if(index>=registry->slots.size())fail(ErrorCode::reference_extent,"Player registry slot");
            ++out.effects.registry_slots;
            const auto& link=registry->slots[index];
            if(std::holds_alternative<Unknown>(link))fail(ErrorCode::unknown_field,"Player registry slot");
            if(std::holds_alternative<Absent>(link))continue;
            PlayerView candidate;
            if(std::holds_alternative<CurrentPlayer>(link))candidate={&player.pbw,out.fields.pbw.bonus236};
            else {
                const auto selected=std::get<PlayerIndex>(link).value;
                if(selected>=registry->players.size())fail(ErrorCode::reference_extent,"Registry player index");
                candidate=registry->players[selected];
            }
            if(!candidate.pbw)fail(ErrorCode::absent_reference,"Registry Player.PBW");
            if(pt::local_control(player.global,*candidate.pbw))return candidate;
        }
        return {};
    }
    std::int16_t bonus(std::uint16_t id) {
        ++out.effects.bonus_lookups;
        if(id==0||id>0x790)return 0;
        std::span<const BonusRecord> records;
        std::size_t count=0;
        if(player.global.field2b==2) {
            count=32;
            if(std::holds_alternative<Unknown>(context.mode2_bonuses))fail(ErrorCode::unknown_field,"Mode2 bonus table");
            if(std::holds_alternative<Absent>(context.mode2_bonuses))fail(ErrorCode::absent_reference,"Mode2 bonus table");
            records=std::get<BonusTable>(context.mode2_bonuses).records;
        } else {
            // Original calls af7304 twice. The immutable view makes repeated
            // reads stable; preserve both searches and their lazy requirements.
            if(find_player())count=40;
            if(const auto selected=find_player())records=selected->bonus236;
        }
        for(std::size_t i=0;i<count;++i) {
            if(i>=records.size())fail(ErrorCode::reference_extent,"Bonus record");
            if(need(records[i].id,"Bonus record ID")==id)return need(records[i].value,"Bonus record value");
        }
        return 0;
    }
    bool time_gate() const {
        const auto state=need(manager.ad50,"ManagerAD50");
        Word numerator=0;
        if(state==1002)numerator=need(context.manager8044,"Manager8044");
        else if(need(manager.ad64,"ManagerAD64")&0x40000000U)numerator=need(context.manager8040,"Manager8040");
        const auto clock=number(fp::to_signed_word_rtz(need(context.clock2c_bits,"Clock2c")));
        if(state!=1002&&(need(manager.ad64,"ManagerAD64")&0x40000000U)==0) {
            const auto previous=need(context.manager803c,"Manager803c");
            const auto duration=need(context.managerad5c,"ManagerAD5c");
            numerator=(clock*60U)*duration-previous;
        }
        if(clock==0)fail(ErrorCode::division_zero,"2ff184 divisor");
        // UMULL by88888889, highword>>5 is exactly unsigned quotient/60.
        return (numerator/clock)/60U<10U;
    }
    Word ratio() const {
        const auto numerator=from(need(out.fields.pbw.field34c,"PBW34c"));
        const auto denominator=from(need(out.fields.pbw.field34e,"PBW34e"));
        return div(numerator,denominator);
    }
    Word skill_modifier(std::uint8_t& flag) const {
        Word result=0;
        if(skill(46))result=0x41c80000U;
        else if(skill(47)||skill(1))result=0x41a00000U;
        else if(skill(48))result=0x41700000U;
        else if(skill(49))result=0x41200000U;
        else if(skill(50))result=0xc0a00000U;
        else if(skill(51))result=0xc1200000U;
        else if(skill(52))result=0xc1700000U;
        switch(player.pbw.field31) {
        case 2:case 4:case 5:case 6:case 10:case 12:case 15:break;
        default:if(skill(22)||skill(8))result=add(result,0x40a00000U);break;
        }
        if(less(0,result))flag=1;
        else if(less(result,0))flag=2;
        return result;
    }
    std::uint16_t first() {
        auto& flag=out.effects.helper_flags[0];
        const Word base=query(6);
        Word value=base;
        if(bonus(40)>0)value=add(value,0x40c00000U);
        if(bonus(41)>0)value=add(value,0x41100000U);
        // The source loads276 before274, then adds274+base and276+result.
        const auto high=from(need(out.fields.pw.field276,"PW276"));
        const auto low=from(need(out.fields.pw.field274,"PW274"));
        value=add(high,add(low,value));
        value=add(skill_modifier(flag),value);
        if(type(54)&&time_gate())value=add(value,0x40400000U);
        if(player.pw.field1f4&0x40000000U) {
            if(skill(39))value=add(value,0x41200000U);
            else if(skill(38))value=add(value,0x41c80000U);
            flag=1;
        }
        if(player.pw.field1f4&0x80000000U){flag=1;value=add(value,0x41a00000U);}
        if(skill(197)||skill(272)) {
            const auto kind=need(out.fields.pw.field262,"PW262");
            if(kind>=2)value=mul(value,0x3f99999aU);
            else if(kind)value=mul(value,0x3f8ccccdU);
        }
        if(need(player.selection_pw.field800,"PW800")&2U) {
            const auto kind=need(out.fields.pw.field805,"PW805");
            if(kind==1)value=mul(value,0x3f8ccccdU);
            else if(kind==2||kind==5)value=mul(value,0x3f933333U);
            else if(kind==6)value=mul(value,0x3f99999aU);
        }
        if(need(out.fields.pbw.field34c,"PBW34c")<=10&&type(18)) {
            flag=1;value=mul(value,0x3faccccdU);
        } else if(signed_word(ratio())<=0x3ecccccd) {
            if(skill(106)){flag=2;value=mul(value,0x3f333333U);}
            else if(skill(104)||skill(4)){flag=1;value=mul(value,0x3fa66666U);}
        }
        if(signed_word(value)<signed_word(one))value=one;
        // GE is false for unordered; LT is true. Preserve that ARM predicate.
        if(flag)flag=less(base,value)||equal(base,value)?1:2;
        return static_cast<std::uint16_t>(number(fp::to_unsigned_word_rtz(value)));
    }
    std::uint16_t second() {
        auto& flag=out.effects.helper_flags[1];
        Word value=add(query(0),one);
        if(bonus(42)>0)value=add(value,0x41400000U);
        if(bonus(43)>0)value=add(value,0x41900000U);
        value=add(from(need(out.fields.pw.field275,"PW275")),value);
        if(type(54)&&time_gate())value=add(value,0x41200000U);
        const auto modifier=need(out.fields.pw.field278_bits,"PW278");
        value=player.pw.fielde8&0x100U?mul(modifier,value):add(modifier,value);
        if(skill(53))value=add(mul(value,0x3f8a3d71U),0x41f00000U);
        else if(skill(54))value=add(mul(value,0x3f87ae14U),0x41c80000U);
        else if(skill(55)||skill(9))value=add(mul(value,0x3f83d70aU),0x41a00000U);
        else if(skill(56))value=add(value,0x41700000U);
        else if(skill(57))value=sub(value,0x41200000U);
        else if(skill(58))value=sub(mul(value,0x3f733333U),0x41200000U);
        else if(skill(59))value=sub(mul(value,0x3f666666U),0x41200000U);
        if(signed_word(ratio())<=0x3ecccccd) {
            if(skill(106))value=add(value,0x41a80000U);
            else if(skill(105)||skill(104)||skill(4))value=add(value,0x42340000U);
            else value=add(value,0x41f00000U);
        }
        if(skill(197)||skill(272)) {
            const auto kind=need(out.fields.pw.field262,"PW262");
            if(kind>=2){value=mul(value,0x3fa66666U);flag=1;}
            else if(kind){value=mul(value,0x3f933333U);flag=1;}
        }
        if(need(player.selection_pw.field800,"PW800")&4U) {
            const auto kind=need(out.fields.pw.field806,"PW806");
            if(kind==1)value=mul(value,0x3f8ccccdU);
            else if(kind==2||kind==5)value=mul(value,0x3f933333U);
            else if(kind==6)value=mul(value,0x3f99999aU);
        }
        if(player_transition::type13_flag(player.pbw,player.pw,0)&&
           player_transition::type13_flag(player.pbw,player.pw,1)&&
           player_transition::type13_flag(player.pbw,player.pw,2)){value=mul(value,0x3f8a3d71U);flag=1;}
        else if(player_transition::type13_flag(player.pbw,player.pw,2)&&
                player_transition::type13_flag(player.pbw,player.pw,1)){value=mul(value,0x3f87ae14U);flag=1;}
        if(signed_word(value)<signed_word(one))value=one;
        // BLE after284 vs0 includes unordered. BCS after0 vs288 also includes
        // unordered: only an ordered positive timer enables either penalty.
        if(less(0,need(out.fields.pw.field284_bits,"PW284")))value=sub(value,mul(value,0x3e4ccccdU));
        else if(less(0,need(out.fields.pw.field288_bits,"PW288")))value=sub(value,mul(value,0x3f000000U));
        if(signed_word(value)<signed_word(one))value=one;
        return static_cast<std::uint16_t>(number(fp::to_unsigned_word_rtz(value)));
    }
    std::int32_t buff(std::uint8_t selector) {
        ++out.effects.buff_queries;
        const std::array<std::optional<std::uint8_t>,3>* ids;
        const std::array<std::optional<std::int16_t>,3>* values;
        if(out.effects.local) {
            if(player.pbw.field30==0&&need(out.fields.pw.field262,"PW262")!=0&&!type(39))return 0;
            ids=&player.global.fieldsd38;values=&player.global.fieldsd3c;
        } else {
            if(player.pbw.field30!=1)return 0;
            if(std::holds_alternative<Unknown>(context.remote_buffs))fail(ErrorCode::unknown_field,"PBW2c");
            if(std::holds_alternative<Absent>(context.remote_buffs))fail(ErrorCode::absent_reference,"PBW2c");
            const auto* remote=std::get<const RemoteBuffs*>(context.remote_buffs);
            if(!remote)fail(ErrorCode::absent_reference,"PBW2c");
            ids=&remote->ids;values=&remote->values;
        }
        Word result=0;
        for(std::size_t i=0;i<3;++i) {
            if(need((*ids)[i],"Buff ID")!=selector)continue;
            const auto value=need((*values)[i],"Buff value");
            if(value)result=(result+static_cast<Word>(value))&0xffffU;
        }
        return result<0x8000U?static_cast<std::int32_t>(result):static_cast<std::int32_t>(result)-65536;
    }
    void five() {
        auto& fields=out.fields.pbw.fields388_bits;
        for(Word i=0;i<5;++i)fields[i]=query(i+1);
        constexpr std::array<std::size_t,5> type_map{0,1,2,4,3};
        for(std::size_t i=0;i<5;++i)if(type(static_cast<std::uint8_t>(59+i))) {
            auto& value=fields[type_map[i]];
            value=less(*value,0)?0x40400000U:add(*value,0x40400000U);
        }
        for(auto& value:fields) {
            Word adjustment=less(0,need(out.fields.pw.field1a0_bits,"PW1a0"))?0xc1200000U:0;
            if(less(0,need(out.fields.pw.field1a4_bits,"PW1a4")))adjustment=sub(adjustment,0x41f00000U);
            value=add(adjustment,*value);
        }
        constexpr std::array<std::size_t,5> bonus_map{0,1,2,4,3};
        for(std::size_t i=0;i<5;++i) {
            const auto timer=need(out.fields.pw.fields248_bits[bonus_map[i]],"PW248..258");
            fields[i]=add(less(0,timer)?0x41f00000U:0,*fields[i]);
        }
        constexpr std::array<std::uint8_t,5> selectors{5,6,7,9,8};
        for(std::size_t i=0;i<5;++i)fields[i]=add(from(buff(selectors[i])),*fields[i]);
        constexpr std::array<std::uint16_t,5> first_ids{174,177,180,186,183};
        for(std::size_t i=0;i<5;++i) {
            const auto id=first_ids[i];
            if(skill(id))fields[i]=add(*fields[i],0x41700000U);
            else if(skill(id-1))fields[i]=add(*fields[i],0x41a00000U);
            else if(skill(id+1))fields[i]=sub(*fields[i],0x41a00000U);
        }
        // Source table100c4b4 is the five s16 indices16,17,18,19,20.
        for(std::size_t i=0;i<5;++i) {
            auto& value=fields[type_map[i]];
            if(less(0,need(out.fields.pw.fields864_bits[i],"PW864..874"))) {
                const auto kind=need(out.fields.pw.fields814[i],"PW814..818");
                if(kind==1)value=add(*value,0x40a00000U);
                else if(kind==2||kind==5)value=add(*value,0x40e00000U);
                else if(kind==6)value=add(*value,0x41200000U);
            }
            if(need(player.selection_pw.field800,"PW800")&0x400000U) {
                const auto kind=need(out.fields.pw.field81c,"PW81c");
                if(kind==1)value=add(*value,0x40a00000U);
                else if(kind==2)value=add(*value,0x41200000U);
            }
        }
        for(auto& value:fields) {
            if(*value>=0xc2c60000U)value=0xc2c60000U;
            if(signed_word(*value)>0x42c60000)value=0x42c60000U;
        }
    }
    void run() {
        out.effects.local=pt::local_control(player.global,player.pbw);
        if(out.effects.local)out.fields.pbw.field37c=first();
        phase=Phase::derived_fields;
        const auto first_value=need(out.fields.pbw.field37c,"PBW37c");
        if(first_value>=1050)out.fields.pbw.field37c=1050;
        out.fields.pbw.field380_bits=mul(number(fp::from_unsigned_word(*out.fields.pbw.field37c)),0x3c23d70aU);
        phase=Phase::second_query;
        if(out.effects.local)out.fields.pbw.field37e=second();
        phase=Phase::derived_fields;
        auto second_value=number(fp::from_unsigned_word(need(out.fields.pbw.field37e,"PBW37e")));
        if(signed_word(second_value)<=signed_word(one))second_value=one;
        out.fields.pbw.field384_bits=second_value;
        phase=Phase::five_parameters;
        five();
    }
};
} // namespace
Result apply(const player_controller_step::PlayerFields& player,const pf::Fields& fields,
    const player_input_filter::Manager& manager,const Context& context) noexcept {
    try {
        Engine engine{player,manager,context,{fields,{}}};
        engine.run();
        return engine.out;
    } catch(const Error& error){return error;}
}
} // namespace mh4u::player_update_statistics
