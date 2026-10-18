#include "joint_update.hpp"
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>

namespace mh4u::joint_update {
namespace {
using B3=joint_pose::Bits3;using B4=joint_pose::Bits4;
namespace tm=motion_transform_math;
struct Failure {Error error;};
struct Run {
    const Parameters& parameters;
    motion_initialization::Resolver resolver;
    Limits limits;
    Output output;
    std::size_t li{},ji{},ci{};
    Error error(ErrorCode code,std::string_view message)const{
        Error out{};out.code=code;out.message=message;out.layer=li;out.joint=ji;out.channel=ci;return out;
    }
    [[noreturn]]void fail(ErrorCode code,std::string_view text)const{throw Failure{error(code,text)};}
    template<class T>T math(tm::Result<T> r)const{
        if(auto* e=std::get_if<tm::Error>(&r)){auto out=error(ErrorCode::arithmetic,"Transform arithmetic failed");out.arithmetic=*e;throw Failure{out};}
        return std::get<T>(r);
    }
    template<class T>const T& known(const std::optional<T>& x)const{
        if(!x)fail(ErrorCode::missing_template,"Consumed template snapshot is unknown");return *x;
    }
    B4 sample(motion_binding::Channel& channel,std::uint32_t frame){
        if(!resolver.resolve){auto out=error(ErrorCode::unresolved_track,"Present track has no resolver");out.track=channel.track;throw Failure{out};}
        auto resolved=resolver.resolve(resolver.context,*channel.track);
        if(auto* failure=std::get_if<motion_initialization::ResolutionError>(&resolved)){
            auto out=error(ErrorCode::unresolved_track,"Track identity did not resolve");out.track=channel.track;out.resolution=*failure;throw Failure{out};
        }
        const auto& view=std::get<motion_sampler::TrackView>(resolved);
        auto sampled=motion_sampler::sample(channel,&view,frame,limits.sampling);
        if(auto* failure=std::get_if<motion_sampler::Error>(&sampled)){
            auto out=error(ErrorCode::sampling,"Track sampling failed");out.track=channel.track;out.sampling=*failure;throw Failure{out};
        }
        const auto& value=std::get<motion_sampler::Sample>(sampled);channel=value.channel;++output.samples;return value.value_bits;
    }
};
B3 xyz(const B4& v){return {v[0],v[1],v[2]};}
B4 vector(const B3& v){return {v[0],v[1],v[2],0};}
void xyz_into(B4& target,const B3& source){for(unsigned i=0;i<3;++i)target[i]=source[i];}
}
Result apply_layers(std::span<const joint_pose::Joint> joints,std::span<const Metadata> metadata,
    std::span<const LayerView> layers,const Parameters& parameters,
    motion_initialization::Resolver resolver,Limits limits){
    Run run{parameters,resolver,limits,{},0,0,0};
    if(joints.size()>limits.joints||layers.size()>limits.layers||
       joints.size()>std::numeric_limits<std::uint32_t>::max()||layers.size()>std::numeric_limits<std::uint32_t>::max())
        return run.error(ErrorCode::resource_limit,"Joint/layer count exceeds bound");
    if(!joints.empty()&&(joints.size()>std::numeric_limits<std::size_t>::max()/3||
       layers.size()>std::numeric_limits<std::size_t>::max()/(joints.size()*3)))
        return run.error(ErrorCode::resource_limit,"Sample work count exceeds size_t capacity");
    for(const auto& layer:layers)if(layer.tracks.joints.size()>limits.bindings_per_layer)
        return run.error(ErrorCode::resource_limit,"Copied binding count exceeds bound");
    if(metadata.size()!=joints.size())return run.error(ErrorCode::metadata_extent,"Metadata must cover exactly the joints");
    if(parameters.leaf!=Leaf::rotation_translation&&parameters.leaf!=Leaf::rotation_translation_scale)
        return run.error(ErrorCode::unsupported_leaf,"Unknown joint-update leaf");
    if(std::holds_alternative<joint_pose::Unknown>(parameters.model))return run.error(ErrorCode::unknown_model,"rModel presence unknown");
    if(std::holds_alternative<joint_pose::Null>(parameters.model))return run.error(ErrorCode::null_model,"Original rModel+6c would dereference null");
    const auto& table=std::get<joint_pose::PresentModel>(parameters.model).matrices;
    if(std::holds_alternative<joint_pose::Unknown>(table))return run.error(ErrorCode::unknown_table,"Table pointer unknown");
    try{
        run.output.joints.assign(joints.begin(),joints.end());run.output.tracks.reserve(layers.size());
        for(const auto& layer:layers)run.output.tracks.push_back(layer.tracks);
        const bool full=parameters.leaf==Leaf::rotation_translation_scale;
        unsigned mode=1;
        for(run.li=0;run.li<layers.size();++run.li){
            const auto& layer=layers[run.li];if(layer.layer.selector==0xffff)continue;
            const auto attributes=layer.layer.attributes;
            if(attributes&0x80000U)mode|=2;
            const bool additive=(attributes&0x40000U)!=0;
            const bool global=additive||(parameters.flags238&0x10000000U);
            for(run.ji=0;run.ji<joints.size();++run.ji){
                const auto& meta=metadata[run.ji];if(meta.flags94&1)continue;
                const bool mirror=full&&(attributes&0x100U)&&((meta.mapping98&0xff)!=0xff);
                const std::size_t source_index=mirror?meta.mapping98&0xff:run.ji;
                if(source_index>=joints.size())run.fail(ErrorCode::mirror_extent,"Mapped joint index exceeds original joint count");
                auto& bindings=run.output.tracks[run.li].joints;
                if(source_index>=bindings.size())run.fail(ErrorCode::binding_extent,"Consumed joint binding missing");
                auto& binding=bindings[source_index];
                if(mode!=1&&!(binding.flags&15))continue;
                B4 q;B3 t,s;
                run.ci=0;
                if(binding.channels[0].track)q=run.sample(binding.channels[0],layer.prepared_frame_bits);
                else if(global)q=run.known(parameters.templates.rotation);
                else{
                    if(std::holds_alternative<joint_pose::Null>(table))run.fail(ErrorCode::null_table,"Absent rotation consumes null matrix table");
                    const auto matrices=std::get<std::span<const joint_pose::Matrix>>(table);
                    if(source_index>=matrices.size())run.fail(ErrorCode::table_extent,"Consumed matrix absent");
                    auto result=joint_pose::matrix_rotation(matrices[source_index]);
                    if(auto* failure=std::get_if<joint_pose::Error>(&result)){auto out=run.error(ErrorCode::matrix,"Matrix rotation failed");out.matrix=*failure;throw Failure{out};}
                    q=std::get<B4>(result);
                }
                run.ci=1;
                if(binding.channels[1].track)t=xyz(run.sample(binding.channels[1],layer.prepared_frame_bits));
                else t=global?run.known(parameters.templates.translation):joints[source_index].reference_translation;
                run.ci=2;
                if(full&&binding.channels[2].track)s=xyz(run.sample(binding.channels[2],layer.prepared_frame_bits));
                else if(full||mode!=0)s=run.known(parameters.templates.scale);
                if(mirror){t[0]^=0x80000000U;q[1]^=0x80000000U;q[2]^=0x80000000U;}
                auto& joint=run.output.joints[run.ji];
                if(mode){joint.rotation=q;joint.translation=vector(t);joint.scale=vector(s);}
                else{
                    const auto weight=std::bit_cast<std::uint32_t>(layer.layer.weight);
                    run.ci=0;const auto wq=run.math(tm::multiply(binding.channels[0].weight_bits,weight));
                    run.ci=1;const auto wt=run.math(tm::multiply(binding.channels[1].weight_bits,weight));
                    std::uint32_t ws{};if(full){run.ci=2;ws=run.math(tm::multiply(binding.channels[2].weight_bits,weight));}
                    if(additive){
                        run.ci=0;const auto delta=run.math(tm::nlerp(run.known(parameters.templates.rotation),q,wq));
                        joint.rotation=run.math(tm::multiply_rotation(delta,joint.rotation));
                        if(full){run.ci=2;xyz_into(joint.scale,run.math(tm::scale_additive(xyz(joint.scale),s,run.known(parameters.templates.scale),ws)));}
                        run.ci=1;xyz_into(joint.translation,run.math(tm::add_scaled(xyz(joint.translation),t,wt)));
                    }else{
                        run.ci=0;joint.rotation=run.math(tm::nlerp(joint.rotation,q,wq));
                        if(full){run.ci=2;joint.scale=vector(run.math(tm::lerp(xyz(joint.scale),s,ws)));}
                        run.ci=1;joint.translation=vector(run.math(tm::lerp(xyz(joint.translation),t,wt)));
                    }
                }
                ++run.output.updated_joints;
            }
            mode=0;
        }
        return std::move(run.output);
    }catch(const Failure& failure){return failure.error;}
    catch(const std::bad_alloc&){return run.error(ErrorCode::resource_limit,"Joint update allocation failed");}
    catch(const std::length_error&){return run.error(ErrorCode::resource_limit,"Joint update allocation exceeds capacity");}
}
} // namespace mh4u::joint_update
