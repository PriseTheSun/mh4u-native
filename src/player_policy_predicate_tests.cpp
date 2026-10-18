#include "player_policy_predicate.hpp"
#include "picosha2.h"
#include <bit>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>

namespace pp=mh4u::player_policy_predicate;
namespace pi=mh4u::player_input;
namespace {
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t at{};
    std::span<const std::uint8_t> take(std::size_t count) {
        if(count>bytes.size()-at)throw std::runtime_error("truncated fixture");
        const auto out=bytes.subspan(at,count);at+=count;return out;
    }
    std::uint32_t word() {
        const auto b=take(4);return b[0]|(std::uint32_t{b[1]}<<8)|(std::uint32_t{b[2]}<<16)|(std::uint32_t{b[3]}<<24);
    }
};
void require(bool value,const char* reason){if(!value)throw std::runtime_error(reason);}
}
int main(int argc,char** argv) try {
    if(argc!=3)throw std::runtime_error("expected fixture path and independent SHA256");
    std::ifstream stream(argv[1],std::ios::binary|std::ios::ate);
    if(!stream)throw std::runtime_error("fixture unavailable");
    const auto size=stream.tellg();require(size>=12&&size<=64*1024*1024,"fixture outside byte budget");
    stream.seekg(0);std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.read(reinterpret_cast<char*>(bytes.data()),size);require(static_cast<bool>(stream),"fixture read failed");
    require(picosha2::hash256_hex_string(bytes.begin(),bytes.end())==argv[2],"fixture identity mismatch");
    Reader r{bytes};const auto magic=r.take(8);
    require(std::string_view(reinterpret_cast<const char*>(magic.data()),magic.size())=="MH4UPP01","fixture version");
    const auto count=r.word();require(count<=bytes.size()/130,"fixture count exceeds storage");
    for(std::uint32_t i=0;i<count;++i) {
        const auto record=r.take(22);const auto p=pp::decode(std::span<const std::uint8_t,22>(record.data(),22));
        std::array<std::uint32_t,27> v{};for(auto& x:v)x=r.word();
        require(v[26]<=(bytes.size()-r.at)/4,"flag words exceed fixture");
        std::vector<std::optional<std::uint32_t>> flags;flags.reserve(v[26]);
        for(std::uint32_t n=0;n<v[26];++n)flags.emplace_back(r.word());
        pp::Context c{};
        c.channels={{static_cast<std::uint16_t>(v[0]),static_cast<std::uint16_t>(v[1])},
                    {static_cast<std::uint16_t>(v[2]),static_cast<std::uint16_t>(v[3])}};
        c.magnitude_gate={static_cast<std::uint8_t>(v[4]),static_cast<std::uint8_t>(v[5]),static_cast<std::uint8_t>(v[6])};
        c.field3a8=static_cast<std::uint16_t>(v[7]);c.field3ac=static_cast<std::uint16_t>(v[8]);
        c.field3b8=static_cast<std::uint16_t>(v[9]);c.field3bc=static_cast<std::uint16_t>(v[10]);
        c.reference_flag=static_cast<std::uint8_t>(v[11]);
        mh4u::player_reference::State reference{v[13],static_cast<std::uint8_t>(v[14]),v[15],std::nullopt};
        if(v[20])reference.points=mh4u::player_reference::Points{std::bit_cast<float>(v[21]),std::bit_cast<float>(v[22]),std::bit_cast<float>(v[23]),std::bit_cast<float>(v[24])};
        c.reference=pi::ReferenceContext{v[12],reference};c.field24=v[16];c.field154=v[17];
        c.direction_bounds=std::array<std::uint16_t,2>{static_cast<std::uint16_t>(v[18]),static_cast<std::uint16_t>(v[19])};
        c.flags3d4=flags;const auto result=pp::evaluate(p,c);
        if(!std::holds_alternative<bool>(result)||std::get<bool>(result)!=(v[25]!=0)) {
            std::cerr<<"case="<<i<<" predicate="<<(p.word&0x3fff)<<" expected="<<v[25];
            if(const auto* e=std::get_if<pp::Error>(&result))std::cerr<<" error="<<static_cast<int>(e->code)<<" field="<<e->field;
            else std::cerr<<" actual="<<std::get<bool>(result);
            throw std::runtime_error("ARM predicate mismatch");
        }
    }
    require(r.at==bytes.size(),"unexpected fixture tail");
    std::size_t boundaries=0;
    const auto check=[&](bool ok){require(ok,"boundary mismatch");++boundaries;};
    pp::Context empty{};
    for(auto id:{0,31,39,0x3fff}) {
        const auto result=pp::evaluate({static_cast<std::uint16_t>(id),{}},empty);
        check(std::holds_alternative<bool>(result)&&std::get<bool>(result)==(id==0));
    }
    check(std::holds_alternative<pp::Error>(pp::evaluate({8,{}},empty)));
    auto c=empty;c.magnitude_gate={4,1,1};
    check(std::get<bool>(pp::evaluate({12,{}},c)));
    check(!std::get<bool>(pp::evaluate({23,{65535,0,0}},c))); // no flags required after gate
    c.magnitude_gate={0,std::nullopt,std::nullopt};c.channels.magnitudes={110,110};
    check(std::get<bool>(pp::evaluate({8,{}},c)));
    check(std::holds_alternative<pp::Error>(pp::evaluate({22,{}},c)));
    std::array<std::optional<std::uint32_t>,5> flags{0,std::nullopt,0,0,1};c.flags3d4=flags;
    check(std::get<bool>(pp::evaluate({25,{}},c)));
    check(!std::get<bool>(pp::evaluate({22,{}},c)));
    check(std::holds_alternative<pp::Error>(pp::evaluate({22,{32,0,0}},c)));
    c.field3bc=0;
    check(!std::get<bool>(pp::evaluate({1,{}},c))); // masked arc ignores missing angle context
    check(!std::get<bool>(pp::evaluate({33,{3,1,0}},empty))); // invalid sector ignores gate
    c.reference_flag=1;c.reference=pi::ReferenceContext{0,{0,0,0,mh4u::player_reference::Points{0,0,std::bit_cast<float>(0x7f800000U),0}}};
    check(std::holds_alternative<pp::Error>(pp::evaluate({33,{0x400,0,0}},c)));
    c.channels.angles[0]=0;
    check(std::get<bool>(pp::evaluate({33,{0x400,1,0}},c))); // mode1 ignores unusable reference
    check(std::get<std::uint32_t>(pi::gated_magnitude_class(c.channels,2,{4,1,1}))==0);
    check(std::holds_alternative<pi::Error>(pi::gated_magnitude_class(c.channels,2,{0,{},{}})));
    // Six independent ARM observations: reference-mode-review/original-six.
    // The correction mode is consumed independently of the heading flag4000.
    std::array<std::optional<std::uint32_t>,33> correction_flags;correction_flags.fill(0);
    for(const auto id:{34U,35U,36U})for(const bool invert:{false,true}){
        auto corrected=empty;corrected.channels={{0,0},{110,0}};corrected.magnitude_gate={0,0,0};
        corrected.reference_flag=0;corrected.reference=pi::ReferenceContext{invert?0x8000U:0U,{0,1,std::nullopt,std::nullopt}};
        corrected.field154=invert?0U:0x8000U;corrected.direction_bounds=std::array<std::uint16_t,2>{0x4000,0xc000};
        correction_flags[0]=id==36?1U:0U;corrected.flags3d4=correction_flags;
        const pp::Predicate p{static_cast<std::uint16_t>(id),{static_cast<std::uint16_t>(id==36?0:0x400),static_cast<std::uint16_t>(id==35?0:0x400),0}};
        const auto observed=pp::evaluate(p,corrected);check(std::holds_alternative<bool>(observed)&&std::get<bool>(observed)==invert);
        corrected.reference->source.stored_mode.reset();check(std::holds_alternative<pp::Error>(pp::evaluate(p,corrected)));
    }
    std::cout<<"player-policy-predicate: "<<count<<" ARM cases; "<<boundaries<<" boundaries passed\n";
    return 0;
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
