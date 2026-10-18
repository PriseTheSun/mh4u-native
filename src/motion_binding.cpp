#include "motion_binding.hpp"
#include <cfenv>
#include <limits>

#ifdef __FAST_MATH__
#error "motion_binding requires round-to-nearest binary32 conversion"
#endif
namespace mh4u::motion_binding {
Result prepare_tracks(const motion_request::Layer& previous,const Tracks& previous_tracks,
    std::span<const std::uint8_t> bone_to_joint,const Action* action,Limits limits) {
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    if(previous_tracks.joints.size()>limits.joints||(action&&(action->tracks.size()>limits.tracks||
       action->tracks.size()>std::numeric_limits<std::uint32_t>::max())))
        return Error{ErrorCode::resource_limit,"Caller track/joint budget exceeded"};
    if(!action)return Prepared{previous,previous_tracks,false};
    if(!action->resource_identity||action->bank>15||action->slot>255)
        return Error{ErrorCode::resource_identity,"Verified selected bank/slot identity required"};
    if(!action->header.event_descriptors_present)
        return Error{ErrorCode::event_descriptors,"Null source event base is not four absent groups"};
    if(std::fegetround()!=FE_TONEAREST)
        return Error{ErrorCode::floating_environment,"Round-to-nearest binary32 conversion required"};
    // Validate accessed ownership before allocating or publishing any state.
    for(std::size_t ordinal=0;ordinal<action->tracks.size();++ordinal) {
        const auto bone=action->tracks[ordinal].metadata>>24;
        if(bone>=bone_to_joint.size())return Error{ErrorCode::bone_map_extent,"Accessed bone mapping is unknown",ordinal};
        const auto joint=bone_to_joint[bone];
        if(joint!=255&&joint>=previous_tracks.joints.size())
            return Error{ErrorCode::joint_extent,"Mapped joint is outside owned binding storage",ordinal};
    }
    Prepared out{previous,previous_tracks,true};
    out.tracks.translation={};out.tracks.rotation={};
    // Unsigned SUB precedes VCVT: frame_count=0 deliberately becomes 2^32-1.
    // This prefix has no signed-frame sampler conversion or clamp.
    volatile float end=static_cast<float>(action->header.frame_count-1U);
    volatile float loop=action->header.loop_frame_raw<0?0.0f:static_cast<float>(action->header.loop_frame_raw);
    out.layer.end_frame=end;out.layer.field58=loop;
    if(action->flags&1U)out.layer.attributes|=0x4000U;
    if(action->flags&2U)out.layer.attributes|=0x100000U;
    for(std::uint32_t group=0;group<4;++group)
        out.layer.bindings[group]=motion_request::Binding{action->resource_identity,action->bank,action->slot,group};
    for(std::size_t ordinal=0;ordinal<action->tracks.size();++ordinal) {
        const auto& track=action->tracks[ordinal];
        const auto mapped=bone_to_joint[track.metadata>>24];
        Joint* joint=mapped==255?nullptr:&out.tracks.joints[mapped];
        const auto kind=(track.metadata>>8)&255U;
        Channel* channel=nullptr;
        if(kind<3) {
            if(!joint)continue;
            channel=&joint->channels[kind];
            if(kind==2)out.layer.attributes|=0x40U;
        } else if(kind==3)channel=&out.tracks.rotation;
        else if(kind==4)channel=&out.tracks.translation;
        if(channel) {
            channel->track=TrackReference{action->resource_identity,action->bank,action->slot,static_cast<std::uint32_t>(ordinal)};
            channel->frame_bits=0;channel->weight_bits=track.weight_bits;
            channel->key_data=track.key_data_offset?
                std::optional<BufferReference>{{action->resource_identity,*track.key_data_offset}}:std::nullopt;
        }
        // Every kind, even an unknown/root kind, revisits the mapped joint's
        // existing weights. Bits accumulate; duplicate zero-weight tracks do
        // not clear flags set by earlier tracks. +/-0 both compare equal.
        if(joint)for(unsigned component=0;component<3;++component) {
            const auto weight=joint->channels[component].weight_bits;
            if((weight&0x7fffffffU)!=0)joint->flags|=1U<<component;
            if(weight==0x3f800000U)joint->flags|=0x10U<<component;
        }
    }
    return out;
}
} // namespace mh4u::motion_binding
