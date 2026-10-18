#include "player_spatial_acquisition.hpp"
#include <bit>
#include <new>
#include <stdexcept>

#ifdef __FAST_MATH__
#error "player_spatial_acquisition requires ordered binary32 arithmetic"
#endif
namespace mh4u::player_spatial_acquisition {
namespace {
namespace math=motion_transform_math;
namespace sq=spatial_query;
namespace pd=player_state1_dynamics;
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view field){throw Failure{{code,field,{}}};}
template<class T>T known(const std::optional<T>& value,std::string_view field){
    if(!value)fail(ErrorCode::unknown_field,field);
    return *value;
}
Word add(Word a,Word b,std::string_view field){
    const auto result=math::add(a,b);
    if(const auto* e=std::get_if<math::Error>(&result))throw Failure{{ErrorCode::arithmetic,field,*e}};
    return std::get<Word>(result);
}
float finite(Word word,std::string_view field){
    if((word&0x7f800000U)==0x7f800000U)fail(ErrorCode::nonfinite_operand,field);
    return std::bit_cast<float>(word);
}
Word attribute_mask(const player_controller_step::PlayerFields& p,const player_fields::Fields& f){
    // b4620c -> d05694(player,0,0). The other d05694 parameters are
    // literal zero here, so PWf8 and alternative helper routes are not read.
    Word mask=2;
    if(known(f.pbw.field478,"pbw+478")&8U)mask|=0x800U;
    if(p.pw.fieldf0&0x4000U)mask|=0x10000U;
    const auto kind=known(f.pw.field5a8,"pw+5a8");
    if(kind==17||(p.pw.fieldf0&0x20000U))mask|=0x80U;
    const auto state=p.pbw.field371;
    const auto action=p.pbw.field372;
    if(state==7&&(action==13||action==15||action==16))mask|=0x80U;
    // d051cc also returns true for state0/actions120,121; b4620c
    // explicitly excludes exactly those results from the 0x2000 bit.
    if(state==6&&(action==9||action==35||action==74||action==75||action==76||
        action==79||action==83||action==84||action==85))mask|=0x2000U;
    if(p.selection_pbw.field370==2)mask|=0x400000U;
    return mask;
}
Word wrapper_flags(const player_controller_step::PlayerFields& p,const Context& c){
    // ARM register LSL uses the low byte; byte9 already has that width.
    const auto area=known(p.selection_pbw.field9,"pbw+9");
    const auto enabled=known(c.area_enable_mask,"area manager+18");
    const Word bit=area<32?(Word{1}<<area):0;
    Word flags=(enabled&bit)?4U:0U;
    if(p.selection_pbw.field370==2)flags|=0x20000U;
    if(player_transition::local_control(p.global,p.pbw))flags|=0x40000U;
    return flags;
}
}
Result apply(const player_controller_step::PlayerFields& p,const player_fields::Fields& fields,
    const Request& request,const Context& context){
    try{
        Output out{fields,false,{}};
        const auto cache=known(fields.pw.field6a8,"pw+6a8");
        std::optional<Word> height;
        if(cache){
            out.effects.cache_used=true;
            height=fields.pw.field6a4_bits; // A nonzero override replaces this raw load.
        }else{
            auto rotated=pd::rotate_y({0,0x43960000U,0x42700000U},p.pbw.field24);
            if(const auto* e=std::get_if<pd::Error>(&rotated))throw Failure{{ErrorCode::rotation,"aacf0c offset",*e}};
            const auto offset=std::get<pd::Bits3>(rotated);
            const auto& b=fields.pbw;
            const sq::Bits3 current{
                add(offset[0],known(b.field10_bits,"pbw+10"),"offset current X"),
                add(offset[1],known(b.field14_bits,"pbw+14"),"offset current Y"),
                add(offset[2],known(b.field18_bits,"pbw+18"),"offset current Z")};
            const sq::Bits3 previous{
                add(offset[0],known(b.field6e8_bits,"pbw+6e8"),"offset previous X"),
                add(offset[1],known(b.field6ec_bits,"pbw+6ec"),"offset previous Y"),
                add(offset[2],known(b.field6f0_bits,"pbw+6f0"),"offset previous Z")};
            const auto area=known(p.selection_pbw.field9,"pbw+9");
            const auto attributes=attribute_mask(p,fields);
            const auto flags=wrapper_flags(p,context);
            if(!context.scene)fail(ErrorCode::unknown_field,"spatial scene");
            auto result=sq::height(*context.scene,{current,true,previous[1],area,flags,attributes,false},context.query_limits);
            if(const auto* e=std::get_if<sq::Error>(&result))throw Failure{{ErrorCode::query,"2ba230/2bb178",*e}};
            out.effects.current_point=current;out.effects.previous_point=previous;
            out.effects.attribute_mask=attributes;out.effects.wrapper_flags=flags;
            out.effects.height=std::get<sq::HeightResult>(std::move(result));
            const auto& acquired=*out.effects.height;height=acquired.height_bits;
            if(acquired.status==1){
                out.fields.pw.field6a4_bits=*height;
                out.fields.pw.field6a8=std::uint8_t(cache+1U);
                out.effects.cache_written=true;
            }else if(acquired.status==0)return out;
        }
        const auto limit=add(known(fields.pbw.field14_bits,"pbw+14"),request.offset_bits,"b40b88 height limit");
        if(request.override_height){
            // VCVT.F32.U32 under the nearest-even environment checked by add.
            volatile float converted=static_cast<float>(request.override_height);
            out.returned=converted>std::bit_cast<float>(limit);
        }else out.returned=finite(known(height,"pw+6a4"),"b40b88 acquired height")<std::bit_cast<float>(limit);
        return out;
    }catch(const Failure& e){return e.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::capacity,"Spatial acquisition allocation",{}};}
    catch(const std::length_error&){return Error{ErrorCode::capacity,"Spatial acquisition extent",{}};}
}
} // namespace mh4u::player_spatial_acquisition
