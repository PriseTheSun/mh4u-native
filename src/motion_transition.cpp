#include "motion_transition.hpp"
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>

namespace mh4u::motion_transition {
namespace {
namespace math=motion_transform_math;
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view message,std::size_t joint=0,std::size_t layer=0){
    throw Failure{{code,message,joint,layer,Component::none,std::nullopt}};
}
template<class T>T result(math::Result<T> value,Component component,std::size_t joint=0,std::size_t layer=0){
    if(const auto* error=std::get_if<math::Error>(&value))
        throw Failure{{ErrorCode::arithmetic,"Transition arithmetic failed",joint,layer,component,*error}};
    return std::get<T>(value);
}
joint_pose::Bits3 xyz(const joint_pose::Bits4& v){return {v[0],v[1],v[2]};}
joint_pose::Bits4 padded(const joint_pose::Bits3& v){return {v[0],v[1],v[2],0};}
}
Result apply(std::span<const joint_pose::Joint> joints,
    std::span<const joint_update::Metadata> metadata,std::span<const motion_pending::Snapshot> snapshots,
    std::span<const LayerView> layers,const Parameters& parameters,Limits limits){
    try {
        if(joints.size()>limits.joints||layers.size()>limits.layers||
           joints.size()>std::numeric_limits<std::uint32_t>::max()||
           layers.size()>std::numeric_limits<std::uint32_t>::max())
            fail(ErrorCode::resource_limit,"Transition state exceeds work policy or original count width");
        auto rt=result(math::rt_matrix(parameters.model_rotation,parameters.model_translation),Component::model_rt);
        bool found=false;
        for(const auto& view:layers)if(view.layer.flags&0x100U){found=true;break;}
        Output out{{joints.begin(),joints.end()},rt,found?parameters.accumulated_flags:0U,found,0};
        if(!found)return out;
        for(std::size_t i=0;i<joints.size();++i){
            if(i>=snapshots.size())fail(ErrorCode::snapshot_extent,"Consumed snapshot is absent",i);
            const auto& snapshot=snapshots[i];
            if(snapshot.layer_index==0xffffffffU)continue;
            if(i>=metadata.size())fail(ErrorCode::metadata_extent,"Consumed joint metadata is absent",i,snapshot.layer_index);
            if(metadata[i].flags94&0x41U)continue;
            const std::size_t layer=snapshot.layer_index;
            if(layer>=layers.size())fail(ErrorCode::layer_extent,"Snapshot selects an absent layer",i,layer);
            const auto weight=std::bit_cast<std::uint32_t>(layers[layer].layer.field60);
            if((weight&0x7fffffffU)==0)continue;
            auto& joint=out.joints[i];
            joint.rotation=result(math::nlerp(joint.rotation,snapshot.rotation,weight),Component::rotation,i,layer);
            joint.translation=padded(result(math::lerp(xyz(joint.translation),xyz(snapshot.translation),weight),Component::translation,i,layer));
            joint.scale=padded(result(math::lerp(xyz(joint.scale),xyz(snapshot.scale),weight),Component::scale,i,layer));
            ++out.blended_joints;
        }
        return out;
    }catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Transition result allocation failed",0,0,Component::none,std::nullopt};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Transition result capacity exceeded",0,0,Component::none,std::nullopt};}
}
} // namespace mh4u::motion_transition
