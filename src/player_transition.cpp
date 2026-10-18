#include "player_transition.hpp"
#include "player_input.hpp"
#include "source_rng.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>

#if defined(__FAST_MATH__)
#error "player_transition requires strict binary32 semantics; disable fast-math"
#endif

namespace mh4u::player_transition {
namespace {
constexpr std::string_view source_sha="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435";
bool valid_sha(std::string_view s) {
    return s.size()==64 && std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
std::uint32_t word(std::span<const std::uint8_t> b,std::size_t o) {
    return std::uint32_t{b[o]}|(std::uint32_t{b[o+1]}<<8)|(std::uint32_t{b[o+2]}<<16)|(std::uint32_t{b[o+3]}<<24);
}
float multiply(float left,float right) {
    // Every VFP multiply rounds to binary32; no fused or host-double shortcut.
    volatile float result=left*right; return result;
}
}
bool local_control(const GlobalFields& global,const PbwFields& pbw) noexcept {
    if(global.field2b==2)return pbw.field30!=3 && pbw.field33==0;
    const auto signed_index=global.field2f<128?int(global.field2f):int(global.field2f)-256;
    return signed_index==int(pbw.field33);
}
std::string_view error_name(ErrorCode code) noexcept {
    switch(code) {
    case ErrorCode::invalid_expected:return "invalid_expected";
    case ErrorCode::constants_identity:return "constants_identity";
    case ErrorCode::unsupported_constants:return "unsupported_constants";
    case ErrorCode::type_table_extent:return "type_table_extent";
    case ErrorCode::optional_dispatch_body:return "optional_dispatch_body";
    case ErrorCode::early_optional_helpers:return "early_optional_helpers";
    case ErrorCode::old_state7_helpers:return "old_state7_helpers";
    case ErrorCode::type13_helpers:return "type13_helpers";
    case ErrorCode::angular_reference:return "angular_reference";
    case ErrorCode::unsupported_float_environment:return "unsupported_float_environment";
    case ErrorCode::sector_failure:return "sector_failure";
    }
    return "unknown";
}
Result<Constants> Constants::decode(std::span<const std::uint8_t> bytes,const ExpectedConstants& expected) {
    if(!valid_sha(expected.package_sha256)||!valid_sha(expected.source_elf_sha256))
        return Error{ErrorCode::invalid_expected,"Expected independent lowercase SHA-256 pins"};
    if(bytes.size()!=88)return Error{ErrorCode::unsupported_constants,"Expected exactly 88 bytes"};
    if(picosha2::hash256_hex_string(bytes.begin(),bytes.end())!=expected.package_sha256)
        return Error{ErrorCode::constants_identity,"Constant package digest differs"};
    if(std::string_view(reinterpret_cast<const char*>(bytes.data()),8)!="MH4UPT01")
        return Error{ErrorCode::unsupported_constants,"Unsupported constant profile"};
    const auto elf=picosha2::bytes_to_hex_string(bytes.begin()+8,bytes.begin()+40);
    if(elf!=source_sha || elf!=expected.source_elf_sha256)
        return Error{ErrorCode::constants_identity,"Constants do not belong to this code profile"};
    if(word(bytes,40)!=source_rng::source_multiplier)
        return Error{ErrorCode::unsupported_constants,"RNG multiplier differs from shipped2b555c literal"};
    Constants value;value.scale_bits_=word(bytes,44);
    value.power_bits_=word(bytes,48);value.angle_offset_=word(bytes,52);
    // Bound the numeric behavior of independently pinned constants before use.
    if(value.scale_bits_!=0x3f8ccccdU || value.power_bits_!=0x3f866666U || value.angle_offset_!=0x1555U)
        return Error{ErrorCode::unsupported_constants,"Unsupported numeric constant variant"};
    std::copy(bytes.begin()+56,bytes.end(),value.type_bytes_.begin());return value;
}
Result<Output> apply(const State& input,const Request& request,const Constants& constants) {
    if(input.pbw.field31>=16)return Error{ErrorCode::type_table_extent,"Source type is outside the retained 32-byte table"};
    if(input.global.field5d!=0)return Error{ErrorCode::optional_dispatch_body,"Original optional dispatcher body is outside this profile"};
    const bool local_gate=local_control(input.global,input.pbw);const auto type=input.pbw.field31;
    if(local_gate) {
        if((type==4||type==5||type==6||type==15)&&!(input.pbw.field3d4&0x10000000U))
            return Error{ErrorCode::early_optional_helpers,"Early conditional helpers are outside this profile"};
        if(input.pbw.field371==7&&request.state_arg!=7)
            return Error{ErrorCode::old_state7_helpers,"Leaving source state7 needs external helpers"};
        if(type==13)return Error{ErrorCode::type13_helpers,"Source type13 helper path is outside this profile"};
        if((request.flags_arg&0x9000U)&&input.pbw.field451!=0)
            return Error{ErrorCode::angular_reference,"This transition profile requires reference_flag0 for angular queries"};
    }
    if(!std::numeric_limits<float>::is_iec559||sizeof(float)!=4||std::fegetround()!=FE_TONEAREST)
        return Error{ErrorCode::unsupported_float_environment,"Requires binary32 and round-to-nearest, matching FPSCR0"};
    Output output{input,request.flags_arg,std::nullopt};auto& s=output.state;auto& flags=output.effective_flags;
    s.pw.fieldf8=s.pw.fieldf0;s.pw.fieldfc=s.pw.fieldf4;s.pw.fieldf0=0;s.pw.fieldf4=0;
    s.pbw.fields5={};s.pbw.field377=s.pbw.field371;s.pbw.field378=s.pbw.field372;
    s.pbw.field371=static_cast<std::uint8_t>(request.state_arg);s.pbw.field372=static_cast<std::uint16_t>(request.action_arg);
    s.pw.field4c=0;
    // Execute original global update before copying its result to pw+bc.
    const auto old_bc=s.pw.fieldbc;
    s.global.field22=source_rng::advance_seed(s.global.field22);
    s.pw.fieldbc=(flags&0x20U)?static_cast<std::uint16_t>((s.global.field22&0xff00U)|(old_bc&255U)):s.global.field22;
    s.pw.field48c=0;s.pbw.field39c=0x3f800000U;s.pw.fieldc4=0x3f800000U;
    s.pw.policy=player_policy_table::install(player_policy_table::Absent{},0);
    // The earlier conditional flag1 rewrite is unreachable in the admitted profile.
    if(!(flags&1U)){s.pw.fielddc=0;s.player.field3b8=0;}
    s.pw.field17c=0;
    if(flags&0x400U)s.pw.fieldb0=static_cast<std::uint16_t>(s.pbw.field24-8192U);
    if(flags&0x800U)s.pw.fieldb0=static_cast<std::uint16_t>(s.pbw.field24+8192U);
    if(local_gate&&(flags&0x9000U)) {
        const bool bit1000=flags&0x1000U,bit8000=flags&0x8000U;flags&=~0x9000U;
        std::uint32_t offset=0;
        switch(type) {
        case 0:offset=constants.angle_offset_;break;
        case 1:offset=bit8000?4096U:8192U;break;
        case 3:case 11:offset=bit8000?constants.angle_offset_:4096U;break;
        case 7:offset=bit8000?8192U:4096U;break;
        case 14:offset=bit8000?(bit1000?5120U:8192U):4096U;break;
        case 2:case 8:case 9:case 12:offset=4096U;break;
        default:break;
        }
        const player_input::Channels channels{s.pbw.angles,s.pbw.magnitudes};
        const player_input::SectorScope scope{0,s.pbw.field451};
        const auto positive=player_input::in_sector(channels,0x800,scope);
        if(std::holds_alternative<player_input::Error>(positive))return Error{ErrorCode::sector_failure,"Sector owner rejected angular query"};
        if(std::get<bool>(positive))s.pw.fieldb0+=offset;
        else {
            const auto negative=player_input::in_sector(channels,0x400,scope);
            if(std::holds_alternative<player_input::Error>(negative))return Error{ErrorCode::sector_failure,"Sector owner rejected angular query"};
            if(std::get<bool>(negative))s.pw.fieldb0-=offset;
        }
    }
    if(!(flags&0x4000U))s.pw.field261=0;
    if(!(flags&0x100U))s.pw.fields488[1]&=static_cast<std::uint8_t>(~1U);
    if(flags&0x80U)s.pw.fields488[1]|=1U;
    if(!(flags&0x2000U))s.player.fieldsd10={};
    if(s.pw.fieldf8&0x01000000U){s.pbw.field20=0;s.pbw.field28=0;}
    s.pw.fields35d={0,255};s.pw.fields3f5={0,255};
    s.pw.fields290={constants.type_bytes_[type*2],constants.type_bytes_[type*2+1]};
    if(type==7&&(s.pw.field1f4&0x08000000U))s.pw.fields290[0]=0x71;
    float factor=(s.pw.field1f4&0x4000U)?std::bit_cast<float>(constants.scale_bits_):1.0F;
    if(std::find(s.pbw.fields2f8.begin(),s.pbw.fields2f8.end(),38)!=s.pbw.fields2f8.end()) {
        const auto power=std::bit_cast<float>(constants.power_bits_);
        if(s.pw.field261&1U)factor=multiply(factor,power);
        for(unsigned i=s.pw.field261>>1;i>0;--i){const auto first=multiply(factor,power);factor=multiply(first,power);}
    }
    for(auto& value:s.pw.fields290)value=static_cast<std::uint8_t>(static_cast<std::uint32_t>(multiply(static_cast<float>(value),factor)));
    s.pw.field58=0;s.pw.field54=0;s.pw.fieldsc={};s.pw.fields488[2]=0;s.pw.fields488[0]=0;
    s.pw.fielde8&=~(0x20U|0x40U|0x10000U);s.pw.fieldec&=~(0x80000U|0x100000U|0x200000U|8U);
    s.pw.field9d8=0;s.pw.field334=0;
    // Concrete uPlayer virtual+134 contains only BX LR, verified from source.
    if(local_gate) {
        if(request.state_arg!=6)s.pw.field504=65535;
        else if(s.pw.field490)s.pw.field504=s.pw.field4c2;
        if(!(flags&2U)) {
            const auto selector=(flags&0x10U)?6U:(request.state_arg==0&&request.action_arg==147)?9U:1U;
            output.returned_dispatch_call=ObservedDispatchCall{selector,flags};
            s.pw.field79=4;
        }
    }
    s.pw.field5e4=0;s.pw.field6c0=0;s.pw.field33b=0;s.pw.fieldf0|=0x20000000U;
    return output;
}
} // namespace mh4u::player_transition
