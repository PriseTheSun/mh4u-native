#include "model_animation_output.hpp"
#include <new>
#include <stdexcept>

namespace mh4u::model_animation_output {
namespace {
namespace mt=motion_transition;
namespace jo=joint_output;
Error capacity(){return{Phase::domain,DomainError{DomainCode::resource_limit,"Animation output exceeds its bounded copy/work policy"}};}
}
Result apply(const model_animation_frame::State& source,const Hierarchy& hierarchy,
             const jo::Parameters& parameters,Limits limits){
    try{
        const auto& animation=source.animation;
        if(animation.joints.size()!=hierarchy.joints.size())return Error{Phase::domain,
            DomainError{DomainCode::joint_extent,"Canonical pose and output metadata have different joint counts"}};
        if(animation.layers.size()>limits.layers||animation.joints.size()>limits.joints||animation.snapshots.size()>limits.snapshots)return capacity();
        if(const auto* order=std::get_if<std::vector<std::uint8_t>>(&hierarchy.order);order&&order->size()>limits.order)return capacity();
        std::vector<joint_update::Metadata> metadata;metadata.reserve(hierarchy.joints.size());
        for(const auto& joint:hierarchy.joints)metadata.push_back(joint.metadata);
        std::vector<mt::LayerView> layers;layers.reserve(animation.layers.size());
        for(const auto& layer:animation.layers)layers.push_back({layer.animation.layer});
        const auto& model=source.model;
        auto blended=mt::apply(animation.joints,metadata,animation.snapshots,layers,
            {model.rotation,{model.position[0],model.position[1],model.position[2]},animation.accumulated_flags},limits.transition);
        if(auto* error=std::get_if<mt::Error>(&blended))return Error{Phase::transition,*error};
        auto transition=std::get<mt::Output>(std::move(blended));
        jo::State projected;
        projected.local_rt=transition.local_rt;projected.world=hierarchy.world;
        projected.scale={model.scale[0],model.scale[1],model.scale[2]};
        projected.flags4=hierarchy.flags4;projected.flags104=hierarchy.flags104;projected.flags230=hierarchy.flags230;
        projected.flags238=animation.calculation_flags;
        projected.attachment_index=hierarchy.attachment_index;projected.attachment_mode=std::uint32_t(hierarchy.attachment_mode_high16)<<16;
        projected.dirty=hierarchy.dirty;projected.fieldc4c=hierarchy.fieldc4c;projected.order=hierarchy.order;
        projected.joints.reserve(hierarchy.joints.size());
        for(std::size_t i=0;i<hierarchy.joints.size();++i){const auto& joint=hierarchy.joints[i];
            projected.joints.push_back({transition.joints[i],joint.metadata,joint.matrix,joint.depth,joint.callback,joint.owner});}
        auto matrices=jo::apply(projected,parameters,limits.output);
        if(auto* error=std::get_if<jo::Error>(&matrices))return Error{Phase::hierarchy,*error};
        auto output=std::get<jo::Output>(std::move(matrices));
        Output result;
        result.accumulated_flags=transition.accumulated_flags;
        result.effects={transition.blend_layer_found,output.attachment_cleared,output.selected,transition.blended_joints,output.updated_joints};
        auto& h=result.hierarchy;auto& state=output.state;
        h.local_rt=state.local_rt;h.world=state.world;h.flags4=state.flags4;h.flags104=state.flags104;h.flags230=state.flags230;
        h.attachment_index=state.attachment_index;h.attachment_mode_high16=std::uint16_t(state.attachment_mode>>16);
        h.dirty=state.dirty;h.fieldc4c=state.fieldc4c;h.order=std::move(state.order);
        result.joints.reserve(state.joints.size());h.joints.reserve(state.joints.size());
        for(const auto& joint:state.joints){result.joints.push_back(joint.pose);
            h.joints.push_back({joint.metadata,joint.matrix,joint.depth,joint.callback,joint.owner});}
        return result;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
}
