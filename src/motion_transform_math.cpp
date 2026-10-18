#include "motion_transform_math.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#ifdef __FAST_MATH__
#error "motion_transform_math requires ordered binary32 arithmetic"
#endif
namespace mh4u::motion_transform_math {
namespace {
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view text){throw Failure{{code,text}};}
float value(std::uint32_t b){return std::bit_cast<float>(b);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
void environment(){
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    if(std::fegetround()!=FE_TONEAREST)fail(ErrorCode::floating_environment,"Nearest-even arithmetic required");
    volatile float tiny=value(1),twice=tiny+tiny;
    if(bits(twice)!=2)fail(ErrorCode::floating_environment,"Gradual underflow required");
}
float input(float x){if(!std::isfinite(x))fail(ErrorCode::nonfinite_operand,"Consumed operand is nonfinite");return x;}
float output(float x){if(!std::isfinite(x))fail(ErrorCode::numeric_overflow,"Nonfinite binary32 intermediate");return x;}
float add(float a,float b){input(a);input(b);volatile float r=a+b;return output(r);}
float sub(float a,float b){input(a);input(b);volatile float r=a-b;return output(r);}
float mul(float a,float b){input(a);input(b);volatile float r=a*b;return output(r);}
float div(float a,float b){input(a);input(b);volatile float r=a/b;return output(r);}
float squared_norm(const std::array<float,4>& q){
    float norm=mul(q[0],q[0]);for(unsigned i=1;i<4;++i)norm=add(norm,mul(q[i],q[i]));return norm;
}
Bits4 unit(const std::array<float,4>& q,std::string_view zero_message){
    const float norm=squared_norm(q);
    if(norm<=0)fail(ErrorCode::degenerate_quaternion,zero_message);
    volatile float root=std::sqrt(norm);output(root);volatile float inverse=1.0F/root;output(inverse);
    Bits4 out;for(unsigned i=0;i<4;++i)out[i]=bits(mul(q[i],inverse));return out;
}
template<class T,class F>Result<T> guarded(F fn){try{environment();return fn();}catch(const Failure& f){return f.error;}}
}
Result<std::uint32_t> multiply(std::uint32_t a,std::uint32_t b){return guarded<std::uint32_t>([&]{return bits(mul(value(a),value(b)));});}
Result<std::uint32_t> add(std::uint32_t a,std::uint32_t b){return guarded<std::uint32_t>([&]{return bits(add(value(a),value(b)));});}
Result<std::uint32_t> divide(std::uint32_t a,std::uint32_t b){return guarded<std::uint32_t>([&]{return bits(div(value(a),value(b)));});}
Result<Bits3> lerp(const Bits3& a,const Bits3& b,std::uint32_t weight){return guarded<Bits3>([&]{
    const float w=value(weight),one=sub(1,w);Bits3 out;
    for(unsigned i=0;i<3;++i)out[i]=bits(add(mul(value(a[i]),one),mul(value(b[i]),w)));
    return out;
});}
Result<Bits3> add_scaled(const Bits3& a,const Bits3& b,std::uint32_t weight){return guarded<Bits3>([&]{
    Bits3 out;for(unsigned i=0;i<3;++i)out[i]=bits(add(value(a[i]),mul(value(b[i]),value(weight))));return out;
});}
Result<Bits4> nlerp(const Bits4& a,const Bits4& b,std::uint32_t weight){return guarded<Bits4>([&]{
    float dot=mul(value(a[0]),value(b[0]));
    for(unsigned i=1;i<4;++i)dot=add(dot,mul(value(a[i]),value(b[i])));
    const float w=value(weight),one=sub(1,w);std::array<float,4> v;
    for(unsigned i=0;i<4;++i){const float incoming=value(b[i]^(dot<0?0x80000000U:0U));v[i]=add(mul(value(a[i]),one),mul(incoming,w));}
    return unit(v,"Quaternion blend has zero norm");
});}
Result<Bits4> multiply_rotation(const Bits4& a,const Bits4& b){return guarded<Bits4>([&]{
    const float x=value(a[0]),y=value(a[1]),z=value(a[2]),w=value(a[3]);
    const float X=value(b[0]),Y=value(b[1]),Z=value(b[2]),W=value(b[3]);
    return Bits4{bits(sub(add(add(mul(w,X),mul(x,W)),mul(y,Z)),mul(z,Y))),
                 bits(sub(add(add(mul(w,Y),mul(y,W)),mul(z,X)),mul(x,Z))),
                 bits(sub(add(add(mul(w,Z),mul(z,W)),mul(x,Y)),mul(y,X))),
                 bits(sub(sub(sub(mul(w,W),mul(x,X)),mul(y,Y)),mul(z,Z)))};
});}
Result<Bits4> inverse_rotation(const Bits4& q,const std::optional<Bits4>& fallback){return guarded<Bits4>([&]{
    const std::array<float,4> v{value(q[0]),value(q[1]),value(q[2]),value(q[3])};
    const float norm=squared_norm(v);
    if(norm<=0){if(!fallback)fail(ErrorCode::missing_template,"Inverse quaternion fallback unknown");return *fallback;}
    volatile float inverse=1.0F/norm;output(inverse);
    Bits4 out;for(unsigned i=0;i<4;++i)out[i]=bits(mul(v[i],inverse))^(i<3?0x80000000U:0U);return out;
});}
Result<Bits4> normalize_rotation(const Bits4& q){return guarded<Bits4>([&]{
    return unit({value(q[0]),value(q[1]),value(q[2]),value(q[3])},"Quaternion has zero norm");
});}
Result<Bits3> scale_additive(const Bits3& a,const Bits3& b,const Bits3& reference,std::uint32_t weight){return guarded<Bits3>([&]{
    Bits3 out;for(unsigned i=0;i<3;++i){const float delta=sub(value(b[i]),value(reference[i]));
        out[i]=bits(mul(value(a[i]),add(mul(delta,value(weight)),value(reference[i]))));}return out;
});}
Result<Matrix> rt_matrix(const Bits4& q,const Bits3& t){return guarded<Matrix>([&]{
    const float x=value(q[0]),y=value(q[1]),z=value(q[2]),w=value(q[3]);
    const float x2=mul(x,2),y2=mul(y,2),z2=mul(z,2);
    const float yy=mul(y,y2),wy=mul(w,y2),zz=mul(z,z2),xx=mul(x,x2);
    const float wx=mul(w,x2),wz=mul(w,z2),yz=mul(y,z2),xy=mul(x,y2),xz=mul(x,z2);
    return Matrix{bits(sub(1,add(yy,zz))),bits(add(xy,wz)),bits(sub(xz,wy)),0,
        bits(sub(xy,wz)),bits(sub(1,add(xx,zz))),bits(add(yz,wx)),0,
        bits(add(xz,wy)),bits(sub(yz,wx)),bits(sub(1,add(xx,yy))),0,
        t[0],t[1],t[2],0x3f800000U};
});}
Result<Matrix> matrix_product(const Matrix& a,const Matrix& b){return guarded<Matrix>([&]{
    Matrix out;
    for(unsigned row=0;row<4;++row)for(unsigned col=0;col<4;++col){
        float v=mul(value(a[4*row]),value(b[col]));
        for(unsigned k=1;k<4;++k)v=add(v,mul(value(a[4*row+k]),value(b[4*k+col])));
        out[4*row+col]=bits(v);
    }
    return out;
});}
Result<Matrix> scale_rows(const Matrix& a,const Bits3& scale){return guarded<Matrix>([&]{
    Matrix out=a;for(unsigned row=0;row<3;++row)for(unsigned col=0;col<4;++col)
        out[4*row+col]=bits(mul(value(a[4*row+col]),value(scale[row])));
    return out;
});}
Result<Matrix> attachment_basis(const Matrix& a,std::uint32_t mode,const std::optional<Basis>& reference){
    if(mode==0){
        if(!reference)return Error{ErrorCode::missing_template,"Attachment basis template unknown"};
        Matrix out=a;for(unsigned i=0;i<12;++i)out[i]=(*reference)[i];return out;
    }
    if(mode>2)return a;
    return guarded<Matrix>([&]{
        Matrix out=a;
        for(unsigned row=0;row<3;++row){
            float norm=mul(value(a[row*4]),value(a[row*4]));
            for(unsigned col=1;col<4;++col)norm=add(norm,mul(value(a[row*4+col]),value(a[row*4+col])));
            volatile float length=std::sqrt(norm);output(length);
            if(mode==1){
                if(bits(length)>=0x34000000U){volatile float inverse=1.0F/length;output(inverse);
                    for(unsigned col=0;col<4;++col)out[row*4+col]=bits(mul(value(a[row*4+col]),inverse));}
            }else for(unsigned col=0;col<4;++col)out[row*4+col]=col==row?bits(length):0;
        }
        return out;
    });
}
Result<Palette> palette_product(const Matrix& source,const Matrix& world,const Bits3& context){return guarded<Palette>([&]{
    Palette out;
    for(unsigned col=0;col<3;++col)for(unsigned row=0;row<4;++row){
        float v=mul(value(source[4*row]),value(world[col]));
        for(unsigned k=1;k<4;++k)v=add(v,mul(value(source[4*row+k]),value(world[4*k+col])));
        if(row==3)v=add(v,value(context[col]));
        out[4*col+row]=bits(v);
    }
    return out;
});}
} // namespace mh4u::motion_transform_math
