#include "player_update_admission.hpp"
#include <algorithm>
#include <new>
#include <stdexcept>

namespace mh4u::player_update_admission {
namespace {
namespace pcs=player_controller_step;namespace ps=player_selection;
namespace pt=player_transition;namespace oq=player_object_query;
namespace oi=player_object_interaction;namespace pi=player_policy_interpreter;
// VCMPE against+0 followed by BLS: ordered<=0 skips; unordered proceeds.
// No host arithmetic or comparison may collapse NaN into an ordinary false.
constexpr bool positive_or_unordered(std::uint32_t value) noexcept {
    const auto magnitude=value&0x7fffffffU;
    return magnitude>0x7f800000U || (magnitude!=0 && (value&0x80000000U)==0);
}
struct Evaluation {
    Output out;
    const oq::Manager& original_objects;
    const Context& context;
    std::optional<Error> failure;
    const Parameters& parameters()const noexcept{return context.parameters;}
    void fail(Phase phase,ErrorCode code,std::string_view field,Cause cause={}) {
        if(!failure)failure=Error{phase,code,field,std::move(cause)};
    }
    template<class T>T read(std::optional<T> value,Phase phase,std::string_view field) {
        if(value)return *value;
        fail(phase,ErrorCode::unknown_field,field);return {};
    }
    bool local()const noexcept {
        const auto& p=out.state.player;return pt::local_control(p.global,p.pbw);
    }
    oq::Manager objects()const noexcept {
        return out.collection?oq::Manager{&*out.collection}:original_objects;
    }
    std::optional<ps::State> selection() {
        auto result=pcs::selection_view(out.state,out.layers);
        if(const auto* error=std::get_if<pcs::Error>(&result)){
            fail(Phase::projection,ErrorCode::dependency,"selection projection",*error);return {};
        }
        return std::get<ps::State>(std::move(result));
    }
    void store_selection(const ps::State& selected) {
        if(auto error=pcs::store_selection(out.state,out.layers,selected))
            fail(Phase::projection,ErrorCode::dependency,"selection publication",*error);
    }
    void transition(const pt::Request& request) {
        const auto selected=selection();if(!selected)return;
        // b235f8 and the caller's identical376/b1c050/6fa sequence share the
        // existing owner; the layers' shared weight stays authoritative.
        auto result=ps::direct_transition(*selected,request,context.constants);
        if(const auto* error=std::get_if<ps::Error>(&result)){
            fail(Phase::transition,ErrorCode::dependency,error->message,*error);return;
        }
        auto completed=std::get<ps::Completed>(std::move(result));
        store_selection(completed.state);out.effects.transition=std::move(completed.transition);
    }
    void policy() {
        const auto selected=selection();if(!selected)return;
        const auto& p=parameters();
        // selection_view established the canonical two-layer extent.
        const pi::Context facts{out.layers[0].animation.layer,out.state.motion,
            p.prefix.delta_bits,context.constants,p.reference,p.prefix.filter.manager.ad24,
            p.global_c8c,p.direction_bounds};
        auto result=pi::apply(*selected,out.fields,facts);
        if(const auto* error=std::get_if<pi::Error>(&result)){
            fail(Phase::policy,ErrorCode::dependency,error->field,*error);return;
        }
        auto decision=std::get<pi::Output>(std::move(result));
        store_selection(decision.state);out.fields=std::move(decision.fields);
        out.effects.policy=PolicyEffects{decision.returned,decision.visited,decision.visited_count};
        out.effects.transition=std::move(decision.transition);
    }
    void clear_target() {
        auto& pw=out.fields.pw;
        pw.field319=0;pw.field31c=0xffffU;pw.field328_bits=0;pw.field32c_bits=0;
        pw.field338=0;pw.field33a=0;pw.fielda2c=0;
        for(auto& record:out.fields.pbw.linked_records){
            const auto present=read(record.present,Phase::clear_target,"PBW linked record presence");
            if(failure)return;
            if(present)record.flag=1; // Absent preserves even an unknown old flag.
        }
        out.effects.cleared_target=true;
    }
    bool latch_gate() {
        const auto result=player_input_filter::query_manager(parameters().prefix.filter.manager,
            player_input_filter::ManagerQuery::pre_action);
        if(const auto* error=std::get_if<player_input_filter::Error>(&result)){
            fail(Phase::pre_policy,ErrorCode::dependency,error->field,*error);return false;
        }
        if(std::get<bool>(result))return false;
        const auto& p=out.state.player;
        if((p.pbw.field371==0 && p.pbw.field372==108)||p.pbw.field371==9)return false;
        if(read(out.fields.pw.field7f1,Phase::pre_policy,"PW7f1")==0 || failure)return false;
        if(p.selection_pbw.field370==2)return false;
        if(!local())return true;
        return read(out.fields.pbw.field34c,Phase::pre_policy,"PBW34c")>0;
    }
    bool pre_policy() {
        auto& p=out.state.player;auto& pw=out.fields.pw;
        // Source loads PW1ea into a temporary before this gate, but no admitted
        // path uses it. This semantic projection omits that dead read; it does
        // not reproduce source memory accesses or exceptions from unmapped RAM.
        if(p.pbw.field371==8)return false;
        if(p.pw.field490!=0){
            fail(Phase::pre_policy,ErrorCode::unsupported_pre_policy,"b17f78 PW490 branch");return false;
        }
        if(p.selection_pw.field9a8!=0){
            fail(Phase::pre_policy,ErrorCode::unsupported_pre_policy,"b17f78 PW9a8 search");return false;
        }
        const auto first=read(pw.field7fd,Phase::pre_policy,"PW7fd");if(failure)return false;
        if(first==1 && (p.pw.fieldf0&0x4000U) &&
            !(p.pbw.field371==2 && p.pbw.field372==19)){
            transition({6,19,12});return true;
        }
        const auto mode=read(parameters().global_c8c,Phase::pre_policy,"globalC8c");
        if(failure)return false;
        if((mode&0xffU)==20){
            const auto area=read(p.selection_pbw.field9,Phase::pre_policy,"PBW9");
            if(failure)return false;
            if(area==1 && read(pw.field7fd,Phase::pre_policy,"PW7fd")==1 &&
                (p.pw.fieldf0&0x4000000U)){
                transition({6,19,12});return true;
            }
        }
        if(pt::state_action_gate(out.state.player.pbw,1)){
            fail(Phase::pre_policy,ErrorCode::unsupported_pre_policy,"b17f78 5ba61c branch");return false;
        }
        const auto gate=latch_gate();if(failure)return false;
        if(!gate){
            const auto kind=read(pw.field7f4,Phase::pre_policy,"PW7f4");if(failure)return false;
            if(kind!=2 && kind!=4 && kind!=5 && kind!=6 && kind!=7)pw.field7f1=0;
            return false;
        }
        const auto previous=read(pw.field7f1,Phase::pre_policy,"PW7f1");if(failure)return false;
        pw.field7f1=0;
        const auto kind=read(pw.field7f4,Phase::pre_policy,"PW7f4");if(failure)return false;
        if(kind>=8)return false;
        if(kind!=2 && kind!=6 && kind!=7 && previous==0)return false;
        fail(Phase::pre_policy,ErrorCode::unsupported_pre_policy,"b17f78 b22748 branch");return false;
    }
    Result finish() {
        if(failure)return *failure;
        return std::move(out);
    }
    Result run() {
        auto prefix=player_update_prefix::apply(out.state.player,out.fields,parameters().prefix);
        if(const auto* error=std::get_if<player_update_prefix::Error>(&prefix))
            return Error{Phase::prefix,ErrorCode::dependency,"update prefix",*error};
        auto prepared=std::get<player_update_prefix::Output>(std::move(prefix));
        out.state.player=std::move(prepared.player);out.fields=std::move(prepared.fields);
        out.effects.filter=prepared.filter;out.effects.lookup=std::move(prepared.lookup);
        auto& p=out.state.player;auto& pw=out.fields.pw;
        pw.field15a=pw.field159; // Copy unknown knowledge without manufacturing zero.
        if(local()){
            const auto& types=p.pbw.fields2f8;
            if(std::find(types.begin(),types.end(),22)==types.end()){
                auto queried=oq::scan(p,out.fields,original_objects);
                if(const auto* error=std::get_if<oq::Error>(&queried))
                    return Error{Phase::scan,ErrorCode::dependency,error->field,*error};
                auto scanned=std::get<oq::ScanOutput>(std::move(queried));
                out.effects.scan=scanned.effects;out.collection=std::move(scanned.collection);
                if(scanned.matched)pw.field159=1;
                else{pw.field159=0;pw.field158=0;}
            }else{pw.field159=0;pw.field158=0;}
        }else{pw.field159=0;pw.field15a=0;pw.field158=0;}
        p.pw.fieldec&=~0x20000U;
        p.pw.fieldec&=~0x40000U;
        if(!parameters().virtual120)
            return Error{Phase::dispatch,ErrorCode::unknown_field,"virtual120 binding",{}};
        if(*parameters().virtual120!=Virtual120::uplayer_b3dd60)
            return Error{Phase::dispatch,ErrorCode::unsupported_virtual,"virtual120 binding",{}};
        out.effects.virtual120_returned=true; // Exact known4-byte BX LR body.
        if(local()){
            const auto override=read(pw.field0a,Phase::caller,"PW0a");if(failure)return finish();
            if(override!=0){pw.field1b8_bits=0x40800000U;pw.field1bc_bits=0x40800000U;
                out.effects.local_override=true;return finish();}
        }
        bool search=false;
        if(local()){
            const auto target=read(pw.field31c,Phase::caller,"PW31c");if(failure)return finish();
            if(target==0xffffU && p.pbw.field371!=6 && p.pbw.field371!=7 && p.pbw.field371!=8){
                const auto first=read(pw.field224,Phase::caller,"PW224");if(failure)return finish();
                if(first<=0){
                    const auto second=read(pw.field238,Phase::caller,"PW238");if(failure)return finish();
                    if(second<1){search=positive_or_unordered(read(pw.field324_bits,Phase::caller,"PW324"));
                        if(failure)return finish();}
                }
            }
        }
        if(search){
            const auto manager=objects();
            auto queried=oi::query(p,out.fields,manager,parameters().resources);
            if(const auto* error=std::get_if<oi::Error>(&queried))
                return Error{Phase::interaction,ErrorCode::dependency,error->field,*error};
            const auto interaction=std::get<oi::Output>(std::move(queried));
            out.effects.interaction=interaction;
            if(interaction.selected){
                clear_target();if(failure)return finish();
                // The original reloads the chosen root's D after b2ac60. Use
                // the same collection owner and the actual returned identity.
                auto resolved_manager=oq::resolve_manager(manager);
                if(const auto* error=std::get_if<oq::Error>(&resolved_manager))
                    return Error{Phase::clear_target,ErrorCode::dependency,error->field,*error};
                auto resolved=oq::resolve(*std::get<const oq::Collection*>(resolved_manager),*interaction.selected);
                if(const auto* error=std::get_if<oq::Error>(&resolved))
                    return Error{Phase::clear_target,ErrorCode::dependency,error->field,*error};
                pw.field31c=read(std::get<oq::ResolvedData>(resolved).fields->field1c4,Phase::clear_target,"D1c4");
                if(failure)return finish();
                transition({2,25,0});return finish();
            }
        }
        const auto admitted=pre_policy();if(failure)return finish();
        out.effects.pre_policy_returned=admitted;
        if(!admitted)policy();
        return finish();
    }
};
}
Result apply(const pcs::State& state,std::span<const motion_pending::Layer> layers,
    const player_fields::Fields& fields,const oq::Manager& objects,const Context& context,
    motion_command::Limits limits) {
    try {
    if(auto error=motion_command::validate_storage(layers,limits))
        return Error{Phase::storage,ErrorCode::capacity,error->message,
            pcs::Error{pcs::Phase::storage,*error}};
    Output out{state,{layers.begin(),layers.end()},fields,{},{}};
    return Evaluation{std::move(out),objects,context,{}}.run();
    }catch(const std::bad_alloc&){
        return Error{Phase::storage,ErrorCode::capacity,"Admission host allocation",{}};
    }catch(const std::length_error&){
        return Error{Phase::storage,ErrorCode::capacity,"Admission host extent",{}};
    }
}
} // namespace mh4u::player_update_admission
