#include "motion_initialization.hpp"
#include "motion_transform_math.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <utility>

#ifdef __FAST_MATH__
#error "motion_initialization requires strict binary32 operations"
#endif
namespace mh4u::motion_initialization {
namespace {
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view message) {
    throw Failure{Error{code,message,{},{},{},{}}};
}
float value(std::uint32_t bits){return std::bit_cast<float>(bits);}
std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
struct Math {
    bool checked=false;
    void environment() {
        if(checked)return;
        if(std::fegetround()!=FE_TONEAREST)
            fail(ErrorCode::floating_environment,"Nearest-even binary32 arithmetic required");
        volatile float tiny=value(1),twice=tiny+tiny;
        if(bits(twice)!=2)fail(ErrorCode::floating_environment,"Gradual underflow required");
        checked=true;
    }
    float input(float x) {
        environment();
        if(!std::isfinite(x))fail(ErrorCode::nonfinite_operand,"Consumed arithmetic operand is nonfinite");
        return x;
    }
    float result(float x) {
        if(!std::isfinite(x))fail(ErrorCode::numeric_overflow,"Binary32 operation produced a nonfinite result");
        return x;
    }
    float add(float x,float y){input(x);input(y);volatile float r=x+y;return result(r);}
    float sub(float x,float y){input(x);input(y);volatile float r=x-y;return result(r);}
    float mul(float x,float y){input(x);input(y);volatile float r=x*y;return result(r);}
    float mla(float acc,float x,float y){return add(acc,mul(x,y));}
    float mls(float acc,float x,float y){return sub(acc,mul(x,y));}
};
using Floats=std::array<float,4>;
Floats floats(const Bits4& b){return {value(b[0]),value(b[1]),value(b[2]),value(b[3])};}
Bits4 raw(const Floats& f){return {bits(f[0]),bits(f[1]),bits(f[2]),bits(f[3])};}
template<class T> const T& required(const std::optional<T>& x) {
    if(!x)fail(ErrorCode::missing_template,"Consumed global template snapshot is unknown");
    return *x;
}
Bits4 vector(const Bits3& v){return {v[0],v[1],v[2],0};}
Floats inverse(const Bits4& q,const Templates& t) {
    auto result=motion_transform_math::inverse_rotation(q,t.inverse_fallback_10e6570);
    if(const auto* value=std::get_if<Bits4>(&result))return floats(*value);
    const auto error=std::get<motion_transform_math::Error>(result);
    using E=motion_transform_math::ErrorCode;
    switch(error.code){
    case E::floating_environment:fail(ErrorCode::floating_environment,error.message);
    case E::nonfinite_operand:fail(ErrorCode::nonfinite_operand,"Consumed arithmetic operand is nonfinite");
    case E::missing_template:fail(ErrorCode::missing_template,"Consumed global template snapshot is unknown");
    default:fail(ErrorCode::numeric_overflow,"Binary32 operation produced a nonfinite result");
    }
}
// VMLA/VMLS are separate binary32 multiply and add/subtract on this source.
// Preserve the instruction order within each component, including signed zero.
Bits4 product(const Floats& a,const Floats& b,Math& m) {
    float x=m.mul(a[3],b[0]);x=m.mla(x,a[0],b[3]);x=m.mla(x,a[1],b[2]);x=m.mls(x,a[2],b[1]);
    float y=m.mul(a[3],b[1]);y=m.mla(y,a[1],b[3]);y=m.mla(y,a[2],b[0]);y=m.mls(y,a[0],b[2]);
    float z=m.mul(a[3],b[2]);z=m.mla(z,a[2],b[3]);z=m.mla(z,a[0],b[1]);z=m.mls(z,a[1],b[0]);
    float w=m.mul(a[3],b[3]);w=m.mls(w,a[0],b[0]);w=m.mls(w,a[1],b[1]);w=m.mls(w,a[2],b[2]);
    return raw({x,y,z,w});
}
void mirror(Bits4& t,Bits4& q) {t[0]^=0x80000000U;q[1]^=0x80000000U;q[2]^=0x80000000U;}
Bits4 relative_translation(const Bits4& sample,const Bits4& q,const Bits4& prediction,Math& m) {
    const auto r=floats(q),p=floats(prediction),s=floats(sample);
    const float xx2=m.mul(r[0],2),yy2=m.mul(r[1],2),zz2=m.mul(r[2],2);
    const float yy=m.mul(r[1],yy2),wy=m.mul(r[3],yy2),xx=m.mul(r[0],xx2),zz=m.mul(r[2],zz2);
    const float xy=m.mul(r[0],yy2),wx=m.mul(r[3],xx2),xz=m.mul(r[0],zz2),yz=m.mul(r[1],zz2),wz=m.mul(r[3],zz2);
    const std::array<std::array<float,3>,3> matrix{{
        {m.sub(1,m.add(yy,zz)),m.sub(xy,wz),m.add(xz,wy)},
        {m.add(xy,wz),m.sub(1,m.add(xx,zz)),m.sub(yz,wx)},
        {m.sub(xz,wy),m.add(yz,wx),m.sub(1,m.add(xx,yy))}}};
    Bits4 out{};
    for(unsigned i=0;i<3;++i) {
        float transformed=m.mul(matrix[i][0],p[0]);
        transformed=m.mla(transformed,matrix[i][1],p[1]);
        transformed=m.mla(transformed,matrix[i][2],p[2]);
        out[i]=bits(m.sub(s[i],transformed));
    }
    return out;
}
} // namespace
Result initialize(const State& previous,std::span<const std::uint8_t> map,
    const motion_binding::Action* action,const Parameters& p,Resolver resolver,Limits limits) {
    auto prepared=motion_binding::prepare_tracks(previous.layer,previous.tracks,map,action,limits.binding);
    if(const auto* e=std::get_if<motion_binding::Error>(&prepared))
        return Error{ErrorCode::binding,"Track preparation failed",{},{},*e,{}};
    auto bound=std::get<motion_binding::Prepared>(std::move(prepared));
    Output out{State{bound.layer,std::move(bound.tracks),previous.prediction_rotation,previous.prediction_translation,
        previous.previous_rotation,previous.base_rotation,previous.previous_translation,previous.base_translation},bound.action_present,0};
    if(!bound.action_present)return out;
    try {
        Math math;
        auto sample=[&](motion_binding::Channel& channel,std::uint32_t frame) {
            std::optional<motion_sampler::TrackView> view;
            if(channel.track) {
                if(!resolver.resolve)throw Failure{Error{ErrorCode::unresolved_track,"Present track has no resolver",channel.track,{},{},{}}};
                auto resolved=resolver.resolve(resolver.context,*channel.track);
                if(const auto* e=std::get_if<ResolutionError>(&resolved))
                    throw Failure{Error{ErrorCode::unresolved_track,"Present track resolution failed",channel.track,*e,{},{}}};
                view=std::get<motion_sampler::TrackView>(resolved);
            }
            auto result=motion_sampler::sample(channel,view?&*view:nullptr,frame,limits.sampling);
            if(const auto* e=std::get_if<motion_sampler::Error>(&result))
                throw Failure{Error{ErrorCode::sampling,"Original channel sampling failed",channel.track,{},{},*e}};
            auto sampled=std::get<motion_sampler::Sample>(result);
            channel=sampled.channel;++out.samples;return sampled.value_bits;
        };
        auto& state=out.state;const auto attr=state.layer.attributes;
        auto& translation=state.tracks.translation;auto& rotation=state.tracks.rotation;
        // prepare_tracks makes loop nonnegative, so the original negative-loop
        // header-copy paths cannot be reached by this complete entry point.
        if(attr&8U)state.base_translation=vector(required(p.templates.base_translation_10e62c0));
        else {
            const auto first=sample(translation,bits(state.layer.field58));
            const auto last=sample(translation,bits(state.layer.end_frame));
            state.base_translation={};
            for(unsigned i=0;i<3;++i)state.base_translation[i]=bits(math.sub(value(last[i]),value(first[i])));
        }
        if(attr&0x1000U)state.base_rotation=required(p.templates.identity_10e6560);
        else {
            const auto first=sample(rotation,bits(state.layer.field58));
            const auto last=sample(rotation,bits(state.layer.end_frame));
            state.base_rotation=product(inverse(first,p.templates),floats(last),math);
        }
        if(attr&0x8100U)mirror(state.base_translation,state.base_rotation);
        if(attr&0x400U) {
            const float frame=math.mls(state.layer.frame,state.layer.speed,value(p.model_delta_time_bits));
            if(frame<=0) {
                state.previous_translation=vector(required(p.templates.previous_translation_10e6370));
                state.previous_rotation=required(p.templates.identity_10e6560);
            } else {
                state.previous_translation=sample(translation,bits(frame));state.previous_translation[3]=0;
                state.previous_rotation=sample(rotation,bits(frame));
            }
            if(attr&0x8100U)mirror(state.previous_translation,state.previous_rotation);
        } else if(attr&2U) {
            state.previous_translation=vector(required(p.templates.previous_translation_10e6370));
            state.previous_rotation=required(p.templates.identity_10e6560);
            // The source returns here without mirroring the previous templates.
        } else if(attr&0x10000U) {
            auto t=sample(translation,bits(state.layer.requested_frame));
            auto q=sample(rotation,bits(state.layer.requested_frame));
            if(attr&0x8100U)mirror(t,q);
            state.previous_rotation=product(floats(q),inverse(state.prediction_rotation,p.templates),math);
            state.previous_translation=relative_translation(t,state.previous_rotation,state.prediction_translation,math);
        } else {
            state.previous_translation=sample(translation,bits(state.layer.frame));state.previous_translation[3]=0;
            state.previous_rotation=sample(rotation,bits(state.layer.frame));
            if(attr&0x8100U)mirror(state.previous_translation,state.previous_rotation);
        }
        return out;
    } catch(const Failure& f){return f.error;}
}
} // namespace mh4u::motion_initialization
