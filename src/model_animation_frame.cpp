#include "model_animation_frame.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <new>
#include <limits>
#include <stdexcept>

namespace mh4u::model_animation_frame {
namespace {
namespace mp=motion_pending;namespace mi=motion_initialization;
namespace jp=joint_pose;namespace ju=joint_update;namespace math=motion_transform_math;
struct Failure{Error error;};
[[noreturn]]void fail(Error e){throw Failure{std::move(e)};}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
float value(std::uint32_t b){return std::bit_cast<float>(b);}
std::uint32_t number(math::Result<std::uint32_t> r,std::size_t layer){
    if(auto* source=std::get_if<math::Error>(&r)){Error e{ErrorCode::preparation,source->message};e.layer=layer;e.preparation=*source;fail(std::move(e));}
    return std::get<std::uint32_t>(r);
}
float finite(float f,std::size_t layer){
    if(!std::isfinite(f)){Error e{ErrorCode::preparation,"Prepared-frame operand is nonfinite"};e.layer=layer;e.preparation=math::Error{math::ErrorCode::nonfinite_operand,e.message};fail(std::move(e));}return f;
}
jp::Templates pose_templates(const mp::Parameters& p){return{p.initialization.templates.previous_translation_10e6370,p.initialization.templates.identity_10e6560,p.scale_10e6380};}
}
Result step(const State& source,std::span<const std::uint8_t> bone_to_joint,const Parameters& parameters,Resolvers resolvers,Limits limits){
    try{
        const auto& source_animation=source.animation;
        if(source_animation.layers.size()>limits.layers||source_animation.joints.size()>limits.joints||source_animation.joints.size()>std::numeric_limits<std::uint32_t>::max()||source_animation.snapshots.size()>limits.snapshots)
            fail({ErrorCode::resource_limit,"Frame state exceeds declared copy/work limits"});
        for(const auto& layer:source_animation.layers)if(layer.animation.tracks.joints.size()>limits.bindings_per_layer)
            fail({ErrorCode::resource_limit,"Frame channel storage exceeds declared copy limit"});
        if(std::holds_alternative<jp::Unknown>(parameters.pending.model_reference))fail({ErrorCode::unknown_model,"Frame model pointer presence is unknown"});
        Output out{source};auto& state=out.state.animation;
        if(std::holds_alternative<jp::Null>(parameters.pending.model_reference)||state.layers.empty())return out;
        const bool pending=std::any_of(state.layers.begin(),state.layers.end(),[](const auto& l){return(l.animation.layer.flags&2)!=0;});
        if(pending){
            auto result=mp::apply(state,bone_to_joint,parameters.pending,resolvers.actions,resolvers.tracks,limits.pending);
            if(auto* e=std::get_if<mp::Error>(&result)){Error error{ErrorCode::pending,e->message};error.pending=*e;fail(std::move(error));}
            auto applied=std::get<mp::Output>(std::move(result));state=std::move(applied.state);out.pending_found=applied.pending_found;out.restored_joints=applied.restored_joints;out.initializer_calls=applied.initializer_calls;
        }
        std::uint32_t attributes=0;for(const auto& l:state.layers)attributes|=l.animation.layer.attributes;
        state.calculation_flags=(state.calculation_flags&0xff000000U)|((attributes|state.accumulated_flags)&0x00ffffffU);
        const auto templates=pose_templates(parameters.pending);
        if(state.calculation_flags&0x20000000U){
            for(std::size_t i=0;i<state.layers.size();++i){auto& l=state.layers[i].animation.layer;if(l.selector==0xffff)continue;
                float current=finite(l.frame,i);const float end=finite(l.end_frame,i);
                const auto increment=number(math::multiply(bits(l.speed),parameters.pending.initialization.model_delta_time_bits),i);
                float next=value(number(math::add(bits(current),increment),i));
                // Match source ordered comparisons, retaining signed zero at ties.
                if(current>end)current=end;
                if(current<0)current=0;
                if(end<next)next=end;
                if(next<0)next=0;
                if(l.attributes&0x800U)current=std::floor(current);
                l.prepared_frame=current;l.prepared_next_frame=next;++out.prepared_layers;
            }
            auto result=motion_application::apply(out.state.model,state.layers,parameters.pending.initialization.templates,resolvers.tracks,limits.application);
            if(auto* e=std::get_if<motion_application::Error>(&result)){Error error{ErrorCode::application,e->message};error.application=*e;fail(std::move(error));}
            auto applied=std::get<motion_application::Output>(std::move(result));out.state.model=applied.model;state.layers=std::move(applied.layers);out.model_samples=applied.samples;
            if(!state.joints.empty()&&!(state.layers[0].animation.layer.attributes&0x200U)){
                std::vector<ju::LayerView> views;views.reserve(state.layers.size());for(const auto& l:state.layers)views.push_back({l.animation.layer,l.animation.tracks,bits(l.animation.layer.prepared_frame)});
                ju::Parameters p;p.leaf=(state.calculation_flags&0x82004140U)?ju::Leaf::rotation_translation_scale:ju::Leaf::rotation_translation;p.flags238=state.calculation_flags;p.model=parameters.pending.model_reference;p.templates=templates;
                auto updated=ju::apply_layers(state.joints,parameters.metadata,views,p,resolvers.tracks,limits.joint_update);
                if(auto* e=std::get_if<ju::Error>(&updated)){Error error{ErrorCode::joint_update,e->message};error.joint_update=*e;fail(std::move(error));}
                auto joints=std::get<ju::Output>(std::move(updated));state.joints=std::move(joints.joints);for(std::size_t i=0;i<state.layers.size();++i)state.layers[i].animation.tracks=std::move(joints.tracks[i]);out.joint_leaf=p.leaf;out.joint_samples=joints.samples;
            }
        }else if(std::any_of(state.layers.begin(),state.layers.end(),[](const auto& l){return(l.animation.layer.flags&0x100U)!=0;})){
            auto restored=jp::restore(state.joints,state.calculation_flags,parameters.pending.model_reference,templates);
            if(auto* e=std::get_if<jp::Error>(&restored)){Error error{ErrorCode::restoration,e->message};error.restoration=*e;fail(std::move(error));}
            state.joints=std::get<jp::Restored>(std::move(restored)).joints;out.restored_joints=true;
        }
        motion_clock::Snapshots snapshots{static_cast<std::uint32_t>(state.joints.size()),std::span<const mp::Snapshot>(state.snapshots)};
        auto advanced=motion_clock::advance(state.layers,snapshots,{parameters.pending.initialization.model_delta_time_bits,parameters.pending.initialization.templates.inverse_fallback_10e6570},resolvers.events,limits.clock);
        if(auto* e=std::get_if<motion_clock::Error>(&advanced)){Error error{ErrorCode::clock,e->message};error.clock=*e;fail(std::move(error));}
        auto clock=std::get<motion_clock::Output>(std::move(advanced));state.layers=std::move(clock.layers);state.snapshots=std::get<std::vector<mp::Snapshot>>(std::move(clock.snapshots));out.loop_iterations=clock.loop_iterations;out.event_queries=clock.event_queries;
        return out;
    }catch(const Failure& e){return e.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Frame state allocation failed"};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Frame state exceeds host capacity"};}
}
}
