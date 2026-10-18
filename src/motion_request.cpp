#include "motion_request.hpp"
#include "motion_event_kernel.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>

#ifdef __FAST_MATH__
#error "motion_request requires strict binary32 operations"
#endif
namespace mh4u::motion_request {
Result apply(const Layer& previous,const Model& model,const Request& request) noexcept {
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    if(!std::isfinite(request.interpolation)||!std::isfinite(request.start_frame)||!std::isfinite(request.speed)||
       ((request.attributes&0x20000U)&&!std::isfinite(previous.offset)))
        return Error{ErrorCode::nonfinite_input,"Finite request operands required by this profile"};
    if(std::fegetround()!=FE_TONEAREST)
        return Error{ErrorCode::floating_environment,"Round-to-nearest binary32 required"};
    volatile float tiny=std::bit_cast<float>(std::uint32_t{1});
    volatile float doubled=tiny+tiny;
    if(std::bit_cast<std::uint32_t>(static_cast<float>(doubled))!=2U)
        return Error{ErrorCode::floating_environment,"Gradual binary32 underflow required"};
    const Action* action=nullptr;
    std::uint64_t resource_identity=0;
    const auto bank=(request.selector>>8)&15U,slot=request.selector&255U;
    // The original sentinel test compares the full argument, before narrowing.
    if(request.selector!=0xffffU) {
        const auto& source=model.banks[bank];
        if(std::holds_alternative<Unknown>(source))return Error{ErrorCode::unknown_bank,"Selected bank identity is unknown"};
        if(const auto* resource=std::get_if<Resource>(&source);resource&&resource->slots) {
            resource_identity=resource->identity;
            const auto slots=*resource->slots;
            if(slots.size()>65535)return Error{ErrorCode::table_extent,"Original table count is u16"};
            if(slot<slots.size()&&slots[slot])action=&*slots[slot];
        }
    }
    Layer out=previous;
    out.flags=2;out.field08=2;out.interpolation=request.interpolation;
    if(request.layer==0&&previous.selector==0xffffU) {
        out.remaining_interpolation=0;out.field60=0;
    } else {
        out.remaining_interpolation=request.interpolation;out.field60=1;out.flags=0x102;
    }
    out.speed=request.speed;out.attributes=request.attributes;
    out.selector=static_cast<std::uint16_t>(request.selector);out.weight=1;out.field58=0;
    if(!model.skeleton_present||!action) {
        out.frame=0;out.requested_frame=0;out.previous_frame=0;out.end_frame=0;
        out.attributes&=~0x104040U;
        out.masks.fill(0);out.selector=0xffff;
        return Output{out,false};
    }
    if(!action->frame_count)return Error{ErrorCode::frame_domain,"Zero frame count wraps outside supported signed-frame profile"};
    if(!resource_identity)return Error{ErrorCode::resource_identity,"Present action requires stable resource identity"};
    volatile float converted=static_cast<float>(action->frame_count-1U);
    const float end=converted;
    if(end>=2147483648.0f)return Error{ErrorCode::frame_domain,"Frame endpoint must convert to signed32 without saturation"};
    float begin=request.start_frame;
    if(begin<0)begin=0;
    if(end<begin)begin=end;
    float effective=request.start_frame;
    if(request.attributes&0x20000U) {volatile float sum=previous.offset+effective;effective=sum;}
    if(!std::isfinite(effective))return Error{ErrorCode::frame_domain,"Offset addition must remain finite"};
    // Source clamps only the upper effective endpoint, not its lower bound.
    if(end<effective)effective=end;
    if(effective<-2147483648.0f)return Error{ErrorCode::frame_domain,"Effective frame must convert to signed32 without saturation"};
    if(!action->event_descriptors_present)return Error{ErrorCode::event_descriptors,"Source setter requires all four event descriptors"};
    for(const auto& group:action->groups) {
        if(group.size()>std::size_t(std::numeric_limits<std::int32_t>::max()))
            return Error{ErrorCode::event_accumulator,"Event count exceeds signed source profile"};
        std::uint64_t sum=0;
        for(const auto& record:group) {
            sum+=record.duration;
            if(sum>std::uint64_t(std::numeric_limits<std::int32_t>::max()))
                return Error{ErrorCode::event_accumulator,"Signed duration overflow is outside this profile"};
        }
    }
    out.end_frame=end;if(action->loop_frame_raw<0)out.attributes|=4;
    out.frame=effective;out.previous_frame=effective;out.requested_frame=request.start_frame;
    const auto first=static_cast<std::int32_t>(begin),last=static_cast<std::int32_t>(effective);
    for(std::uint32_t ordinal=0;ordinal<4;++ordinal) {
        out.bindings[ordinal]=Binding{resource_identity,bank,slot,ordinal};
        out.masks[ordinal]=motion_events::detail::accumulate_validated(action->groups[ordinal],first,last);
    }
    return Output{out,true};
}
} // namespace mh4u::motion_request
