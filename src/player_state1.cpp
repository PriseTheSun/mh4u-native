#include "player_state1.hpp"
#include <new>
#include <stdexcept>
#include <utility>

#ifdef __FAST_MATH__
#error "player_state1 requires strict binary32 operations"
#endif

namespace mh4u::player_state1 {
namespace {
Error failure(ErrorCode code,std::string_view message){return {code,message,{}};}
Error capacity(){return failure(ErrorCode::capacity,"State1 result exceeds bounded host storage");}
std::optional<Error> execute_command(Output& out,const mc::Command& command,std::uint32_t active_count,
    const motion_request::Model& model,const mc::Globals& globals,
    const mc::Sampling& sampling,mc::Limits limits){
    out.effects.command=command;
    auto applied=mc::apply(out.layers,active_count,out.state.motion,model,command,globals,sampling,limits);
    if(auto* error=std::get_if<mc::Error>(&applied))return Error{ErrorCode::motion,error->message,*error};
    auto result=std::get<mc::Output>(std::move(applied));
    out.layers=std::move(result.layers);out.state.motion=result.fields;
    out.effects.requests=result.requests;out.effects.request_count=result.request_count;
    out.effects.queries=result.queries;out.effects.samples=result.samples;
    return {};
}
class Continuation {
    Output& out;
    const ContinuationContext& context;
    std::uint32_t active_count;
    const motion_request::Model& model;
    const mc::Globals& globals;
    const mc::Sampling& sampling;
    mc::Limits limits;
    std::optional<Error> error;
    pcs::PlayerFields& p;

    void fail(ErrorCode code,std::string_view message,Cause cause={}){
        if(!error)error=Error{code,message,std::move(cause)};
    }
    std::uint32_t magnitude(){
        ++out.effects.magnitude_queries;
        const auto value=player_input::gated_magnitude_class({p.pbw.angles,p.pbw.magnitudes},0,
            {p.pbw.field31,p.selection_pbw.field6f9,p.pw.field50d});
        if(const auto* e=std::get_if<player_input::Error>(&value)){fail(ErrorCode::input,e->message,*e);return 0;}
        return std::get<std::uint32_t>(value);
    }
    bool elapsed(){
        if(!out.fields.field50_bits){fail(ErrorCode::unknown_field,"pw+50");return false;}
        const auto value=*out.fields.field50_bits;
        if((value&0x7f800000U)==0x7f800000U){
            fail(ErrorCode::nonfinite_operand,"State1 consumed timer50 must be finite");return false;
        }
        // Exact finite binary32 comparison with zero, including both signed
        // zeros and subnormals. No host DAZ/FTZ or rounding mode can change it.
        return (value&0x80000000U)!=0||(value&0x7fffffffU)==0;
    }
    void orient(){
        const auto source=pr::resolve_state(p.pw.fieldf0,out.shared_fields.pw.field150,
            out.shared_fields.pw.field14c,context.reference);
        if(const auto* e=std::get_if<pr::Error>(&source)){fail(ErrorCode::reference,e->message,*e);return;}
        const auto result=pr::orient(std::get<pr::State>(source),p.pbw.angles[0]);
        if(const auto* e=std::get_if<pr::Error>(&result)){fail(ErrorCode::reference,e->message,*e);return;}
        p.pw.fieldb0=std::get<std::uint32_t>(result);
    }
    bool layer_gate(){
        if(out.layers.empty()){fail(ErrorCode::layer_extent,"State1 continuation consumes canonical layer0 flags");return false;}
        return (out.layers[0].animation.layer.flags&4U)!=0;
    }
    void transition(const pt::Request& request){
        if(!context.constants){fail(ErrorCode::unknown_field,"State1 transition constants");return;}
        auto projected=pcs::selection_view(out.state,out.layers);
        if(auto* e=std::get_if<pcs::Error>(&projected)){fail(ErrorCode::layer_extent,"State1 transition requires canonical layer1",*e);return;}
        auto result=ps::direct_transition(std::get<ps::State>(projected),request,*context.constants);
        if(auto* e=std::get_if<ps::Error>(&result)){fail(ErrorCode::transition,e->message,*e);return;}
        auto completed=std::get<ps::Completed>(std::move(result));
        if(auto e=pcs::store_selection(out.state,out.layers,completed.state)){fail(ErrorCode::controller,"State1 transition projection",*e);return;}
        out.effects.transition=std::move(completed.transition);
    }
    // b1d48c. Repeated virtual calls stay in source order; they are not a
    // cached magnitude or a generic input-to-action mapping.
    bool action1(){
        if(!pt::local_control(p.global,p.pbw))return false;
        if(magnitude()!=0){if(!error)orient();return false;}
        if(error)return false;
        if(elapsed()){
            if(magnitude()==0){
                if(error)return false;
                if(!p.selection_pbw.field6f9){fail(ErrorCode::unknown_field,"pbw+6f9");return false;}
                const auto action=*p.selection_pbw.field6f9!=1&&p.selection_pw.field594!=0?26U:3U;
                transition({1,action,0});return !error;
            }
        }
        if(error)return false;
        if(magnitude()!=0&&!error)orient();
        return false;
    }
    template<class T>T read(const std::optional<T>& field,std::string_view name){
        if(!field){fail(ErrorCode::unknown_field,name);return T{};}return *field;
    }
    bool finite_bits(std::uint32_t value,std::string_view name){
        if((value&0x7f800000U)==0x7f800000U){fail(ErrorCode::nonfinite_operand,name);return false;}
        return true;
    }
    void dynamics(bool cost){
        namespace pd=player_state1_dynamics;
        if(cost)++out.effects.cost_calls;
        auto result=cost?pd::cost(p,out.shared_fields,context.delta_time_bits,context.context_ad50)
                        :pd::advance(p,out.shared_fields,context.delta_time_bits);
        if(const auto* e=std::get_if<pd::Error>(&result)){fail(ErrorCode::dynamics,e->field,*e);return;}
        auto next=std::get<pd::Output>(std::move(result));out.shared_fields=std::move(next.fields);
        if(next.updated){if(cost)++out.effects.cost_updates;else ++out.effects.position_updates;}
    }
    bool spatial_gate(){
        const auto& x=out.shared_fields;
        const auto gate=read(x.pw.field7f0,"pw+7f0");
        if(error)return false;
        if(gate==1)return true;
        const auto flags=read(x.pbw.field478,"pbw+478");
        if(error||(flags&0x40000U)||p.selection_pw.field9a8!=0)return false;
        namespace sa=player_spatial_acquisition;
        auto result=sa::apply(p,x,{0,0xc2700000U},context.spatial);
        if(const auto* e=std::get_if<sa::Error>(&result)){
            fail(ErrorCode::spatial_acquisition,e->field,*e);return false;
        }
        auto acquired=std::get<sa::Output>(std::move(result));
        out.shared_fields=std::move(acquired.fields);
        out.effects.spatial=std::move(acquired.effects);
        return acquired.returned;
    }
    bool enough_cost(){
        const auto value=read(out.shared_fields.pbw.field354_bits,"pbw+354");
        if(error||!finite_bits(value,"pbw+354"))return false;
        // Exact finite comparison against the positive binary32 integer150.
        return !(value&0x80000000U)&&value>=0x43160000U;
    }
    bool objects(){
        ++out.effects.manager_queries;
        auto result=player_object_query::query(p,out.shared_fields,context.manager);
        if(const auto* e=std::get_if<player_object_query::Error>(&result)){fail(ErrorCode::object_query,e->field,*e);return false;}
        out.effects.object_query=std::get<player_object_query::Output>(result);
        return out.effects.object_query->matched;
    }
    bool action2(){
        if(p.pw.fieldf0&0x100U){dynamics(true);if(error)return false;}
        if(!pt::local_control(p.global,p.pbw)){dynamics(false);return false;}
        const auto first=magnitude();if(error)return false;
        if(first!=0&&(p.pbw.field3d4&1U)&&(p.pw.fieldf0&0x100U)){
            orient();if(error)return false;
            dynamics(false);if(error)return false;
            if(spatial_gate()){
                p.pw.fieldb0=p.pbw.field24;transition({2,0,0xc});return !error;
            }
            if(error)return false;
            if(!enough_cost()){
                if(!error)transition({1,11,0});
                return !error;
            }
            if(objects()&&(p.pbw.field3d4&1U)){
                transition({1,13,0});if(!error)p.pw.field17c|=1U;return !error;
            }
            return false;
        }
        if(magnitude()==1){if(!error)transition({1,9,0});return !error;}
        if(error)return false;
        if(elapsed()){
            if(magnitude()==0){if(!error)transition({1,3,0});return !error;}
        }
        if(error)return false;
        if(magnitude()!=0&&!error)orient();
        return false;
    }
    void selected_zero(){
        if(!context.constants){fail(ErrorCode::unknown_field,"State1 selection transition constants");return;}
        auto result=pcs::apply_wrapper(out.state,out.layers,active_count,ps::Wrapper::b1d114,
            {0,4,0,0},model,globals,*context.constants,sampling,limits,context.area);
        if(auto* e=std::get_if<pcs::Error>(&result)){fail(ErrorCode::controller,"State1 selected state0 wrapper failed",*e);return;}
        auto value=std::get<pcs::Output>(std::move(result));
        out.state=std::move(value.state);out.layers=std::move(value.layers);
        out.effects.selection_request=std::move(value.selection_request);
        out.effects.transition=std::move(value.transition);
        out.effects.requests=value.animation_requests;
        out.effects.request_count=value.animation_request_count;
        out.effects.queries=value.queries;out.effects.samples=value.samples;
    }
public:
    Continuation(Output& output,const ContinuationContext& c,std::uint32_t count,
        const motion_request::Model& m,const mc::Globals& g,const mc::Sampling& s,mc::Limits l):
        out(output),context(c),active_count(count),model(m),globals(g),sampling(s),limits(l),p(output.state.player){}
    std::optional<Error> run(std::uint16_t action,std::uint8_t substate){
        if((action==1||action==2)&&(substate==1||substate==2)){
            const bool changed=action==1?action1():action2();
            if(!error&&!changed&&substate==1&&layer_gate()){
                ++p.pbw.fields5[0];
                mc::Command request{};request.entry=mc::Entry::restart;request.selector=4;request.speed=1;
                error=execute_command(out,request,active_count,model,globals,sampling,limits);
            }
        }else if(action==4&&substate==1){
            if(layer_gate())transition({1,5,0});
        }else if(action==9&&substate==1&&pt::local_control(p.global,p.pbw)){
            if(magnitude()!=0){if(!error)orient();}
            else if(!error&&elapsed())selected_zero();
        }
        return error;
    }
};
}
Result apply(const pcs::State& input,const Fields& fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
    const Tables& tables,const motion_request::Model& model,const mc::Globals& globals,
    const mc::Sampling& sampling,mc::Limits limits,const ContinuationContext& context){
    try{
        if(auto error=mc::validate_storage(layers,limits))
            return Error{ErrorCode::capacity,error->message,*error};
        const auto& source=input.player;
        if(source.pbw.field371!=1&&source.pbw.field371!=3)
            return failure(ErrorCode::unsupported_state,"Only state1 handlers and the source state3 return are recovered here");
        const auto action=source.pbw.field372;
        if(source.pbw.field371==1&&action!=1&&action!=2&&action!=4&&action!=9)
            return failure(ErrorCode::unsupported_action,"This state1 action handler is not reconstructed");
        const auto substate=source.pbw.fields5[0];
        Output out{input,fields,{layers.begin(),layers.end()},{},
            context.shared_fields?*context.shared_fields:player_fields::Fields{}};
        auto& p=out.state.player;
        p.selection_pbw.field376=0; // Dispatcher entry, including no-op substates.
        // b66a4c selects b66ae8 (BX LR) for state3. No action, layer,
        // resource, timer or helper context is consumed by that source path.
        if(source.pbw.field371==3)return out;
        if(substate!=0){
            if(auto error=Continuation(out,context,active_count,model,globals,sampling,limits).run(action,substate))return *error;
            return out;
        }
        if(layers.empty())return failure(ErrorCode::layer_extent,"State1 request requires canonical layer0 storage");

        p.pbw.fields5[0]=1;
        // b22094(player,0,0,0), followed by b6cdf0 with handler-specific flags.
        p.selection_pbw.field370=0;
        p.selection_pw.fielda10=0;
        p.pw.fieldf0=(p.pw.fieldf0|1U)&~6U;
        const auto install=[&](TableSlot slot,bool before){
            p.pw.policy=tbl::install(slot==TableSlot::f2efb8?tables.f2efb8:tables.f2f2d0,0);
            out.effects.installation=Installation{slot,before};
        };
        mc::Command command{};
        command.entry=action==1||action==2?mc::Entry::if_changed:mc::Entry::restart;
        command.speed=1.0F;
        switch(action){
        case 1:
            p.pw.fieldf0|=0x80U;
            install(TableSlot::f2efb8,true);
            if(layers[0].animation.layer.selector==4){p.pbw.fields5[0]=2;command.selector=4;}
            else {command.selector=3;command.interpolation=6.0F;}
            break;
        case 2:
            p.pw.fieldf0|=0x180U;
            p.pw.field17c|=1U;
            p.pbw.field39c=p.pw.fieldc4=0x3fa00000U;
            if(layers[0].animation.layer.selector==4){p.pbw.fields5[0]=2;command.selector=4;}
            else command.selector=3;
            command.interpolation=4.0F;
            break;
        case 4:
            p.pw.fieldf0|=0xc0U;
            p.pbw.field24=p.pw.fieldb0;
            command.selector=42;
            break;
        case 9:
            p.pw.fieldf0|=0x40U;
            out.fields.field50_bits=0x41a00000U;
            command.selector=49;command.interpolation=6.0F;
            break;
        }
        if(auto error=execute_command(out,command,active_count,model,globals,sampling,limits))return *error;
        // Tails run even if the setter refused the request or if_changed did
        // not reach it. Neither case is an error in the original command owner.
        switch(action){
        case 1:out.fields.field50_bits=0x41a00000U;break;
        case 2:
            out.fields.field50_bits=0x41a00000U;
            install(TableSlot::f2efb8,false);
            p.pw.policy.timer144_bits=0x41f00000U;
            p.pw.fieldb0=p.pbw.field24;
            break;
        case 4:install(TableSlot::f2f2d0,false);break;
        case 9:install(TableSlot::f2efb8,false);break;
        }
        return out;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
}
