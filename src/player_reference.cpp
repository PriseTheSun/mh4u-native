#include "player_reference.hpp"
#include "player_reference_table.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>

#ifdef __FAST_MATH__
#error "player_reference requires strict floating-point operations"
#endif

namespace mh4u::player_reference {
namespace {
float sub(float a,float b) noexcept { volatile float r=a-b;return r; }
using Word=std::uint32_t;
bool nan(Word v){return (v&0x7fffffffU)>0x7f800000U;}
bool lt(Word a,Word b){
    if(nan(a)||nan(b))return true; // ARM LT includes unordered (N!=V).
    if((a&0x7fffffffU)==0&&(b&0x7fffffffU)==0)return false;
    if((a^b)&0x80000000U)return (a>>31)!=0;
    return a>>31?a>b:a<b;
}
struct Failure {Error error;};
Word arithmetic(binary32_arithmetic::Result r){
    if(auto* e=std::get_if<binary32_arithmetic::Error>(&r))
        throw Failure{Error{ErrorCode::unsupported_floating_environment,e->reason,*e}};
    return std::get<Word>(r);
}
Word lookup(Word ratio){
    namespace fp=binary32_arithmetic;
    const Word scaled=arithmetic(fp::mul(ratio,0x42000000));
    const Word index=arithmetic(fp::to_unsigned_word_rtz(scaled))&0xffffU;
    if(index>=detail::table.size())throw Failure{Error{ErrorCode::angular_table_extent,"5687b8 table index"}};
    const auto& row=detail::table[index];
    const Word residual=arithmetic(fp::sub(scaled,arithmetic(fp::from_unsigned_word(index))));
    return arithmetic(fp::add(row[0],arithmetic(fp::mul(residual,row[1]))));
}
}
StateResult resolve_state(std::uint32_t flags,std::optional<std::uint8_t> stored_mode,
    std::optional<std::uint32_t> stored_heading,const Reference& reference) noexcept {
    State state{flags,stored_mode,stored_heading,{}};
    if((flags&0x4000U)&&(!stored_mode||*stored_mode==1))return state;
    if(std::holds_alternative<Unknown>(reference))return Error{ErrorCode::unknown_field,"manager+1c"};
    if(const auto* points=std::get_if<Points>(&reference))state.points=*points;
    return state;
}
Result heading(const State& s) noexcept {
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    if(s.flags&0x4000U){
        if(!s.stored_mode)return Error{ErrorCode::unknown_field,"pw+150"};
        if(*s.stored_mode==1){
            if(!s.stored_heading)return Error{ErrorCode::unknown_field,"pw+14c"};
            return *s.stored_heading;
        }
    }
    if(!s.points)return std::uint32_t{0};
    if(std::fegetround()!=FE_TONEAREST)
        return Error{ErrorCode::unsupported_floating_environment,"Reference heading requires round-to-nearest binary32 arithmetic"};
    // fegetround() alone does not detect FPCR.FZ or MXCSR FTZ/DAZ. Volatile
    // operands force a real subnormal operation instead of constant folding.
    volatile float tiny=std::bit_cast<float>(std::uint32_t{1});
    volatile float doubled=tiny+tiny;
    if(std::bit_cast<std::uint32_t>(static_cast<float>(doubled))!=2U)
        return Error{ErrorCode::unsupported_floating_environment,"Reference heading requires gradual binary32 underflow"};
    const auto& p=*s.points;
    if(!std::isfinite(p.first_x)||!std::isfinite(p.first_z)||!std::isfinite(p.second_x)||!std::isfinite(p.second_z))
        return Error{ErrorCode::nonfinite_input,"Reference coordinates must be finite"};
    const float x=sub(p.second_x,p.first_x);
    float z=sub(p.second_z,p.first_z);
    if(!std::isfinite(x)||!std::isfinite(z))return Error{ErrorCode::nonfinite_difference,"Reference coordinate differences must remain finite"};
    return angular_core(std::bit_cast<Word>(x),std::bit_cast<Word>(z));
}
Result angular_core(Word x,Word z) noexcept {
    namespace fp=binary32_arithmetic;
    if((x&0x7fffffffU)==0&&(z&0x7fffffffU)==0)return Word{0};
    try {
        Word numerator,denominator,offset;bool subtract;
        const Word minus_x=x^0x80000000U;
        if(!lt(z,0)){
            if(!lt(x,0)){
                if(!lt(z,x)){numerator=x;denominator=z;offset=0;subtract=false;}
                else {numerator=z;denominator=x;offset=0x42800000;subtract=true;}
            }else{
                if(lt(z,minus_x)){numerator=z;denominator=minus_x;offset=0x43400000;subtract=false;}
                else {numerator=minus_x;denominator=z;offset=0x43800000;subtract=true;}
            }
        }else{
            z^=0x80000000U;
            if(!lt(x,0)){
                if(!lt(z,x)){numerator=x;denominator=z;offset=0x43000000;subtract=true;}
                else {numerator=z;denominator=x;offset=0x42800000;subtract=false;}
            }else{
                if(lt(z,minus_x)){numerator=z;denominator=minus_x;offset=0x43400000;subtract=true;}
                else {numerator=minus_x;denominator=z;offset=0x43000000;subtract=false;}
            }
        }
        const Word value=lookup(arithmetic(fp::div(numerator,denominator)));
        const Word angle=arithmetic(subtract?fp::sub(offset,value):fp::add(value,offset));
        return arithmetic(fp::to_unsigned_word_rtz(arithmetic(fp::mul(angle,0x43800000))))&0xffffU;
    }catch(const Failure& e){return e.error;}

}
Result orient(const State& state,std::uint16_t angle0) noexcept {
    const auto result=heading(state);
    if(const auto* error=std::get_if<Error>(&result))return *error;
    return (std::get<std::uint32_t>(result)-0x4000U+angle0)&0xffffU;
}
} // namespace mh4u::player_reference
