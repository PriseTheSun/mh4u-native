#include "player_update_continuation.hpp"

namespace mh4u::player_update_continuation {
namespace {
constexpr bool ordered_positive(std::uint32_t value) noexcept {
    return value!=0 && value<=0x7f800000U;
}
} // namespace
Result apply(const player_controller_step::State& state,std::span<const motion_pending::Layer> layers,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const Context& context,motion_command::Limits limits) {
    namespace admission=player_update_admission;
    namespace statistics=player_update_statistics;
    auto admitted=admission::apply(state,layers,fields,objects,{context.admission,context.constants},limits);
    if(const auto* e=std::get_if<admission::Error>(&admitted))
        return Error{Phase::admission,e->code==admission::ErrorCode::capacity?ErrorCode::capacity:ErrorCode::dependency,e->field,*e};
    auto prepared=std::get<admission::Output>(std::move(admitted));
    auto stats=statistics::apply(prepared.state.player,prepared.fields,
        context.admission.prefix.filter.manager,context.statistics);
    if(const auto* e=std::get_if<statistics::Error>(&stats))
        return Error{Phase::statistics,e->code==statistics::ErrorCode::capacity?ErrorCode::capacity:ErrorCode::dependency,e->field,*e};
    auto updated=std::get<statistics::Output>(std::move(stats));
    prepared.fields=std::move(updated.fields);
    auto& player=prepared.state.player;
    auto& shared=prepared.fields;
    const bool local=player_transition::local_control(player.global,player.pbw);
    EarlyReturns returns{};
    if(player.pbw.field371==8)returns.first=FirstReturn::state8;
    else {
        if(!local)return Error{Phase::first_helper,ErrorCode::unsupported_first_helper,"b2f09c remote body",{}};
        if(!shared.pw.field1bc_bits)return Error{Phase::first_helper,ErrorCode::unknown_field,"PW1bc",{}};
        if(!ordered_positive(*shared.pw.field1bc_bits))
            return Error{Phase::first_helper,ErrorCode::unsupported_first_helper,"b2f09c active body",{}};
        returns.first=FirstReturn::local_positive_timer;
    }
    if(!shared.pw.fielda15)return Error{Phase::second_helper,ErrorCode::unknown_field,"PWa15",{}};
    if(*shared.pw.fielda15==0)returns.second=SecondReturn::disabled;
    else {
        if(!local)return Error{Phase::second_helper,ErrorCode::unsupported_second_helper,"b42108 remote body",{}};
        if(!shared.pw.field296)return Error{Phase::second_helper,ErrorCode::unknown_field,"PW296",{}};
        if(!shared.pw.field294)return Error{Phase::second_helper,ErrorCode::unknown_field,"PW294",{}};
        if(*shared.pw.field296<=*shared.pw.field294)returns.second=SecondReturn::local_ordered_values;
        else {
            if(!player.selection_pbw.field6f9)return Error{Phase::second_helper,ErrorCode::unknown_field,"PBW6f9",{}};
            // b42158..64 directly tests the byte against0, unlike b07e80.
            if(*player.selection_pbw.field6f9!=0)
                return Error{Phase::second_helper,ErrorCode::unsupported_second_helper,"b42108 active body",{}};
            returns.second=SecondReturn::local_zero_flag;
        }
    }
    // Copying unknown knowledge is allowed: the source only loads/stores4f8.
    // Subsequent consumers still must establish its value before interpreting.
    shared.pw.field4fc=shared.pw.field4f8;
    shared.pw.field4f8=0;
    shared.pbw.field93ee=0;shared.pbw.field93e9=0;
    player.pw.fieldf8=(player.pw.fieldf8&~0x20000000U)|(player.pw.fieldf0&0x20000000U);
    player.pw.fieldf0&=~0x20000000U;
    return Output{std::move(prepared),updated.effects,returns};
}
} // namespace mh4u::player_update_continuation
