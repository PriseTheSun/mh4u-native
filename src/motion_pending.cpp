#include "motion_pending.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mh4u::motion_pending {
namespace {
struct Failure {Error error;};
[[noreturn]]void fail(ErrorCode code,std::string_view message,std::size_t layer=0,std::size_t joint=0){throw Failure{Error{code,message,layer,joint,{},{},{}}};}
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
// Original FPSCR=0 VCMP predicates expressed without host FTZ/DAZ arithmetic.
// +Inf > 0, unordered NaN is false, and both signed zeros compare equal.
bool positive(float value){const auto b=bits(value);return b>0&&b<=0x7f800000U;}
bool nonzero(float value){return(bits(value)&0x7fffffffU)!=0;}
Bits4 padded(const joint_pose::Bits3& v){return {v[0],v[1],v[2],0};}
Bits4 padded(Bits4 v){v[3]=0;return v;}
void clear(motion_binding::Channel& c){c.track.reset();c.weight_bits=0;}
std::uint32_t uncovered(std::uint32_t flags){return 0xfU&~(flags&7U);}
void check_channels(const State& state,std::size_t i){
    if(state.layers[i].animation.tracks.joints.size()<state.joints.size())
        fail(ErrorCode::binding_extent,"Selected layer lacks active joint channel storage",i);
}
void copy_snapshot(Snapshot& target,const joint_pose::Joint& source,std::uint32_t layer,bool full){
    target.rotation=source.rotation;target.layer_index=layer;
    if(full){target.translation=padded(source.translation);target.scale=padded(source.scale);}
}
}
Result apply(const State& previous,std::span<const std::uint8_t> map,const Parameters& p,
             ActionResolver actions,motion_initialization::Resolver samples,Limits limits){
    try{
        if(previous.layers.size()>limits.layers||previous.layers.size()>std::numeric_limits<std::uint32_t>::max()||
           previous.joints.size()>limits.joints||previous.snapshots.size()>limits.joints)
            fail(ErrorCode::resource_limit,"Active model tables exceed copy/work policy");
        for(std::size_t i=0;i<previous.layers.size();++i)
            if(previous.layers[i].animation.tracks.joints.size()>limits.joints)
                fail(ErrorCode::resource_limit,"Layer channel storage exceeds copy policy",i);
        Output out{previous,false,false,0};auto& state=out.state;
        const auto count=state.layers.size(),joints=state.joints.size();
        std::size_t first_pending=0;while(first_pending<count&&!(state.layers[first_pending].animation.layer.flags&2U))++first_pending;
        if(first_pending==count)return out;out.pending_found=true;
        std::size_t first_active=0;while(first_active<count&&state.layers[first_active].animation.layer.selector==0xffffU)++first_active;
        if(first_active==count){
            state.calculation_flags&=~0x20000000U;
            if(std::holds_alternative<joint_pose::Unknown>(p.model_reference))fail(ErrorCode::unknown_model,"No-active path requires explicit model presence");
            if(std::holds_alternative<joint_pose::Null>(p.model_reference))return out;
            if(state.snapshots.size()<joints)fail(ErrorCode::snapshot_extent,"Model lacks active transition snapshots");
            std::size_t highest=count-1;while(!(state.layers[highest].animation.layer.flags&2U))--highest;
            for(std::size_t i=first_pending;i<=highest;++i)state.layers[i].animation.layer.flags&=static_cast<std::uint16_t>(~2U);
            for(std::size_t j=0;j<joints;++j){copy_snapshot(state.snapshots[j],state.joints[j],static_cast<std::uint32_t>(highest),true);state.snapshots[j].flags&=0x20U;}
            state.accumulated_flags|=state.calculation_flags&0xffffffU;
            const auto& t=p.initialization.templates;
            const joint_pose::Templates templates{t.previous_translation_10e6370,t.identity_10e6560,p.scale_10e6380};
            auto restored=joint_pose::restore(state.joints,state.calculation_flags,p.model_reference,templates,limits.restoration);
            if(const auto* e=std::get_if<joint_pose::Error>(&restored))return Error{ErrorCode::restoration,"Joint restoration failed",0,e->joint_index,{},{},*e};
            state.joints=std::move(std::get<joint_pose::Restored>(restored).joints);out.restored_joints=true;return out;
        }
        state.calculation_flags|=0x20000000U;
        if(state.snapshots.size()<joints)fail(ErrorCode::snapshot_extent,"Model lacks active transition snapshots");
        for(std::size_t end=count;end>first_pending;){
            const auto i=--end;auto& selected=state.layers[i];auto& layer=selected.animation.layer;auto& bindings=selected.animation.tracks;
            if(!(layer.flags&2U)){
                if(layer.selector!=0xffffU&&bits(layer.weight)==0x3f800000U){
                    check_channels(state,i);
                    for(std::size_t j=0;j<joints;++j){auto& snapshot=state.snapshots[j];if(!(snapshot.flags&0x38U))snapshot.flags|=(bindings.joints[j].flags&0x70U)>>4;}
                }
                continue;
            }
            check_channels(state,i);layer.flags&=static_cast<std::uint16_t>(~2U);
            const bool blend=std::bit_cast<std::int32_t>(bits(layer.interpolation))>0x34000000&& (positive(layer.weight)||i==first_active);
            if(blend)for(std::size_t j=0;j<joints;++j){auto& snapshot=state.snapshots[j];
                if(!(snapshot.flags&0x38U)&&(snapshot.layer_index==i||(uncovered(snapshot.flags)&bindings.joints[j].flags)))snapshot.flags|=8U;
            }
            const auto& t=p.initialization.templates;
            if(!t.identity_10e6560||!t.previous_translation_10e6370)fail(ErrorCode::missing_template,"Pending reset requires explicit Q/T templates",i);
            selected.animation.previous_rotation=selected.animation.base_rotation=selected.rotation_98=*t.identity_10e6560;
            selected.animation.previous_translation=selected.animation.base_translation=selected.translation_c8=padded(*t.previous_translation_10e6370);
            clear(bindings.translation);clear(bindings.rotation);
            for(auto& binding:layer.bindings)binding.reset();
            for(std::size_t j=0;j<joints;++j){for(auto& channel:bindings.joints[j].channels)clear(channel);bindings.joints[j].flags=0;}
            std::optional<motion_binding::Action> action;
            if(layer.selector!=0xffffU){
                const auto bank=(std::uint32_t(layer.selector)>>8)&15U,slot=std::uint32_t(layer.selector)&255U;
                if(!actions.resolve)fail(ErrorCode::unresolved_action,"Present selector has no action resolver",i);
                auto resolved=actions.resolve(actions.context,bank,slot);
                if(const auto* e=std::get_if<ActionResolutionError>(&resolved))return Error{ErrorCode::unresolved_action,"Action resolution failed",i,0,*e,{},{}};
                const auto& found=std::get<std::optional<motion_binding::Action>>(resolved);
                if(found)action.emplace(*found);
                if(action&&(!action->resource_identity||action->bank!=bank||action->slot!=slot))fail(ErrorCode::action_identity,"Resolved action differs from selected bank/slot or lacks identity",i);
            }
            auto initialized=motion_initialization::initialize(selected.animation,map,action?&*action:nullptr,p.initialization,samples,limits.initialization);
            if(const auto* e=std::get_if<motion_initialization::Error>(&initialized))return Error{ErrorCode::initialization,"Animation initialization failed",i,0,{},*e,{}};
            selected.animation=std::move(std::get<motion_initialization::Output>(initialized).state);++out.initializer_calls;
            // References to members of selected.animation remain valid across
            // assignment; vectors have their own lifetime and are re-read here.
            auto& after=selected.animation;const bool remaining=nonzero(after.layer.remaining_interpolation);
            if(remaining)after.layer.flags|=0x100U;
            if(blend)for(std::size_t j=0;j<joints;++j){auto& snapshot=state.snapshots[j];
                if(!(snapshot.flags&0x30U)&&((snapshot.flags&8U)||(uncovered(snapshot.flags)&after.tracks.joints[j].flags))){
                    if(remaining)snapshot.flags|=0x10U;
                    copy_snapshot(snapshot,state.joints[j],remaining?static_cast<std::uint32_t>(i):0xffffffffU,remaining);
                }
            }
        }
        state.accumulated_flags|=state.calculation_flags&0xffffffU;
        for(std::size_t j=0;j<joints;++j)state.snapshots[j].flags&=0x20U;
        return out;
    }catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate bounded pending state",0,0,{},{},{}};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Pending state exceeds host capacity",0,0,{},{},{}};}
}
} // namespace mh4u::motion_pending
