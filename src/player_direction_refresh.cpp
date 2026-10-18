#include "player_direction_refresh.hpp"
#include <type_traits>

namespace mh4u::player_direction_refresh {
namespace {
namespace arithmetic=binary32_arithmetic;
Error missing(Phase phase,ErrorCode code,std::string_view field) noexcept {
    return {phase,code,field,{}};
}
Error numeric(Phase phase,const arithmetic::Error& cause) noexcept {
    return {phase,ErrorCode::arithmetic,"b6d2a4 binary32 expression",cause};
}
}
Result apply(const player_controller_step::State& source,
    std::optional<Word> delta_bits,const Bindings& bindings) noexcept {
    static_assert(std::is_nothrow_move_constructible_v<player_controller_step::State>);
    Effects effects;
    const auto record=[&](Call call){effects.calls[effects.call_count++]=call;};
    const auto& player=source.player;
    const auto& pbw=player.pbw;
    if(!player.selection_pbw.field6f9)
        return missing(Phase::byte6f9,ErrorCode::unknown_field,"PBW6f9");
    record(Call::byte6f9);
    const bool flag=*player.selection_pbw.field6f9==1;
    const auto state=pbw.field371;
    const auto action=pbw.field372;
    effects.rate=4004;
    if((flag&&state==1&&(action==0||action==1)) ||
       (state==4&&(action==4||action==15)))effects.rate=2548;
    else if(state==1&&(action==1||action==5||action==23||action==15||action==3))effects.rate=1820;
    else if(state==1&&(action==2||action==10||action==22||action==24||action==32||action==33))effects.rate=1456;
    else if(state==0&&(action==3||action==162))effects.rate=728;
    else {
        record(Call::state_action);
        if(player_transition::state_action_gate(pbw,0))effects.rate=1456;
        if(state==1&&action==4)effects.rate=0;
    }
    record(Call::byte6f9);
    if(flag){
        if(!bindings.virtual144)return missing(Phase::virtual144,ErrorCode::unknown_binding,"uPlayer virtual144");
        if(*bindings.virtual144!=Virtual144::uplayer_b47aa0)
            return missing(Phase::virtual144,ErrorCode::unsupported_binding,"uPlayer virtual144");
        record(Call::virtual144); // The real concrete b47aa0 returns zero.
    }
    if(!delta_bits)return missing(Phase::delta,ErrorCode::unknown_field,"Player14");
    const auto half=arithmetic::from_unsigned_word(effects.rate>>1);
    if(const auto* error=std::get_if<arithmetic::Error>(&half))return numeric(Phase::rate_conversion,*error);
    effects.difference=static_cast<std::uint16_t>(player.pw.fieldb0-pbw.field24);
    const auto scaled=arithmetic::mul(*delta_bits,std::get<Word>(half));
    if(const auto* error=std::get_if<arithmetic::Error>(&scaled))return numeric(Phase::scale,*error);
    const auto step=arithmetic::to_signed_word_rtz(std::get<Word>(scaled));
    if(const auto* error=std::get_if<arithmetic::Error>(&step))return numeric(Phase::step_conversion,*error);
    effects.step=std::get<Word>(step);
    if(!bindings.virtual84)return missing(Phase::virtual84,ErrorCode::unknown_binding,"uPlayer virtual84");
    if(*bindings.virtual84!=Virtual84::uplayer_b085bc)
        return missing(Phase::virtual84,ErrorCode::unsupported_binding,"uPlayer virtual84");
    record(Call::virtual84); // Player244 is loaded, but the return is discarded.
    effects.snapped=static_cast<Word>(static_cast<std::uint16_t>(effects.difference+effects.step))<(effects.step<<1);
    Output out{source,effects};
    out.state.player.pbw.field24=effects.snapped?player.pw.fieldb0:
        static_cast<Word>(static_cast<std::uint16_t>(effects.difference<32768?
            pbw.field24+effects.step:pbw.field24-effects.step));
    return out;
}
} // namespace mh4u::player_direction_refresh
