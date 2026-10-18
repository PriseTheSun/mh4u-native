#include "input_quantization.hpp"
#include <bit>
#include <cfenv>
#include <limits>

#ifdef __FAST_MATH__
#error "input_quantization requires strict floating-point operations"
#endif

namespace mh4u::input_quantization {
namespace {
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);

constexpr float literal(std::uint32_t bits) {return std::bit_cast<float>(bits);}
// VFP2 VMLA/VMLS round the product before addition/subtraction. Separate
// volatile stores preserve those float32 boundaries and prevent fused FMA.
float add(float a,float b) noexcept {volatile float value=a+b;return value;}
float subtract(float a,float b) noexcept {volatile float value=a-b;return value;}
float multiply(float a,float b) noexcept {volatile float value=a*b;return value;}
float divide(float a,float b) noexcept {volatile float value=a/b;return value;}
float multiply_add(float accumulator,float a,float b) noexcept {return add(accumulator,multiply(a,b));}

std::int8_t reduce(std::int32_t value) noexcept {
    const auto low=static_cast<std::uint32_t>(value/256)&255u;
    return static_cast<std::int8_t>(low<128?static_cast<int>(low):static_cast<int>(low)-256);
}

// Recovered d12724 numerical path restricted to finite signed8 pairs.
// Its exceptional float/subnormal paths are not a general-purpose atan2 API.
float angle_radians(int x,int y) noexcept {
    if(y==0)return x<0?literal(0x40490fdb):0.0f;
    if(x==0)return y<0?literal(0xbfc90fdb):literal(0x3fc90fdb);
    float numerator=static_cast<float>(y),denominator=static_cast<float>(x);
    float high=0.0f,low=0.0f;
    const auto absolute=[](int value){return value<0?-value:value;};
    if(absolute(y)>absolute(x)) {
        numerator=static_cast<float>(x);denominator=-static_cast<float>(y);
        high=literal(y<0?0xbfc90000:0x3fc90000);
        low=literal(y<0?0xb9fdaa22:0x39fdaa22);
    } else if(x<0) {
        high=literal(y<0?0xc0490000:0x40490000);
        low=literal(y<0?0xba7daa22:0x3a7daa22);
    }
    const auto nbits=std::bit_cast<std::uint32_t>(numerator),dbits=std::bit_cast<std::uint32_t>(denominator);
    float ratio;
    if(((dbits-nbits)<<1)<0x01000000u) {
        const bool opposed=((nbits^dbits)&0x80000000u)!=0;
        const float half=opposed?-0.5f:0.5f;
        if(opposed) {
            low=subtract(low,literal(0x37ce0ac3));high=subtract(high,literal(0x3eed6000));
        } else {
            low=add(low,literal(0x37ce0ac3));high=add(high,literal(0x3eed6000));
        }
        const float shifted=subtract(numerator,multiply(half,denominator));
        denominator=multiply_add(denominator,numerator,half);
        ratio=divide(shifted,denominator);
    } else ratio=divide(numerator,denominator);
    const float square=multiply(ratio,ratio);
    float polynomial=multiply_add(literal(0x3dd5b88f),square,literal(0xbd65ad2d));
    const float cube=multiply(ratio,square);
    polynomial=multiply_add(literal(0xbe11b50f),square,polynomial);
    polynomial=multiply_add(literal(0x3e4cc861),square,polynomial);
    polynomial=multiply_add(literal(0xbeaaaaa8),square,polynomial);
    low=multiply_add(low,cube,polynomial);
    return add(add(low,ratio),high);
}

// For 0<=n<=32768, floor(sqrt(n)) agrees with the original VSQRT + truncation
// on every signed8 pair. Integer search avoids a platform libm dependency.
unsigned magnitude(int x,int y) noexcept {
    const unsigned square=static_cast<unsigned>(x*x+y*y);
    unsigned low=0,high=182;
    while(low+1<high) {const auto middle=(low+high)/2;if(middle*middle<=square)low=middle;else high=middle;}
    return low>127?127:low;
}
Pair pair(PairInput input) noexcept {
    Pair result;result.first=reduce(input.first);result.second=reduce(input.second);
    float radians=angle_radians(result.first,result.second);
    if(radians<0)radians=add(radians,literal(0x40c90fdb));
    const float degrees=multiply(multiply(radians,literal(0x3e22f983)),literal(0x43b40000));
    result.degrees=static_cast<std::uint16_t>(degrees);
    result.magnitude=static_cast<std::uint16_t>(magnitude(result.first,result.second));
    if(result.magnitude<30)result.degrees=result.magnitude=0;
    result.angle=static_cast<std::uint16_t>(static_cast<std::uint32_t>(result.degrees)*65536u/360u);
    if(result.magnitude>=37) {
        constexpr std::array<std::uint8_t,16> sectors{8,9,9,1,1,5,5,4,4,6,6,2,2,10,10,8};
        const auto index=static_cast<unsigned>(multiply(static_cast<float>(result.degrees),literal(0x3d360b61)));
        result.sector=sectors[index];
    }
    return result;
}
}

Result convert(const std::array<PairInput,2>& input) noexcept {
    if(std::fegetround()!=FE_TONEAREST)return Error::unsupported_rounding;
    Sample result;
    constexpr std::array<std::array<std::uint16_t,4>,2> bits{{{{0x2000,0x1000,0x800,0x400}},{{0x20,0x10,8,4}}}};
    for(unsigned channel=0;channel<2;++channel) {
        result.pairs[channel]=pair(input[channel]);
        for(unsigned bit=0;bit<4;++bit)if(result.pairs[channel].sector&(1u<<bit))result.direction_bits|=bits[channel][bit];
    }
    return result;
}
}
