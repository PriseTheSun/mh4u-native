#include "motion_command.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

#ifdef __FAST_MATH__
#error "motion_command requires strict binary32 operations"
#endif

namespace mh4u::motion_command {
namespace {
static_assert(sizeof(float)==4&&std::numeric_limits<float>::is_iec559);
float add(float a,float b) noexcept {volatile float value=a+b;return value;}
float divide(float a,float b) noexcept {volatile float value=a/b;return value;}
Error error(ErrorCode code,std::string_view message) noexcept {return {code,message,{},{},{},0};}
std::optional<Error> issue(Output& out,const motion_request::Model& model,
                           std::uint32_t layer,std::uint32_t selector,std::uint32_t flags,
                           float interpolation,float frame,float speed,const Globals& globals,const Sampling& sampling,std::uint32_t active_count,const Limits& limits) {
    if(layer>=out.layers.size())return error(ErrorCode::layer_domain,"Requested layer is outside explicit storage");
    if(!std::isfinite(interpolation)||!std::isfinite(frame)||!std::isfinite(speed))
        return error(ErrorCode::nonfinite_input,"Finite binary32 command operands required");
    if(std::fegetround()!=FE_TONEAREST)return error(ErrorCode::floating_environment,"Nearest-even binary32 required");
    volatile float tiny=std::bit_cast<float>(std::uint32_t{1});
    volatile float doubled=tiny+tiny;
    if(std::bit_cast<std::uint32_t>(static_cast<float>(doubled))!=2U)
        return error(ErrorCode::floating_environment,"Gradual binary32 underflow required");
    auto& state=out.fields;
    const auto old_selector=out.layers[0].animation.layer.selector; // concrete virtual +84
    if(interpolation>0)interpolation=add(interpolation,2.0f);
    if(layer==0) {
        state.fieldd68=std::bit_cast<std::uint32_t>(interpolation);
        if(speed!=0)interpolation=divide(interpolation,speed);
    }
    if(!std::isfinite(interpolation))return error(ErrorCode::nonfinite_input,"Interpolation division left finite profile");
    if(!(out.layers[0].animation.layer.flags&2)) {
        if(model.skeleton_present) {
            if(active_count>out.layers.size())return error(ErrorCode::query_extent,"Active query prefix exceeds explicit layer storage");
            if(active_count>limits.query.layers)return error(ErrorCode::resource_limit,"Active query prefix exceeds work policy");
            auto checked=[&](motion_transform_math::Result<std::uint32_t> v)->std::variant<std::uint32_t,Error>{
                if(auto* e=std::get_if<motion_transform_math::Error>(&v))return Error{ErrorCode::arithmetic,"Command frame/difference arithmetic failed",{},{},*e,out.queries};
                return std::get<std::uint32_t>(v);
            };
            const auto previous=std::bit_cast<std::uint32_t>(out.layers[0].animation.layer.previous_frame);
            auto difference=checked(motion_transform_math::add(previous,0xbf800000U));
            if(auto* e=std::get_if<Error>(&difference))return *e;
            auto first_frame=std::get<std::uint32_t>(difference);const bool below=std::bit_cast<float>(first_frame)<0;
            if(below)first_frame=0;
            const auto second_frame=below?0x3f800000U:previous;
            std::array<motion_initialization::Bits4,2> translations;
            for(unsigned qi=0;qi<2;++qi){
                std::vector<motion_query::LayerView> views;views.reserve(active_count);
                for(std::uint32_t i=0;i<active_count;++i){const auto& a=out.layers[i].animation;views.push_back({a.layer,a.tracks.translation,a.tracks.rotation});}
                auto result=motion_query::query(views,qi?second_frame:first_frame,sampling.templates,{true,false},sampling.resolver,limits.query);
                if(auto* e=std::get_if<motion_query::Error>(&result))return Error{ErrorCode::query_failed,"Live model query failed",{},*e,{},out.queries};
                auto& queried=std::get<motion_query::Output>(result);translations[qi]=*queried.translation;
                for(std::size_t i=0;i<queried.channels.size();++i){out.layers[i].animation.tracks.translation=queried.channels[i].translation;out.layers[i].animation.tracks.rotation=queried.channels[i].rotation;}
                ++out.queries;out.samples+=queried.samples;
            }
            for(unsigned i=0;i<3;++i){auto delta=checked(motion_transform_math::add(translations[1][i],translations[0][i]^0x80000000U));if(auto* e=std::get_if<Error>(&delta))return *e;state.fieldsd70[i]=std::get<std::uint32_t>(delta);}
            state.fieldsd70[3]=0;
        }else {
        if(!globals.vector10e6370)return error(ErrorCode::unknown_vector,"Source global vector snapshot is unknown");
        for(unsigned i=0;i<3;++i)state.fieldsd70[i]=(*globals.vector10e6370)[i];
        state.fieldsd70[3]=0;
        }
    }
    // b04f70 compares signed representation before a floating comparison.
    const auto raw=std::bit_cast<std::int32_t>(interpolation);
    const std::uint32_t attributes=raw>0x358637bd||interpolation<0?0x10000U:0U;
    const motion_request::Request request{layer,selector,attributes,interpolation,frame,speed};
    const auto applied=motion_request::apply(out.layers[layer].animation.layer,model,request);
    if(const auto failure=std::get_if<motion_request::Error>(&applied))
        return Error{ErrorCode::request_failed,"Numeric motion setter rejected the command",*failure,{},{},0};
    const auto& result=std::get<motion_request::Output>(applied);
    out.layers[layer].animation.layer=result.layer;
    out.requests[out.request_count++]={request,flags,result.accepted};
    if(!result.accepted)return std::nullopt;
    const unsigned table=(flags&8)?1:(flags&0x10)?2:0;
    if(!globals.fields6c_tables[table])return error(ErrorCode::unknown_table,"Selected source table snapshot is unknown");
    out.layers[layer].animation.layer.interpolation_curve_bits=*globals.fields6c_tables[table];
    if(flags&0x20)out.layers[layer].animation.layer.attributes=0x2084;
    if(layer==0) {
        state.fieldsd48[0]=old_selector;
        state.fieldsd48[1]=state.fieldsd48[2]=std::bit_cast<std::uint32_t>(frame);
        state.fieldsd48[3]=state.fieldsd48[4]=0;
        state.fieldsd48[5]=std::bit_cast<std::uint32_t>(out.layers[0].animation.layer.end_frame);
        state.fieldsd48[6]=(flags&1)?flags:flags|0x8000;
    }
    return std::nullopt;
}
}
std::optional<Error> validate_storage(std::span<const motion_pending::Layer> layers,const Limits& limits) noexcept {
    if(layers.size()>limits.storage_layers||layers.size()>std::numeric_limits<std::uint32_t>::max())
        return error(ErrorCode::resource_limit,"Layer storage exceeds policy/source count");
    std::size_t bindings=0;
    for(const auto& layer:layers){
        const auto count=layer.animation.tracks.joints.size();
        if(count>limits.joint_bindings-bindings)
            return error(ErrorCode::resource_limit,"Aggregate copied joint bindings exceed policy");
        bindings+=count;
    }
    return std::nullopt;
}
Result apply(std::span<const motion_pending::Layer> previous,std::uint32_t active_count,
 const Fields& fields,const motion_request::Model& model,const Command& command,
 const Globals& globals,const Sampling& sampling,Limits limits) {
 try {
    if(auto failure=validate_storage(previous,limits))return *failure;
    Output out;out.layers.assign(previous.begin(),previous.end());out.fields=fields;
    auto entry=command.entry;
    if(entry==Entry::dispatch) {
        switch(command.mode) {
        case 0:entry=Entry::if_changed;break;
        case 1:entry=Entry::restart;break;
        case 2:entry=Entry::variant;break;
        default:return out;
        }
    }
    if(entry==Entry::if_changed){
        if(previous.empty())return error(ErrorCode::layer_domain,"Selector getter requires layer zero storage");
        if(command.selector==previous[0].animation.layer.selector)return out;
    }
    float interpolation=command.interpolation,speed=command.speed;
    auto flags=command.flags;
    switch(entry) {
    case Entry::restart:case Entry::if_changed:case Entry::variant:
        flags=entry==Entry::variant?0x11:0;
        if(interpolation<0){interpolation=-interpolation;flags|=2;}
        speed=1;
        break;
    case Entry::common:break;
    case Entry::layer:
        if(const auto failure=issue(out,model,command.layer,command.selector,flags,interpolation,command.start_frame,speed,globals,sampling,active_count,limits))return *failure;
        return out;
    default:return error(ErrorCode::entry,"Unknown native command entry");
    }
    if(const auto failure=issue(out,model,0,command.selector,flags,interpolation,command.start_frame,speed,globals,sampling,active_count,limits))return *failure;
    if(active_count>1) {
        if(const auto failure=issue(out,model,1,0xffff,0,0,0,1,globals,sampling,active_count,limits))return *failure;
    }
    return out;
 }catch(const std::bad_alloc&){return error(ErrorCode::resource_limit,"Cannot allocate bounded command result");}
 catch(const std::length_error&){return error(ErrorCode::resource_limit,"Command result exceeds host capacity");}
}
}
