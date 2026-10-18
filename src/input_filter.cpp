#include "input_filter.hpp"
#include <cfenv>
#include <limits>

#ifdef __FAST_MATH__
#error "input_filter requires strict floating-point operations"
#endif

namespace mh4u::input_filter {
namespace {
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
std::variant<Limits,Error> selected(const Config& config) noexcept {
    if(config.source_mode>1)return Error::unsupported_mode;
    const auto limits=config.limits[config.source_mode];
    if(limits.inner<0||limits.outer<=limits.inner)return Error::invalid_limits;
    return limits;
}
std::int32_t integer_root(std::uint32_t n) noexcept {
    std::uint32_t low=0,high=46341;
    while(low+1<high) {
        const auto middle=(low+high)/2;
        if(middle*middle<=n)low=middle;else high=middle;
    }
    return static_cast<std::int32_t>(low);
}
float divide(float a,float b) noexcept {volatile float result=a/b;return result;}
float multiply(float a,float b) noexcept {volatile float result=a*b;return result;}
}

FilterResult filter(const Config& config,Axes input) noexcept {
    const auto selection=selected(config);
    if(const auto error=std::get_if<Error>(&selection))return *error;
    const auto limits=std::get<Limits>(selection);
    const std::int64_t inner=limits.inner,outer=limits.outer,difference=outer-inner;
    std::int64_t x=input.first,y=input.second;
    if(config.source_mode==1) {
        const auto reduce=[inner](std::int64_t value) {
            if(value>inner)return value-inner;
            if(value<-inner)return value+inner;
            return std::int64_t{0};
        };
        x=reduce(x);y=reduce(y);
    }
    // Wider arithmetic only checks the original domain; it never wraps an
    // overflowing ARM intermediate into an apparently valid native result.
    const auto n=x*x+y*y;
    if(n>std::numeric_limits<std::int32_t>::max())return Error::unsafe_arithmetic;
    if(config.source_mode==0&&n<=inner*inner)return Axes{};
    if(config.source_mode==1&&n<difference*difference)
        return Axes{static_cast<std::int16_t>(x),static_cast<std::int16_t>(y)};
    if(n==0||n>131071)return Error::unsafe_arithmetic;
    const auto root=integer_root(static_cast<std::uint32_t>(n)*16384u);
    const auto scale=config.source_mode==1||n>=outer*outer?difference*128:root-inner*128;
    // With signed16 input/limits and the root-domain bound, these products fit
    // signed32 and quotients fit signed16; the checks above establish those bounds.
    const auto output=[root,scale](std::int64_t value){return value*scale/root;};
    const auto first=output(x),second=output(y);
    return Axes{static_cast<std::int16_t>(first),static_cast<std::int16_t>(second)};
}

NormalizeResult normalize(const Config& config,Axes filtered) noexcept {
    const auto selection=selected(config);
    if(const auto error=std::get_if<Error>(&selection))return *error;
    if(std::fegetround()!=FE_TONEAREST)return Error::unsupported_rounding;
    const auto limits=std::get<Limits>(selection);
    const int denominator=static_cast<int>(limits.outer)-limits.inner;
    const std::array<int,2> values{filtered.first,filtered.second};
    Normalized result;
    for(unsigned i=0;i<2;++i) {
        const auto value=values[i];
        const float ratio=value==0?0.0f:value>=denominator?1.0f:
            divide(static_cast<float>(value),static_cast<float>(denominator));
        result.ratios[i]=ratio;
        result.words[i]=static_cast<std::int32_t>(multiply(ratio,32767.0f));
    }
    return result;
}

ComposeResult compose(const std::array<Config,2>& configs,const std::array<Axes,2>& input) noexcept {
    Sample result;
    for(std::uint8_t i=0;i<2;++i) {
        const auto filtered=filter(configs[i],input[i]);
        if(const auto error=std::get_if<Error>(&filtered))return Failure{*error,Phase::filter,i};
        result.filtered[i]=std::get<Axes>(filtered);
    }
    for(std::uint8_t i=0;i<2;++i) {
        const auto normalized=normalize(configs[1],result.filtered[i]);
        if(const auto error=std::get_if<Error>(&normalized))return Failure{*error,Phase::normalize,i};
        result.normalized[i]=std::get<Normalized>(normalized);
    }
    return result;
}
}
