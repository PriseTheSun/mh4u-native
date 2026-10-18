#include "motion_query.hpp"
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mh4u::motion_query {
namespace {
namespace math=motion_transform_math;
struct Failure {Error error;};
// VCMPE/LO is ordered less-than; unordered NaNs do not select either clamp.
// This is a bit predicate, so copies also preserve subnormals under host FTZ.
bool less(std::uint32_t a,std::uint32_t b){
    const auto aa=a&0x7fffffffU,bb=b&0x7fffffffU;
    if(aa>0x7f800000U||bb>0x7f800000U||(!aa&&!bb))return false;
    if((a^b)&0x80000000U)return (a&0x80000000U)!=0;
    return (a&0x80000000U)?aa>bb:aa<bb;
}
std::uint32_t floor_frame(std::uint32_t b){
    // d12f8c: large values, infinities and NaNs return their original bits.
    // For exponent<23 its x+1e30 comparison is always positive (FPSCR0),
    // leaving these integer mantissa operations. Signed zero is preserved.
    const int exponent=static_cast<int>((b>>23)&255U)-127;
    if(exponent>=23)return b;
    if(exponent<0)return (b&0x80000000U)?(b==0x80000000U?b:0xbf800000U):0;
    const auto mask=0x007fffffU>>exponent;
    if(!(b&mask))return b;
    if(b&0x80000000U)b+=0x00800000U>>exponent;
    return b&~mask;
}
math::Bits3 xyz(const Bits4& v){return {v[0],v[1],v[2]};}
Bits4 vector(const math::Bits3& v){return {v[0],v[1],v[2],0};}
}
Result query(std::span<const LayerView> layers,std::uint32_t frame,
    const motion_initialization::Templates& templates,OutputSelection selected,
    motion_initialization::Resolver resolver,Limits limits){
    if(layers.size()>limits.layers||layers.size()>std::numeric_limits<std::uint32_t>::max())
        return Error{ErrorCode::resource_limit,"Layer count exceeds caller/u32 bound",0,Component::none,{},{},{},{}};
    if(!templates.base_translation_10e62c0||!templates.identity_10e6560)
        return Error{ErrorCode::missing_template,"Entry translation/rotation template is unknown",0,Component::none,{},{},{},{}};
    try{
        Output out;
        out.channels.reserve(layers.size());
        for(const auto& layer:layers)out.channels.push_back({layer.translation,layer.rotation});
        auto translation=vector(*templates.base_translation_10e62c0);
        auto rotation=*templates.identity_10e6560;
        for(std::size_t i=0;i<layers.size();++i){
            const auto& layer=layers[i].layer;
            if(layer.selector==0xffffU||(layer.flags&2U))continue;
            auto sample_frame=frame;
            const auto end=std::bit_cast<std::uint32_t>(layer.end_frame);
            if(less(end,sample_frame))sample_frame=end;
            if(less(sample_frame,0))sample_frame=0;
            if(layer.attributes&0x800U)sample_frame=floor_frame(sample_frame);
            auto sample=[&](motion_binding::Channel& channel,Component component){
                std::optional<motion_sampler::TrackView> view;
                if(channel.track){
                    if(!resolver.resolve)throw Failure{{ErrorCode::unresolved_track,"Present track has no resolver",i,component,channel.track,{},{},{}}};
                    auto resolved=resolver.resolve(resolver.context,*channel.track);
                    if(const auto* e=std::get_if<motion_initialization::ResolutionError>(&resolved))
                        throw Failure{{ErrorCode::unresolved_track,"Present track resolution failed",i,component,channel.track,*e,{},{}}};
                    view=std::get<motion_sampler::TrackView>(resolved);
                }
                auto result=motion_sampler::sample(channel,view?&*view:nullptr,sample_frame,limits.sampling);
                if(const auto* e=std::get_if<motion_sampler::Error>(&result))
                    throw Failure{{ErrorCode::sampling,"Original channel sampling failed",i,component,channel.track,{},*e,{}}};
                auto sampled=std::get<motion_sampler::Sample>(result);
                channel=sampled.channel;++out.samples;return sampled.value_bits;
            };
            auto t=sample(out.channels[i].translation,Component::translation);
            auto q=sample(out.channels[i].rotation,Component::rotation);
            if(layer.attributes&0x100U){t[0]^=0x80000000U;q[1]^=0x80000000U;q[2]^=0x80000000U;}
            if(out.accepted_layers==0){
                // Original weight products are discarded on this path. Do
                // not impose a numeric domain on values that cannot affect it.
                translation=vector(xyz(t));rotation=q;
            }else{
                auto arithmetic=[&]<class T>(math::Result<T> result,Component component)->T{
                    if(const auto* e=std::get_if<math::Error>(&result))
                        throw Failure{{ErrorCode::arithmetic,"Model-channel blend failed",i,component,{},{},{},*e}};
                    return std::get<T>(result);
                };
                const auto weight=std::bit_cast<std::uint32_t>(layer.weight);
                const auto wt=arithmetic(math::multiply(weight,out.channels[i].translation.weight_bits),Component::translation);
                const auto wq=arithmetic(math::multiply(weight,out.channels[i].rotation.weight_bits),Component::rotation);
                if(layer.attributes&0x40000U){
                    translation=vector(arithmetic(math::add_scaled(xyz(translation),xyz(t),wt),Component::translation));
                    const auto delta=arithmetic(math::nlerp(*templates.identity_10e6560,q,wq),Component::rotation);
                    rotation=arithmetic(math::multiply_rotation(delta,rotation),Component::rotation);
                }else{
                    translation=vector(arithmetic(math::lerp(xyz(translation),xyz(t),wt),Component::translation));
                    rotation=arithmetic(math::nlerp(rotation,q,wq),Component::rotation);
                }
            }
            ++out.accepted_layers;
        }
        if(selected.translation)out.translation=translation;
        if(selected.rotation)out.rotation=rotation;
        return out;
    }catch(const Failure& failure){return failure.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Query allocation failed",0,Component::none,{},{},{},{}};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Query allocation exceeds container capacity",0,Component::none,{},{},{},{}};}
}
} // namespace mh4u::motion_query
