#include "player_orientation.hpp"
#include <bit>
#include <cmath>
#include <limits>
#ifdef __FAST_MATH__
#error "player_orientation requires ordered binary32 arithmetic"
#endif
namespace mh4u::player_orientation {
namespace {
namespace math=motion_transform_math;
using Word=std::uint32_t;
constexpr Word zero=0,one=0x3f800000U;
struct Failure {math::Error cause;};
Word checked(math::Result<Word> result){
    if(const auto* error=std::get_if<math::Error>(&result))throw Failure{*error};
    return std::get<Word>(result);
}
Word add(Word a,Word b){return checked(math::add(a,b));}
Word mul(Word a,Word b){return checked(math::multiply(a,b));}
Word neg(Word a){return a^0x80000000U;}
Word sub(Word a,Word b){return add(a,neg(b));}
float value(Word b){return std::bit_cast<float>(b);}
Word bits(float v){return std::bit_cast<Word>(v);}
Word root(Word x){
    const float v=value(x);
    if(!std::isfinite(v))throw Failure{{math::ErrorCode::nonfinite_operand,"Nonfinite source sqrt operand"}};
    if(v<=0)throw Failure{{math::ErrorCode::degenerate_quaternion,"Source matrix-to-quaternion root is not positive"}};
    volatile float result=std::sqrt(v);
    if(!std::isfinite(result))throw Failure{{math::ErrorCode::numeric_overflow,"Nonfinite source sqrt result"}};
    return bits(result);
}
Word half_over(Word denominator){
    volatile float result=0.5F/value(denominator);
    if(!std::isfinite(result))throw Failure{{math::ErrorCode::numeric_overflow,"Nonfinite source reciprocal"}};
    return bits(result);
}
// Source d13f14/d12be0, restricted by b05350 to angles0..65535.
// The minimum positive argument is normal. General large-input reduction,
// sign-negative entry, errno/NaN/Inf and subnormal paths are unreachable here.
struct Reduced {Word argument;unsigned quadrant;bool tiny;};
Reduced reduce(Word x){
    const Word doubled=x<<1;
    if(doubled<0x7e921fb6U)return {x,0,doubled<0x73000000U};
    const Word n=sub(add(mul(x,0x3f22f983U),0x4b000000U),0x4b000000U);
    // Reduction integer is0..4 in this domain, so this conversion is exact
    // and representable. The source vcvt uses truncation after the RNE pair.
    const unsigned quadrant=static_cast<unsigned>(value(n))&3U;
    Word r=sub(x,mul(n,0x3fc90000U));
    r=sub(r,mul(n,0x39fda000U));
    r=sub(r,mul(n,0x33a22000U));
    r=sub(r,mul(n,0x2c34611aU));
    return {r,quadrant,false};
}
Word sine_polynomial(Word x,Word square){
    Word p=sub(0x3c0882daU,mul(square,0x394c6d33U));
    p=add(0xbe2aaaa0U,mul(square,p));
    return add(x,mul(x,mul(p,square)));
}
Word cosine_polynomial(Word square){
    Word p=add(0x3d2a9fcaU,mul(square,0xbab23ab9U));
    p=add(0xbeffffddU,mul(square,p));
    return add(one,mul(square,p));
}
struct Trig {Word sine,cosine;};
Trig trigonometry(std::uint16_t angle){
    // Unsigned16->f32 is exact; preserve both source multiplication rounds.
    const Word x=mul(mul(bits(static_cast<float>(angle)),0x40c90fdbU),0x37800000U);
    const auto r=reduce(x);
    if(r.tiny)return {x,one};
    const Word square=mul(r.argument,r.argument);
    const Word sp=sine_polynomial(r.argument,square),cp=cosine_polynomial(square);
    const Word sine=(r.quadrant&1U)?cp:sp;
    const Word cosine=(r.quadrant&1U)?sp:cp;
    return {(r.quadrant&2U)?neg(sine):sine,
            ((r.quadrant+1U)&2U)?neg(cosine):cosine};
}
math::Matrix euler_matrix(const Trig& x,const Trig& y,const Trig& z,std::uint16_t mode){
    const Word sx=x.sine,sy=y.sine,sz=z.sine,cx=x.cosine,cy=y.cosine,cz=z.cosine;
    math::Matrix matrix{};
    // Direct straight-line source orders, no runtime instruction interpreter.
    switch(mode){
    case 0: { // Source 0x6d4110..0x6d418c.
        const auto t0=mul(sz,cy); // 6d4110
        const auto t1=neg(sy); // 6d4114
        const auto t2=mul(sz,sy); // 6d411c
        const auto t3=mul(sz,sx); // 6d4120
        const auto t4=mul(cz,sy); // 6d4124
        const auto t5=mul(cz,cy); // 6d4128
        matrix[2]=t1;
        matrix[3]=zero;
        const auto t6=mul(cz,cx); // 6d4134
        const auto t7=mul(cy,sx); // 6d4138
        const auto t8=mul(cy,cx); // 6d413c
        matrix[0]=t5;
        matrix[1]=t0;
        const auto t9=mul(t4,sx); // 6d4144
        const auto t10=mul(t2,cx); // 6d4148
        const auto t11=add(t3,mul(t4,cx)); // 6d414c
        const auto t12=add(t6,mul(t2,sx)); // 6d4150
        const auto t13=sub(t9,mul(sz,cx)); // 6d4154
        const auto t14=sub(t10,mul(cz,sx)); // 6d4158
        matrix[4]=t13;
        matrix[5]=t12;
        matrix[6]=t7;
        matrix[7]=zero;
        matrix[8]=t11;
        matrix[9]=t14;
        matrix[10]=t8;
        matrix[11]=zero;
        matrix[12]=zero;
        matrix[13]=zero;
        matrix[14]=zero;
        matrix[15]=one;
        break;
    }
    case 1: { // Source 0x6d41d8..0x6d425c.
        const auto t0=neg(mul(cx,sz)); // 6d41d8
        const auto t1=mul(cx,sy); // 6d41dc
        const auto t2=mul(sx,sz); // 6d41e4
        const auto t3=neg(mul(sx,sy)); // 6d41e8
        const auto t4=mul(cx,cz); // 6d41ec
        const auto t5=neg(mul(cz,sy)); // 6d41f0
        const auto t6=neg(mul(sx,cz)); // 6d41f4
        const auto t7=mul(cy,cz); // 6d41f8
        const auto t8=mul(t0,cy); // 6d41fc
        const auto t9=mul(t1,sz); // 6d4200
        const auto t10=mul(t2,cy); // 6d4204
        const auto t11=mul(t3,sz); // 6d4208
        matrix[0]=t7;
        matrix[1]=sz;
        matrix[2]=t5;
        const auto t12=add(t8,mul(sx,sy)); // 6d4218
        const auto t13=add(t9,mul(sx,cy)); // 6d421c
        matrix[3]=zero;
        const auto t14=add(t10,mul(cx,sy)); // 6d4224
        const auto t15=add(t11,mul(cx,cy)); // 6d4228
        matrix[4]=t12;
        matrix[5]=t4;
        matrix[6]=t13;
        matrix[7]=zero;
        matrix[8]=t14;
        matrix[9]=t6;
        matrix[10]=t15;
        matrix[11]=zero;
        matrix[12]=zero;
        matrix[13]=zero;
        matrix[14]=zero;
        matrix[15]=one;
        break;
    }
    case 2: { // Source 0x6d42a8..0x6d4320.
        const auto t0=neg(mul(cx,sy)); // 6d42ac
        const auto t1=mul(sx,sy); // 6d42b0
        const auto t2=mul(sx,cy); // 6d42b4
        const auto t3=mul(cx,cy); // 6d42b8
        const auto t4=mul(cx,cz); // 6d42bc
        const auto t5=neg(mul(cx,sz)); // 6d42c0
        const auto t6=mul(cy,sz); // 6d42c4
        const auto t7=mul(cy,cz); // 6d42c8
        const auto t8=mul(sy,sz); // 6d42cc
        const auto t9=mul(cz,sy); // 6d42d0
        matrix[2]=t0;
        matrix[3]=zero;
        matrix[4]=t5;
        matrix[5]=t4;
        const auto t10=add(t6,mul(t1,cz)); // 6d42e8
        matrix[6]=sx;
        const auto t11=sub(t7,mul(t1,sz)); // 6d42f0
        matrix[7]=zero;
        matrix[10]=t3;
        const auto t12=sub(t8,mul(t2,cz)); // 6d42fc
        matrix[11]=zero;
        const auto t13=add(t9,mul(t2,sz)); // 6d4304
        matrix[12]=zero;
        matrix[13]=zero;
        matrix[14]=zero;
        matrix[15]=one;
        matrix[0]=t11;
        matrix[1]=t10;
        matrix[8]=t13;
        matrix[9]=t12;
        break;
    }
    case 3: { // Source 0x6d436c..0x6d43f4.
        const auto t0=mul(cx,sy); // 6d4370
        const auto t1=mul(sx,sy); // 6d4374
        const auto t2=neg(sz); // 6d4378
        const auto t3=mul(cx,cz); // 6d4380
        const auto t4=mul(cx,cy); // 6d4384
        const auto t5=mul(cy,sz); // 6d4388
        const auto t6=mul(cy,cz); // 6d438c
        const auto t7=mul(cz,sx); // 6d4390
        const auto t8=mul(sy,cz); // 6d4394
        const auto t9=mul(t1,sz); // 6d4398
        const auto t10=mul(t4,sz); // 6d439c
        const auto t11=mul(t5,sx); // 6d43a0
        const auto t12=mul(t0,sz); // 6d43a4
        matrix[0]=t6;
        const auto t13=add(t9,mul(cx,cy)); // 6d43ac
        const auto t14=add(t10,mul(sx,sy)); // 6d43b0
        const auto t15=sub(t11,mul(cx,sy)); // 6d43b4
        const auto t16=sub(t12,mul(cy,sx)); // 6d43b8
        matrix[1]=t14;
        matrix[2]=t15;
        matrix[3]=zero;
        matrix[4]=t2;
        matrix[7]=zero;
        matrix[8]=t8;
        matrix[9]=t16;
        matrix[10]=t13;
        matrix[11]=zero;
        matrix[12]=zero;
        matrix[13]=zero;
        matrix[14]=zero;
        matrix[15]=one;
        matrix[5]=t3;
        matrix[6]=t7;
        break;
    }
    case 4: { // Source 0x6d4448..0x6d44cc.
        const auto t0=mul(sx,sz); // 6d4448
        const auto t1=mul(sx,sy); // 6d444c
        const auto t2=mul(sy,sz); // 6d4450
        const auto t3=mul(sx,cy); // 6d4454
        const auto t4=mul(cx,sz); // 6d4458
        const auto t5=mul(cz,sx); // 6d445c
        const auto t6=mul(cy,cz); // 6d4460
        const auto t7=mul(cx,cz); // 6d4464
        const auto t8=mul(cx,sy); // 6d4468
        const auto t9=mul(cx,cy); // 6d446c
        const auto t10=mul(t0,cy); // 6d4470
        const auto t11=neg(sx); // 6d4474
        const auto t12=add(t2,mul(t3,cz)); // 6d4478
        const auto t13=mul(t5,sy); // 6d447c
        const auto t14=add(t6,mul(t1,sz)); // 6d4480
        const auto t15=sub(t10,mul(cz,sy)); // 6d4484
        const auto t16=sub(t13,mul(cy,sz)); // 6d4488
        matrix[0]=t14;
        matrix[1]=t4;
        matrix[2]=t15;
        matrix[3]=zero;
        matrix[4]=t16;
        matrix[5]=t7;
        matrix[6]=t12;
        matrix[7]=zero;
        matrix[8]=t8;
        matrix[9]=t11;
        matrix[10]=t9;
        matrix[11]=zero;
        matrix[12]=zero;
        matrix[13]=zero;
        matrix[14]=zero;
        matrix[15]=one;
        break;
    }
    case 5: { // Source 0x6d4518..0x6d459c.
        const auto t0=mul(sx,sy); // 6d451c
        const auto t1=mul(cx,sy); // 6d4520
        const auto t2=neg(mul(sx,cy)); // 6d4524
        const auto t3=mul(cx,cy); // 6d4528
        const auto t4=mul(cz,sy); // 6d452c
        const auto t5=mul(cx,sz); // 6d4530
        const auto t6=mul(sx,cz); // 6d4534
        const auto t7=mul(cy,cz); // 6d4538
        const auto t8=mul(cx,cz); // 6d453c
        const auto t9=neg(mul(sz,cy)); // 6d4540
        const auto t10=mul(sx,sz); // 6d4544
        const auto t11=add(t5,mul(t4,sx)); // 6d454c
        const auto t12=add(t6,mul(t1,sz)); // 6d4550
        matrix[0]=t7;
        matrix[3]=zero;
        matrix[4]=t9;
        const auto t13=sub(t10,mul(t8,sy)); // 6d4564
        const auto t14=sub(t8,mul(t0,sz)); // 6d4568
        matrix[5]=t14;
        matrix[6]=t12;
        matrix[9]=t2;
        matrix[10]=t3;
        matrix[7]=zero;
        matrix[8]=sy;
        matrix[11]=zero;
        matrix[12]=zero;
        matrix[13]=zero;
        matrix[14]=zero;
        matrix[15]=one;
        matrix[1]=t11;
        matrix[2]=t13;
        break;
    }
    default:break; // Domain checked by calculate before arithmetic.
    }
    return matrix;
}
Quaternion from_matrix(const math::Matrix& m){
    Quaternion q{};
    const Word trace=add(add(m[0],m[5]),m[10]);
    if(value(trace)>0){
        const Word length=root(add(trace,one)),reciprocal=half_over(length);
        q[3]=mul(length,0x3f000000U);
        q[0]=mul(sub(m[6],m[9]),reciprocal);
        q[1]=mul(sub(m[8],m[2]),reciprocal);
        q[2]=mul(sub(m[1],m[4]),reciprocal);
    }else{
        constexpr std::array<unsigned,3> next{1,2,0};
        unsigned i=value(m[0])<value(m[5])?1U:0U;
        if(value(m[i*5])<value(m[10]))i=2;
        const unsigned j=next[i],k=next[j];
        const Word length=root(add(sub(sub(m[i*5],m[j*5]),m[k*5]),one));
        const Word reciprocal=half_over(length);
        q[i]=mul(length,0x3f000000U);
        q[3]=mul(sub(m[j*4+k],m[k*4+j]),reciprocal);
        q[j]=mul(add(m[i*4+j],m[j*4+i]),reciprocal);
        q[k]=mul(add(m[i*4+k],m[k*4+i]),reciprocal);
    }
    // Original uses multiplication by-1, after the branch on W<0.
    if(value(q[3])<0)for(auto& component:q)component=mul(component,0xbf800000U);
    return q;
}
}
Result calculate(const Angles& angles,std::optional<std::uint16_t> mode){
    if(!mode)return Error{ErrorCode::unknown_mode,"Source Euler mode is unknown",{}};
    if(*mode>5)return Error{ErrorCode::unsupported_mode,"Only source Euler modes0..5 are admitted",{}};
    try{
        // Shared owner checks environment and every consumed add/multiply.
        const auto x=trigonometry(angles[0]);
        const auto y=trigonometry(angles[1]);
        const auto z=trigonometry(angles[2]);
        return from_matrix(euler_matrix(x,y,z,*mode));
    }catch(const Failure& failure){return Error{ErrorCode::arithmetic,"Source orientation arithmetic rejected",failure.cause};}
}
}
