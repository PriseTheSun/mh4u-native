#include "player_animation.hpp"
#include <new>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace mh4u::player_animation {
namespace {
namespace pcs=player_controller_step;
namespace mf=model_animation_frame;
namespace mt=motion_transition;
namespace jo=joint_output;
Error domain(DomainCode code,std::string_view message){return{Phase::domain,DomainError{code,message}};}
Error capacity(){return domain(DomainCode::resource_limit,"Player animation exceeds its bounded storage/copy policy");}
std::optional<Error> validate(const State& source,const Limits& limits,bool command=false){
    const auto& animation=source.frame.animation;
    if(animation.layers.size()!=2)return domain(DomainCode::active_layers,"Player driver requires exactly two explicit active layers");
    if(animation.joints.size()!=source.hierarchy.joints.size())
        return domain(DomainCode::joint_extent,"Canonical pose and output metadata have different joint counts");
    if(animation.joints.size()>limits.joints||animation.snapshots.size()>limits.snapshots)return capacity();
    if(const auto* order=std::get_if<std::vector<std::uint8_t>>(&source.hierarchy.order);order&&order->size()>limits.order)return capacity();
    std::size_t bindings=0;
    const auto count=[&](const motion_pending::Layer& layer){
        const auto n=layer.animation.tracks.joints.size();
        if(n>limits.bindings-bindings)return false;
        bindings+=n;return true;
    };
    for(const auto& layer:animation.layers)if(!count(layer))return capacity();
    for(const auto& layer:source.inactive)if(!count(layer))return capacity();
    // Preserve the command owner's existing error identity while enforcing its
    // policy before the temporary join allocates any copied binding tables.
    if(command&&(limits.command.storage_layers<8||bindings>limits.command.joint_bindings))
        return Error{Phase::controller,pcs::Error{pcs::Phase::storage,motion_command::Error{
            motion_command::ErrorCode::resource_limit,"Controller layer copy exceeds its declared resource policy",{},{},{},0}}};
    return std::nullopt;
}
// Called only after validate: the driver's split storage has exactly 2+6 slots.
std::vector<motion_pending::Layer> command_storage(const State& source){
    std::vector<motion_pending::Layer> storage;storage.reserve(8);
    storage.insert(storage.end(),source.frame.animation.layers.begin(),source.frame.animation.layers.end());
    storage.insert(storage.end(),source.inactive.begin(),source.inactive.end());
    return storage;
}
void consume_command(State& destination,pcs::State controller,std::vector<motion_pending::Layer> layers){
    destination.controller=std::move(controller);
    for(std::size_t i=0;i<2;++i)destination.frame.animation.layers[i]=std::move(layers[i]);
    for(std::size_t i=0;i<6;++i)destination.inactive[i]=std::move(layers[i+2]);
}
jo::State project_output(const State& source){
    const auto& h=source.hierarchy;jo::State result;
    result.local_rt=h.local_rt;result.world=h.world;
    result.scale={source.frame.model.scale[0],source.frame.model.scale[1],source.frame.model.scale[2]};
    result.flags4=h.flags4;result.flags104=h.flags104;result.flags230=h.flags230;
    result.flags238=source.frame.animation.calculation_flags;
    result.attachment_index=h.attachment_index;result.attachment_mode=h.attachment_mode;
    result.dirty=h.dirty;result.fieldc4c=h.fieldc4c;result.order=h.order;
    result.joints.reserve(h.joints.size());
    for(std::size_t i=0;i<h.joints.size();++i){const auto& joint=h.joints[i];
        result.joints.push_back({source.frame.animation.joints[i],joint.metadata,joint.matrix,joint.depth,joint.callback,joint.owner});
    }
    return result;
}
void consume_output(State& destination,jo::State source){
    auto& h=destination.hierarchy;h.local_rt=source.local_rt;h.world=source.world;
    h.flags4=source.flags4;h.flags104=source.flags104;h.flags230=source.flags230;
    h.attachment_index=source.attachment_index;h.attachment_mode=source.attachment_mode;
    h.dirty=source.dirty;h.fieldc4c=source.fieldc4c;h.order=std::move(source.order);
    for(std::size_t i=0;i<source.joints.size();++i){const auto& joint=source.joints[i];
        destination.frame.animation.joints[i]=joint.pose;
        h.joints[i]={joint.metadata,joint.matrix,joint.depth,joint.callback,joint.owner};
    }
}
}
Result apply(const State& source,const Command& command,const CommandContext& context,Limits limits){
    try{
        if(auto error=validate(source,limits,true))return *error;
        auto storage=command_storage(source);
        pcs::Result result=std::visit([&](const auto& request)->pcs::Result{
            using T=std::decay_t<decltype(request)>;
            if constexpr(std::is_same_v<T,player_selection::CommonArguments>)
                return pcs::apply_common(source.controller,storage,2,request,context.model,context.globals,context.constants,context.sampling,limits.command,context.area);
            else return pcs::apply_wrapper(source.controller,storage,2,request.wrapper,request.arguments,context.model,context.globals,context.constants,context.sampling,limits.command,context.area);
        },command);
        if(auto* error=std::get_if<pcs::Error>(&result))return Error{Phase::controller,*error};
        auto applied=std::get<pcs::Output>(std::move(result));
        Output out{source,CommandEffects{applied.selection_request,applied.animation_requests,
            applied.animation_request_count,applied.transition,applied.queries,applied.samples},std::nullopt};
        consume_command(out.state,std::move(applied.state),std::move(applied.layers));
        return out;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
State1Result dispatch_state1(const State& source,const player_state1::Fields& fields,const State1Context& context,Limits limits){
    try{
        if(auto error=validate(source,limits,true))return *error;
        auto storage=command_storage(source);
        auto result=player_state1::apply(source.controller,fields,storage,2,context.tables,
            context.model,context.globals,context.sampling,limits.command,context.continuation);
        if(auto* error=std::get_if<player_state1::Error>(&result))return Error{Phase::state1,*error};
        auto applied=std::get<player_state1::Output>(std::move(result));
        State1Output out{source,std::move(applied.fields),std::move(applied.effects),std::move(applied.shared_fields)};
        consume_command(out.state,std::move(applied.state),std::move(applied.layers));
        return out;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
Result step(const State& source,const FrameContext& context,Limits limits){
    try{
        if(auto error=validate(source,limits))return *error;
        std::vector<joint_update::Metadata> metadata;metadata.reserve(source.hierarchy.joints.size());
        for(const auto& joint:source.hierarchy.joints)metadata.push_back(joint.metadata);
        auto updated=mf::step(source.frame,context.bone_to_joint,{context.pending,metadata},context.resolvers,limits.frame);
        if(auto* error=std::get_if<mf::Error>(&updated))return Error{Phase::animation,*error};
        auto frame=std::get<mf::Output>(std::move(updated));
        Output out{source,std::nullopt,FrameEffects{frame.pending_found,frame.restored_joints,false,false,
            frame.joint_leaf,std::nullopt,frame.initializer_calls,frame.prepared_layers,frame.model_samples,
            frame.joint_samples,frame.loop_iterations,frame.event_queries,0,0}};
        out.state.frame=std::move(frame.state);auto& animation=out.state.frame.animation;
        const std::array<mt::LayerView,2> layers{{{animation.layers[0].animation.layer},{animation.layers[1].animation.layer}}};
        const auto& model=out.state.frame.model;
        auto blended=mt::apply(animation.joints,metadata,animation.snapshots,layers,
            {model.rotation,{model.position[0],model.position[1],model.position[2]},animation.accumulated_flags},limits.transition);
        if(auto* error=std::get_if<mt::Error>(&blended))return Error{Phase::transition,*error};
        auto transition=std::get<mt::Output>(std::move(blended));animation.joints=std::move(transition.joints);
        animation.accumulated_flags=transition.accumulated_flags;out.state.hierarchy.local_rt=transition.local_rt;
        out.frame->blend_layer_found=transition.blend_layer_found;out.frame->blended_joints=transition.blended_joints;
        auto projected=project_output(out.state);auto matrices=jo::apply(projected,context.output,limits.output);
        if(auto* error=std::get_if<jo::Error>(&matrices))return Error{Phase::hierarchy,*error};
        auto hierarchy=std::get<jo::Output>(std::move(matrices));out.frame->attachment_cleared=hierarchy.attachment_cleared;
        out.frame->output_callback=hierarchy.selected;out.frame->output_joints=hierarchy.updated_joints;
        consume_output(out.state,std::move(hierarchy.state));return out;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
Result tick(const State& source,const Command& command,const CommandContext& commands,const FrameContext& frame,Limits limits){
    auto requested=apply(source,command,commands,limits);
    if(auto* error=std::get_if<Error>(&requested))return *error;
    auto applied=std::get<Output>(std::move(requested));
    auto updated=step(applied.state,frame,limits);
    if(auto* error=std::get_if<Error>(&updated))return *error;
    auto out=std::get<Output>(std::move(updated));out.command=std::move(applied.command);return out;
}
} // namespace mh4u::player_animation
