#include "player_position_bridge.hpp"
#include <bit>
#include <new>
#include <stdexcept>

#ifdef __FAST_MATH__
#error "player_position_bridge requires ordered binary32 arithmetic"
#endif

namespace mh4u::player_position_bridge {
namespace {
namespace math=motion_transform_math;
namespace mf=model_animation_frame;
namespace mo=model_animation_output;
using Word=std::uint32_t;
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view field){throw Failure{{code,field,{},{},{},{}}};}
template<class T>T known(const std::optional<T>& v,std::string_view field){if(!v)fail(ErrorCode::unknown_field,field);return *v;}
Word arithmetic(math::Result<Word> result,std::string_view field){
    if(auto* error=std::get_if<math::Error>(&result))throw Failure{{ErrorCode::arithmetic,field,*error,{},{},{}}};
    return std::get<Word>(result);
}
Word add(Word a,Word b,std::string_view field){return arithmetic(math::add(a,b),field);}
Word sub(Word a,Word b,std::string_view field){return add(a,b^0x80000000U,field);}
Word mul(Word a,Word b,std::string_view field){return arithmetic(math::multiply(a,b),field);}
Word bits(float f){return std::bit_cast<Word>(f);}
float number(Word word,std::string_view field){
    // Validate consumed binary32 operands and the environment without changing
    // their stored bits; raw copies are never passed through this operation.
    (void)mul(word,0x3f800000U,field);return std::bit_cast<float>(word);
}
Error capacity(){return{ErrorCode::resource_limit,"position bridge copy/work policy",{},{},{},{}};}
}
Result apply(const mf::State& source,const mo::Hierarchy& source_hierarchy,
    const player_controller_step::State& source_controller,const player_fields::Fields& source_shared,
    const Fields& source_fields,const Context& context,player_animation::Limits limits){
    try{
        const auto& a=source.animation;
        if(a.layers.size()>limits.frame.layers||a.joints.size()>limits.joints||a.snapshots.size()>limits.snapshots||source_hierarchy.joints.size()>limits.joints)return capacity();
        if(const auto* order=std::get_if<std::vector<std::uint8_t>>(&source_hierarchy.order);order&&order->size()>limits.order)return capacity();
        std::size_t bindings=0;
        for(const auto& layer:a.layers){const auto count=layer.animation.tracks.joints.size();
            if(count>limits.bindings-bindings)return capacity();bindings+=count;}
        // b07c08 copies these raw lanes even when the later update is gated.
        math::Bits3 position{known(source_shared.pbw.field10_bits,"PBW10"),
            known(source_shared.pbw.field14_bits,"PBW14"),known(source_shared.pbw.field18_bits,"PBW18")};
        Output out{source,source_hierarchy,source_controller,source_shared,source_fields,{}};
        auto& f=out.fields;auto& controller=out.controller;auto& model=out.frame.model;
        auto& animation=out.frame.animation;
        const auto publish_pbw=[&]{out.shared.pbw.field10_bits=position[0];out.shared.pbw.field14_bits=position[1];
            out.shared.pbw.field18_bits=position[2];out.shared.pbw.field1c_bits=0;};
        const auto flag=known(f.fieldd2c,"d2c");
        out.hierarchy.flags4=flag?out.hierarchy.flags4|0x800U:out.hierarchy.flags4&~0x800U;
        const auto gates=known(f.fieldd64,"d64");
        if(gates&8U){out.effects.early_return=true;publish_pbw();return out;}
        if(animation.layers.empty())fail(ErrorCode::layer_extent,"layer0 source fields");
        if(known(context.orientation,"virtual80")!=OrientationCallback::b05328)fail(ErrorCode::unsupported_callback,"virtual80");
        auto& pbw=controller.player.pbw;
        pbw.field20&=0xffffU;pbw.field24&=0xffffU;pbw.field28&=0xffffU;
        auto oriented=player_orientation::calculate({std::uint16_t(pbw.field20),std::uint16_t(pbw.field24),std::uint16_t(pbw.field28)},f.euler_mode);
        if(auto* error=std::get_if<player_orientation::Error>(&oriented))return Error{ErrorCode::orientation,"b05350",{},*error,{},{}};
        model.rotation=std::get<player_orientation::Quaternion>(oriented);
        const auto old_rotation_y=model.rotation[1];
        model.position={position[0],position[1],position[2],0};
        auto& aux=controller.motion.fieldsd48;
        const auto& first=animation.layers[0].animation.layer;
        aux[1]=bits(first.previous_frame);aux[2]=bits(first.frame);
        const auto prior_frame=bits(first.frame),remaining=bits(first.remaining_interpolation);
        const auto delta=context.frame.pending.initialization.model_delta_time_bits;
        bool interpolation_active=false,overrun=false;Word overrun_step=0;
        const auto set_hold=[&](bool hold){for(auto& layer:animation.layers){auto& attributes=layer.animation.layer.attributes;
            attributes=hold?attributes|0x10U:attributes&~0x10U;}};
        const auto timer34=known(f.fieldd34_bits,"d34");
        if(number(timer34,"d34")>0.0F){
            const auto next=sub(timer34,delta,"d34 decrement");f.fieldd34_bits=number(next,"d34 clamp")<=0.0F?0U:next;
            set_hold(true);
        }else{
            bool hold=false;
            if(number(remaining,"layer0 interpolation")>0.0F&&(aux[6]&0x8000U)){
                interpolation_active=true;const auto next=sub(remaining,delta,"interpolation decrement");
                hold=number(next,"interpolation branch")>0.0F;
                if(number(next,"interpolation overrun")<0.0F){
                    const auto step=mul(next,bits(first.speed),"interpolation speed");
                    const auto frame=sub(prior_frame,step,"interpolation frame");
                    for(auto& layer:animation.layers)layer.animation.layer.frame=layer.animation.layer.previous_frame=std::bit_cast<float>(frame);
                    overrun_step=sub(frame,prior_frame,"interpolation correction");overrun=true;
                }
            }
            set_hold(hold);
        }
        const auto timer30=known(f.fieldd30_bits,"d30");
        auto& attributes=animation.layers[0].animation.layer.attributes;
        if(number(timer30,"d30")>0.0F){const auto next=sub(timer30,delta,"d30 decrement");
            f.fieldd30_bits=number(next,"d30 clamp")<=0.0F?0U:next;attributes|=1U;
        }else attributes&=~1U;
        std::vector<joint_update::Metadata> metadata;metadata.reserve(out.hierarchy.joints.size());
        for(const auto& joint:out.hierarchy.joints)metadata.push_back(joint.metadata);
        auto advanced=mf::step(out.frame,context.frame.bone_to_joint,{context.frame.pending,metadata},context.frame.resolvers,limits.frame);
        if(auto* error=std::get_if<mf::Error>(&advanced))return Error{ErrorCode::animation,"9573b8",{},{},*error,{}};
        auto frame=std::get<mf::Output>(std::move(advanced));
        out.effects.animation=player_animation::FrameEffects{frame.pending_found,frame.restored_joints,false,false,frame.joint_leaf,{},
            frame.initializer_calls,frame.prepared_layers,frame.model_samples,frame.joint_samples,frame.loop_iterations,frame.event_queries,0,0};
        out.frame=std::move(frame.state);
        aux[3]=overrun?overrun_step:aux[4];
        aux[4]=mul(bits(out.frame.animation.layers[0].animation.layer.speed),delta,"d58 speed step");
        const auto blend=known(f.fieldd38_bits,"d38");
        if(blend==0x3f800000U){for(unsigned i=0;i<3;++i)position[i]=out.frame.model.position[i];}
        else{
            // Unlike math::lerp's weighted-sum instruction order, this wrapper
            // uses old + ((new-old)*weight), with separately rounded operations.
            for(unsigned i=0;i<3;++i){const auto difference=sub(out.frame.model.position[i],position[i],"position difference");
                position[i]=add(position[i],mul(difference,blend,"position blend"),"position accumulation");}
            f.fieldd38_bits=0x3f800000U;out.effects.interpolated_position=true;
        }
        if((aux[6]&0x8000U)&&!(aux[6]&2U)&&interpolation_active&&number(delta,"b06b38 delta gate")>0.0F)
            fail(ErrorCode::unsupported_route,"b06b38");
        f.fieldd44=0;
        if((gates&1U)&&known(f.euler_mode,"model38 Euler mode")==4&&
           number(out.frame.model.rotation[1],"direction new QY")!=number(old_rotation_y,"direction old QY"))
            fail(ErrorCode::unsupported_route,"d00628 direction refresh");
        if(gates&2U)out.effects.publication_skipped=true;
        else{
            for(unsigned i=0;i<3;++i)out.frame.model.position[i]=add(position[i],controller.player.fieldsd10[i],"publication offset");
            out.frame.model.position[3]=0;
            if(known(context.publication,"virtual4c/50")!=OutputCallbacks::standard_95b490_95b940)
                fail(ErrorCode::unsupported_callback,"virtual4c/50");
            auto published=mo::apply(out.frame,out.hierarchy,context.frame.output,
                {limits.frame.layers,limits.joints,limits.snapshots,limits.order,limits.transition,limits.output});
            if(auto* error=std::get_if<mo::Error>(&published))return Error{ErrorCode::publication,"95b490/95b940",{},{},{},*error};
            auto output=std::get<mo::Output>(std::move(published));
            out.frame.animation.joints=std::move(output.joints);out.frame.animation.accumulated_flags=output.accumulated_flags;
            out.hierarchy=std::move(output.hierarchy);
            auto& effects=*out.effects.animation;effects.blend_layer_found=output.effects.blend_layer_found;effects.blended_joints=output.effects.blended_joints;
            effects.attachment_cleared=output.effects.attachment_cleared;effects.output_callback=output.effects.output_callback;effects.output_joints=output.effects.output_joints;
        }
        publish_pbw();return out;
    }catch(const Failure& failure){return failure.error;}
    catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
}
