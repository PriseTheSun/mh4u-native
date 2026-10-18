#include "player_selection.hpp"
#include <bit>
#include <cfenv>
#include <limits>
#ifdef __FAST_MATH__
#error "player_selection requires strict binary32 semantics"
#endif
namespace mh4u::player_selection {
namespace {
Error error(ErrorCode code,std::string_view message){return {code,message,std::nullopt};}
bool float_environment(){
    return sizeof(float)==4&&std::numeric_limits<float>::is_iec559&&
        std::numeric_limits<float>::radix==2&&std::numeric_limits<float>::digits==24&&
        std::fegetround()==FE_TONEAREST;
}
Result<std::uint32_t> area_selector(const State& state,const AreaContext& area){
    const auto remapped=remap_area(state.pbw.field9,area);
    if(const auto* failed=std::get_if<Error>(&remapped))return *failed;
    const auto mapped=std::get<std::uint32_t>(remapped);
    std::uint32_t category=0;
    if(mapped<21){
        if(mapped>=area.categories.size()||!area.categories[mapped])
            return error(ErrorCode::unknown_selection_context,"Area selection consumes the mapped-ID category byte");
        category=*area.categories[mapped];
    }
    if(category<1||category>4)return 1U;
    const std::uint16_t sought=category<=2?0xda:0xdc;
    const auto membership=contains_id(state.pbw,sought);
    if(const auto* failed=std::get_if<Error>(&membership))return *failed;
    if(std::get<bool>(membership))return 1U;
    if(state.transition.pw.field1f4&(category<=2?0x200U:0x400U))return 1U;
    if(!state.pw.field800)return error(ErrorCode::unknown_selection_context,"Area selection consumes pw+800");
    if(*state.pw.field800&(category<=2?0x100U:0x80U))return 1U;
    return category<=2?0x15U:0x21fU;
}
std::uint32_t converted_bits(std::int32_t value){
    const volatile float converted=static_cast<float>(value);
    return std::bit_cast<std::uint32_t>(static_cast<float>(converted));
}
}
Result<std::uint32_t> remap_area(std::optional<std::uint8_t> source_id,const AreaContext& area){
    if(!source_id||!area.mode_c8c)
        return error(ErrorCode::unknown_selection_context,"Area remapping consumes the source ID and external mode+c8c");
    std::uint32_t mapped=*source_id;
    if((*area.mode_c8c&255U)==13U&&mapped!=0){
        if(mapped>=area.mapping.size()||!area.mapping[mapped].mapped)
            return error(ErrorCode::unknown_selection_context,"Area remapping consumes the source-ID mapped byte");
        const auto& record=area.mapping[mapped];
        if(mapped==1)mapped=*record.mapped;
        else if(mapped>=6)mapped=9; // The mapped byte above is still consumed.
        else{
            if(!record.gate)return error(ErrorCode::unknown_selection_context,"Area remapping consumes the source-ID gate byte");
            mapped=*record.gate==1?9:*record.mapped;
        }
    }
    return mapped;
}
Result<bool> contains_id(const PbwFields& source,std::uint16_t sought){
    for(const auto& field:source.fields2fc){
        if(!field)return error(ErrorCode::unknown_selection_context,"d048c4 consumes a pbw+2fc search halfword");
        if(*field==sought)return true;
    }
    return false;
}
std::string_view error_name(ErrorCode code) noexcept {
    switch(code){
    case ErrorCode::unsupported_pl_state:return "unsupported_pl_state";
    case ErrorCode::invalid_wrapper:return "invalid_wrapper";
    case ErrorCode::unsupported_float_environment:return "unsupported_float_environment";
    case ErrorCode::transition_rejected:return "transition_rejected";
    case ErrorCode::unknown_selection_context:return "unknown_selection_context";
    case ErrorCode::unsupported_selection_context:return "unsupported_selection_context";
    }
    return "unknown_error";
}
BeginResult begin_common(const State& input,const CommonArguments& args,const AreaContext& area){
    if(args.pl_state!=0&&args.pl_state!=1&&args.pl_state!=3)
        return error(ErrorCode::unsupported_pl_state,"Only full-width pl_state 0, 1 and 3 have a closed selection profile");
    if(args.pl_state!=3&&!float_environment())
        return error(ErrorCode::unsupported_float_environment,"Conversion requires IEEE binary32 round-to-nearest");
    State state=input;
    state.pw.field176=0;state.pw.field5b4=0;state.pw.field5b2=0;state.pw.field1b0=0;
    if(state.transition.pbw.field31==2)state.pw.field170=0;
    state.pbw.field370=static_cast<std::uint8_t>(args.pl_state);state.pw.fielda10=0;
    if(args.pl_state==3)return Completed{std::move(state),std::nullopt};
    auto continuation=PendingAnimation::Continuation::state1;
    std::uint32_t selector=state.pw.field9a8?24U:8U;
    if(args.pl_state==0){
        if(!state.pw.field9a8){
            if(!state.pw.field7fc)return error(ErrorCode::unknown_selection_context,"State0 consumes pw+7fc on this branch");
            if(*state.pw.field7fc==1)return error(ErrorCode::unsupported_selection_context,"State0 pw+7fc==1 requires the specialized selection path");
        }
        if(!state.pbw.field6f9)return error(ErrorCode::unknown_selection_context,"State0 consumes pbw+6f9 through b07e80");
        if(*state.pbw.field6f9==1)return error(ErrorCode::unsupported_selection_context,"State0 pbw+6f9==1 requires the specialized selection path");
        continuation=PendingAnimation::Continuation::state0_zero;
        if(!state.pw.field9a8){
            if(state.pw.field594){selector=0x246;continuation=PendingAnimation::Continuation::state0_ten;}
            else{
                if(!state.pbw.field358)return error(ErrorCode::unknown_selection_context,"State0 consumes signed integer pbw+358 on this branch");
                if(std::bit_cast<std::int32_t>(*state.pbw.field358)>0x43160000){
                    auto selected=area_selector(state,area);
                    if(const auto* failed=std::get_if<Error>(&selected))return *failed;
                    selector=std::get<std::uint32_t>(selected);
                }else selector=0x203;
            }
        }
    }
    const AnimationRequest request{selector,args.request_mode,
        converted_bits(args.interpolation),converted_bits(args.frame)};
    return PendingAnimation{std::move(state),request,args.flags,continuation};
}
BeginResult begin_wrapper(const State& state,Wrapper wrapper,const WrapperArguments& args,const AreaContext& area){
    std::uint32_t mode;
    switch(wrapper){
    case Wrapper::b1d114:mode=0;break;
    case Wrapper::b23b74:mode=2;break;
    default:return error(ErrorCode::invalid_wrapper,"Unknown source wrapper");
    }
    return begin_common(state,{args.pl_state,args.interpolation,args.frame,0,mode,args.extra_bits},area);
}
Result<Completed> resume(const PendingAnimation& pending,const State& post_request,const pt::Constants& constants){
    const pt::Request args=pending.continuation_==PendingAnimation::Continuation::state0_zero?pt::Request{0,0,pending.flags_}:
        pending.continuation_==PendingAnimation::Continuation::state0_ten?pt::Request{10,0,pending.flags_}:
        pending.request_.selector==24?pt::Request{0,0,pending.flags_}:
        post_request.pw.field594?pt::Request{10,14,pending.flags_}:pt::Request{0,24,pending.flags_};
    return direct_transition(post_request,args,constants);
}
Result<Completed> direct_transition(const State& current,const pt::Request& args,const pt::Constants& constants){
    State state=current;
    state.pbw.field376=1;
    auto result=pt::apply(state.transition,args,constants);
    if(const auto* failed=std::get_if<pt::Error>(&result))
        return Error{ErrorCode::transition_rejected,failed->message,failed->code};
    auto output=std::get<pt::Output>(std::move(result));
    state.transition=std::move(output.state);state.pbw.field6fa=0;
    return Completed{std::move(state),TransitionEffects{args,output.effective_flags,output.returned_dispatch_call}};
}
} // namespace mh4u::player_selection
