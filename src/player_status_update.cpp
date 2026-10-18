#include "player_status_update.hpp"
#include <bit>

namespace mh4u::player_status_update {
namespace {
namespace fp=binary32_arithmetic;
namespace oq=player_object_query;
constexpr bool positive(Word v) noexcept {
    return (v&0x80000000U)==0&&(v&0x7fffffffU)!=0&&(v&0x7fffffffU)<=0x7f800000U;
}
constexpr std::int32_t signed_word(Word v) noexcept {return std::bit_cast<std::int32_t>(v);}
struct Engine {
    const player_controller_step::PlayerFields& player;
    const oq::Manager& manager;
    std::optional<Word> delta;
    Output out;
    Phase phase=Phase::flags;
    [[noreturn]] void fail(ErrorCode code,std::string_view field,Cause cause={})const {
        throw Error{phase,code,field,cause};
    }
    template<class T>T need(const std::optional<T>& value,std::string_view field)const {
        if(!value)fail(ErrorCode::unknown_field,field);
        return *value;
    }
    Word number(fp::Result value)const {
        if(const auto* error=std::get_if<fp::Error>(&value))fail(ErrorCode::dependency,"binary32",*error);
        return std::get<Word>(value);
    }
    Word from(std::int32_t value)const {return number(fp::from_signed_word(std::bit_cast<Word>(value)));}
    bool listed(std::uint16_t id) {
        out.effects.list_queries[out.effects.query_count++]=id;
        const auto result=player_selection::contains_id(player.selection_pbw,id);
        if(const auto* error=std::get_if<player_selection::Error>(&result))fail(ErrorCode::dependency,"PBW2fc",*error);
        return std::get<bool>(result);
    }
    void first_flags() {
        auto& pw=out.fields.pw;
        Word flags=0;
        if(need(pw.field17e,"PW17e")!=0)flags=(player.pw.fieldec&0x8000U)?0x80000U:1U;
        const auto left=need(pw.field274,"PW274"),right=need(pw.field276,"PW276");
        if(static_cast<int>(left)+static_cast<int>(right)>0)flags|=0x10U;
        const auto modifier=need(pw.field278_bits,"PW278");
        if(positive(number(fp::add(from(need(pw.field275,"PW275")),modifier))))flags|=0x20U;
        if(player.pbw.field31==11){if(need(pw.field265,"PW265")&1U)flags|=0x08000000U;}
        else if(player.pbw.field31==7&&positive(need(pw.field5c_bits,"PW5c")))flags|=0x08000000U;
        if(player.pbw.field371==6){
            switch(player.pbw.field372){
            case 26:flags|=8U;break;
            case 27:case 28:case 29:case 94:flags|=2U;break;
            case 30:case 31:case 32:case 115:case 116:flags|=4U;break;
            case 66:case 68:case 69:case 70:case 71:case 72:case 73:flags|=0x00200000U;break;
            case 106:case 107:case 108:case 109:case 110:flags|=0x02000000U;break;
            default:
                if(need(pw.field190,"PW190")>0)flags|=2U;
                else if(need(pw.field198,"PW198")>0)flags|=4U;
            }
        }
        if(need(pw.field238,"PW238")>0)flags|=0x00200000U;
        if(need(pw.field240,"PW240")>0)flags|=0x02000000U;
        if(positive(need(pw.field68_bits,"PW68")))flags|=0x200U;
        if(positive(need(pw.field6c_bits,"PW6c")))flags|=0x400U;
        if(positive(need(out.fields.pbw.field368_bits,"PBW368")))flags|=0x2000U;
        const auto status=player_status_predicates::status224(pw.field224,player.pbw);
        if(!status)fail(ErrorCode::unknown_field,"PW224");
        out.effects.status224=*status;if(*status)flags|=0x80U;
        if(positive(need(pw.field288_bits,"PW288")))flags|=0x00100000U;
        else if(positive(need(pw.field284_bits,"PW284")))flags|=0x40U;
        if(positive(need(pw.field1a8_bits,"PW1a8")))flags|=0x8000U;
        if(positive(need(pw.field28c_bits,"PW28c")))flags|=0x4000U;
        if(need(pw.field4fc,"PW4fc")!=0)flags|=0x10000U;
        if(positive(need(pw.field268_bits,"PW268")))flags|=0x1000U;
        if(positive(need(pw.field1c0_bits,"PW1c0")))flags|=0x800U;
        if(need(pw.field1d0,"PW1d0")>0)flags|=0x04000000U;
        if(positive(need(pw.field1b4_bits,"PW1b4")))flags|=0x40000U;
        if(listed(39)||listed(38)){
            phase=Phase::object_scan;
            const auto result=oq::scan_status(player,manager);
            if(const auto* error=std::get_if<oq::Error>(&result))fail(ErrorCode::dependency,"686f08",*error);
            out.effects.scan=std::get<oq::StatusScanOutput>(result);
            if(!out.effects.scan->matched){pw.field2fc_bits=0;out.effects.timer_cleared=true;}
            else {
                phase=Phase::timer;
                if(signed_word(need(pw.field2fc_bits,"PW2fc"))>=0x43340000)flags|=0x40000000U;
                else {
                    const auto increased=player_position_timer::increment(pw.field2fc_bits,delta);
                    if(const auto* error=std::get_if<player_position_timer::Error>(&increased))fail(ErrorCode::dependency,"b04c24",*error);
                    pw.field2fc_bits=std::get<Word>(increased);out.effects.timer_incremented=true;
                    if(signed_word(*pw.field2fc_bits)>=0x43340000){
                        phase=Phase::event;
                        fail(ErrorCode::unsupported_event,"b22164 threshold crossing: c08238 event dispatch");
                    }
                }
            }
            phase=Phase::flags;
        }
        if(!(flags&0x40000000U)&&(listed(119)||listed(13))){
            const auto high=need(out.fields.pbw.field34e,"PBW34e");
            if(need(out.fields.pbw.field34c,"PBW34c")>=high)flags|=0x80000000U;
        }
        if(!(flags&0xc0000000U)&&positive(need(pw.field2f0_bits,"PW2f0")))flags|=0x10000000U;
        if(player.pbw.field31==13){
            if(positive(need(pw.field940_bits,"PW940")))flags|=0x00400000U;
            if(positive(need(pw.field948_bits,"PW948")))flags|=0x00800000U;
            if(positive(need(pw.field950_bits,"PW950")))flags|=0x01000000U;
        }
        if(player.pw.fieldec&0x08000000U)flags|=0x20000U;
        out.field1f4=flags;
    }
    void secondary_flags() {
        phase=Phase::secondary_flags;
        auto& pw=out.fields.pw;
        Word flags=need(pw.field1f8,"PW1f8")&~0x0fb00c00U;
        if(positive(need(pw.field1a4_bits,"PW1a4")))flags|=0x800U;
        else if(positive(need(pw.field1a0_bits,"PW1a0")))flags|=0x400U;
        pw.field299=0;
        if(positive(need(pw.field2a4_bits,"PW2a4")))flags|=0x08000000U;
        else if(positive(need(pw.field29c_bits,"PW29c"))){pw.field299=2;flags|=0x04000000U;}
        else if(need(pw.field29a,"PW29a")>0){
            pw.field299=1;
            const auto numerator=need(pw.field2a0,"PW2a0"),denominator=need(pw.field2a2,"PW2a2");
            const auto ratio=signed_word(number(fp::div(from(numerator),from(denominator))));
            flags|=ratio>0x3f28f5c3?0x00800000U:ratio>0x3ea8f5c3?0x01000000U:0x02000000U;
        }
        if(positive(need(pw.field2c8_bits,"PW2c8")))flags|=0x00100000U;
        if(positive(need(pw.field2cc_bits,"PW2cc")))flags|=0x00200000U;
        pw.field1f8=flags;
    }
    void run() {
        out.effects.local=player_transition::local_control(player.global,player.pbw);
        if(!out.effects.local)return;
        first_flags();secondary_flags();
    }
};
}
Result apply(const player_controller_step::PlayerFields& player,const player_fields::Fields& fields,
    const player_object_query::Manager& manager,std::optional<Word> delta_bits) {
    try{
        Engine engine{player,manager,delta_bits,{player.pw.field1f4,fields,{}}};
        engine.run();return engine.out;
    }catch(const Error& error){return error;}
}
}
