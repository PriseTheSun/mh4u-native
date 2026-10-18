#include "player_controller_step.hpp"
#include <bit>
#include <new>
#include <stdexcept>
#include <utility>
namespace mh4u::player_controller_step {
SelectionView selection_view(const State& state,std::span<const motion_pending::Layer> layers) noexcept {
    if(layers.size()<2)return Error{Phase::storage,mc::Error{mc::ErrorCode::layer_domain,
        "Player selection requires the canonical layer1 weight",{},{},{},0}};
    const auto& p=state.player;
    return ps::State{{{std::bit_cast<std::uint32_t>(layers[1].animation.layer.weight),p.fieldsd10},
             p.pbw,p.pw,p.global},p.selection_pbw,p.selection_pw};
}
std::optional<Error> store_selection(State& state,std::span<motion_pending::Layer> layers,
    const ps::State& selection) noexcept {
    if(layers.size()<2)return Error{Phase::storage,mc::Error{mc::ErrorCode::layer_domain,
        "Player selection requires the canonical layer1 weight",{},{},{},0}};
    const auto& t=selection.transition;
    state.player={t.player.fieldsd10,t.pbw,t.pw,t.global,selection.pbw,selection.pw};
    layers[1].animation.layer.weight=std::bit_cast<float>(t.player.field3b8);
    return std::nullopt;
}
namespace {
Error capacity_error(){return {Phase::storage,mc::Error{mc::ErrorCode::resource_limit,
    "Controller layer copy exceeds its declared resource policy or host capacity",{},{},{},0}};}
Result complete(const State& input,std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
                ps::BeginResult begun,const motion_request::Model& model,const mc::Globals& globals,
                const pt::Constants& constants,const mc::Sampling& sampling,mc::Limits limits) {
    if(const auto* failed=std::get_if<ps::Error>(&begun))return Error{Phase::selection,*failed};
    Output output{input,{layers.begin(),layers.end()},std::nullopt,{},0,std::nullopt,0,0};
    if(const auto* done=std::get_if<ps::Completed>(&begun)) {
        if(auto e=store_selection(output.state,output.layers,done->state))return *e;output.transition=done->transition;
        return output;
    }
    const auto& pending=std::get<ps::PendingAnimation>(begun);
    if(auto e=store_selection(output.state,output.layers,pending.before_request()))return *e;
    const auto& request=pending.request();output.selection_request=request;
    const mc::Command command{mc::Entry::dispatch,request.mode,0,request.selector,0,
        std::bit_cast<float>(request.interpolation_bits),std::bit_cast<float>(request.frame_bits),1.0F};
    auto result=mc::apply(output.layers,active_count,output.state.motion,model,command,globals,sampling,limits);
    if(const auto* failed=std::get_if<mc::Error>(&result))return Error{Phase::animation_command,*failed};
    auto applied=std::get<mc::Output>(std::move(result));
    output.layers=std::move(applied.layers);output.state.motion=applied.fields;
    output.animation_requests=applied.requests;output.animation_request_count=applied.request_count;
    output.queries=applied.queries;output.samples=applied.samples;
    // Re-project AFTER the command. A layer1 sentinel can change +3b8; flag1
    // of the following transition must preserve the new authoritative value.
    auto resumed=ps::resume(pending,std::get<ps::State>(selection_view(output.state,output.layers)),constants);
    if(const auto* failed=std::get_if<ps::Error>(&resumed))return Error{Phase::transition,*failed};
    auto done=std::get<ps::Completed>(std::move(resumed));
    if(auto e=store_selection(output.state,output.layers,done.state))return *e;output.transition=std::move(done.transition);
    return output;
}
}
Result apply_common(const State& state,std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
                    const ps::CommonArguments& arguments,const motion_request::Model& model,
                    const mc::Globals& globals,const pt::Constants& constants,const mc::Sampling& sampling,mc::Limits limits,const ps::AreaContext& area) {
    try{
    if(auto error=mc::validate_storage(layers,limits))return Error{Phase::storage,*error};
    auto view=selection_view(state,layers);if(auto* error=std::get_if<Error>(&view))return *error;
    return complete(state,layers,active_count,ps::begin_common(std::get<ps::State>(view),arguments,area),model,globals,constants,sampling,limits);
    }catch(const std::bad_alloc&){return capacity_error();}catch(const std::length_error&){return capacity_error();}
}
Result apply_wrapper(const State& state,std::span<const motion_pending::Layer> layers,std::uint32_t active_count,
                     ps::Wrapper wrapper,const ps::WrapperArguments& arguments,const motion_request::Model& model,
                     const mc::Globals& globals,const pt::Constants& constants,const mc::Sampling& sampling,mc::Limits limits,const ps::AreaContext& area) {
    try{
    if(auto error=mc::validate_storage(layers,limits))return Error{Phase::storage,*error};
    auto view=selection_view(state,layers);if(auto* error=std::get_if<Error>(&view))return *error;
    return complete(state,layers,active_count,ps::begin_wrapper(std::get<ps::State>(view),wrapper,arguments,area),model,globals,constants,sampling,limits);
    }catch(const std::bad_alloc&){return capacity_error();}catch(const std::length_error&){return capacity_error();}
}
} // namespace mh4u::player_controller_step
