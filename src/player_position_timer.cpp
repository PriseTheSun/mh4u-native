#include "player_position_timer.hpp"

namespace mh4u::player_position_timer {
namespace {
namespace fp=binary32_arithmetic;
bool nan(Word bits) noexcept {return (bits&0x7fffffffU)>0x7f800000U;}
bool positive(Word bits) noexcept {return bits>0&&bits<=0x7f800000U;}
bool high(Word bits) noexcept {return positive(bits)||nan(bits);}
Error unknown(Phase phase,std::string_view field){return {ErrorCode::unknown_field,phase,field,{}};}
WordResult integrate(std::optional<Word> value,std::optional<Word> rate,Word delta,
    Phase multiply_phase,Phase add_phase,std::string_view value_name,std::string_view rate_name){
    if(!rate)return unknown(multiply_phase,rate_name);
    if(!value)return unknown(add_phase,value_name);
    auto product=fp::mul(*rate,delta);
    if(auto* error=std::get_if<fp::Error>(&product))return Error{ErrorCode::arithmetic,multiply_phase,rate_name,*error};
    auto summed=fp::add(*value,std::get<Word>(product));
    if(auto* error=std::get_if<fp::Error>(&summed))return Error{ErrorCode::arithmetic,add_phase,value_name,*error};
    return std::get<Word>(summed);
}
}
WordResult increment(std::optional<Word> timer,std::optional<Word> delta){
    if(!timer)return unknown(Phase::increment,"timer");
    if(!delta)return unknown(Phase::increment,"player14");
    auto value=fp::add(*timer,*delta);
    if(auto* error=std::get_if<fp::Error>(&value))return Error{ErrorCode::arithmetic,Phase::increment,"timer+player14",*error};
    return std::get<Word>(value);
}
TimerResult decrement(std::optional<Word> timer,std::optional<Word> delta){
    if(!timer)return unknown(Phase::decrement,"timer");
    if(!positive(*timer))return TimerOutput{0,1};
    if(!delta)return unknown(Phase::decrement,"player14");
    auto value=fp::sub(*timer,*delta);
    if(auto* error=std::get_if<fp::Error>(&value))return Error{ErrorCode::arithmetic,Phase::decrement,"timer-player14",*error};
    const auto bits=std::get<Word>(value);
    return high(bits)?TimerOutput{bits,0}:TimerOutput{0,1};
}
Result apply(const player_fields::Fields& fields,std::optional<Word> delta){
    if(!fields.pw.fieldb8_bits)return unknown(Phase::entry,"PWb8");
    // BLS after VCMPE: unordered does not skip this tail.
    if(!high(*fields.pw.fieldb8_bits))return Output{fields,{}};
    auto timer=decrement(fields.pw.fieldb8_bits,delta);
    if(auto* error=std::get_if<Error>(&timer))return *error;
    const auto value=std::get<TimerOutput>(timer);
    Output out{fields,{true,value.returned,false,false}};
    out.fields.pw.fieldb8_bits=value.value_bits;
    if(!high(value.value_bits)){
        if(!fields.pw.field90_bits)return unknown(Phase::restore,"PW90");
        if(!fields.pw.field94_bits)return unknown(Phase::restore,"PW94");
        if(!fields.pw.field98_bits)return unknown(Phase::restore,"PW98");
        out.fields.pbw.field10_bits=fields.pw.field90_bits;
        out.fields.pbw.field14_bits=fields.pw.field94_bits;
        out.fields.pbw.field18_bits=fields.pw.field98_bits;
        out.fields.pbw.field1c_bits=0;
        out.effects.position_restored=true;
        return out;
    }
    // A positive/unordered result here is reachable only after delta was used.
    if(!delta)return unknown(Phase::x_multiply,"player14");
    auto x=integrate(fields.pbw.field10_bits,fields.pw.fielda0_bits,*delta,
        Phase::x_multiply,Phase::x_add,"PBW10","PWa0");
    if(auto* error=std::get_if<Error>(&x))return *error;
    out.fields.pbw.field10_bits=std::get<Word>(x);
    auto y=integrate(fields.pbw.field14_bits,fields.pw.fielda4_bits,*delta,
        Phase::y_multiply,Phase::y_add,"PBW14","PWa4");
    if(auto* error=std::get_if<Error>(&y))return *error;
    out.fields.pbw.field14_bits=std::get<Word>(y);
    auto z=integrate(fields.pbw.field18_bits,fields.pw.fielda8_bits,*delta,
        Phase::z_multiply,Phase::z_add,"PBW18","PWa8");
    if(auto* error=std::get_if<Error>(&z))return *error;
    out.fields.pbw.field18_bits=std::get<Word>(z);
    out.effects.position_integrated=true;
    return out;
}
} // namespace mh4u::player_position_timer
