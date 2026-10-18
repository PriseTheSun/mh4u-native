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
// Volatile boundaries retain binary32 rounding and prevent VMLA becoming FMA.
float add(float a,float b) noexcept { volatile float r=a+b;return r; }
float sub(float a,float b) noexcept { volatile float r=a-b;return r; }
float mul(float a,float b) noexcept { volatile float r=a*b;return r; }
float div(float a,float b) noexcept { volatile float r=a/b;return r; }
float lookup(float ratio) noexcept {
    const float scaled=mul(ratio,32.0f);
    const auto index=static_cast<std::uint32_t>(scaled); // Finite ratio in [0,1].
    const auto& row=detail::table[index];
    return add(std::bit_cast<float>(row[0]),mul(sub(scaled,static_cast<float>(index)),std::bit_cast<float>(row[1])));
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
    if(x==0&&z==0)return std::uint32_t{0};
    float angle;
    if(z>=0) {
        if(x>=0)angle=z>=x?lookup(div(x,z)):sub(64,lookup(div(z,x)));
        else angle=z<-x?add(192,lookup(div(z,-x))):sub(256,lookup(div(-x,z)));
    } else {
        z=-z;
        if(x>=0)angle=z>=x?sub(128,lookup(div(x,z))):add(64,lookup(div(z,x)));
        else angle=z<-x?sub(192,lookup(div(z,-x))):add(128,lookup(div(-x,z)));
    }
    return static_cast<std::uint32_t>(mul(angle,256))&0xffffU;
}
Result orient(const State& state,std::uint16_t angle0) noexcept {
    const auto result=heading(state);
    if(const auto* error=std::get_if<Error>(&result))return *error;
    return (std::get<std::uint32_t>(result)-0x4000U+angle0)&0xffffU;
}
} // namespace mh4u::player_reference
