#include "binary32_arithmetic.hpp"
#include <bit>
#include <cfenv>
#include <limits>
#include <optional>

#if defined(__FAST_MATH__)
#error "binary32_arithmetic requires nonfused IEEE binary32 arithmetic"
#endif
namespace mh4u::binary32_arithmetic {
namespace {
constexpr Word sign=0x80000000U,exponent=0x7f800000U,fraction=0x007fffffU;
constexpr Word quiet=0x00400000U,default_nan=0x7fc00000U;
bool nan(Word v){return (v&exponent)==exponent&&(v&fraction)!=0;}
bool signaling(Word v){return nan(v)&&(v&quiet)==0;}
bool infinity(Word v){return (v&~sign)==exponent;}
bool zero(Word v){return (v&~sign)==0;}
std::optional<Word> propagated(Word left,Word right){
    // ARM11 FPSCR.DN=0: first signaling operand, else first quiet operand.
    // Subtraction does not invert the NaN sign before propagation.
    if(signaling(left))return left|quiet;
    if(signaling(right))return right|quiet;
    if(nan(left))return left;
    if(nan(right))return right;
    return {};
}
std::optional<Error> environment(){
    static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559&&
        std::numeric_limits<float>::radix==2&&std::numeric_limits<float>::digits==24);
    if(std::fegetround()!=FE_TONEAREST)return Error{ErrorCode::unsupported_environment,"Requires round-to-nearest ties-to-even"};
    volatile float tiny=std::bit_cast<float>(Word{1});
    volatile float doubled=tiny+tiny;
    if(std::bit_cast<Word>(float(doubled))!=2U)return Error{ErrorCode::unsupported_environment,"Requires gradual input and output underflow"};
    return {};
}
}
Result add(Word left,Word right){
    if(auto error=environment())return *error;
    if(auto result=propagated(left,right))return *result;
    if(infinity(left)&&infinity(right))return (left^right)&sign?default_nan:left;
    if(infinity(left))return left;
    if(infinity(right))return right;
    const volatile float result=std::bit_cast<float>(left)+std::bit_cast<float>(right);
    return std::bit_cast<Word>(float(result));
}
Result sub(Word left,Word right){
    if(auto error=environment())return *error;
    if(auto result=propagated(left,right))return *result;
    if(infinity(left)&&infinity(right))return (left^right)&sign?left:default_nan;
    if(infinity(left))return left;
    if(infinity(right))return right^sign;
    const volatile float result=std::bit_cast<float>(left)-std::bit_cast<float>(right);
    return std::bit_cast<Word>(float(result));
}
Result mul(Word left,Word right){
    if(auto error=environment())return *error;
    if(auto result=propagated(left,right))return *result;
    if((infinity(left)&&zero(right))||(zero(left)&&infinity(right)))return default_nan;
    const Word result_sign=(left^right)&sign;
    if(infinity(left)||infinity(right))return result_sign|exponent;
    if(zero(left)||zero(right))return result_sign;
    const volatile float result=std::bit_cast<float>(left)*std::bit_cast<float>(right);
    return std::bit_cast<Word>(float(result));
}
Result div(Word left,Word right){
    if(auto error=environment())return *error;
    if(auto result=propagated(left,right))return *result;
    if((infinity(left)&&infinity(right))||(zero(left)&&zero(right)))return default_nan;
    const Word result_sign=(left^right)&sign;
    if(infinity(left)||zero(right))return result_sign|exponent;
    if(infinity(right)||zero(left))return result_sign;
    const volatile float result=std::bit_cast<float>(left)/std::bit_cast<float>(right);
    return std::bit_cast<Word>(float(result));
}
Result from_signed_word(Word value){
    if(auto error=environment())return *error;
    const volatile float result=static_cast<float>(std::bit_cast<std::int32_t>(value));
    return std::bit_cast<Word>(float(result));
}
Result from_unsigned_word(Word value){
    if(auto error=environment())return *error;
    const volatile float result=static_cast<float>(value);
    return std::bit_cast<Word>(float(result));
}
Result to_signed_word_rtz(Word value){
    if(auto error=environment())return *error;
    if(nan(value))return Word{0};
    const Word magnitude=value&~sign;
    if(magnitude>=0x4f000000U)return value&sign?Word{0x80000000U}:Word{0x7fffffffU};
    // Only finite values strictly inside the signed range reach the host cast.
    return std::bit_cast<Word>(static_cast<std::int32_t>(std::bit_cast<float>(value)));
}
Result to_unsigned_word_rtz(Word value){
    if(auto error=environment())return *error;
    if(nan(value)||(value&sign))return Word{0};
    if(value>=0x4f800000U)return Word{0xffffffffU};
    // The host conversion cannot see NaN, negative values or overflow.
    return static_cast<Word>(std::bit_cast<float>(value));
}
} // namespace mh4u::binary32_arithmetic
