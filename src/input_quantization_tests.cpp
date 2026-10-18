#include "input_quantization.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace iq=mh4u::input_quantization;
namespace {
std::uint64_t checks=0,observations=0;
void require(bool passed,const std::string& reason) {++checks;if(!passed)throw std::runtime_error(reason);}
iq::Sample checked_convert(const std::array<iq::PairInput,2>& input) {
    const auto result=iq::convert(input);require(std::holds_alternative<iq::Sample>(result),"normal rounding rejected");return std::get<iq::Sample>(result);
}
struct RoundingGuard {int value=std::fegetround();~RoundingGuard(){std::fesetround(value);}};
void semantics() {
    const auto zero=checked_convert({iq::PairInput{},iq::PairInput{255,-255}});
    require(zero==iq::Sample{},"sub-byte reduction must truncate toward zero");
    const auto axes=checked_convert({iq::PairInput{32767,0},iq::PairInput{0,-32768}});
    require(axes.pairs[0]==iq::Pair{127,0,0,0,127,8},"positive first component axis");
    require(axes.pairs[1]==iq::Pair{0,-128,270,49152,127,2},"negative second component axis");
    require(axes.direction_bits==0x410,"numeric direction mask");
    const auto low=checked_convert({iq::PairInput{29*256,0},iq::PairInput{30*256,0}});
    require(low.pairs[0]==iq::Pair{29,0,0,0,0,0},"dead zone below 30");
    require(low.pairs[1]==iq::Pair{30,0,0,0,30,0},"magnitude 30 survives dead zone");
    const auto sector=checked_convert({iq::PairInput{36*256,0},iq::PairInput{37*256,0}});
    require(sector.pairs[0].magnitude==36&&sector.pairs[0].sector==0,"sector below 37");
    require(sector.pairs[1].magnitude==37&&sector.pairs[1].sector==8,"sector at 37");
    const auto saturated=checked_convert({iq::PairInput{127*256,127*256},iq::PairInput{-128*256,-128*256}});
    require(saturated.pairs[0]==iq::Pair{127,127,45,8192,127,9},"positive diagonal saturation");
    require(saturated.pairs[1]==iq::Pair{-128,-128,225,40960,127,6},"negative diagonal saturation");
    const auto wrap=checked_convert({iq::PairInput{std::numeric_limits<std::int32_t>::max(),std::numeric_limits<std::int32_t>::min()},iq::PairInput{65536,-65536}});
    require(wrap.pairs[0]==iq::Pair{-1,0,0,0,0,0}&&wrap.pairs[1]==iq::Pair{},"signed32 low-byte wrap");
    RoundingGuard guard;
    for(int mode:{FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}) {
        require(std::fesetround(mode)==0,"rounding mode unavailable for negative check");
        require(iq::convert({})==iq::Result{iq::Error::unsupported_rounding},"unsupported rounding accepted");
        require(std::fegetround()==mode,"converter mutated rounding environment");
    }
}
class Input {
    std::ifstream stream;
public:
    explicit Input(const char* path):stream(path,std::ios::binary) {if(!stream)throw std::runtime_error("oracle cannot be opened");}
    unsigned number(unsigned size) {
        unsigned value=0;
        for(unsigned i=0;i<size;++i){const int byte=stream.get();if(byte<0)throw std::runtime_error("truncated oracle");value|=static_cast<unsigned>(byte)<<(8*i);}
        return value;
    }
    unsigned header(const std::string& magic) {
        for(unsigned char byte:magic)if(number(1)!=byte)throw std::runtime_error("oracle magic mismatch");
        const auto count=number(4);if(count==0||count>65536)throw std::runtime_error("oracle count out of bounds");return count;
    }
    void finish() {if(stream.get()!=std::char_traits<char>::eof())throw std::runtime_error("trailing oracle bytes");}
};
struct Expected {std::uint16_t degrees,angle;std::uint8_t magnitude,sector;};
Expected expected(Input& in) {return {static_cast<std::uint16_t>(in.number(2)),static_cast<std::uint16_t>(in.number(2)),static_cast<std::uint8_t>(in.number(1)),static_cast<std::uint8_t>(in.number(1))};}
void s8_oracle(const char* path) {
    Input in(path);require(in.header(std::string("M4IQv1\0\0",8))==65536,"s8 oracle coverage differs");
    std::vector<Expected> rows;rows.reserve(65536);
    for(unsigned i=0;i<65536;++i)rows.push_back(expected(in));in.finish();
    for(int x=-128;x<128;++x)for(int y=-128;y<128;++y) {
        const auto result=checked_convert({iq::PairInput{x*256,y*256},iq::PairInput{y*256,x*256}});
        for(unsigned ch=0;ch<2;++ch) {
            const int first=ch==0?x:y,second=ch==0?y:x;const auto& row=rows[(first+128)*256+second+128];
            const iq::Pair want{static_cast<std::int8_t>(first),static_cast<std::int8_t>(second),row.degrees,row.angle,row.magnitude,row.sector};
            require(result.pairs[ch]==want,"ARM pair mismatch x="+std::to_string(x)+" y="+std::to_string(y)+" channel="+std::to_string(ch));++observations;
        }
    }
}
void s32_oracle(const char* path) {
    Input in(path);const auto count=in.header(std::string("M4IQb1\0\0",8));
    for(unsigned i=0;i<count;++i) {
        std::array<iq::PairInput,2> input;
        for(auto& pair:input) {pair.first=std::bit_cast<std::int32_t>(in.number(4));pair.second=std::bit_cast<std::int32_t>(in.number(4));}
        iq::Sample want;
        for(auto& pair:want.pairs) {
            pair.first=std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(in.number(1)));pair.second=std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(in.number(1)));
            const auto row=expected(in);pair.degrees=row.degrees;pair.angle=row.angle;pair.magnitude=row.magnitude;pair.sector=row.sector;
        }
        want.direction_bits=static_cast<std::uint16_t>(in.number(2));
        require(checked_convert(input)==want,"ARM signed32 boundary mismatch case="+std::to_string(i));++observations;
    }
    in.finish();
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=1&&argc!=5)throw std::runtime_error("usage: input-quantization-tests [--s8-oracle PATH --s32-oracle PATH]");
        semantics();
        if(argc==5) {
            if(std::string(argv[1])!="--s8-oracle"||std::string(argv[3])!="--s32-oracle")throw std::runtime_error("unknown oracle arguments");
            s8_oracle(argv[2]);s32_oracle(argv[4]);
        }
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_observations\":"<<observations<<"}\n";
        return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
