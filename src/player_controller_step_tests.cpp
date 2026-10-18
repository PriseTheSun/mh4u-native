#include "player_controller_step.hpp"
#include <bit>
#include <iostream>
#include <stdexcept>
#include <vector>
#ifdef MH4U_PLAYER_CONTROLLER_STEP_GENERATED
#include "player_controller_step_fixtures.hpp"
#include "player_transition_constants.hpp"
#endif
namespace {
namespace pcs=mh4u::player_controller_step;
namespace ps=mh4u::player_selection;
namespace pt=mh4u::player_transition;
namespace mc=mh4u::motion_command;
namespace mr=mh4u::motion_request;
namespace mp=mh4u::motion_pending;
std::size_t checks=0;
void check(bool good,const char* message){++checks;if(!good)throw std::runtime_error(message);}
template<class T,class... U>T value(std::variant<U...> result){
    if(!std::holds_alternative<T>(result))throw std::runtime_error("Unexpected result variant");
    return std::get<T>(std::move(result));
}
bool bits_equal(float x,float y){return std::bit_cast<std::uint32_t>(x)==std::bit_cast<std::uint32_t>(y);}
bool layers_equal(std::span<const mp::Layer> x,std::span<const mp::Layer> y){
    if(x.size()!=y.size())return false;
    for(std::size_t i=0;i<x.size();++i){
        const auto& xa=x[i].animation;const auto& ya=y[i].animation;
        if(xa.tracks!=ya.tracks||xa.prediction_rotation!=ya.prediction_rotation||xa.prediction_translation!=ya.prediction_translation||
           xa.previous_rotation!=ya.previous_rotation||xa.base_rotation!=ya.base_rotation||xa.previous_translation!=ya.previous_translation||
           xa.base_translation!=ya.base_translation||x[i].rotation_98!=y[i].rotation_98||x[i].translation_c8!=y[i].translation_c8)return false;
        const auto& a=xa.layer;const auto& b=ya.layer;
        if(a.selector!=b.selector||a.flags!=b.flags||a.field08!=b.field08||a.attributes!=b.attributes||a.bindings!=b.bindings||a.masks!=b.masks||
           a.interpolation_curve_bits!=b.interpolation_curve_bits||a.previous_masks!=b.previous_masks)return false;
        const std::array aa{a.requested_frame,a.offset,a.interpolation,a.remaining_interpolation,a.weight,a.frame,a.previous_frame,a.end_frame,a.field58,a.speed,a.field60,a.prepared_frame,a.prepared_next_frame,a.previous_step};
        const std::array bb{b.requested_frame,b.offset,b.interpolation,b.remaining_interpolation,b.weight,b.frame,b.previous_frame,b.end_frame,b.field58,b.speed,b.field60,b.prepared_frame,b.prepared_next_frame,b.previous_step};
        for(std::size_t j=0;j<aa.size();++j)if(!bits_equal(aa[j],bb[j]))return false;
    }
    return true;
}
bool equal(const pcs::State& a,const pcs::State& b){return a.player==b.player&&a.motion==b.motion;}
void projection(){
    pcs::State state;std::array<mp::Layer,2> layers{};state.player.fieldsd10={1,2,3};state.player.selection_pw.field594=17;
    for(auto word:{0U,0x80000000U,0x3f800000U,0x7fc00001U,0xffffffffU}){
        layers[1].animation.layer.weight=std::bit_cast<float>(word);const auto view=value<ps::State>(pcs::selection_view(state,layers));
        check(view.transition.player.field3b8==word,"Shared weight projection changed bits");
        check(view.transition.player.fieldsd10==state.player.fieldsd10&&view.pw.field594==17,"Other selection fields lost");
    }
    check(equal(state,state)&&layers_equal(layers,layers),"Bitwise comparison mishandled unchanged NaN");
    check(value<pcs::Error>(pcs::selection_view(state,{})).phase==pcs::Phase::storage,"Absent shared owner accepted");
}
#ifdef MH4U_PLAYER_CONTROLLER_STEP_GENERATED
struct Captured {pcs::State state;std::array<mp::Layer,2> layers;std::uint32_t active_count;};
Captured materialize(const controller_oracle::Captured& source){
    Captured out{source.state,{},source.active_count};
    for(unsigned i=0;i<2;++i)out.layers[i].animation.layer=source.layers[i];
    return out;
}
void original(){
    const auto constants=value<pt::Constants>(pt::Constants::decode(pt::generated::constants_bytes,pt::generated::expected_constants()));
    const auto globals=controller_oracle::globals();
    std::vector<std::optional<mr::Action>> slots(controller_oracle::slot_count);
    for(const auto& source:controller_oracle::actions){
        mr::Action action{source.frames,source.loop,true,{}};
        for(std::size_t i=0;i<4;++i)action.groups[i]=std::span(controller_oracle::records).subspan(source.ranges[i][0],source.ranges[i][1]);
        slots[source.slot]=action;
    }
    mr::Model resources;resources.skeleton_present=true;resources.banks[0]=mr::Resource{1,std::span<const std::optional<mr::Action>>(slots)};
    std::size_t index=0,calls=0,accepted=0,weight_preserved_after_sentinel=0;
    for(const auto& c:controller_oracle::cases){
        auto model=resources;if(!c.bank_present)model.banks[0]=mr::Absent{};
        const auto before=materialize(c.before),after=materialize(c.after);
        auto result=c.entry==0?pcs::apply_common(before.state,before.layers,before.active_count,c.arguments,model,globals,constants):
            pcs::apply_wrapper(before.state,before.layers,before.active_count,c.entry==1?ps::Wrapper::b1d114:ps::Wrapper::b23b74,
                {c.arguments.pl_state,c.arguments.interpolation,c.arguments.frame,c.arguments.extra_bits},model,globals,constants);
        if(const auto* failure=std::get_if<pcs::Error>(&result))throw std::runtime_error("Controller rejected ARM case "+std::to_string(index)+" phase "+std::to_string(static_cast<int>(failure->phase)));
        const auto out=value<pcs::Output>(std::move(result));
        if(!(equal(out.state,after.state)&&layers_equal(out.layers,after.layers)))throw std::runtime_error("Full canonical state differs from ARM case "+std::to_string(index));
        check((equal(materialize(c.before).state,before.state)&&layers_equal(materialize(c.before).layers,before.layers)),"Input changed");check((equal(out.state,after.state)&&layers_equal(out.layers,after.layers)),"State differs");
        check(out.selection_request.has_value()==c.routed,"Router presence differs");
        if(c.routed)check(*out.selection_request==c.router,"Router arguments differ");
        check(out.animation_request_count==c.count,"Setter count differs");
        for(std::size_t i=0;i<c.count;++i){
            const auto& got=out.animation_requests[i];const auto& want=c.requests[i];
            check(got.request.layer==want.request.layer&&got.request.selector==want.request.selector&&got.request.attributes==want.request.attributes,"Setter integer arguments differ");
            check(bits_equal(got.request.interpolation,want.request.interpolation)&&bits_equal(got.request.start_frame,want.request.start_frame)&&bits_equal(got.request.speed,want.request.speed),"Setter float arguments differ");
            check(got.command_flags==want.command_flags&&got.accepted==want.accepted,"Setter outcome/command flags differ");
            ++calls;if(got.accepted)++accepted;
        }
        check(out.transition.has_value()==c.transitioned,"Transition presence differs");
        if(c.transitioned){
            const auto& t=*out.transition;
            check(std::array{t.arguments.state_arg,t.arguments.action_arg,t.arguments.flags_arg}==c.transition_args,"Transition arguments differ");
            check(t.effective_flags==c.effective_flags,"Transition effective flags differ");
            check(t.returned_dispatch_call.has_value()==c.dispatched,"Returned dispatch presence differs");
            if(c.dispatched)check(t.returned_dispatch_call==pt::ObservedDispatchCall{c.dispatch_selector,c.dispatch_flags},"Returned dispatch arguments differ");
            if(c.count==2&&(c.transition_args[2]&1U)){
                check(bits_equal(out.layers[1].animation.layer.weight,1.0F),"Transition restored stale weight before sentinel");
                ++weight_preserved_after_sentinel;
            }
        }
        check(value<ps::State>(pcs::selection_view(out.state,out.layers)).transition.player.field3b8==std::bit_cast<std::uint32_t>(out.layers[1].animation.layer.weight),"Shared owner diverged");
        ++index;
    }
    // Every rejection remains transactional across all three producers.
    const auto& base=controller_oracle::cases[16];const auto initial=materialize(base.before);
    auto args=base.arguments;args.pl_state=257;
    check(value<pcs::Error>(pcs::apply_common(initial.state,initial.layers,initial.active_count,args,resources,globals,constants)).phase==pcs::Phase::selection,"Selection error phase lost");
    mr::Model unknown;unknown.skeleton_present=true;
    check(value<pcs::Error>(pcs::apply_common(initial.state,initial.layers,initial.active_count,base.arguments,unknown,globals,constants)).phase==pcs::Phase::animation_command,"Unknown bank hidden");
    auto root_required=initial;root_required.layers[0].animation.layer.flags=0;
    auto error=value<pcs::Error>(pcs::apply_common(root_required.state,root_required.layers,root_required.active_count,base.arguments,resources,globals,constants));
    check(error.phase==pcs::Phase::animation_command&&std::get<mc::Error>(error.cause).code==mc::ErrorCode::query_failed,"Unknown query templates skipped");
    error=value<pcs::Error>(pcs::apply_common(initial.state,initial.layers,initial.active_count,base.arguments,resources,mc::Globals{},constants));
    check(error.phase==pcs::Phase::animation_command&&std::get<mc::Error>(error.cause).code==mc::ErrorCode::unknown_table,"Unknown snapshot invented");
    auto rejected=initial;rejected.state.player.global.field5d=1;const auto saved=rejected;
    error=value<pcs::Error>(pcs::apply_common(rejected.state,rejected.layers,rejected.active_count,base.arguments,resources,globals,constants));
    check(error.phase==pcs::Phase::transition&&std::get<ps::Error>(error.cause).transition_code==pt::ErrorCode::optional_dispatch_body,"Transition error hidden");
    check(equal(rejected.state,saved.state)&&layers_equal(rejected.layers,saved.layers)&&equal(initial.state,base.before.state)&&layers_equal(initial.layers,materialize(base.before).layers),"Rejected composition changed input");
    const auto& skipped=controller_oracle::cases[240];const auto skipped_before=materialize(skipped.before);
    check(value<pcs::Output>(pcs::apply_common(skipped_before.state,skipped_before.layers,skipped_before.active_count,skipped.arguments,unknown,mc::Globals{},constants)).animation_request_count==0,"Unchanged selector unnecessarily resolved resources");
    args=base.arguments;args.pl_state=3;
    const auto done=value<pcs::Output>(pcs::apply_common(initial.state,initial.layers,initial.active_count,args,unknown,mc::Globals{},constants));
    check(!done.selection_request&&!done.transition&&done.animation_request_count==0,"State3 reached command or transition");
    check(index==257&&calls==372&&accepted==160&&weight_preserved_after_sentinel>0,"Original coverage changed");
    std::cout<<"{\"complete_arm_cases\":"<<index<<",\"setter_calls\":"<<calls<<",\"accepted_setters\":"<<accepted<<",\"shared_weight_preserved_after_sentinel\":"<<weight_preserved_after_sentinel<<",\"state_mismatches\":0}\n";
}
#endif
}
int main(){try{projection();
#ifdef MH4U_PLAYER_CONTROLLER_STEP_GENERATED
original();
#endif
std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
