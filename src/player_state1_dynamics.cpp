#include "player_state1_dynamics.hpp"
#include "player_state1_dynamics_table.hpp"
#include "player_controller_step.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>

#ifdef __FAST_MATH__
#error "player_state1_dynamics requires strict binary32 operations"
#endif
namespace mh4u::player_state1_dynamics {
namespace {
namespace ps=player_selection;
namespace pt=player_transition;
struct Arithmetic {
    std::optional<Error> error;
    bool environment_checked{};
    void fail(ErrorCode code,std::string_view field){if(!error)error=Error{code,field,{}};}
    bool environment(){
        if(error)return false;
        if(environment_checked)return true;
        static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
        if(std::fegetround()!=FE_TONEAREST){fail(ErrorCode::unsupported_floating_environment,"binary32 rounding");return false;}
        volatile float tiny=std::bit_cast<float>(std::uint32_t{1});
        volatile float twice=tiny+tiny;
        if(std::bit_cast<std::uint32_t>(static_cast<float>(twice))!=2){
            fail(ErrorCode::unsupported_floating_environment,"binary32 gradual underflow");return false;
        }
        environment_checked=true;return true;
    }
    std::uint32_t word(std::optional<std::uint32_t> value,std::string_view field){
        if(!value){fail(ErrorCode::unknown_field,field);return 0;}
        return *value;
    }
    float operand(std::optional<std::uint32_t> value,std::string_view field){
        const auto bits=word(value,field);if(error)return 0;
        if((bits&0x7f800000U)==0x7f800000U){fail(ErrorCode::nonfinite_operand,field);return 0;}
        if(!environment())return 0;
        return std::bit_cast<float>(bits);
    }
    float finite(float value,std::string_view field){
        if(!std::isfinite(value)){fail(ErrorCode::numeric_overflow,field);return 0;}
        return value;
    }
    float mul(float a,float b,std::string_view field){
        if(!environment())return 0;
        volatile float r=a*b;return finite(r,field);
    }
    float add(float a,float b,std::string_view field){
        if(!environment())return 0;
        volatile float r=a+b;return finite(r,field);
    }
    float sub(float a,float b,std::string_view field){
        if(!environment())return 0;
        volatile float r=a-b;return finite(r,field);
    }
    bool contains(const ps::PbwFields& fields,std::uint16_t id){
        if(error)return false;
        const auto result=ps::contains_id(fields,id);
        if(const auto* e=std::get_if<ps::Error>(&result)){
            error=Error{ErrorCode::unknown_field,"pbw+2fc",*e};return false;
        }
        return std::get<bool>(result);
    }
    // Exact 568ab8 (component0) / 568a80 (component1), no host sin/cos.
    float coefficient(std::uint32_t angle,unsigned component){
        const auto low=angle&0xffffU;const auto& row=detail::coefficients[low>>8];
        const auto fraction=mul(static_cast<float>(low&255U),0x1p-8F,"table fraction");
        const auto product=mul(std::bit_cast<float>(row[component+2]),fraction,"table slope");
        return add(std::bit_cast<float>(row[component]),product,"table interpolation");
    }
};
}
Result cost(const player_controller_step::PlayerFields& player,const player_fields::Fields& fields,
    std::optional<std::uint32_t> delta_bits,std::optional<std::uint32_t> context_ad50){
    Output output{fields,false};Arithmetic a;
    if(!pt::local_control(player.global,player.pbw))return output;
    float amount=-0.5F;
    const auto flags=a.word(fields.pw.field1f8,"pw+1f8");if(a.error)return *a.error;
    if(flags&0x80U)amount=a.mul(amount,3.0F,"b464fc multiplier3");
    else if(flags&0x40U)amount=a.mul(amount,2.0F,"b464fc multiplier2");
    if(a.error)return *a.error;
    if(a.contains(player.selection_pbw,111)||a.contains(player.selection_pbw,272))
        amount=a.mul(amount,0.5F,"b464fc multiplier0.5");
    if(a.error)return *a.error;
    if(a.contains(player.selection_pbw,112))amount=a.mul(amount,std::bit_cast<float>(0x3f99999aU),"b464fc multiplier1.2");
    if(a.error)return *a.error;
    // b2a58c repeats the original local-control read before its context gate.
    if(!pt::local_control(player.global,player.pbw))return output;
    const auto mode=a.word(context_ad50,"context+ad50");if(a.error)return *a.error;
    if(mode==100)return output;
    // amount is strictly negative in this fixed -0.5 source profile.
    if(pt::local_control(player.global,player.pbw)){
        const auto first=a.operand(fields.pw.field220_bits,"pw+220");if(a.error)return *a.error;
        if(first>0.0F)return output;
        const auto second=a.operand(fields.pw.field838_bits,"pw+838");if(a.error)return *a.error;
        if(second>0.0F)return output;
    }
    if(player.pw.field1f4&0x10000000U){
        if(a.contains(player.selection_pbw,37))amount=a.mul(amount,0.5F,"b2a58c multiplier0.5");
        else if(a.contains(player.selection_pbw,36))amount=a.mul(amount,0.25F,"b2a58c multiplier0.25");
        if(a.error)return *a.error;
    }
    const auto delta=a.operand(delta_bits,"player+14");if(a.error)return *a.error;
    amount=a.mul(delta,amount,"b2a58c delta product");if(a.error)return *a.error;
    const auto previous=a.operand(fields.pbw.field354_bits,"pbw+354");if(a.error)return *a.error;
    auto result=a.add(previous,amount,"b2a58c sum");if(a.error)return *a.error;
    if(result<=0.0F)result=0.0F;
    const auto upper=a.operand(player.selection_pbw.field358,"pbw+358");if(a.error)return *a.error;
    if(result>=upper)result=upper;
    output.fields.pbw.field354_bits=std::bit_cast<std::uint32_t>(result);output.updated=true;return output;
}
Result advance(const player_controller_step::PlayerFields& player,const player_fields::Fields& fields,
    std::optional<std::uint32_t> delta_bits){
    Output output{fields,false};Arithmetic a;
    const auto first0=a.coefficient(player.pbw.field20,0);
    const auto second0=a.coefficient(player.pbw.field20,1);
    const auto z0=a.mul(first0,0.0F,"b2a318 z0");
    const auto y0=a.mul(second0,0.0F,"b2a318 y0");
    auto z=a.add(z0,a.mul(second0,2.5F,"b2a318 z product"),"b2a318 z sum");
    const auto y=a.sub(y0,a.mul(first0,2.5F,"b2a318 y product"),"b2a318 y difference");
    if(a.error)return *a.error;
    const auto first1=a.coefficient(player.pbw.field24,0);
    const auto second1=a.coefficient(player.pbw.field24,1);
    const auto x1=a.mul(second1,0.0F,"aacf0c x1");
    const auto z1=a.mul(second1,z,"aacf0c z1");
    auto x=a.add(x1,a.mul(first1,z,"aacf0c x product"),"aacf0c x sum");
    z=a.sub(z1,a.mul(first1,0.0F,"aacf0c z product"),"aacf0c z difference");
    if(a.error)return *a.error;
    const auto first2=a.coefficient(player.pbw.field28,0);
    const auto second2=a.coefficient(player.pbw.field28,1);
    const auto final_x_product=a.mul(second2,x,"b2a318 final x product");
    const auto final_y_product=a.mul(first2,y,"b2a318 final y product");
    x=a.sub(final_x_product,final_y_product,"b2a318 final x");
    if(a.error)return *a.error;
    const auto delta=a.operand(delta_bits,"player+14");if(a.error)return *a.error;
    const auto dx=a.mul(x,delta,"b2a318 delta x");
    const auto dz=a.mul(z,delta,"b2a318 delta z");if(a.error)return *a.error;
    const auto old_x=a.operand(fields.pbw.field10_bits,"pbw+10");if(a.error)return *a.error;
    const auto dy=a.mul(0.0F,delta,"b2a318 delta y");
    const auto new_x=a.add(old_x,dx,"b2a318 position x");if(a.error)return *a.error;
    const auto old_y=a.operand(fields.pbw.field14_bits,"pbw+14");if(a.error)return *a.error;
    const auto new_y=a.add(old_y,dy,"b2a318 position y");if(a.error)return *a.error;
    const auto old_z=a.operand(fields.pbw.field18_bits,"pbw+18");if(a.error)return *a.error;
    const auto new_z=a.add(old_z,dz,"b2a318 position z");if(a.error)return *a.error;
    output.fields.pbw.field10_bits=std::bit_cast<std::uint32_t>(new_x);
    output.fields.pbw.field14_bits=std::bit_cast<std::uint32_t>(new_y);
    output.fields.pbw.field18_bits=std::bit_cast<std::uint32_t>(new_z);
    output.updated=true;return output;
}
} // namespace mh4u::player_state1_dynamics
