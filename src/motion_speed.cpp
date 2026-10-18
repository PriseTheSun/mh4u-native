#include "motion_speed.hpp"
#include "motion_command.hpp"
#include <bit>

namespace mh4u::motion_speed {
namespace {
using Word=std::uint32_t;
namespace fp=binary32_arithmetic;
// VCMPE/BLE against +0: unordered, both zeros and negative values return.
// This is a raw predicate so a signaling NaN is not quieted in copied state.
bool positive_ordered(Word word) noexcept {return word>0&&word<=0x7f800000U;}
}
Result primary(const motion_request::Layer& layer,const motion_command::Fields& fields,
    Word speed_bits){
    Output out{secondary(layer,speed_bits),false};
    const Word remaining=std::bit_cast<Word>(layer.remaining_interpolation);
    if(!positive_ordered(remaining)||(speed_bits&0x7fffffffU)==0)return out;
    auto ratio=fp::div(remaining,std::bit_cast<Word>(layer.interpolation));
    if(const auto* error=std::get_if<fp::Error>(&ratio))return Error{Phase::ratio,*error};
    auto scaled=fp::mul(std::get<Word>(ratio),fields.fieldd68);
    if(const auto* error=std::get_if<fp::Error>(&scaled))return Error{Phase::scaled,*error};
    auto interpolation=fp::div(fields.fieldd68,speed_bits);
    if(const auto* error=std::get_if<fp::Error>(&interpolation))return Error{Phase::interpolation,*error};
    auto next_remaining=fp::div(std::get<Word>(scaled),speed_bits);
    if(const auto* error=std::get_if<fp::Error>(&next_remaining))return Error{Phase::remaining,*error};
    out.layer.remaining_interpolation=std::bit_cast<float>(std::get<Word>(next_remaining));
    out.layer.interpolation=std::bit_cast<float>(std::get<Word>(interpolation));
    out.rescaled=true;
    return out;
}
motion_request::Layer secondary(const motion_request::Layer& layer,Word speed_bits) noexcept {
    auto out=layer;out.speed=std::bit_cast<float>(speed_bits);return out;
}
} // namespace mh4u::motion_speed
