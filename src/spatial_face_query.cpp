#include "spatial_face_query.hpp"
#include <bit>
#include <cmath>

#ifdef __FAST_MATH__
#error "spatial_face_query requires ordered binary32 arithmetic"
#endif

namespace mh4u::spatial_face_query {
namespace {
namespace math=motion_transform_math;
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view field){throw Failure{{code,field,{}}};}
Word consume(math::Result<Word> value,std::string_view field){
    if(const auto* e=std::get_if<math::Error>(&value)){
        ErrorCode code=ErrorCode::unsupported_arithmetic;
        if(e->code==math::ErrorCode::floating_environment)code=ErrorCode::floating_environment;
        else if(e->code==math::ErrorCode::nonfinite_operand)code=ErrorCode::nonfinite_operand;
        else if(e->code==math::ErrorCode::numeric_overflow)code=ErrorCode::numeric_overflow;
        throw Failure{{code,field,*e}};
    }
    return std::get<Word>(value);
}
Word mul(Word a,Word b,std::string_view field){return consume(math::multiply(a,b),field);}
Word add(Word a,Word b,std::string_view field){return consume(math::add(a,b),field);}
Word neg(Word a){return a^0x80000000U;}
// RNE finite SUB is ADD with the second sign flipped, including signed zero.
Word sub(Word a,Word b,std::string_view field){return add(a,neg(b),field);}
Word plus_product(Word total,Word a,Word b,std::string_view field){
    const auto product=mul(a,b,field);return add(total,product,field);
}
Word minus_product(Word total,Word a,Word b,std::string_view field){
    const auto product=mul(a,b,field);return sub(total,product,field);
}
float value(Word word){return std::bit_cast<float>(word);}
float finite(Word word,std::string_view field){
    const auto out=value(word);if(!std::isfinite(out))fail(ErrorCode::nonfinite_operand,field);return out;
}
}

Result<std::optional<Replacement>> intersect(const Face& face,const Vertical& ray,
                                            std::optional<Word> maximum_t){
    try{
        const auto& a=face.vertices[0];const auto& b=face.vertices[1];const auto& c=face.vertices[2];
        const auto& n=face.source_normal;
        // 18023c..180274: VNMLA is (-acc)-product, not -(acc+product).
        auto plane=mul(a[0],n[0],"plane X");
        plane=plus_product(plane,a[1],n[1],"plane Y");
        plane=minus_product(neg(plane),a[2],n[2],"plane Z");
        if(!ray.direction_y)fail(ErrorCode::unknown_operand,"direction Y");
        const auto direction=*ray.direction_y;
        auto orientation=mul(0,n[0],"orientation X");
        orientation=plus_product(orientation,direction,n[1],"orientation Y");
        orientation=plus_product(orientation,0,n[2],"orientation Z");
        if(value(orientation)>0.0F)return std::optional<Replacement>{};
        const auto denominator=mul(n[1],direction,"plane direction product");
        if((denominator&0x7fffffffU)<0x3727c5acU)return std::optional<Replacement>{};
        if(!ray.origin)fail(ErrorCode::unknown_operand,"origin");
        const auto& origin=*ray.origin;
        // 1804c8,1804d0,1804d8,1804dc. Preserve interleaved consumption as
        // well as the two separate sums; do not replace with a generic dot.
        auto yz_distance=mul(origin[1],n[1],"distance Y");
        const auto x_distance=plus_product(plane,origin[0],n[0],"distance X");
        yz_distance=plus_product(yz_distance,origin[2],n[2],"distance Z");
        const auto distance=add(yz_distance,x_distance,"distance sum");
        const auto t=consume(math::divide(neg(distance),denominator),"intersection parameter");
        if(value(t)<0.0F)return std::optional<Replacement>{};
        const auto py=plus_product(origin[1],direction,t,"intersection height");
        if(!maximum_t)fail(ErrorCode::unknown_operand,"maximum t");
        if(value(t)>finite(*maximum_t,"maximum t"))return std::optional<Replacement>{};
        const auto px=origin[0],pz=origin[2];
        // 18030c..180398 differs in consumption order from cc9608..cc965c.
        // Evaluate all expressions before the ordered comparisons, including
        // when edge0 would reject. B/C Y are not used by this source branch.
        const auto ba_x=sub(b[0],a[0],"inside BA X");
        const auto ac_x=sub(a[0],c[0],"inside AC X");
        const auto pa_x=sub(px,a[0],"inside PA X");
        const auto cb_x=sub(c[0],b[0],"inside CB X");
        const auto ba_z=sub(b[2],a[2],"inside BA Z");
        const auto cb_z=sub(c[2],b[2],"inside CB Z");
        const auto pb_x=sub(px,b[0],"inside PB X");
        const auto pb_z=sub(pz,b[2],"inside PB Z");
        const auto ac_z=sub(a[2],c[2],"inside AC Z");
        const auto pa_z=sub(pz,a[2],"inside PA Z");
        const auto pc_x=sub(px,c[0],"inside PC X");
        const auto pc_z=sub(pz,c[2],"inside PC Z");
        auto e0=mul(pa_x,ba_z,"inside edge0 first");
        auto e1=mul(pb_x,cb_z,"inside edge1 first");
        auto e2=mul(pc_x,ac_z,"inside edge2 first");
        e0=minus_product(e0,ba_x,pa_z,"inside edge0 second");
        e1=minus_product(e1,cb_x,pb_z,"inside edge1 second");
        e2=minus_product(e2,ac_x,pc_z,"inside edge2 second");
        if(value(e0)<0.0F||value(e1)<0.0F||value(e2)<0.0F)return std::optional<Replacement>{};
        return std::optional<Replacement>{Replacement{{px,py,pz,0},
            {0x80000000U,neg(direction),0x80000000U,0},
            {n[0],n[1],n[2],plane},t,0}};
    }catch(const Failure& failure){return failure.error;}
}
} // namespace mh4u::spatial_face_query
