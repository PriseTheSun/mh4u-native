#include "motion_application.hpp"
#include <bit>
#include <cmath>
#include <utility>
#ifdef __FAST_MATH__
#error "motion_application requires ordered binary32 arithmetic"
#endif
namespace mh4u::motion_application {
namespace {
namespace tm=motion_transform_math;
struct Failure {Error error;};
float value(std::uint32_t b){return std::bit_cast<float>(b);}
std::uint32_t bits(float x){return std::bit_cast<std::uint32_t>(x);}
Bits3 xyz(const Bits4& a){return {a[0],a[1],a[2]};}
Bits4 vector(const Bits3& a){return {a[0],a[1],a[2],0};}
void mirror(Bits4& t,Bits4& q){t[0]^=0x80000000U;q[1]^=0x80000000U;q[2]^=0x80000000U;}
struct Run {
    const motion_initialization::Templates& templates;
    motion_initialization::Resolver resolver;
    Limits limits;Output out;std::size_t li{};Component component=Component::none;
    bool environment_checked=false;
    Error error(ErrorCode code,std::string_view message)const{return {code,message,li,component,{},{},{},{}};}
    [[noreturn]]void fail(ErrorCode code,std::string_view message)const{throw Failure{error(code,message)};}
    template<class T>T math(tm::Result<T> result){
        if(auto* e=std::get_if<tm::Error>(&result)){auto err=error(ErrorCode::arithmetic,"Original model arithmetic failed");err.arithmetic=*e;throw Failure{err};}
        return std::get<T>(result);
    }
    [[noreturn]]void arithmetic(tm::ErrorCode code,std::string_view message){auto err=error(ErrorCode::arithmetic,"Original model arithmetic failed");err.arithmetic=tm::Error{code,message};throw Failure{err};}
    float input(float x){
        if(!environment_checked){math(tm::multiply(0x3f800000U,0x3f800000U));environment_checked=true;}
        if(!std::isfinite(x))arithmetic(tm::ErrorCode::nonfinite_operand,"Consumed operand is nonfinite");return x;
    }
    float result(float x){if(!std::isfinite(x))arithmetic(tm::ErrorCode::numeric_overflow,"Nonfinite binary32 intermediate");return x;}
    float add(float a,float b){input(a);input(b);volatile float x=a+b;return result(x);}
    float sub(float a,float b){input(a);input(b);volatile float x=a-b;return result(x);}
    float mul(float a,float b){input(a);input(b);volatile float x=a*b;return result(x);}
    float mla(float a,float b,float c){return add(a,mul(b,c));}
    template<class T>const T& known(const std::optional<T>& v){if(!v)fail(ErrorCode::missing_template,"Consumed global template is unknown");return *v;}
    Bits4 sample(motion_binding::Channel& channel,std::uint32_t frame){
        std::optional<motion_sampler::TrackView> view;
        if(channel.track){
            if(!resolver.resolve){auto e=error(ErrorCode::unresolved_track,"Present track has no resolver");e.track=channel.track;throw Failure{e};}
            auto result=resolver.resolve(resolver.context,*channel.track);
            if(auto* e=std::get_if<motion_initialization::ResolutionError>(&result)){auto err=error(ErrorCode::unresolved_track,"Present track resolution failed");err.track=channel.track;err.resolution=*e;throw Failure{err};}
            view=std::get<motion_sampler::TrackView>(result);
        }
        auto result=motion_sampler::sample(channel,view?&*view:nullptr,frame,limits.sampling);
        if(auto* e=std::get_if<motion_sampler::Error>(&result)){auto err=error(ErrorCode::sampling,"Original channel sampling failed");err.track=channel.track;err.sampling=*e;throw Failure{err};}
        const auto sampled=std::get<motion_sampler::Sample>(result);channel=sampled.channel;++out.samples;return sampled.value_bits;
    }
    Bits3 difference(const Bits4& a,const Bits4& b){Bits3 out;for(unsigned i=0;i<3;++i)out[i]=bits(sub(value(a[i]),value(b[i])));return out;}
    Bits4 inverse(const Bits4& q){return math(tm::inverse_rotation(q,templates.inverse_fallback_10e6570));}
    Bits3 rotate(const Bits4& q,const Bits3& v,bool relative_order=false){
        const auto m=math(tm::rt_matrix(q,{}));Bits3 out;
        for(unsigned i=0;i<3;++i){float r;
            if(relative_order){r=mul(value(m[i+4]),value(v[1]));r=mla(r,value(m[i+8]),value(v[2]));r=add(r,mla(0,value(m[i]),value(v[0])));}
            else{r=mul(value(m[i]),value(v[0]));r=mla(r,value(m[i+4]),value(v[1]));r=mla(r,value(m[i+8]),value(v[2]));}
            out[i]=bits(r);
        }return out;
    }
    void predict(motion_pending::Layer& layer,const Bits4& current_t,const Bits4& current_q){
        auto& state=layer.animation;const auto& clock=state.layer;
        if(!(clock.flags&1U)&&(clock.flags&4U)&&(clock.field08&1U))return;
        Bits4 base_t=current_t,base_q=current_q,t,q;
        const auto end=bits(clock.end_frame);
        const auto frame=(clock.flags&(1U|4U))?end:bits(clock.prepared_next_frame);
        component=Component::translation;t=sample(state.tracks.translation,frame);
        component=Component::rotation;q=sample(state.tracks.rotation,frame);
        if(!(clock.flags&1U)&&(clock.flags&4U)){
            component=Component::translation;base_t=sample(state.tracks.translation,bits(clock.previous_frame));
            component=Component::rotation;base_q=sample(state.tracks.rotation,bits(clock.previous_frame));
        }
        if(clock.attributes&0x8100U)mirror(t,q);
        component=Component::translation;const auto inv=inverse(base_q);
        state.prediction_translation=vector(rotate(inv,difference(t,base_t)));
        component=Component::rotation;
        state.prediction_rotation=math(tm::multiply_rotation(inv,q));
        ++out.prediction_updates;
    }
    void model_delta(const Bits3& accumulated){
        auto& m=out.model;const auto basis=math(tm::rt_matrix(m.rotation,{}));Bits3 local,scaled,delta;
        for(unsigned i=0;i<3;++i){float x=mul(value(basis[4*i]),value(accumulated[0]));x=mla(x,value(basis[4*i+1]),value(accumulated[1]));x=mla(x,value(basis[4*i+2]),value(accumulated[2]));local[i]=bits(x);}
        for(unsigned i=0;i<3;++i)scaled[i]=bits(mul(mul(value(m.scale[i]),value(m.null_scale[i])),value(local[i])));
        for(unsigned i=0;i<3;++i){float x=mul(value(basis[i]),value(scaled[0]));x=mla(x,value(basis[i+4]),value(scaled[1]));x=mla(x,value(basis[i+8]),value(scaled[2]));delta[i]=bits(x);}
        for(unsigned i=0;i<3;++i)m.position[i]=bits(add(value(m.position[i]),value(delta[i])));
        m.position[3]=0;m.translation_delta=vector(delta);
    }
};
}
Result apply(const Model& previous,std::span<const motion_pending::Layer> layers,
    const motion_initialization::Templates& templates,motion_initialization::Resolver resolver,Limits limits){
    Run run{templates,resolver,limits,Output{previous,{}}};
    if(layers.size()>limits.layers)return run.error(ErrorCode::resource_limit,"Layer count exceeds work bound");
    for(const auto& layer:layers)if(layer.animation.tracks.joints.size()>limits.bindings_per_layer)
        return run.error(ErrorCode::resource_limit,"Copied binding count exceeds work bound");
    try{
        // These two templates are loaded unconditionally before the count gate.
        Bits3 accumulated_t=run.known(templates.base_translation_10e62c0);
        Bits4 accumulated_q=run.known(templates.identity_10e6560);
        run.out.layers.assign(layers.begin(),layers.end());bool first=true;
        for(run.li=0;run.li<layers.size();++run.li){
            auto& layer=run.out.layers[run.li];auto& state=layer.animation;
            const auto& clock=state.layer;if(clock.selector==0xffff)continue;
            run.component=Component::translation;auto t=run.sample(state.tracks.translation,bits(clock.prepared_frame));t[3]=0;
            run.component=Component::rotation;auto q=run.sample(state.tracks.rotation,bits(clock.prepared_frame));
            if(clock.attributes&0x8100U)mirror(t,q);
            layer.translation_c8=t;layer.rotation_98=q;++run.out.sampled_layers;
            if(!(clock.attributes&1U)){
                Bits3 incoming_t=xyz(t);Bits4 incoming_q=q;
                if(!(clock.attributes&0x2020U)){
                    const auto inv=run.inverse(state.previous_rotation);
                    const auto basis=run.math(tm::multiply_rotation(previous.rotation,inv));
                    run.component=Component::translation;
                    incoming_t=run.rotate(basis,run.difference(t,state.previous_translation),true);
                    run.component=Component::rotation;incoming_q=run.math(tm::multiply_rotation(inv,q));
                }
                if(first){accumulated_t=incoming_t;accumulated_q=incoming_q;first=false;}
                else{
                    run.component=Component::translation;const auto wt=run.math(tm::multiply(bits(clock.weight),state.tracks.translation.weight_bits));
                    run.component=Component::rotation;const auto wq=run.math(tm::multiply(bits(clock.weight),state.tracks.rotation.weight_bits));
                    if(clock.attributes&0x40000U){
                        run.component=Component::translation;accumulated_t=run.math(tm::add_scaled(accumulated_t,incoming_t,wt));
                        run.component=Component::rotation;const auto delta=run.math(tm::nlerp(run.known(templates.identity_10e6560),incoming_q,wq));
                        accumulated_q=run.math(tm::multiply_rotation(delta,accumulated_q));
                    }else{
                        run.component=Component::translation;accumulated_t=run.math(tm::lerp(accumulated_t,incoming_t,wt));
                        run.component=Component::rotation;accumulated_q=run.math(tm::nlerp(accumulated_q,incoming_q,wq));
                    }
                }
                ++run.out.contributing_layers;
            }
            run.predict(layer,t,q);state.previous_translation=t;state.previous_rotation=q;
        }
        run.component=Component::model;auto& model=run.out.model;
        if(first){model.rotation_delta=run.known(templates.identity_10e6560);model.translation_delta=vector(run.known(templates.previous_translation_10e6370));return run.out;}
        const auto attributes=layers.front().animation.layer.attributes;
        if(attributes&0x2020U){
            model.position=vector(accumulated_t);
            if(attributes&0x2000U)for(unsigned i=0;i<3;++i)model.position[i]=bits(run.mul(value(accumulated_t[i]),value(model.scale[i])));
            model.translation_delta=vector(run.known(templates.previous_translation_10e6370));
        }else run.model_delta(accumulated_t);
        if(attributes&0x80U){model.rotation=accumulated_q;model.rotation_delta=run.known(templates.identity_10e6560);}
        else{model.rotation=run.math(tm::normalize_rotation(run.math(tm::multiply_rotation(model.rotation,accumulated_q))));model.rotation_delta=accumulated_q;}
        return run.out;
    }catch(const Failure& f){return f.error;}
}
} // namespace mh4u::motion_application
