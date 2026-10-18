#include "input_filter.hpp"
#include "input_quantization.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace f=mh4u::input_filter;
namespace q=mh4u::input_quantization;
namespace {
std::uint64_t checks=0,filter_observations=0,normalizer_observations=0,composition_observations=0;
void require(bool condition,const std::string& reason) {++checks;if(!condition)throw std::runtime_error(reason);}
f::Config config(unsigned mode,std::int16_t inner,std::int16_t outer) {
    f::Config result{static_cast<std::uint8_t>(mode),{{{-7,-8},{-9,-10}}}};
    if(mode<2)result.limits[mode]={inner,outer};return result;
}
template<class T,class R>T accepted(R result,const std::string& message) {
    require(std::holds_alternative<T>(result),message);return std::get<T>(result);
}
struct RoundingGuard {int saved=std::fegetround();~RoundingGuard(){std::fesetround(saved);}};
void semantics() {
    for(unsigned mode=0;mode<2;++mode) {
        const auto c=config(mode,40,145);
        require(f::filter(c,{40,0})==f::FilterResult{f::Axes{}} ,"deadzone edge");
        require(f::filter(c,{41,0})==f::FilterResult{f::Axes{1,0}},"deadzone next");
        require(f::filter(c,{145,0})==f::FilterResult{f::Axes{105,0}},"outer edge");
        require(f::filter(c,{-145,0})==f::FilterResult{f::Axes{-105,0}},"signed filter");
        require(f::filter(c,{146,0})==f::FilterResult{f::Axes{105,0}},"outside outer");
        require(f::filter(config(mode,0,1),{})==f::FilterResult{f::Axes{}},"zero inner");
        for(auto bad:{f::Limits{-1,145},f::Limits{40,40},f::Limits{40,39},f::Limits{0,-1}}) {
            auto invalid=c;invalid.limits[mode]=bad;
            require(f::filter(invalid,{})==f::FilterResult{f::Error::invalid_limits},"invalid filter limits accepted");
            require(f::normalize(invalid,{})==f::NormalizeResult{f::Error::invalid_limits},"invalid normalizer limits accepted");
        }
        const auto n=accepted<f::Normalized>(f::normalize(c,{-109,105}),"normalizer rejected");
        require(n.ratios[0]<-1.0f&&n.words[0]==-34015&&n.ratios[1]==1.0f&&n.words[1]==32767,"negative asymmetry/positive clamp");
        const auto extreme=accepted<f::Normalized>(f::normalize(config(mode,0,1),{-32768,32767}),"extreme normalize rejected");
        require(extreme.words==std::array<std::int32_t,2>{-1073709056,32767},"signed16 normalization boundary");
        const auto zero=accepted<f::Normalized>(f::normalize(c,{}),"zero normalize rejected");
        require(std::bit_cast<std::uint32_t>(zero.ratios[0])==0&&zero.words[1]==0,"positive zero");
    }
    for(unsigned mode:{2u,3u,255u}) {
        require(f::filter(config(mode,40,145),{})==f::FilterResult{f::Error::unsupported_mode},"unsupported filter mode accepted");
        require(f::normalize(config(mode,40,145),{})==f::NormalizeResult{f::Error::unsupported_mode},"unsupported normalizer mode accepted");
    }
    require(f::filter(config(0,0,32767),{362,5})==f::FilterResult{f::Axes{362,5}},"safe root boundary");
    require(f::filter(config(0,0,32767),{362,6})==f::FilterResult{f::Error::unsafe_arithmetic},"signed shift overflow accepted");
    require(f::filter(config(0,32766,32767),{32000,0})==f::FilterResult{f::Axes{}},"large valid radial early return");
    require(f::filter(config(1,0,32767),{32766,0})==f::FilterResult{f::Axes{32766,0}},"large valid axial early return");
    for(unsigned mode:{0u,1u})require(f::filter(config(mode,0,32767),{-32768,-32768})==f::FilterResult{f::Error::unsafe_arithmetic},"signed squared-sum overflow accepted");
    const std::array<f::Config,2> configs{config(0,40,145),config(1,36,145)};
    const std::array<f::Axes,2> input{f::Axes{-145,0},f::Axes{145,0}};
    const auto result=accepted<f::Sample>(f::compose(configs,input),"composition rejected");
    require(result.filtered==std::array<f::Axes,2>{f::Axes{-105,0},f::Axes{109,0}},"per-pair filter selection");
    require(result.normalized[0].words[0]==-31564&&result.normalized[1].words[0]==32767,"both pairs must normalize through config1");
    auto invalid=configs;invalid[1].source_mode=2;
    require(f::compose(invalid,input)==f::ComposeResult{f::Failure{f::Error::unsupported_mode,f::Phase::filter,1}},"transactional pair failure");
    const auto before_configs=configs;const auto before_input=input;
    for(int mode:{FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}) {
        RoundingGuard guard;require(std::fesetround(mode)==0,"rounding mode unavailable");
        require(f::normalize(configs[0],{})==f::NormalizeResult{f::Error::unsupported_rounding},"unsupported rounding accepted");
        require(f::compose(configs,input)==f::ComposeResult{f::Failure{f::Error::unsupported_rounding,f::Phase::normalize,0}},"transactional rounding failure");
        require(f::filter(configs[0],input[0])==f::FilterResult{f::Axes{-105,0}},"integer filter must not depend on rounding");
        require(std::fegetround()==mode,"rounding environment mutated");
    }
    require(configs==before_configs&&input==before_input,"inputs mutated");
}
class Reader {
    std::ifstream stream;
public:
    explicit Reader(const char* path):stream(path,std::ios::binary){if(!stream)throw std::runtime_error("cannot open oracle");}
    std::uint32_t number(unsigned size) {
        std::uint32_t value=0;
        for(unsigned i=0;i<size;++i){int byte=stream.get();if(byte<0)throw std::runtime_error("truncated oracle");value|=static_cast<std::uint32_t>(byte)<<(i*8);}
        return value;
    }
    std::int16_t s16(){return std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(number(2)));}
    unsigned header(const std::string& magic) {
        for(unsigned char byte:magic)if(number(1)!=byte)throw std::runtime_error("oracle magic");
        const auto count=number(4);if(!count||count>1000000)throw std::runtime_error("oracle count");return count;
    }
    void finish(){if(stream.get()!=std::char_traits<char>::eof())throw std::runtime_error("trailing oracle bytes");}
    f::Config config(){f::Config c;c.source_mode=static_cast<std::uint8_t>(number(1));for(auto& l:c.limits){l.inner=s16();l.outer=s16();}return c;}
    f::Axes axes(){const auto first=s16();const auto second=s16();return {first,second};}
};
void filter_oracle(const char* path) {
    Reader in(path);const auto count=in.header(std::string("M4IFf1\0\0",8));
    for(unsigned i=0;i<count;++i) {
        const auto c=in.config();const auto input=in.axes();const auto expected=in.axes();
        require(f::filter(c,input)==f::FilterResult{expected},"filter ARM mismatch row="+std::to_string(i));++filter_observations;
    }
    in.finish();
}
std::array<f::Normalized,2> normalized(Reader& in) {
    std::array<f::Normalized,2> expected;
    for(auto& n:expected)for(auto& ratio:n.ratios)ratio=std::bit_cast<float>(in.number(4));
    for(auto& n:expected)for(auto& word:n.words)word=std::bit_cast<std::int32_t>(in.number(4));
    return expected;
}
bool exact(const f::Normalized& first,const f::Normalized& second) {
    return first.words==second.words&&std::bit_cast<std::uint32_t>(first.ratios[0])==std::bit_cast<std::uint32_t>(second.ratios[0])&&
        std::bit_cast<std::uint32_t>(first.ratios[1])==std::bit_cast<std::uint32_t>(second.ratios[1]);
}
void normalizer_oracle(const char* path) {
    Reader in(path);const auto count=in.header(std::string("M4IFn1\0\0",8));
    for(unsigned i=0;i<count;++i) {
        const auto c=in.config();const std::array<f::Axes,2> input{in.axes(),in.axes()};const auto expected=normalized(in);
        for(unsigned ch=0;ch<2;++ch) {
            const auto actual=accepted<f::Normalized>(f::normalize(c,input[ch]),"normalizer rejected ARM case");
            require(exact(actual,expected[ch]),"normalizer ARM mismatch row="+std::to_string(i)+" pair="+std::to_string(ch));normalizer_observations+=2;
        }
    }
    in.finish();
}
void composition_oracle(const char* path) {
    Reader in(path);const auto count=in.header(std::string("M4IFc1\0\0",8));
    for(unsigned i=0;i<count;++i) {
        const std::array<f::Config,2> configs{in.config(),in.config()};
        const std::array<f::Axes,2> input{in.axes(),in.axes()},filtered{in.axes(),in.axes()};const auto expected=normalized(in);
        q::Sample quantized;
        for(auto& p:quantized.pairs) {
            p.first=std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(in.number(1)));p.second=std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(in.number(1)));
            p.degrees=static_cast<std::uint16_t>(in.number(2));p.angle=static_cast<std::uint16_t>(in.number(2));p.magnitude=static_cast<std::uint16_t>(in.number(1));p.sector=static_cast<std::uint8_t>(in.number(1));
        }
        quantized.direction_bits=static_cast<std::uint16_t>(in.number(2));
        const auto actual=accepted<f::Sample>(f::compose(configs,input),"composition rejected ARM case");
        require(actual.filtered==filtered&&exact(actual.normalized[0],expected[0])&&exact(actual.normalized[1],expected[1]),"composition ARM mismatch row="+std::to_string(i));
        const std::array<q::PairInput,2> words{q::PairInput{actual.normalized[0].words[0],actual.normalized[0].words[1]},q::PairInput{actual.normalized[1].words[0],actual.normalized[1].words[1]}};
        require(q::convert(words)==q::Result{quantized},"composition quantization ARM mismatch row="+std::to_string(i));++composition_observations;
    }
    in.finish();
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=1&&argc!=7)throw std::runtime_error("usage: input-filter-tests [--filter-oracle PATH --normalizer-oracle PATH --composition-oracle PATH]");
        semantics();
        if(argc==7) {
            if(std::string(argv[1])!="--filter-oracle"||std::string(argv[3])!="--normalizer-oracle"||std::string(argv[5])!="--composition-oracle")throw std::runtime_error("unknown oracle argument");
            filter_oracle(argv[2]);normalizer_oracle(argv[4]);composition_oracle(argv[6]);
        }
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"filter_observations\":"<<filter_observations<<",\"normalizer_axis_observations\":"<<normalizer_observations<<",\"composition_observations\":"<<composition_observations<<"}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
