#include "player_animation.hpp"
#include <new>
#include <stdexcept>
#include <utility>
#include <type_traits>

namespace mh4u::player_animation {
namespace {
namespace pcs=player_controller_step;
namespace mf=model_animation_frame;
namespace mo=model_animation_output;
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

UpdateDispatchOutput consume_dispatch(const State& source,player_update_dispatch::Output applied){
    auto& prepared=applied.prepared.prepared;
    UpdateDispatchOutput out{source,std::move(applied.action_fields),std::move(prepared.fields),
        std::move(prepared.collection),{std::move(prepared.effects),std::move(applied.prepared.statistics),
        applied.prepared.returns,std::move(applied.dispatches),applied.dispatch_count}};
    consume_command(out.state,std::move(prepared.state),std::move(prepared.layers));
    return out;
}
UpdateAdjustmentOutput consume_adjustment(const State& source,player_update_post_dispatch::Output applied){
    return {consume_dispatch(source,std::move(applied.dispatched)),std::move(applied.effects)};
}
UpdateStatusOutput consume_status(const State& source,player_update_status::Output applied){
    return {consume_adjustment(source,std::move(applied.adjusted)),std::move(applied.effects)};
}
UpdateNotificationsOutput consume_notifications(const State& source,player_update_notifications::Output applied){
    return {consume_status(source,std::move(applied.status)),std::move(applied.queue),std::move(applied.effects)};
}
// Each caller checkpoint shares this one bounded projection and allocation
// boundary. The operation receives temporary storage and publishes nothing.
template<class Result,class Operation>Result prepare_update(const State& source,const Limits& limits,Operation operation){
    try{
        if(auto error=validate(source,limits,true))return *error;
        auto storage=command_storage(source);
        return operation(std::span<const motion_pending::Layer>{storage});
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
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
UpdateDispatchResult dispatch_update(const State& source,const player_state1::Fields& action_fields,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const player_update_dispatch::Context& context,Limits limits){
    return prepare_update<UpdateDispatchResult>(source,limits,[&](std::span<const motion_pending::Layer> storage)->UpdateDispatchResult{
        auto result=player_update_dispatch::apply(source.controller,action_fields,storage,2,
            fields,objects,context,limits.command);
        if(auto* error=std::get_if<player_update_dispatch::Error>(&result))return Error{Phase::update_dispatch,*error};
        return consume_dispatch(source,std::get<player_update_dispatch::Output>(std::move(result)));
    });
}
UpdateAdjustmentResult adjust_update(const State& source,const player_state1::Fields& action_fields,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const player_update_post_dispatch::Context& context,Limits limits){
    return prepare_update<UpdateAdjustmentResult>(source,limits,[&](std::span<const motion_pending::Layer> storage)->UpdateAdjustmentResult{
        auto result=player_update_post_dispatch::apply(source.controller,action_fields,storage,2,
            fields,objects,context,limits.command);
        if(auto* error=std::get_if<player_update_post_dispatch::Error>(&result))return Error{Phase::update_adjustment,*error};
        return consume_adjustment(source,std::get<player_update_post_dispatch::Output>(std::move(result)));
    });
}
UpdateStatusResult update_status(const State& source,const player_state1::Fields& action_fields,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const player_update_post_dispatch::Context& context,Limits limits){
    return prepare_update<UpdateStatusResult>(source,limits,[&](std::span<const motion_pending::Layer> storage)->UpdateStatusResult{
        auto result=player_update_status::apply(source.controller,action_fields,storage,2,
            fields,objects,context,limits.command);
        if(auto* error=std::get_if<player_update_status::Error>(&result))return Error{Phase::update_status,*error};
        auto applied=std::get<player_update_status::Output>(std::move(result));
        return consume_status(source,std::move(applied));
    });
}
UpdateNotificationsResult update_notifications(const State& source,const player_state1::Fields& action_fields,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const player_update_post_dispatch::Context& context,const notification_queue::State& queue,Limits limits){
    return prepare_update<UpdateNotificationsResult>(source,limits,[&](std::span<const motion_pending::Layer> storage)->UpdateNotificationsResult{
        auto result=player_update_notifications::apply(source.controller,action_fields,storage,2,
            fields,objects,context,queue,limits.command);
        if(auto* error=std::get_if<player_update_notifications::Error>(&result))return Error{Phase::update_notifications,*error};
        auto applied=std::get<player_update_notifications::Output>(std::move(result));
        return consume_notifications(source,std::move(applied));
    });
}
UpdateDirectionResult update_direction(const State& source,const player_state1::Fields& action_fields,
    const player_fields::Fields& fields,const player_object_query::Manager& objects,
    const player_update_direction::Context& context,const notification_queue::State& queue,Limits limits){
    return prepare_update<UpdateDirectionResult>(source,limits,[&](std::span<const motion_pending::Layer> storage)->UpdateDirectionResult{
        auto result=player_update_direction::apply(source.controller,action_fields,storage,2,
            fields,objects,context,queue,limits.command);
        if(auto* error=std::get_if<player_update_direction::Error>(&result))return Error{Phase::update_direction,*error};
        auto applied=std::get<player_update_direction::Output>(std::move(result));
        return UpdateDirectionOutput{consume_notifications(source,std::move(applied.notified)),std::move(applied.material),std::move(applied.effects)};
    });
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
        out.state.frame=std::move(frame.state);
        auto published=mo::apply(out.state.frame,out.state.hierarchy,context.output,
            {limits.frame.layers,limits.joints,limits.snapshots,limits.order,limits.transition,limits.output});
        if(auto* error=std::get_if<mo::Error>(&published)){
            if(auto* e=std::get_if<mo::DomainError>(&error->cause))return domain(
                e->code==mo::DomainCode::joint_extent?DomainCode::joint_extent:DomainCode::resource_limit,e->message);
            if(auto* e=std::get_if<motion_transition::Error>(&error->cause))return Error{Phase::transition,*e};
            return Error{Phase::hierarchy,std::get<joint_output::Error>(error->cause)};
        }
        auto output=std::get<mo::Output>(std::move(published));
        out.state.frame.animation.joints=std::move(output.joints);
        out.state.frame.animation.accumulated_flags=output.accumulated_flags;
        out.state.hierarchy=std::move(output.hierarchy);
        out.frame->blend_layer_found=output.effects.blend_layer_found;out.frame->blended_joints=output.effects.blended_joints;
        out.frame->attachment_cleared=output.effects.attachment_cleared;out.frame->output_callback=output.effects.output_callback;
        out.frame->output_joints=output.effects.output_joints;return out;
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
