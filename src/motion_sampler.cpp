#include "motion_sampler.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>

#ifdef __FAST_MATH__
#error "motion_sampler requires ordered binary32 operations without fast-math"
#endif

namespace mh4u::motion_sampler {
namespace {
float f(std::uint32_t bits){return std::bit_cast<float>(bits);}
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
float add(float a,float b){volatile float value=a+b;return value;}
float sub(float a,float b){volatile float value=a-b;return value;}
float mul(float a,float b){volatile float value=a*b;return value;}
float div(float a,float b){volatile float value=a/b;return value;}
float square_root(float a){volatile float value=std::sqrt(a);return value;}
bool environment(){
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    if(std::fegetround()!=FE_TONEAREST)return false;
    // FPSCR=0 in the original oracle: neither flush-to-zero nor denormals-as-zero.
    // Both operands must be volatile reads: a volatile result alone would
    // still permit constant folding before the store and hide FTZ/DAZ.
    volatile float normal=f(0x00800000U),half=0.5F,tiny=f(1);
    return bits(mul(normal,half))==0x00400000U&&bits(add(tiny,tiny))==2;
}
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view message,std::uint64_t offset=0){throw Failure{{code,message,offset}};}
float finite(float value){
    if(!std::isfinite(value))fail(ErrorCode::numeric_overflow,"Compressed intermediate is outside the finite binary32 profile");
    return value;
}
struct Buffer {
    const KeyBuffer& keys;
    unsigned codec;
    std::size_t stride;
    std::size_t steps{};
    Limits limits;
    void need(std::size_t offset,std::size_t count)const {
        if(offset>keys.bytes.size()||count>keys.bytes.size()-offset)
            fail(ErrorCode::key_extent,"Original key access exceeds the owned buffer",keys.base.byte_offset+offset);
    }
    unsigned integer(std::size_t offset,unsigned bytes)const {
        need(offset,bytes);unsigned result=0;
        for(unsigned i=0;i<bytes;++i)result|=std::to_integer<unsigned>(keys.bytes[offset+i])<<(8*i);
        return result;
    }
    float delta(std::size_t offset){
        if(steps++>=limits.key_steps)fail(ErrorCode::resource_limit,"Caller key traversal budget exceeded",keys.base.byte_offset+offset);
        need(offset,stride);
        if(codec==4)return static_cast<float>(integer(offset+6,2));
        if(codec==5)return static_cast<float>(integer(offset+3,1));
        if(codec==6)return static_cast<float>(integer(offset+7,1));
        return static_cast<float>(integer(offset,4)>>28);
    }
    std::array<float,4> vector(std::size_t offset,const std::array<std::uint32_t,8>& extremes)const {
        const unsigned width=stride==8?2:1;
        need(offset,width*3);
        const float factor=f(stride==8?0x37800801U:0x3b888889U);
        std::array<float,4> out{};
        for(unsigned i=0;i<3;++i){
            const float quant=mul(static_cast<float>(static_cast<int>(integer(offset+i*width,width))-8),factor);
            // VMLA.F32 on this VFP2 source rounds the multiply before addition.
            out[i]=finite(add(f(extremes[4+i]),mul(f(extremes[i]),quant)));
        }
        return out;
    }
    std::array<float,4> raw(std::size_t offset,const std::optional<std::array<std::uint32_t,8>>& extremes)const {
        if(codec==4||codec==5)return vector(offset,*extremes);
        need(offset,stride);std::array<float,4> out{};
        if(codec==6){
            const auto packed=std::uint64_t(integer(offset,4))|(std::uint64_t(integer(offset+4,4))<<32);
            for(unsigned i=0;i<4;++i){
                const auto field=static_cast<std::uint32_t>((packed>>(42-i*14))&16383);
                const auto signed_field=field>=8192?static_cast<int>(field)-16384:static_cast<int>(field);
                out[i]=mul(static_cast<float>(signed_field*4),f(0x38800000U));
            }
        }else{
            const auto packed=integer(offset,4);
            for(unsigned i=0;i<4;++i)out[i]=f((*extremes)[4+i]);
            if(codec==7){
                for(unsigned i=0;i<4;++i){
                    const std::uint32_t field=((packed>>(21-i*7))&127U)-8U;
                    volatile float converted=static_cast<float>(field);
                    const auto quant=mul(converted,f(0x3c124925U));
                    out[i]=finite(add(mul(quant,f((*extremes)[i])),out[i]));
                }
            }else{
                const unsigned axis=codec-11;
                const auto quant=mul(static_cast<float>(static_cast<int>(packed&16383U)-8),f(0x38802008U));
                const auto quant_w=mul(static_cast<float>(static_cast<int>((packed>>14)&16383U)-8),f(0x38802008U));
                out[axis]=finite(add(out[axis],mul(f((*extremes)[axis]),quant)));
                out[3]=finite(add(out[3],mul(f((*extremes)[3]),quant_w)));
            }
        }
        return out;
    }
};
float dot(const std::array<float,4>& a,const std::array<float,4>& b){
    auto value=mul(a[0],b[0]);
    for(unsigned i=1;i<4;++i)value=add(value,mul(a[i],b[i]));
    return finite(value);
}
void normalize(std::array<float,4>& value){
    const auto norm=dot(value,value);
    if(norm==0)fail(ErrorCode::degenerate_quaternion,"Original quaternion normalization divides by zero outside finite profile");
    const auto inverse=finite(div(1.0F,square_root(norm)));
    for(auto& component:value)component=finite(mul(component,inverse));
}
}

Result sample(const motion_binding::Channel& channel,const TrackView* track,
              std::uint32_t query_frame_bits,Limits limits){
    if(!channel.track){
        if(track)return Error{ErrorCode::resource_identity,"Resolved track supplied for an absent channel"};
        return Sample{{0,0,0,0x3f800000U},channel};
    }
    if(!track)return Error{ErrorCode::unresolved_track,"Present channel requires its resolved immutable track"};
    if(!track->identity.resource_identity||track->identity.bank>15||track->identity.slot>255||track->identity!=*channel.track)
        return Error{ErrorCode::resource_identity,"Resolved track identity differs from the bound channel"};
    const auto codec=track->metadata&255U;
    if(codec==1||codec==2)return Sample{track->reference_bits,channel};
    if(codec!=4&&codec!=5&&codec!=6&&codec!=7&&codec!=11&&codec!=12&&codec!=13)
        return Error{ErrorCode::unsupported_codec,"Codec has no admitted native decoder"};
    if(!track->keys||!channel.key_data)return Error{ErrorCode::unknown_key_data,"Compressed channel has no owned key buffer or cursor"};
    if(codec!=6&&!track->extremes_bits)return Error{ErrorCode::unknown_extremes,"This compressed codec requires its scale/bias block"};
    const auto& keys=*track->keys;
    if(keys.base.resource_identity!=track->identity.resource_identity||channel.key_data->resource_identity!=track->identity.resource_identity)
        return Error{ErrorCode::resource_identity,"Key data and track do not belong to the same resource"};
    if(keys.bytes.size()>std::numeric_limits<std::uint64_t>::max()-keys.base.byte_offset)
        return Error{ErrorCode::key_extent,"Key extent overflows its source offset"};
    const auto stride=codec==4||codec==6?8U:4U;
    if(channel.key_data->byte_offset<keys.base.byte_offset||
       channel.key_data->byte_offset-keys.base.byte_offset>keys.bytes.size()||
       (channel.key_data->byte_offset-keys.base.byte_offset)%stride!=0)
        return Error{ErrorCode::cursor_extent,"Cursor is not a key boundary inside its owned buffer",channel.key_data->byte_offset};
    if(!environment())return Error{ErrorCode::floating_environment,"Round-to-nearest binary32 with gradual underflow required"};
    auto frame=f(channel.frame_bits);const auto query=f(query_frame_bits);
    if(!std::isfinite(frame)||!std::isfinite(query))return Error{ErrorCode::nonfinite_operand,"Compressed cursor and query must be finite"};
    if(codec!=6)for(unsigned i=0;i<8;++i){
        const bool accessed=codec==4||codec==5?(i!=3&&i!=7):codec==7||(i>=4||i==3||i==codec-11);
        if(accessed&&!std::isfinite(f((*track->extremes_bits)[i])))
            return Error{ErrorCode::nonfinite_operand,"Accessed scale/bias must be finite"};
    }
    try {
        auto offset=static_cast<std::size_t>(channel.key_data->byte_offset-keys.base.byte_offset);
        Buffer buffer{keys,codec,stride,0,limits};
        // Original conditional SUB/CMP reset policy, including its rounding.
        if(frame>query&&sub(frame,query)>query){offset=0;frame=0;}
        float fraction=0;
        if(frame<=query){
            for(;;){
                const auto delta=buffer.delta(offset);
                if(delta==0)break;
                const auto next=finite(add(frame,delta));
                if(next<=query){
                    buffer.need(offset,stride);offset+=stride;frame=next;
                }else{fraction=finite(div(sub(query,frame),delta));break;}
            }
        }else{
            // 0xaea1e8/0xaea248 compare the original key-base pointer with the
            // Channel address, NOT with the moving cursor. They cannot match
            // in this disjoint-storage profile. Do not invent a base clamp.
            for(;;){
                if(offset<stride)fail(ErrorCode::key_extent,"Original backward search crosses the owned key base",keys.base.byte_offset);
                offset-=stride;const auto delta=buffer.delta(offset);
                frame=finite(sub(frame,delta));
                if(frame<=query){fraction=finite(div(sub(query,frame),delta));break;}
            }
        }
        auto values=buffer.raw(offset,track->extremes_bits);
        if(fraction!=0){
            buffer.need(offset,stride);auto next=buffer.raw(offset+stride,track->extremes_bits);
            if(codec>=6&&dot(values,next)<0)for(auto& component:next)component=-component;
            const float remaining=sub(1.0F,fraction);
            for(unsigned i=0;i<4;++i)values[i]=finite(add(mul(values[i],remaining),mul(next[i],fraction)));
        }
        if(codec>=6)normalize(values);
        Sample out{{bits(values[0]),bits(values[1]),bits(values[2]),bits(values[3])},channel};
        out.channel.frame_bits=bits(frame);
        out.channel.key_data=motion_binding::BufferReference{keys.base.resource_identity,keys.base.byte_offset+offset};
        return out;
    }catch(const Failure& failure){return failure.error;}
}
} // namespace mh4u::motion_sampler
