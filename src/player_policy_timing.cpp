#include "player_policy_timing.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>

#if defined(__FAST_MATH__)
#error "player_policy_timing requires strict binary32; disable fast-math"
#endif
namespace mh4u::player_policy_timing {
namespace {
float add(float a,float b)noexcept{volatile float v=a+b;return v;}
float sub(float a,float b)noexcept{volatile float v=a-b;return v;}
float mul(float a,float b)noexcept{volatile float v=a*b;return v;}
std::int32_t truncate_saturate(float f)noexcept{
    // ARM VCVT.s32.f32 saturates finite out-of-range input, truncating toward0.
    if(f>=2147483648.0f)return std::numeric_limits<std::int32_t>::max();
    if(f<=-2147483648.0f)return std::numeric_limits<std::int32_t>::min();
    return static_cast<std::int32_t>(f);
}
}
Result query(const Context& c,const Query& q)noexcept{
    if(!std::numeric_limits<float>::is_iec559||sizeof(float)!=4||std::fegetround()!=FE_TONEAREST)
        return Error{ErrorCode::floating_environment,"FPSCR0/binary32"};
    volatile float tiny=std::bit_cast<float>(1U),twice=tiny+tiny;
    if(std::bit_cast<std::uint32_t>(static_cast<float>(twice))!=2U)
        return Error{ErrorCode::floating_environment,"Gradual underflow"};
    const auto& l=c.layer;const auto& f=c.fields.fieldsd48;
    const auto operand=[](std::string_view field){return Error{ErrorCode::nonfinite_operand,field};};
    const auto overflow=[](std::string_view field){return Error{ErrorCode::numeric_overflow,field};};
    const float advance=std::bit_cast<float>(f[4]);
    float remaining=0;
    if(!(f[6]&1U)){
        if(!std::isfinite(l.remaining_interpolation))return operand("layer.remaining_interpolation");
        if(l.remaining_interpolation>0)remaining=l.remaining_interpolation;
    }
    const bool active=remaining>0;
    float lower=0,upper=0;bool wrapped=false;
    if(active&&q.mode==0){
        if(!c.delta_time_bits)return Error{ErrorCode::unknown_delta,"model.delta_time"};
        const float delta=std::bit_cast<float>(*c.delta_time_bits);
        if(!std::isfinite(delta))return operand("model.delta_time");
        const float difference=sub(l.remaining_interpolation,delta);
        if(!std::isfinite(difference))return overflow("remaining-delta");
        if(difference>=0)return false;
        if(!std::isfinite(l.frame))return operand("layer.frame");
        if(!std::isfinite(l.speed))return operand("layer.speed");
        const float product=mul(difference,l.speed);
        if(!std::isfinite(product))return overflow("difference*speed");
        upper=sub(l.frame,product);
        if(!std::isfinite(upper))return overflow("frame-product");
        if(!std::isfinite(l.previous_frame))return operand("layer.previous_frame");
        lower=l.previous_frame;
    }else if(active&&(q.mode==2||q.mode==6||q.mode==7||q.mode==8||q.mode==10)){
        if(!std::isfinite(l.previous_frame))return operand("layer.previous_frame");
        lower=upper=l.previous_frame;
    }else{
        if(active)return false;
        if(!std::isfinite(l.previous_frame))return operand("layer.previous_frame");
        if(!std::isfinite(advance))return operand("fields.d58");
        upper=add(l.previous_frame,advance);lower=sub(l.previous_frame,remaining);
        if(!std::isfinite(upper)||!std::isfinite(lower))return overflow("frame bounds");
        const float adjusted=add(upper,std::bit_cast<float>(0x3a03126fU));
        if(!std::isfinite(adjusted))return overflow("frame+epsilon");
        const auto rounded=truncate_saturate(adjusted);
        if(rounded!=truncate_saturate(upper))upper=static_cast<float>(rounded);
        const float limit=std::bit_cast<float>(f[5]);
        if(!std::isfinite(limit))return operand("fields.d5c");
        if(limit<=upper){
            if(!std::isfinite(l.field58))return operand("layer.field58");
            const float excess=sub(upper,limit);
            if(!std::isfinite(excess))return overflow("upper-limit");
            if(l.field58>0){upper=add(l.field58,excess);wrapped=true;}
            else upper=limit;
            if(!std::isfinite(upper))return overflow("loop+excess");
        }
    }
    // Source defaults still prepare bounds; only threshold reads are absent.
    if(q.mode>12)return false;
    const float a=std::bit_cast<float>(q.lower_bits),b=std::bit_cast<float>(q.upper_bits);
    if(!std::isfinite(a))return operand("query.lower");
    if((q.mode==3||q.mode==4||q.mode==5||q.mode==7||q.mode==9||q.mode==10||q.mode==11||q.mode==12)&&!std::isfinite(b))return operand("query.upper");
    switch(q.mode){
    case 0:return wrapped?((a<=upper&&l.field58<=a)||lower<a):(lower<a&&a<=upper);
    case 1:case 6:
        if(wrapped){
            if(a<=upper)return true;
            const float unwrapped=add(lower,advance);
            if(!std::isfinite(unwrapped))return overflow("lower+advance");
            return a<=unwrapped;
        }
        return a<=upper;
    case 2:return upper<a;
    case 3:case 7:return upper>=a&&upper<=b;
    case 4:return upper>a&&upper<=b;
    case 5:return upper>=a&&upper<b;
    case 8:return lower<a;
    case 9:return (lower>=a&&lower<=b)||(upper>=a&&upper<=b);
    case 10:return upper>=a&&(lower<=b||upper<=b);
    case 11:return (lower>a&&lower<=b)||(upper>a&&upper<=b);
    case 12:return (lower>=a&&lower<b)||(upper>=a&&upper<b);
    default:return false;
    }
}
}
