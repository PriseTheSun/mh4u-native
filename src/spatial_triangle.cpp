#include "spatial_triangle.hpp"
#include <bit>
#include <cmath>
#include <limits>

#ifdef __FAST_MATH__
#error "spatial_triangle requires ordered binary32 arithmetic"
#endif

namespace mh4u::spatial_triangle {
namespace {
namespace math=motion_transform_math;
using Word=std::uint32_t;
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
// For finite binary32 values under RNE, SUB(a,b) equals ADD(a,sign_flip(b)),
// including signed zero. Reuse the arithmetic owner without reassociation.
Word sub(Word a,Word b,std::string_view field){return add(a,neg(b),field);}
float value(Word w){return std::bit_cast<float>(w);}
Word result(float f,std::string_view field){
    if(!std::isfinite(f))fail(ErrorCode::numeric_overflow,field);
    return std::bit_cast<Word>(f);
}
Word divide(Word a,Word b,std::string_view field){
    // Both operands were already checked by preceding owner operations. No
    // caller can enter this operation before the owner's FP environment check.
    volatile float divided=value(a)/value(b);return result(divided,field);
}
Word square_root(Word a,std::string_view field){
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    volatile float root=std::sqrt(value(a));return result(root,field);
}
Word plus_product(Word total,Word a,Word b,std::string_view field){const auto product=mul(a,b,field);return add(total,product,field);}
Word minus_product(Word total,Word a,Word b,std::string_view field){const auto product=mul(a,b,field);return sub(total,product,field);}
}

Result<bool> normal_gate(const Bits3& n,const Bits3& v){
    try{
        auto dot=mul(v[0],n[0],"normal gate X");
        dot=plus_product(dot,v[1],n[1],"normal gate Y");
        dot=plus_product(dot,v[2],n[2],"normal gate Z");
        return value(dot)<=0.0F;
    }catch(const Failure& failure){return failure.error;}
}

Result<Output> intersect(const Triangle& triangle,const Vertical& query,bool write_hit){
    try{
        const auto& a=triangle.vertices[0];const auto& b=triangle.vertices[1];const auto& c=triangle.vertices[2];
        // cc9520..cc9568: preserve the actual interleaved cross product order.
        const auto ux=sub(b[0],a[0],"edge1 X");
        const auto uy=sub(b[1],a[1],"edge1 Y");
        const auto uz=sub(b[2],a[2],"edge1 Z");
        const auto vx=sub(c[0],a[0],"edge2 X");
        const auto vy=sub(c[1],a[1],"edge2 Y");
        const auto vz=sub(c[2],a[2],"edge2 Z");
        auto ny=mul(uz,vx,"cross Y first");
        auto nz=mul(ux,vy,"cross Z first");
        auto nx=mul(uy,vz,"cross X first");
        ny=minus_product(ny,ux,vz,"cross Y second");
        nz=minus_product(nz,uy,vx,"cross Z second");
        nx=minus_product(nx,uz,vy,"cross X second");
        auto norm2=mul(nx,nx,"normal square X");
        norm2=plus_product(norm2,ny,ny,"normal square Y");
        norm2=plus_product(norm2,nz,nz,"normal square Z");
        const auto length=square_root(norm2,"normal length");
        if(std::bit_cast<std::int32_t>(length)>=0x34000000){
            const auto inverse=divide(0x3f800000U,length,"normal reciprocal");
            nx=mul(nx,inverse,"normalized X");ny=mul(ny,inverse,"normalized Y");nz=mul(nz,inverse,"normalized Z");
        }
        auto plane=mul(a[0],nx,"plane X");
        plane=plus_product(plane,a[1],ny,"plane Y");
        plane=plus_product(plane,a[2],nz,"plane Z");plane=neg(plane);
        // 1eb400 consumes direction before any origin component.
        if(!query.direction_y)fail(ErrorCode::unknown_operand,"direction Y");
        const auto denominator=mul(ny,*query.direction_y,"plane direction product");
        if((denominator&0x7fffffffU)<0x3727c5acU)return Output{};
        if(!query.origin)fail(ErrorCode::unknown_operand,"origin");
        const auto& origin=*query.origin;
        const auto x_distance=plus_product(plane,origin[0],nx,"distance X");
        auto yz_distance=mul(origin[1],ny,"distance Y");
        yz_distance=plus_product(yz_distance,origin[2],nz,"distance Z");
        const auto distance=add(yz_distance,x_distance,"distance sum");
        const auto t=divide(neg(distance),denominator,"intersection parameter");
        if(value(t)<0.0F)return Output{};
        // 1eb4cc uses a separate multiply then add; X/Z are raw copies.
        const auto py=plus_product(origin[1],*query.direction_y,t,"intersection height");
        const auto px=origin[0],pz=origin[2];
        const auto normal_sign=value(distance)>=0.0F?0x80000000U:0U;
        const Bits4 hit_normal{nx^normal_sign,ny^normal_sign,nz^normal_sign,0};
        // cc9608..cc965c: all three oriented XZ expressions are evaluated
        // before their ordered comparisons; equality is admitted.
        const auto pa_x=sub(px,a[0],"inside PA X");
        const auto ba_z=sub(b[2],a[2],"inside BA Z");
        const auto ac_z=sub(a[2],c[2],"inside AC Z");
        const auto pa_z=sub(pz,a[2],"inside PA Z");
        const auto cb_z=sub(c[2],b[2],"inside CB Z");
        const auto pb_x=sub(px,b[0],"inside PB X");
        const auto pc_x=sub(px,c[0],"inside PC X");
        const auto ba_x=sub(b[0],a[0],"inside BA X");
        const auto ac_x=sub(a[0],c[0],"inside AC X");
        const auto pb_z=sub(pz,b[2],"inside PB Z");
        auto e0=mul(pa_x,ba_z,"inside edge0 first");
        const auto cb_x=sub(c[0],b[0],"inside CB X");
        const auto pc_z=sub(pz,c[2],"inside PC Z");
        auto e1=mul(pb_x,cb_z,"inside edge1 first");
        auto e2=mul(pc_x,ac_z,"inside edge2 first");
        e0=minus_product(e0,ba_x,pa_z,"inside edge0 second");
        e1=minus_product(e1,cb_x,pb_z,"inside edge1 second");
        e2=minus_product(e2,ac_x,pc_z,"inside edge2 second");
        if(value(e0)<0.0F||value(e1)<0.0F||value(e2)<0.0F)return Output{};
        Output output{true,{}};
        if(write_hit)output.hit=Hit{{px,py,pz,0},hit_normal,0,t,1};
        return output;
    }catch(const Failure& failure){return failure.error;}
}
} // namespace mh4u::spatial_triangle
