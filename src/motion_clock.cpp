#include "motion_clock.hpp"
#include "motion_event_kernel.hpp"
#include <bit>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

#ifdef __FAST_MATH__
#error "motion_clock requires ordered binary32 arithmetic"
#endif
namespace mh4u::motion_clock {
namespace {
namespace math=motion_transform_math;
using Bits4=motion_initialization::Bits4;
struct Failure {Error error;};
float value(std::uint32_t b){return std::bit_cast<float>(b);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
constexpr std::uint32_t epsilon_bits=0x3a03126fU;
struct Run {
    const Parameters& parameters;EventResolver resolver;const Limits& limits;
    Output output;std::size_t layer{},group{};bool environment_checked=false;
    [[noreturn]]void fail(ErrorCode code,std::string_view message){throw Failure{{code,message,layer,group,{},{},{}}};}
    template<class T>T checked(math::Result<T> result){
        if(auto* v=std::get_if<T>(&result))return *v;
        const auto e=std::get<math::Error>(result);ErrorCode code=ErrorCode::arithmetic;
        switch(e.code){
        case math::ErrorCode::floating_environment:code=ErrorCode::floating_environment;break;
        case math::ErrorCode::nonfinite_operand:code=ErrorCode::nonfinite_operand;break;
        case math::ErrorCode::numeric_overflow:code=ErrorCode::numeric_overflow;break;
        case math::ErrorCode::missing_template:code=ErrorCode::missing_template;break;
        default:break;
        }
        throw Failure{{code,e.message,layer,group,{},{},e}};
    }
    void environment(){if(!environment_checked){checked(math::multiply(0x3f800000U,0x3f800000U));environment_checked=true;}}
    float input(float x){environment();if(!std::isfinite(x))fail(ErrorCode::nonfinite_operand,"Consumed clock operand is nonfinite");return x;}
    float result(float x){if(!std::isfinite(x))fail(ErrorCode::numeric_overflow,"Nonfinite clock intermediate");return x;}
    float add(float a,float b){return value(checked(math::add(bits(a),bits(b))));}
    float mul(float a,float b){return value(checked(math::multiply(bits(a),bits(b))));}
    float sub(float a,float b){input(a);input(b);volatile float v=a-b;return result(v);}
    float div(float a,float b){input(a);input(b);volatile float v=a/b;return result(v);}
    std::int32_t integer(float f){input(f);if(f<-2147483648.0F||f>=2147483648.0F)fail(ErrorCode::frame_domain,"VCVT signed32 saturation is outside clock profile");return static_cast<std::int32_t>(f);}
    float from_integer(std::int32_t n){environment();volatile float f=static_cast<float>(n);return f;}
    void iteration(){if(output.loop_iterations==limits.loop_iterations)fail(ErrorCode::resource_limit,"Clock loop work limit reached");++output.loop_iterations;}
    float curve(float t,const std::array<std::uint32_t,2>& controls){
        // Exact cd5db4 order; constants3,1,6 are source literals.
        const float a=value(controls[0]),b=value(controls[1]);
        const float squared=mul(t,t),a3=mul(a,3),b3=mul(b,3);
        const float linear=mul(t,a3),cubed=mul(squared,t);
        const float quadratic=sub(b3,mul(a,6));
        const float cubic=add(sub(a3,mul(b,3)),1);
        return add(add(linear,mul(cubed,cubic)),mul(squared,quadratic));
    }
    std::uint32_t events(const std::optional<motion_request::Binding>& binding,std::int32_t begin,std::int32_t end){
        if(!binding)return 0;
        auto failure=[&](ErrorCode code,std::string_view message,const std::optional<ResolutionError>& resolution={}){
            throw Failure{{code,message,layer,group,binding,resolution,{}}};
        };
        if(!resolver.resolve)failure(ErrorCode::unresolved_events,"Present event binding has no resolver");
        auto resolved=resolver.resolve(resolver.context,*binding);
        if(auto* error=std::get_if<ResolutionError>(&resolved))failure(ErrorCode::unresolved_events,"Event binding resolution failed",*error);
        const auto& view=std::get<EventGroup>(resolved);
        if(!binding->resource_identity||view.identity!=*binding)failure(ErrorCode::event_identity,"Event group identity differs from canonical binding");
        if(view.records.size()>limits.event_records||view.records.size()>std::size_t(std::numeric_limits<std::int32_t>::max()))failure(ErrorCode::resource_limit,"Event group count exceeds clock profile");
        if(output.event_records_checked>limits.event_work||view.records.size()>limits.event_work-output.event_records_checked)failure(ErrorCode::resource_limit,"Cumulative event work limit reached");
        output.event_records_checked+=view.records.size();++output.event_queries;
        std::uint64_t cumulative=0;
        for(const auto& record:view.records){cumulative+=record.duration;if(cumulative>std::uint64_t(std::numeric_limits<std::int32_t>::max()))failure(ErrorCode::event_accumulator,"Signed event duration overflow is outside profile");}
        return motion_events::detail::accumulate_validated(view.records,begin,end);
    }
};
std::int32_t increment(std::int32_t n){return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(n)+1U);}
std::int32_t decrement(std::int32_t n){return std::bit_cast<std::int32_t>(std::bit_cast<std::uint32_t>(n)-1U);}
struct Interval {std::int32_t begin{},end{};bool enabled{};};
void clear_snapshots(Run& run,std::uint32_t count){
    if(!count)return;
    if(count>run.limits.snapshots)run.fail(ErrorCode::resource_limit,"Consumed snapshot count exceeds work limit");
    if(std::holds_alternative<Unknown>(run.output.snapshots))run.fail(ErrorCode::unknown_snapshots,"Consumed snapshot pointer is unknown");
    if(std::holds_alternative<Null>(run.output.snapshots))run.fail(ErrorCode::null_snapshots,"Consumed snapshot pointer is null");
    auto& snapshots=std::get<std::vector<motion_pending::Snapshot>>(run.output.snapshots);
    if(count>snapshots.size())run.fail(ErrorCode::snapshot_extent,"Snapshot storage does not cover original joint count");
    for(std::size_t i=0;i<count;++i)if(snapshots[i].layer_index==run.layer){snapshots[i].layer_index=0xffffffffU;++run.output.cleared_snapshots;}
}
void advance_layer(Run& r,motion_pending::Layer& pending,std::uint32_t joint_count){
    auto& state=pending.animation;auto& l=state.layer;
    const float step=r.mul(l.speed,value(r.parameters.delta_time_bits));
    if(l.flags&0x100U){
        const float remaining=r.input(l.remaining_interpolation);
        if(remaining!=0||r.input(l.field60)!=0){
            l.remaining_interpolation=r.sub(remaining,value(r.parameters.delta_time_bits));
            if(l.remaining_interpolation>0){
                l.field60=0;
                if(r.input(l.interpolation)!=0)l.field60=r.curve(r.div(l.remaining_interpolation,l.interpolation),l.interpolation_curve_bits);
            }else{l.remaining_interpolation=0;l.field60=0;l.flags&=0xfeffU;clear_snapshots(r,joint_count);}
        }
    }
    if(l.selector==0xffffU){l.flags|=5U;return;}
    const auto old_flags=l.flags;
    if((old_flags&4U)&&!(l.field08&1U))l.offset=r.sub(r.add(r.add(l.previous_frame,l.previous_step),step),l.end_frame);
    else if(old_flags&1U){l.offset=r.sub(r.add(l.frame,step),l.end_frame);if(l.offset<0)l.offset=r.sub(r.add(l.frame,l.previous_step),l.end_frame);}
    else l.offset=0;
    if(r.input(l.offset)<=0)l.offset=0;
    const float old_frame=l.frame;l.previous_frame=old_frame;
    if(!(l.attributes&0x10U))l.frame=r.add(old_frame,step);
    l.field08=old_flags;l.flags=old_flags&0xff00U;
    const auto end=r.integer(l.end_frame),raw_index=r.integer(l.frame);
    std::int32_t index{};bool wrapped=false;
    if(step>=0){
        index=r.integer(r.add(l.frame,value(epsilon_bits)));
        const auto predicted=r.integer(r.add(l.frame,r.add(step,value(epsilon_bits))));
        if(raw_index!=index)l.frame=r.from_integer(index);
        if(predicted>=end)l.flags|=1U;
        if(l.attributes&4U){if(index>=end){l.frame=l.end_frame;l.flags=(l.flags&0xff00U)|4U;}}
        else if(end>0&&index>=end){
            l.flags=(l.flags&0xff00U)|4U;wrapped=true;
            const float interval=r.sub(l.end_frame,l.field58);
            if(interval<=0)l.frame=l.end_frame;
            else do{
                r.iteration();const float previous=l.frame;
                l.frame=r.sub(l.frame,r.sub(l.end_frame,l.field58));
                if(l.frame>=previous)r.fail(ErrorCode::nonprogressing_loop,"Forward loop cannot decrease its binary32 frame");
                index=r.integer(r.add(l.frame,value(epsilon_bits)));
                for(unsigned i=0;i<3;++i)state.previous_translation[i]=bits(r.sub(value(state.previous_translation[i]),value(state.base_translation[i])));
                const auto inverse=r.checked(math::inverse_rotation(state.base_rotation,r.parameters.inverse_fallback_10e6570));
                state.previous_rotation=r.checked(math::multiply_rotation(inverse,state.previous_rotation));
            }while(index>=end);
        }
    }else{
        index=r.integer(r.sub(l.frame,value(epsilon_bits)));
        if(raw_index!=index)l.frame=r.from_integer(raw_index);
        if(r.add(l.frame,step)<0)l.flags|=1U;
        if(l.attributes&4U){if(l.frame<0){l.flags|=4U;l.frame=0;}}
        else if(l.end_frame>0){
            if(l.frame<0){
                l.flags|=4U;
                do{
                    r.iteration();const float previous=l.frame;
                    l.frame=r.add(r.sub(l.end_frame,l.field58),l.frame);
                    if(l.frame<=previous)r.fail(ErrorCode::nonprogressing_loop,"Reverse loop cannot increase its binary32 frame");
                    for(unsigned i=0;i<3;++i)state.previous_translation[i]=bits(r.add(value(state.previous_translation[i]),value(state.base_translation[i])));
                    state.previous_rotation=r.checked(math::multiply_rotation(state.base_rotation,state.previous_rotation));
                }while(l.frame<0);
            }
            index=r.integer(r.add(l.frame,value(epsilon_bits)));
        }
    }
    if(bits(l.end_frame)==0x3f800000U)l.flags|=1U;
    const auto before=r.integer(l.previous_frame);Interval first{before,index,true},second{};
    if(step>=0){
        if(wrapped){first.end=end;if(end>before)first.begin=increment(before);else first.enabled=false;
            second={r.integer(l.field58),index,true};}
        else{
            if(index>before)first.begin=increment(before);
            if(index>=end)first.end=end;if(before>=end)first.begin=end;
            if((l.flags&1U)&&!(l.attributes&0x200000U))second={first.end,end,true};
        }
    }else first={index,before,true};
    for(auto* interval:{&first,&second}){
        if(interval->begin>=end)interval->begin=decrement(end);
        if(interval->end>=end)interval->end=decrement(end);
    }
    if(!(l.attributes&0x10U))for(r.group=0;r.group<4;++r.group){
        l.previous_masks[r.group]=l.masks[r.group];l.masks[r.group]=0;
        if(first.enabled)l.masks[r.group]=r.events(l.bindings[r.group],first.begin,first.end);
        if(second.enabled)l.masks[r.group]|=r.events(l.bindings[r.group],second.begin,second.end);
    }
    l.previous_step=step;
}
}
Result advance(std::span<const motion_pending::Layer> layers,const Snapshots& snapshots,
    const Parameters& parameters,EventResolver resolver,Limits limits){
    Run run{parameters,resolver,limits,{},0,0,false};
    try{
        if(layers.size()>limits.layers||layers.size()>std::numeric_limits<std::uint32_t>::max())run.fail(ErrorCode::resource_limit,"Clock layer count exceeds policy/source count");
        for(const auto& layer:layers)if(layer.animation.tracks.joints.size()>limits.bindings_per_layer)run.fail(ErrorCode::resource_limit,"Copied joint bindings exceed policy");
        if(const auto* table=std::get_if<std::span<const motion_pending::Snapshot>>(&snapshots.table);table&&table->size()>limits.snapshots)run.fail(ErrorCode::resource_limit,"Copied snapshot storage exceeds policy");
        run.output.layers.assign(layers.begin(),layers.end());
        if(std::holds_alternative<Unknown>(snapshots.table))run.output.snapshots=Unknown{};
        else if(std::holds_alternative<Null>(snapshots.table))run.output.snapshots=Null{};
        else{const auto span=std::get<std::span<const motion_pending::Snapshot>>(snapshots.table);run.output.snapshots=std::vector<motion_pending::Snapshot>(span.begin(),span.end());}
        for(run.layer=0;run.layer<layers.size();++run.layer){run.group=0;advance_layer(run,run.output.layers[run.layer],snapshots.joint_count);}
        return run.output;
    }catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate bounded clock result",run.layer,run.group,{},{},{}};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Clock result exceeds host capacity",run.layer,run.group,{},{},{}};}
}
} // namespace mh4u::motion_clock
