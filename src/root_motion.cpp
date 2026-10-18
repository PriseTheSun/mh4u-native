#include "root_motion.hpp"
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <limits>

namespace mh4u::root_motion {
namespace {
using DVector=std::array<double,3>;
using DQuaternion=std::array<double,4>;
using Rotation=RotationBasis;
[[noreturn]] void fail(ErrorCode code,const char* message) { throw Error(code,message); }
template<std::size_t N> void finite(const std::array<float,N>& value) {
    for(float x:value) if(!std::isfinite(x)) fail(ErrorCode::nonfinite_input,"Root motion requires finite input components");
}
void valid_quaternion(const Quaternion& q) {
    finite(q);
    const float norm=((q[0]*q[0]+q[1]*q[1])+q[2]*q[2])+q[3]*q[3];
    if(!(norm>0)||!std::isfinite(norm)) fail(ErrorCode::invalid_quaternion,"Quaternion squared norm must be positive and finite in float32");
}
template<std::size_t N> std::array<double,N> wide(const std::array<float,N>& value) {
    std::array<double,N> result{};for(std::size_t i=0;i<N;++i) result[i]=value[i];return result;
}
template<std::size_t N> std::array<float,N> narrow(const std::array<double,N>& value) {
    std::array<float,N> result{};
    for(std::size_t i=0;i<N;++i) {
        if(!std::isfinite(value[i])||std::abs(value[i])>std::numeric_limits<float>::max())
            fail(ErrorCode::nonfinite_result,"Root motion result exceeds finite float32 representation");
        result[i]=static_cast<float>(value[i]);
    }
    return result;
}
DQuaternion product(const DQuaternion& a,const DQuaternion& b) {
    const auto [x,y,z,w]=a;const auto [u,v,t,s]=b;
    return {w*u+x*s+y*t-z*v,w*v+y*s+z*u-x*t,w*t+z*s+x*v-y*u,w*s-x*u-y*v-z*t};
}
DQuaternion inverse(const DQuaternion& q) {
    const double norm=((q[0]*q[0]+q[1]*q[1])+q[2]*q[2])+q[3]*q[3];
    return {-q[0]/norm,-q[1]/norm,-q[2]/norm,q[3]/norm};
}
DQuaternion normalized(DQuaternion q) {
    const double norm=((q[0]*q[0]+q[1]*q[1])+q[2]*q[2])+q[3]*q[3];
    if(!(norm>0)||!std::isfinite(norm)) fail(ErrorCode::nonfinite_result,"Composed root quaternion has invalid norm");
    const double divisor=std::sqrt(norm);for(double& x:q) x/=divisor;return q;
}
Rotation matrix(const DQuaternion& q) {
    const auto [x,y,z,w]=q;
    return {{{1-2*(y*y+z*z),2*(x*y-z*w),2*(x*z+y*w)},
             {2*(x*y+z*w),1-2*(x*x+z*z),2*(y*z-x*w)},
             {2*(x*z-y*w),2*(y*z+x*w),1-2*(x*x+y*y)}}};
}
DVector transform(const Rotation& r,const DVector& v,bool transpose=false) {
    DVector result{};
    for(std::size_t i=0;i<3;++i) for(std::size_t j=0;j<3;++j) result[i]+=(transpose?r[j][i]:r[i][j])*v[j];
    return result;
}
// A double operation on float32 operands followed by this boundary prevents
// clock multiply/add contraction and retains the source's separate rounding.
float clock_value(double value) { return narrow(std::array<double,1>{value})[0]; }
float clock_add(float a,float b) { return clock_value(static_cast<double>(a)+b); }
float clock_subtract(float a,float b) { return clock_value(static_cast<double>(a)-b); }
std::int32_t clock_index(float value) {
    if(!std::isfinite(value)||static_cast<double>(value)<std::numeric_limits<std::int32_t>::min()||
       static_cast<double>(value)>std::numeric_limits<std::int32_t>::max())
        fail(ErrorCode::unsupported_clock_domain,"Clock conversion exceeds the verified signed int32 domain");
    return static_cast<std::int32_t>(value); // Source VCVT truncates toward zero.
}
}

RotationBasis rotation_basis_raw(const Quaternion& q) {
    valid_quaternion(q);
    return matrix(wide(q));
}

namespace {
Output application_body(const Model& model,const Layer& layer,const Sample& sample,const ResetTemplates& reset) {
    finite(model.position);finite(model.scale);finite(model.null_scale);valid_quaternion(model.rotation);
    finite(layer.previous_translation);valid_quaternion(layer.previous_rotation);
    finite(sample.translation);valid_quaternion(sample.rotation);finite(reset.position);finite(reset.rotation);
    if(!std::isfinite(layer.blend)||!std::isfinite(layer.translation_weight)||!std::isfinite(layer.rotation_weight))
        fail(ErrorCode::nonfinite_input,"Layer blend and weights must be finite even when the first contribution ignores them");
    Output out{model,layer,sample,reset.position,reset.rotation};
    if(layer.attr&0x8100) {
        out.sampled.translation[0]=-out.sampled.translation[0];
        out.sampled.rotation[1]=-out.sampled.rotation[1];out.sampled.rotation[2]=-out.sampled.rotation[2];
    }
    out.layer.previous_translation=out.sampled.translation;out.layer.previous_rotation=out.sampled.rotation;
    if(layer.attr&1) return out; // Still sampled and saved, but contributes no model change.
    const auto model_q=wide(model.rotation);
    auto aggregate_t=wide(out.sampled.translation);auto aggregate_q=wide(out.sampled.rotation);
    if(!(layer.attr&0x2020)) {
        const auto prior_inverse=inverse(wide(layer.previous_rotation));
        for(std::size_t i=0;i<3;++i) aggregate_t[i]-=layer.previous_translation[i];
        aggregate_t=transform(matrix(product(model_q,prior_inverse)),aggregate_t);
        aggregate_q=product(prior_inverse,aggregate_q);
    }
    if(layer.attr&0x2020) {
        if(layer.attr&0x2000) for(std::size_t i=0;i<3;++i) aggregate_t[i]*=model.scale[i];
        out.model.position=narrow(aggregate_t);
    } else {
        // Preserve the two matrix applications around anisotropic scaling.
        // They are not interchangeable with scaling the final world vector.
        const auto orientation=matrix(model_q);
        auto delta=transform(orientation,aggregate_t,true);
        for(std::size_t i=0;i<3;++i) delta[i]*=static_cast<double>(model.scale[i])*model.null_scale[i];
        delta=transform(orientation,delta);out.delta_position=narrow(delta);
        for(std::size_t i=0;i<3;++i) delta[i]+=model.position[i];
        out.model.position=narrow(delta);
    }
    if(layer.attr&0x80) out.model.rotation=narrow(aggregate_q); // No normalization in this source branch.
    else {
        out.model.rotation=narrow(normalized(product(model_q,aggregate_q)));
        out.delta_rotation=narrow(aggregate_q); // Source delta itself is not normalized.
    }
    return out;
}
}

Output apply_one_layer(const Model& model,const Layer& layer,const Sample& sample,const ResetTemplates& reset) {
    if(layer.attr&~supported_attributes) fail(ErrorCode::unsupported_attributes,"Root-motion attributes outside the verified one-layer subset");
    if(layer.internal_flags!=4||layer.previous_flags!=1)
        fail(ErrorCode::unsupported_internal_flags,"Only the verified application profile internal_flags=4,previous_flags=1 is supported");
    return application_body(model,layer,sample,reset);
}

namespace {
std::uint32_t merged_attributes(const AdvanceState& state,const AdvanceParameters& p) {
    return (state.model_attributes&0xff000000U)|((p.attribute_override|p.attr)&0x00ffffffU);
}
AdvanceOutput advance_body(const AdvanceState& state,const AdvanceParameters& p,const AdvanceLimits& limits) {
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24,
                  "Root-motion clock requires IEEE binary32 floats");
    if(p.attr&~supported_advance_attributes)
        fail(ErrorCode::unsupported_attributes,"Clock attributes outside the verified subset");
    if((state.internal_flags|state.previous_flags)&~supported_advance_flags)
        fail(ErrorCode::unsupported_internal_flags,"Clock flags outside the verified subset");
    if(std::fegetround()!=FE_TONEAREST)
        fail(ErrorCode::unsupported_floating_environment,"Clock requires round-to-nearest floating arithmetic");
    finite(std::array<float,7>{state.frame,state.previous_frame,state.previous_step,
                              p.delta_multiplier,p.speed,p.frame_end,p.loop_start});
    finite(state.previous_translation);finite(p.base_translation);
    valid_quaternion(state.previous_rotation);valid_quaternion(p.base_rotation);
    if(p.frame_end<0||p.loop_start<0||std::trunc(p.frame_end)!=p.frame_end||std::trunc(p.loop_start)!=p.loop_start)
        fail(ErrorCode::unsupported_clock_domain,"Clock end/loop must be nonnegative integer-valued float32 inputs");
    const auto end_index=clock_index(p.frame_end);
    clock_index(p.loop_start);clock_index(state.frame);clock_index(state.previous_frame);
    const float step=clock_value(static_cast<double>(p.delta_multiplier)*p.speed);
    const float interval=clock_subtract(p.frame_end,p.loop_start);
    if(step<0&&!(p.attr&4)&&interval<=0)
        fail(ErrorCode::invalid_loop_interval,"Reverse looping requires a positive interval");
    AdvanceOutput out{state,0,0,{}};auto& next=out.state;
    if((state.internal_flags&4)&&!(state.previous_flags&1))
        out.progress=clock_subtract(clock_add(clock_add(state.previous_frame,state.previous_step),step),p.frame_end);
    else if(state.internal_flags&1) {
        out.progress=clock_subtract(clock_add(state.frame,step),p.frame_end);
        if(out.progress<0)out.progress=clock_subtract(clock_add(state.frame,state.previous_step),p.frame_end);
    }
    if(out.progress<=0)out.progress=0;
    next.previous_frame=state.frame;
    if(!(p.attr&0x10))next.frame=clock_add(state.frame,step);
    next.previous_flags=state.internal_flags;
    next.internal_flags=state.internal_flags&0xff00;
    // Exact source literal 0x3a03126f, not a decimal epsilon selected by us.
    constexpr float epsilon=0x1.0624dep-11f;
    const auto consume_loop=[&] {
        if(out.loop_iterations==limits.max_loop_iterations)
            fail(ErrorCode::loop_limit,"Clock loop budget exhausted before completion");
        ++out.loop_iterations;
    };
    std::int32_t event_index=0;
    bool forward_wrap=false;
    if(step>=0) {
        const auto raw_index=clock_index(next.frame);
        const auto index=clock_index(clock_add(next.frame,epsilon));
        event_index=index;
        const auto next_index=clock_index(clock_add(next.frame,clock_add(step,epsilon)));
        if(raw_index!=index)next.frame=clock_value(index);
        if(next_index>=end_index)next.internal_flags|=1;
        if(p.attr&4) {
            if(index>=end_index) { next.frame=p.frame_end;next.internal_flags=(next.internal_flags&0xff00)|4; }
        } else if(end_index>0&&index>=end_index) {
            forward_wrap=true;
            next.internal_flags=(next.internal_flags&0xff00)|4;
            if(interval<=0)next.frame=p.frame_end;
            else while(event_index>=end_index) {
                consume_loop();
                const float frame=clock_subtract(next.frame,interval);
                if(!(frame<next.frame))fail(ErrorCode::invalid_loop_interval,"Forward wrap does not reduce the float32 frame");
                next.frame=frame;
                event_index=clock_index(clock_add(next.frame,epsilon));
                for(std::size_t i=0;i<3;++i)next.previous_translation[i]=clock_subtract(next.previous_translation[i],p.base_translation[i]);
                next.previous_rotation=narrow(product(inverse(wide(p.base_rotation)),wide(next.previous_rotation)));
            }
        }
    } else {
        const auto raw_index=clock_index(next.frame);
        const auto index=clock_index(clock_subtract(next.frame,epsilon));
        event_index=index;
        if(raw_index!=index)next.frame=clock_value(raw_index);
        if(clock_add(next.frame,step)<0)next.internal_flags|=1;
        if(p.attr&4) {
            if(next.frame<0) { next.frame=0;next.internal_flags|=4; }
        } else if(p.frame_end>0) {
            if(next.frame<0)next.internal_flags|=4;
            while(next.frame<0) {
                consume_loop();
                const float frame=clock_add(next.frame,interval);
                if(!(frame>next.frame))fail(ErrorCode::invalid_loop_interval,"Reverse wrap does not increase the float32 frame");
                next.frame=frame;
                for(std::size_t i=0;i<3;++i)next.previous_translation[i]=clock_add(next.previous_translation[i],p.base_translation[i]);
                next.previous_rotation=narrow(product(wide(p.base_rotation),wide(next.previous_rotation)));
            }
            event_index=clock_index(clock_add(next.frame,epsilon));
        }
    }
    if(p.frame_end==1)next.internal_flags|=1;
    auto& events=out.event_plan;events.preserve_masks=(p.attr&0x10)!=0;
    if(!events.preserve_masks) {
        const auto previous_index=clock_index(state.frame);
        const auto append=[&](std::int32_t begin,std::int32_t end) {
            events.ranges[events.count++]={begin>=end_index?end_index-1:begin,
                                          end>=end_index?end_index-1:end};
        };
        if(step<0)append(event_index,previous_index);
        else if(forward_wrap) {
            if(previous_index<end_index)append(previous_index+1,end_index);
            append(clock_index(p.loop_start),event_index);
        } else {
            const auto end=std::min(event_index,end_index);
            const auto begin=previous_index>=end_index?end_index:
                event_index>previous_index?previous_index+1:previous_index;
            append(begin,end);
            // The supported attribute profile excludes source bit0x200000,
            // so the original anticipatory query runs whenever flag1 is set.
            if(next.internal_flags&1)append(end,end_index);
        }
    }
    if(!(p.attr&0x10)) { next.previous_event_masks=state.event_masks;next.event_masks={}; }
    next.previous_step=step;
    return out;
}
}

AdvanceOutput advance_one_layer(const AdvanceState& state,const AdvanceParameters& p,
                                const AdvanceScope& scope,const AdvanceLimits& limits) {
    if(scope.pre_update!=PreUpdate::declared_unchanged||scope.original_joint_count!=0)
        fail(ErrorCode::unsupported_advance_scope,"Clock requires explicitly unchanged pre-update and zero original joints");
    for(bool present:scope.event_sources_present) if(present)
        fail(ErrorCode::unsupported_advance_scope,"Clock event sources are outside the verified profile");
    auto prepared=state;prepared.model_attributes=merged_attributes(state,p);
    return advance_body(prepared,p,limits);
}

namespace {
void validate_pre_update(const PreUpdateState& state,const AdvanceParameters& p,
    const PreUpdateWeights& weights,const ResetTemplates& reset,const PreUpdateScope& scope,
    EventSourcePolicy event_policy=EventSourcePolicy::excluded) {
    if(!scope.layer_active||scope.original_joint_count!=0)
        fail(ErrorCode::unsupported_pre_update_scope,"Pre-update requires one active layer and zero original joints");
    for(bool present:scope.event_sources_present)if(present&&event_policy!=EventSourcePolicy::masks_only)
        fail(ErrorCode::unsupported_pre_update_scope,"Pre-update event sources are outside the verified profile");
    if(p.attr&~supported_attributes)fail(ErrorCode::unsupported_attributes,"Pre-update attributes outside the verified subset");
    if((state.clock.internal_flags|state.clock.previous_flags)&~supported_advance_flags)
        fail(ErrorCode::unsupported_internal_flags,"Pre-update transition/unknown flags are outside the verified subset");
    if(std::fegetround()!=FE_TONEAREST)
        fail(ErrorCode::unsupported_floating_environment,"Pre-update requires round-to-nearest floating arithmetic");
    finite(std::array<float,9>{state.clock.frame,p.frame_end,p.delta_multiplier,p.speed,
        weights.blend,weights.translation_weight,weights.rotation_weight,state.sample_frame,state.next_frame});
    finite(state.model.position);finite(state.model.scale);finite(state.model.null_scale);valid_quaternion(state.model.rotation);
    finite(state.clock.previous_translation);valid_quaternion(state.clock.previous_rotation);
    finite(state.sampled.translation);finite(state.sampled.rotation);finite(state.delta_position);finite(state.delta_rotation);
    finite(reset.position);finite(reset.rotation);
    if(p.frame_end<0||std::trunc(p.frame_end)!=p.frame_end)
        fail(ErrorCode::unsupported_clock_domain,"Pre-update end must be a nonnegative integer-valued float32 input");
    clock_index(p.frame_end);clock_index(state.clock.frame);
}
void assign_application(PreUpdateState& state,const Output& applied,float current,float future) {
    state.model=applied.model;state.clock.previous_translation=applied.layer.previous_translation;
    state.clock.previous_rotation=applied.layer.previous_rotation;state.sampled=applied.sampled;
    state.delta_position=applied.delta_position;state.delta_rotation=applied.delta_rotation;
    state.sample_frame=current;state.next_frame=future;
}
}

PreUpdateOutput pre_update_one_layer(const PreUpdateState& state,const AdvanceParameters& p,
    const PreUpdateWeights& weights,const std::optional<TimedSample>& sample,const ResetTemplates& reset,const PreUpdateScope& scope) {
    validate_pre_update(state,p,weights,reset,scope);
    PreUpdateOutput out{state,false};
    if(!(state.clock.model_attributes&0x20000000U)) {
        if(sample)fail(ErrorCode::unexpected_sample,"Gate-clear pre-update does not sample motion");
        return out;
    }
    if(state.clock.internal_flags!=4||state.clock.previous_flags!=1)
        fail(ErrorCode::unsupported_internal_flags,"Active pre-update requires the verified no-prediction flags4/1");
    if(!sample)fail(ErrorCode::missing_sample,"Active pre-update requires a declared current sample");
    if(!std::isfinite(sample->frame))fail(ErrorCode::nonfinite_input,"Sample frame must be finite");
    const float current=std::max(0.f,std::min(state.clock.frame,p.frame_end));
    const float step=clock_value(static_cast<double>(p.speed)*p.delta_multiplier);
    const float future=std::max(0.f,std::min(clock_add(state.clock.frame,step),p.frame_end));
    if(sample->frame!=current)fail(ErrorCode::sample_frame_mismatch,"Sample frame differs from the pre-update request");
    const Layer layer{state.clock.previous_translation,state.clock.previous_rotation,p.attr,
        state.clock.internal_flags,state.clock.previous_flags,weights.blend,weights.translation_weight,weights.rotation_weight};
    const auto applied=apply_one_layer(state.model,layer,sample->value,reset);
    assign_application(out.state,applied,current,future);out.applied=true;
    return out;
}

UpdatedAdvanceOutput pre_update_and_advance_one_layer(const PreUpdateState& state,const AdvanceParameters& p,
    const PreUpdateWeights& weights,const std::optional<TimedSample>& sample,const ResetTemplates& reset,
    const PreUpdateScope& scope,const AdvanceLimits& limits) {
    auto prepared=state;prepared.clock.model_attributes=merged_attributes(state.clock,p);
    auto pre=pre_update_one_layer(prepared,p,weights,sample,reset,scope);
    const auto advanced=advance_body(pre.state.clock,p,limits);
    pre.state.clock=advanced.state;
    return {pre.state,pre.applied,advanced.progress,advanced.loop_iterations};
}

SampleRequests sample_requests_one_layer(const PreUpdateState& state,const AdvanceParameters& p,const PreUpdateScope& scope,EventSourcePolicy event_policy) {
    if(!scope.layer_active||scope.original_joint_count!=0)
        fail(ErrorCode::unsupported_pre_update_scope,"Repeated update requires one active layer and zero original joints");
    for(bool present:scope.event_sources_present)if(present&&event_policy!=EventSourcePolicy::masks_only)
        fail(ErrorCode::unsupported_pre_update_scope,"Repeated update excludes event sources");
    if(p.attr&~supported_advance_attributes)
        fail(ErrorCode::unsupported_attributes,"Prediction profile excludes mirror/absolute/unknown attributes");
    if((state.clock.internal_flags|state.clock.previous_flags)&~supported_advance_flags)
        fail(ErrorCode::unsupported_internal_flags,"Prediction excludes transition/layer-update/unknown flags");
    if(std::fegetround()!=FE_TONEAREST)
        fail(ErrorCode::unsupported_floating_environment,"Prediction requires round-to-nearest arithmetic");
    finite(std::array<float,7>{state.clock.frame,state.clock.previous_frame,p.frame_end,p.speed,p.delta_multiplier,
        state.sample_frame,state.next_frame});
    if(p.frame_end<0||std::trunc(p.frame_end)!=p.frame_end)
        fail(ErrorCode::unsupported_clock_domain,"Prediction end must be a nonnegative integer-valued float32 input");
    clock_index(state.clock.frame);clock_index(state.clock.previous_frame);clock_index(p.frame_end);
    SampleRequests plan{{},0,state.sample_frame,state.next_frame,PredictionBranch::gate_clear};
    if(!(state.clock.model_attributes&0x20000000U))return plan;
    plan.current_frame=std::max(0.f,std::min(state.clock.frame,p.frame_end));
    const float step=clock_value(static_cast<double>(p.speed)*p.delta_multiplier);
    plan.next_frame=std::max(0.f,std::min(clock_add(state.clock.frame,step),p.frame_end));
    const auto add=[&](float frame) {for(std::uint32_t i=0;i<plan.count;++i)if(plan.frames[i]==frame)return;plan.frames[plan.count++]=frame;};
    add(plan.current_frame);
    if(state.clock.internal_flags&1) {plan.prediction=PredictionBranch::current_to_end;add(p.frame_end);}
    else if(state.clock.internal_flags&4) {
        if(state.clock.previous_flags&1)plan.prediction=PredictionBranch::preserve;
        else {plan.prediction=PredictionBranch::previous_to_end;add(p.frame_end);add(state.clock.previous_frame);}
    } else {plan.prediction=PredictionBranch::current_to_next;add(plan.next_frame);}
    return plan;
}

namespace {
PredictionOutput prediction_body(const TickState& state,const AdvanceParameters& p,const PreUpdateWeights& weights,
    std::span<const TimedSample> samples,const ResetTemplates& reset,const PreUpdateScope& scope,EventSourcePolicy event_policy) {
    const auto& prepared=state;
    const auto plan=sample_requests_one_layer(prepared.pre,p,scope,event_policy);
    validate_pre_update(prepared.pre,p,weights,reset,scope,event_policy);
    finite(state.prediction_delta.translation);finite(state.prediction_delta.rotation);
    if(samples.size()>plan.count)fail(ErrorCode::unexpected_sample,"More samples than the prediction request set");
    for(std::size_t i=0;i<samples.size();++i) {
        if(!std::isfinite(samples[i].frame))fail(ErrorCode::nonfinite_input,"Sample frame must be finite");
        for(std::size_t j=0;j<i;++j)if(samples[i].frame==samples[j].frame)
            fail(ErrorCode::duplicate_sample,"Prediction samples must have distinct frames");
        bool requested=false;for(std::uint32_t j=0;j<plan.count;++j)requested|=samples[i].frame==plan.frames[j];
        if(!requested)fail(ErrorCode::sample_frame_mismatch,"Sample frame is outside the original request set");
        finite(samples[i].value.translation);valid_quaternion(samples[i].value.rotation);
    }
    if(samples.size()!=plan.count)fail(ErrorCode::missing_sample,"Missing a source-requested prediction sample");
    const auto at=[&](float frame)->const Sample& {
        for(const auto& sample:samples)if(sample.frame==frame)return sample.value;
        fail(ErrorCode::missing_sample,"Missing a source-requested prediction sample");
    };
    PredictionOutput out{prepared,false,false};
    if(plan.prediction!=PredictionBranch::gate_clear) {
        const auto& current=at(plan.current_frame);
        const auto& clock=prepared.pre.clock;
        const Layer layer{clock.previous_translation,clock.previous_rotation,p.attr,clock.internal_flags,clock.previous_flags,
            weights.blend,weights.translation_weight,weights.rotation_weight};
        // The application arithmetic does not inspect flags: the source tests
        // them only in the subsequent prediction block. Keep the actual flags.
        const auto applied=application_body(prepared.pre.model,layer,current,reset);
        assign_application(out.state.pre,applied,plan.current_frame,plan.next_frame);out.applied=true;
        if(plan.prediction!=PredictionBranch::preserve) {
            const auto& first=plan.prediction==PredictionBranch::previous_to_end?at(clock.previous_frame):current;
            const auto& last=at(plan.prediction==PredictionBranch::current_to_next?plan.next_frame:p.frame_end);
            const auto qi=inverse(wide(first.rotation));DVector delta{};
            for(std::size_t i=0;i<3;++i)delta[i]=static_cast<double>(last.translation[i])-first.translation[i];
            out.state.prediction_delta.translation=narrow(transform(matrix(qi),delta));
            out.state.prediction_delta.rotation=narrow(product(qi,wide(last.rotation)));
            out.prediction_written=true;
        }
    }
    return out;
}

struct CalculatedTick { TickOutput output;EventPlan events; };
CalculatedTick calculate_tick(const TickState& state,const AdvanceParameters& p,const PreUpdateWeights& weights,
    std::span<const TimedSample> samples,const ResetTemplates& reset,const PreUpdateScope& scope,const AdvanceLimits& limits,
    EventSourcePolicy event_policy) {
    auto prepared=state;prepared.pre.clock.model_attributes=merged_attributes(state.pre.clock,p);
    auto pre=prediction_body(prepared,p,weights,samples,reset,scope,event_policy);
    const auto advanced=advance_body(pre.state.pre.clock,p,limits);
    pre.state.pre.clock=advanced.state;
    return {{pre.state,pre.applied,pre.prediction_written,advanced.progress,advanced.loop_iterations},advanced.event_plan};
}
}
PredictionOutput pre_update_with_prediction_one_layer(const TickState& state,const AdvanceParameters& p,const PreUpdateWeights& weights,
    std::span<const TimedSample> samples,const ResetTemplates& reset,const PreUpdateScope& scope) {
    return prediction_body(state,p,weights,samples,reset,scope,EventSourcePolicy::excluded);
}
TickOutput tick_one_layer(const TickState& state,const AdvanceParameters& p,const PreUpdateWeights& weights,
    std::span<const TimedSample> samples,const ResetTemplates& reset,const PreUpdateScope& scope,const AdvanceLimits& limits) {
    return calculate_tick(state,p,weights,samples,reset,scope,limits,EventSourcePolicy::excluded).output;
}
PendingTick::PendingTick(TickOutput output,const EventPlan& plan,const std::array<bool,4>& present):output_(output) {
    if(!plan.preserve_masks)for(std::uint32_t group=0;group<4;++group)if(present[group])
        for(std::uint32_t ordinal=0;ordinal<plan.count;++ordinal)
            requests_[count_++]={group,ordinal,plan.ranges[ordinal]};
}
TickOutput PendingTick::finish(std::span<const EventSample> samples) const {
    if(samples.size()>count_)fail(ErrorCode::unexpected_event_sample,"More event samples than requested");
    if(samples.size()<count_)fail(ErrorCode::missing_event_sample,"Missing source-requested event samples");
    auto output=output_;std::array<bool,8> seen{};
    for(const auto& sample:samples) {
        std::uint32_t index=0;while(index<count_&&!(requests_[index]==sample.request))++index;
        if(index==count_)fail(ErrorCode::event_request_mismatch,"Event answer does not identify an exact request");
        if(seen[index])fail(ErrorCode::duplicate_event_sample,"Duplicate event answer");
        seen[index]=true;output.state.pre.clock.event_masks[sample.request.group]|=sample.mask;
    }
    return output;
}
PendingTick plan_tick_with_events(const TickState& state,const AdvanceParameters& p,const PreUpdateWeights& weights,
    std::span<const TimedSample> samples,const ResetTemplates& reset,const PreUpdateScope& scope,const AdvanceLimits& limits) {
    const auto result=calculate_tick(state,p,weights,samples,reset,scope,limits,EventSourcePolicy::masks_only);
    return PendingTick(result.output,result.events,scope.event_sources_present);
}

}
