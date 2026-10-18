#include "player_update_post_dispatch.hpp"
#include <new>
#include <stdexcept>
#include <utility>

namespace mh4u::player_update_post_dispatch {
namespace {
namespace math=binary32_arithmetic;
Error unknown(Phase phase,std::string_view field,std::size_t record=0){return {phase,ErrorCode::unknown_field,field,{},record};}
Error capacity(){return {Phase::storage,ErrorCode::capacity,"Post-dispatch candidate storage",{}};}
bool nan(std::uint32_t bits)noexcept{return (bits&0x7fffffffU)>0x7f800000U;}
bool positive(std::uint32_t bits)noexcept{return !(bits&0x80000000U)&&(bits&0x7fffffffU)!=0&&!nan(bits);}
Result<Effects> advance(player_controller_step::State& state,std::span<motion_pending::Layer> layers,
    player_fields::Fields& fields,std::optional<Virtual10c> binding){
    if(!binding)return unknown(Phase::virtual_call,"uPlayer virtual+10c");
    if(*binding!=Virtual10c::uplayer_b25b68)
        return Error{Phase::virtual_call,ErrorCode::unsupported_virtual,"uPlayer virtual+10c",{}};
    Effects effects{};effects.virtual_returned=true;
    auto& player=state.player;
    auto base=std::uint32_t{0x3f800000};
    // The source loads PW10 before testing PW0d. That loaded numeric value
    // has no observable use when PW0d overwrites it with0; unknown PW10 may
    // therefore remain unknown on that branch of our valid-object profile.
    if(player.pbw.field371==1&&player.pw.fieldsc[1]!=1){
        if(!fields.pw.field10_bits)return unknown(Phase::rate,"PW10");
        base=*fields.pw.field10_bits;
    }
    if(player.pw.fieldsc[1]==1)base=0;
    else if(player.pw.fieldsc[0]==1){
        auto product=math::mul(base,0x3dcccccd);
        if(const auto* e=std::get_if<math::Error>(&product))return Error{Phase::rate,ErrorCode::dependency,"PW0c rate",*e};
        base=std::get<std::uint32_t>(product);
    }
    effects.base_bits=base;
    auto first=math::mul(player.pbw.field39c,base);
    if(const auto* e=std::get_if<math::Error>(&first))return Error{Phase::primary_speed,ErrorCode::dependency,"PBW39c rate",*e};
    effects.primary_bits=std::get<std::uint32_t>(first);
    if(layers.empty())return Error{Phase::primary_speed,ErrorCode::layer_extent,"Layer0",{}};
    auto primary=motion_speed::primary(layers[0].animation.layer,state.motion,effects.primary_bits);
    if(const auto* e=std::get_if<motion_speed::Error>(&primary))return Error{Phase::primary_speed,ErrorCode::dependency,"b05f84",*e};
    const auto& primary_result=std::get<motion_speed::Output>(primary);
    layers[0].animation.layer=primary_result.layer;effects.primary_rescaled=primary_result.rescaled;
    fields.pw.fieldc0_bits=effects.primary_bits;
    auto second=math::mul(player.pw.fieldc4,base);
    if(const auto* e=std::get_if<math::Error>(&second))return Error{Phase::secondary_speed,ErrorCode::dependency,"PWc4 rate",*e};
    effects.secondary_bits=std::get<std::uint32_t>(second);
    if(layers.size()<2)return Error{Phase::secondary_speed,ErrorCode::layer_extent,"Layer1",{}};
    layers[1].animation.layer=motion_speed::secondary(layers[1].animation.layer,effects.secondary_bits);
    if(player.selection_pbw.field6fa==0xff)player.selection_pbw.field6fa=0;
    else if(player.selection_pw.field594==0){
        constexpr std::array<std::array<std::uint32_t,2>,3> requests{{{0x80,0xff},{0x89,0xf8},{0x89,0xf7}}};
        for(const auto& request:requests){
            auto result=policy_has_action(player.pw.policy,request[0],request[1]);
            if(const auto* e=std::get_if<Error>(&result))return *e;
            const auto query=std::get<PolicyQuery>(result);
            effects.policy_queries[effects.query_count++]=query;
            if(query.matched){
                if(player.pbw.field371!=7&&player.pbw.field371!=12)player.selection_pbw.field6fa=1;
                break;
            }
        }
    }
    if(player.pw.fieldec&0x08000000){
        const auto mode=player.pw.field334;
        if(mode&0x30000000){
            if(!fields.pw.field339)return unknown(Phase::eligibility,"PW339");
            if(*fields.pw.field339!=1)return effects;
        }
        const std::uint32_t selector=(mode&0x10000000)?37:(mode&0x20000000)?42:0;
        effects.eligibility_selector=selector;
        auto result=eligible(player,fields,selector);
        if(const auto* e=std::get_if<Error>(&result))return *e;
        effects.eligible=std::get<bool>(result);
        if(*effects.eligible&&player.pbw.field371!=5){player.pw.fieldec&=~0x08000000U;fields.pw.field339=0;}
    }
    return effects;
}
}
Result<PolicyQuery> policy_has_action(const player_policy_table::State& policy,std::uint32_t kind,std::uint32_t action){
    namespace table=player_policy_table;
    PolicyQuery out{kind,action,false,0};
    if(std::holds_alternative<table::Unknown>(policy.table))return unknown(Phase::policy,"PW100");
    if(std::holds_alternative<table::Absent>(policy.table)||positive(policy.timer140_bits))return out;
    const auto& source=std::get<table::Table>(policy.table);
    for(std::size_t index=0;;++index){
        auto row=source.scan_record(index);
        if(const auto* e=std::get_if<table::Error>(&row))return Error{Phase::policy,ErrorCode::dependency,"Policy scan record",*e,index};
        const auto& record=std::get<table::ScanRecord>(row);
        if(record.first==0xff)return out;
        ++out.records;
        if(!record.flags06)return unknown(Phase::policy,"Policy flags06",index);
        if(*record.flags06&0x8000){
            if(!record.flags0e)return unknown(Phase::policy,"Policy flags0e",index);
            if((*record.flags0e&0x4000)&&(positive(policy.timer144_bits)||nan(policy.timer144_bits)))continue;
        }
        if(kind==record.first){
            if(!record.action)return unknown(Phase::policy,"Policy action02",index);
            if(action==*record.action){out.matched=true;return out;}
        }
    }
}
Result<bool> eligible(const player_controller_step::PlayerFields& player,const player_fields::Fields& fields,
    std::uint32_t selector){
    if(selector-37U>=11U)return false;
    const auto contains=[&](std::uint16_t id)->Result<bool>{
        auto result=player_selection::contains_id(player.selection_pbw,id);
        if(const auto* e=std::get_if<player_selection::Error>(&result))return Error{Phase::eligibility,ErrorCode::dependency,"PBW2fc",*e};
        return std::get<bool>(result);
    };
    if(selector<42){
        for(const auto id:{std::uint16_t{95},std::uint16_t{94},std::uint16_t{40}}){
            auto result=contains(id);if(const auto* e=std::get_if<Error>(&result))return *e;
            if(std::get<bool>(result))return false;
        }
        if(player_transition::type13_flag(player.pbw,player.pw,2)&&
           player_transition::type13_flag(player.pbw,player.pw,1))return false;
        if(!player.selection_pw.field800)return unknown(Phase::eligibility,"PW800");
        if(*player.selection_pw.field800&0x200)return false;
    }else{
        auto result=contains(94);if(const auto* e=std::get_if<Error>(&result))return *e;
        if(std::get<bool>(result))return false;
        if(!fields.pw.field80f)return unknown(Phase::eligibility,"PW80f");
        if(*fields.pw.field80f>=2)return false;
    }
    return (player.pw.fieldec&0x08000000)!=0;
}
Result<PostOutput> post_phase(const player_controller_step::State& state,std::span<const motion_pending::Layer> layers,
    const player_fields::Fields& fields,std::optional<Virtual10c> binding,motion_command::Limits limits){
    if(auto e=motion_command::validate_storage(layers,limits))return Error{Phase::storage,ErrorCode::capacity,e->message,*e};
    try{
        PostOutput out{state,{layers.begin(),layers.end()},fields,{}};
        auto result=advance(out.state,out.layers,out.fields,binding);
        if(const auto* e=std::get_if<Error>(&result))return *e;
        out.effects=std::get<Effects>(result);return out;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
Result<Output> apply(const player_controller_step::State& state,const player_state1::Fields& action_fields,
    std::span<const motion_pending::Layer> layers,std::uint32_t active_count,const player_fields::Fields& fields,
    const player_object_query::Manager& objects,const Context& context,motion_command::Limits limits){
    try{
        auto result=player_update_dispatch::apply(state,action_fields,layers,active_count,fields,objects,context.dispatch,limits);
        if(const auto* e=std::get_if<player_update_dispatch::Error>(&result))return Error{Phase::dispatch,
            e->code==player_update_dispatch::ErrorCode::capacity?ErrorCode::capacity:ErrorCode::dependency,e->field,*e};
        Output out{std::get<player_update_dispatch::Output>(std::move(result)),{}};
        auto& candidate=out.dispatched.prepared.prepared;
        auto post=advance(candidate.state,candidate.layers,candidate.fields,context.virtual10c);
        if(const auto* e=std::get_if<Error>(&post))return *e;
        out.effects=std::get<Effects>(post);return out;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
}
