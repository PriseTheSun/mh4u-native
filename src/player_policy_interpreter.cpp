#include "player_policy_interpreter.hpp"
#include "player_status_predicates.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <initializer_list>
#include <limits>

#ifdef __FAST_MATH__
#error "player_policy_interpreter requires strict binary32 arithmetic"
#endif
namespace mh4u::player_policy_interpreter {
namespace {
namespace pt=player_transition;namespace tbl=player_policy_table;
float add(float a,float b){volatile float r=a+b;return r;}
float sub(float a,float b){volatile float r=a-b;return r;}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
std::int32_t signed_bits(float f){return std::bit_cast<std::int32_t>(f);}
bool one_of(std::uint32_t v,std::initializer_list<std::uint32_t> list){return std::find(list.begin(),list.end(),v)!=list.end();}
class Evaluation {
    const Context& c;
    std::optional<Error> error;
    std::optional<std::size_t> index;
    Output out;
    pt::State& t;
    player_fields::Fields& x;
    void fail(ErrorCode code,std::string_view field,Cause cause={}){if(!error)error=Error{code,field,index,std::move(cause)};}
    template<class T>T read(const std::optional<T>& v,std::string_view field){if(!v){fail(ErrorCode::unknown_field,field);return T{};}return *v;}
    float finite(float f,std::string_view field){if(!std::isfinite(f))fail(ErrorCode::nonfinite_operand,field);return f;}
    float arithmetic(float f,std::string_view field){if(!std::isfinite(f))fail(ErrorCode::numeric_overflow,field);return f;}
    std::optional<player_input::ReferenceContext> reference()const {
        const auto resolved=player_reference::resolve_state(t.pw.fieldf0,x.pw.field150,x.pw.field14c,c.reference);
        // Predicates consume the optional reference lazily. Preserve the old
        // unavailable-context boundary instead of failing an unrelated record.
        if(const auto* state=std::get_if<player_reference::State>(&resolved))
            return player_input::ReferenceContext{t.pw.fieldb0,*state};
        return {};
    }
    player_input::MagnitudeGate magnitude_gate()const{return {t.pbw.field31,out.state.pbw.field6f9,t.pw.field50d};}
    std::uint32_t magnitude(){
        const auto r=player_input::gated_magnitude_class({t.pbw.angles,t.pbw.magnitudes},0,magnitude_gate());
        if(const auto* e=std::get_if<player_input::Error>(&r)){fail(ErrorCode::input,"virtual+98",*e);return 0;}
        return std::get<std::uint32_t>(r);
    }
    void orient(){
        const auto ref=reference();if(!ref){fail(ErrorCode::unknown_field,"b1061c reference");return;}
        const auto r=player_reference::orient(ref->source,t.pbw.angles[0]);
        if(const auto* e=std::get_if<player_reference::Error>(&r)){fail(ErrorCode::reference,"b1061c",*e);return;}
        t.pw.fieldb0=std::get<std::uint32_t>(r);
    }
    bool predicate(const tbl::Record& record){
        std::array<std::optional<std::uint32_t>,20> flags;flags[0]=t.pbw.field3d4;
        for(std::size_t i=0;i<x.pbw.words3d8.size();++i)
            flags[i+1]=player_input_storage::join(x.pbw.words3d8[i]);
        const player_policy_predicate::Context context{{t.pbw.angles,t.pbw.magnitudes},magnitude_gate(),
            x.pbw.field3a8,x.pbw.field3ac,x.pbw.field3b8,x.pbw.field3bc,flags,t.pbw.field451,
            reference(),t.pbw.field24,x.pw.field154,c.direction_bounds};
        const auto r=player_policy_predicate::evaluate(player_policy_predicate::decode(record.raw),context);
        if(const auto* e=std::get_if<player_policy_predicate::Error>(&r)){fail(ErrorCode::predicate,"b24354",*e);return false;}
        return std::get<bool>(r);
    }
    bool timing(std::uint32_t mode,float lower,float upper){
        const auto r=player_policy_timing::query({c.layer,c.motion,c.delta_time_bits},{mode,bits(lower),bits(upper)});
        if(const auto* e=std::get_if<player_policy_timing::Error>(&r)){fail(ErrorCode::timing,"b09da8",*e);return false;}
        return std::get<bool>(r);
    }
    // b2a7ec's retained-value branch. The producing query remains an explicit
    // unsupported path, never an invented hit/miss or default height.
    bool height(std::uint16_t id){
        if(read(x.pw.field6a8,"pw+6a8")==0){if(!error)fail(ErrorCode::unsupported_context,"b2a7ec spatial acquisition");return false;}
        const auto h=finite(std::bit_cast<float>(read(x.pw.field6a4_bits,"pw+6a4")),"pw+6a4");
        const auto y=finite(std::bit_cast<float>(read(x.pbw.field14_bits,"pbw+14")),"pbw+14");
        const float offset=id==5?60.0F:id==6?180.0F:280.0F,width=id==5?120.0F:id==6?100.0F:20.0F;
        const auto lower=arithmetic(add(y,offset),"height lower");
        if(error||!(lower<=h))return false;
        // Original b2a968/b2a96c adds the two offsets before the coordinate.
        // Reassociating with the rounded lower endpoint changes large y cases.
        const auto upper=arithmetic(add(y,add(offset,width)),"height upper");
        if(error||!(h<=upper))return false;
        if((read(c.global_c8c,"global+c8c")&255U)==20U&&read(out.state.pbw.field9,"pbw+9")==1)
            fail(ErrorCode::unsupported_context,"b2a7ec area20 spatial check");
        return !error&&out.state.pw.field9a8==0;
    }
    enum class Gate{accept,skip,clear};
    Gate context(const tbl::Record& r,bool& already){
        const auto id=static_cast<std::uint16_t>(r.field0e()&0xfffU);
        if(r.field0e()&0x8000U){fail(ErrorCode::unsupported_context,"context bit8000 helper chain");return Gate::skip;}
        if((r.field0e()&0x4000U)&&std::bit_cast<float>(t.pw.policy.timer144_bits)>0)return Gate::clear;
        switch(id){
        case 0:case 0x46:return Gate::accept;
        case 5:case 6:case 7:return height(id)?Gate::accept:Gate::skip;
        case 8:case 9:case 11:
            if(r.field00()&0x80U)return Gate::skip;
            if(t.pbw.field371==1){
                if(id==8)already=one_of(t.pbw.field372,{1,3,5,11,13,14,21,22,23,24,25,26,30,32,33});
                else if(id==9)already=one_of(t.pbw.field372,{2,3,10,11,13,14,22,24,27,30});
                else already=one_of(t.pbw.field372,{1,2,3,5,6,7,9,10,11,13,14});
            }else if(t.pbw.field371==12&&id!=11)already=t.pbw.field372==(id==8?1:2);
            return Gate::accept;
        case 10:return t.pw.fielde8&0x80U?Gate::accept:Gate::skip;
        case 12:
            if(read(x.pw.field159,"pw+159")==1&&read(x.pw.field15a,"pw+15a")!=1&&read(x.pw.field158,"pw+158")==0){
                if(error)return Gate::skip;
                x.pw.field158=1;
                if(std::bit_cast<float>(read(x.pw.field15c_bits,"pw+15c"))<=0){x.pw.field15c_bits=0x45610000U;return Gate::accept;}
            }
            return Gate::skip;
        case 0x12:return read(x.pw.field188,"pw+188")>49?Gate::accept:Gate::clear;
        case 0x24:return read(x.pbw.field478,"pbw+478")&0x20U?Gate::accept:Gate::clear;
        case 0x27:return read(x.pbw.field93ee,"pbw+93ee")!=0?Gate::accept:Gate::clear;
        case 0x2a:{
            const auto accepted=player_status_predicates::status224(x.pw.field224,t.pbw);
            if(!accepted){fail(ErrorCode::unknown_field,"pw+224");return Gate::skip;}
            return *accepted?Gate::accept:Gate::clear;
        }
        case 0x2e:x.pbw.field6fc=1;return read(x.pbw.field6fb,"pbw+6fb")!=0?Gate::accept:Gate::clear;
        case 0x2f:
            if(!one_of(read(c.context_ad24,"context+ad24"),{4,5,6,7}))return Gate::clear;
            return t.pw.fielde8&8U?Gate::clear:Gate::accept;
        case 0x4c:return read(x.pw.field238,"pw+238")!=0?Gate::accept:Gate::clear;
        default:fail(ErrorCode::unsupported_context,"unimplemented context ID");return Gate::skip;
        }
    }
public:
    Evaluation(const player_selection::State& state,const player_fields::Fields& fields,const Context& context):
        c(context),out{state,fields,false,{},0,{}},t(out.state.transition),x(out.fields){}
    Result run(){
        auto& policy=t.pw.policy;
        if(std::holds_alternative<tbl::Unknown>(policy.table))return Error{ErrorCode::unknown_table,"pw+100",{}, {}};
        if(std::holds_alternative<tbl::Absent>(policy.table))return out;
        // Ordered compare: NaN does not take BGT. Preserve the raw timer bits.
        if(std::bit_cast<float>(policy.timer140_bits)>0||!pt::local_control(t.global,t.pbw))return out;
        // Retain the immutable table across the transition that clears pw+100.
        const auto table=std::get<tbl::Table>(policy.table);
        if(table.records().empty())return out;
        if(!std::numeric_limits<float>::is_iec559||sizeof(float)!=4||std::fegetround()!=FE_TONEAREST)
            return Error{ErrorCode::floating_environment,"FPSCR0/binary32",{}, {}};
        volatile float tiny=std::bit_cast<float>(1U),twice=tiny+tiny;
        if(bits(static_cast<float>(twice))!=2U)return Error{ErrorCode::floating_environment,"gradual underflow",{}, {}};
        const auto duration=finite(c.layer.end_frame,"layer.end_frame");
        const auto last=arithmetic(sub(duration,1.0F),"end_frame-1");
        const auto original_orientation=t.pw.fieldb0;
        if(error)return *error;
        for(std::size_t i=0;i<table.records().size();++i){
            index=i;out.visited[out.visited_count++]=static_cast<std::uint8_t>(i);
            const auto& r=table.records()[i];bool end_flag=false,already=false;
            const auto threshold=[&](std::int16_t raw,std::uint16_t override_value,bool start){
                if(override_value)return static_cast<float>(override_value);
                if(raw>=999){if(start)end_flag=true;return c.layer.field58>0?duration:last;}
                return raw<0?arithmetic(add(static_cast<float>(raw),duration),"relative threshold"):static_cast<float>(raw);
            };
            const auto lower=threshold(r.field10(),policy.field104,true);
            auto upper=threshold(r.field12(),policy.field108,false);
            if(policy.field108&&upper>duration)upper=duration;
            const auto window=policy.field106?static_cast<float>(policy.field106):static_cast<float>(r.field14());
            const bool no_upper=signed_bits(upper)<0x3f800000;
            if(signed_bits(window)<0x4479c000&&!(no_upper&&end_flag&&(c.layer.flags&4U))){
                const auto pre_lower=arithmetic(sub(lower,window),"lower-window");
                const bool accepted=timing(no_upper?6:10,pre_lower,upper);
                if(error)return *error;
                if(!accepted)continue;
            }
            if(signed_bits(window)<0x3f800000)policy.latches[i]=0;
            const auto pressed=predicate(r);if(error)return *error;
            if(pressed)policy.latches[i]=1;else if(policy.latches[i]==0)continue;
            if(r.field06()&0x8000U){
                const auto gate=context(r,already);if(error)return *error;
                if(gate==Gate::clear)policy.latches[i]=0;
                if(gate!=Gate::accept)continue;
            }
            if(!policy.latches[i])continue;
            if(r.field00()&0x80U){fail(ErrorCode::unsupported_special_action,"state high-bit dispatch");return *error;}
            std::uint32_t state=r.field00()&0x7fU,action=r.field02(),flags=r.field04();
            const bool ready=no_upper?(end_flag?bool(c.layer.flags&4U):timing(6,lower,upper)):
                timing(r.field10()==r.field12()?0:10,lower,upper);
            if(error)return *error;
            if(!ready)continue;
            if(already){out.returned=true;return out;}
            if((r.field06()&0x8000U)&&one_of(r.field0e()&0xfffU,{0x46,0x55,0x5f})&&out.state.pbw.field370!=0)continue;
            if(state==0&&action==0){fail(ErrorCode::unsupported_selection,"b1d114 state0 wrapper");return *error;}
            if(state==1&&one_of(action,{1,2,10})){if(magnitude()){orient();const auto delta=static_cast<std::uint16_t>(t.pw.fieldb0-t.pbw.field24);
                if(delta>=0x6000&&delta<=0xa000){action=4;flags=0;}}}
            else if(state==1&&one_of(action,{0,6,7,9,13,14,25,27,28,30})){if(magnitude())orient();}
            if((state==0&&one_of(action,{0x8e,0x8f,0x90,0xa3,0xa7,0xa8,0xa9,0xaa,0xab,0xac,0xad,0xae,0xaf,0xb0}))||(state==12&&action==0x1e))t.pw.fieldb0=original_orientation;
            if((state==0&&one_of(action,{8,0x26,0x69,0x6a,0x6b}))||(state==12&&one_of(action,{0x11,0x17,0x1b}))){
                fail(ErrorCode::unsupported_suffix,"spatial orientation suffix");return *error;}
            if((r.field06()&0x4000U)||(state==5&&action==0)){if(magnitude())orient();}
            if(state==0&&action==0x12)x.pbw.field93f1=1;
            if(flags&0x20U)t.pw.fieldbc=0;
            if((flags&0x80U)&&player_selection::flag6f9(read(out.state.pbw.field6f9,"pbw+6f9")))flags&=~0x80U;
            if(error)return *error;
            if(policy.field148!=0)continue;
            const pt::Request request{state,action,flags};
            auto result=pt::apply(t,request,c.constants);
            if(const auto* e=std::get_if<pt::Error>(&result)){fail(ErrorCode::transition,"b1c050",*e);return *error;}
            auto changed=std::get<pt::Output>(std::move(result));
            t=std::move(changed.state);out.transition=player_selection::TransitionEffects{request,changed.effective_flags,changed.returned_dispatch_call};
            out.returned=true;return out;
        }
        return out;
    }
};
}
Result apply(const player_selection::State& state,const player_fields::Fields& fields,const Context& context){return Evaluation(state,fields,context).run();}
}
