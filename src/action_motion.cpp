#include "action_motion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mh4u::action_motion {
namespace {
float narrow(double value) {
    if(!std::isfinite(value)||std::abs(value)>std::numeric_limits<float>::max())throw std::runtime_error("motion_sample_float_range");
    return static_cast<float>(value);
}
// Binder order inv(loop) * end. Neither endpoint nor the product is normalized.
root_motion::Quaternion relative(root_motion::Quaternion a,root_motion::Quaternion b) {
    double norm=0;for(float v:a)norm+=double(v)*v;
    if(!std::isfinite(norm)||norm<=0)throw std::runtime_error("motion_base_quaternion_norm");
    const double x=-a[0]/norm,y=-a[1]/norm,z=-a[2]/norm,w=a[3]/norm;
    return {narrow(w*b[0]+x*b[3]+y*b[2]-z*b[1]),narrow(w*b[1]-x*b[2]+y*b[3]+z*b[0]),
            narrow(w*b[2]+x*b[1]-y*b[0]+z*b[3]),narrow(w*b[3]-x*b[0]-y*b[1]-z*b[2])};
}
constexpr root_motion::PreUpdateWeights weights{1,1,1};
// Confirmed startup templates 0x10e6370 / 0x10e6560. The zero quaternion at
// 0x10e6570 belongs to an inverse fallback outside this supported profile.
constexpr root_motion::ResetTemplates reset{{0,0,0},{0,0,0,1}};
}
root_motion::Sample Player::sample(float source_frame) const {
    auto sampled=package_->sample(action_.identity,source_frame);
    if(const auto* error=std::get_if<model_motion::Error>(&sampled))throw std::runtime_error("motion_sample:"+error->message);
    const auto& value=std::get<model_motion::Sample>(sampled);
    root_motion::Sample result{{0,0,0},{0,0,0,1}};
    // The sampler's null-pointer branch supplies these values; absence remains
    // explicit in Package and is resolved only at this runtime binding.
    if(value.translation)for(std::size_t i=0;i<3;++i)result.translation[i]=narrow((*value.translation)[i]);
    if(value.rotation)for(std::size_t i=0;i<4;++i)result.rotation[i]=narrow((*value.rotation)[i]);
    return result;
}
Player::Player(const model_motion::Package& package,const model_motion::ActionIdentity& identity,const DiagnosticPolicy& policy,
               const motion_events::Package* events)
    :package_(&package),events_(events) {
    const auto found=std::find_if(package.actions().begin(),package.actions().end(),[&](const auto& value){return value.identity==identity;});
    if(found==package.actions().end())throw std::runtime_error("motion_action_identity");
    action_=*found;
    if(events_) {
        event_identity_={identity.entry_id,identity.slot,identity.lmt_sha256};
        const auto event=std::find_if(events_->actions().begin(),events_->actions().end(),[&](const auto& value){return value.identity==event_identity_;});
        if(event==events_->actions().end()||event->frame_count!=action_.frame_count)
            throw std::runtime_error("motion_events_action_identity");
        if(event->state!=motion_events::ActionState::parsed&&event->state!=motion_events::ActionState::events_absent)
            throw std::runtime_error("motion_events_action_unavailable");
        for(std::size_t group=0;group<4;++group) {
            if(event->groups[group]==motion_events::GroupState::unavailable)
                throw std::runtime_error("motion_events_group_unavailable");
            scope_.event_sources_present[group]=event->groups[group]==motion_events::GroupState::present;
        }
    }
    const float end=narrow(double(action_.frame_count)-1);
    const float loop=narrow(std::max(action_.loop_frame_raw,0));
    if(!std::isfinite(policy.initial_frame)||policy.initial_frame<0||policy.initial_frame>end||
       !std::isfinite(policy.source_frames_per_tick)||std::abs(policy.source_frames_per_tick)>end||end<=0||loop>=end)
        throw std::runtime_error("motion_diagnostic_clock_policy");
    const auto initial=sample(policy.initial_frame),start=sample(loop),last=sample(end);
    parameters_={1,policy.source_frames_per_tick,end,loop,{},relative(start.rotation,last.rotation),policy.repeat?0U:4U,0};
    for(std::size_t i=0;i<3;++i)parameters_.base_translation[i]=narrow(double(last.translation[i])-start.translation[i]);
    root_motion::AdvanceState clock{};
    clock.frame=policy.initial_frame;clock.previous_frame=policy.initial_frame;
    clock.previous_translation=initial.translation;clock.previous_rotation=initial.rotation;
    clock.internal_flags=4;clock.previous_flags=1;clock.model_attributes=0x20000000;
    frame_={{{policy.initial_model,clock,initial,{0,0,0},{0,0,0,1},policy.initial_frame,policy.initial_frame},{{0,0,0},{0,0,0,1}}},policy.initial_frame,0};
    // Validate the initial state without advancing the stored player. No
    // hidden repair or flag reinitialization is performed in later ticks.
    (void)calculate_next();
}
Frame Player::calculate_next() const {
    const auto requests=root_motion::sample_requests_one_layer(frame_.state.pre,parameters_,scope_,
        events_?root_motion::EventSourcePolicy::masks_only:root_motion::EventSourcePolicy::excluded);
    std::array<root_motion::TimedSample,3> samples{};
    for(std::size_t i=0;i<requests.count;++i)samples[i]={requests.frames[i],sample(requests.frames[i])};
    const auto result=[&] {
        if(!events_)return root_motion::tick_one_layer(frame_.state,parameters_,weights,{samples.data(),requests.count},reset,scope_,{32});
        const auto pending=root_motion::plan_tick_with_events(frame_.state,parameters_,weights,{samples.data(),requests.count},reset,scope_,{32});
        std::array<root_motion::EventSample,8> answers{};std::size_t count=0;
        for(const auto& request:pending.requests()) {
            const auto value=events_->kernel_range(event_identity_,request.group,request.range.begin,request.range.end);
            if(const auto* error=std::get_if<motion_events::Error>(&value))throw std::runtime_error("motion_event_query:"+error->message);
            answers[count++]={request,std::get<std::uint32_t>(value)};
        }
        return pending.finish({answers.data(),count});
    }();
    return {result.state,result.state.pre.sample_frame,result.loop_iterations};
}
Frame Player::tick() {
    frame_=calculate_next();
    return frame_;
}
} // namespace mh4u::action_motion
