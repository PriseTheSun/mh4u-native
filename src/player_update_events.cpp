#include "player_update_events.hpp"
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace mh4u::player_update_events {
static_assert(std::is_nothrow_move_constructible_v<Error>);
namespace {
namespace stats=player_update_statistics;
struct Operation {
    Output out;
    const Context& context;
    Phase phase=Phase::lookup;
    [[noreturn]] void fail(ErrorCode code,std::string_view field)const {
        throw Error{phase,code,field};
    }
    template<class T>T need(const std::optional<T>& value,std::string_view field)const {
        if(!value)fail(ErrorCode::unknown_field,field);
        return *value;
    }
    Word lookup(Word selector) {
        phase=Phase::lookup;
        if(out.effects.lookup_count>=context.registry_work_limit)
            fail(ErrorCode::capacity,"Registry lookup work");
        if(std::holds_alternative<stats::Unknown>(context.players))
            fail(ErrorCode::unknown_field,"Registry");
        if(std::holds_alternative<stats::Absent>(context.players))
            fail(ErrorCode::absent_registry,"Registry");
        const auto* registry=std::get<const stats::PlayerRegistry*>(context.players);
        if(!registry)fail(ErrorCode::absent_registry,"Registry");
        const auto id=out.player.pbw.field33;
        if(id>=registry->indexed_rows.size())fail(ErrorCode::registry_extent,"Registry indexed row");
        const auto& row=registry->indexed_rows[id];
        const auto result=need(selector==1?row.selector1:row.selector7,
            selector==1?"Registry selector1":"Registry selector7");
        out.effects.lookups[out.effects.lookup_count++]={id,selector,
            0x230U+672U*id+4U*selector,result};
        return result;
    }
    bool predicate() {
        phase=Phase::flags;
        ++out.effects.predicate_calls;
        return player_selection::flag6f9(need(out.player.selection_pbw.field6f9,"PBW6f9"));
    }
    void flags(bool value) {
        phase=Phase::flags;
        const auto before=need(out.fields.pbw.field808,"PBW808");
        auto first=value?(before&~2U):(before|2U);
        out.fields.pbw.field808=first;
        out.effects.word808_writes[out.effects.word808_write_count++]=first;
        // The false path reloads the canonical PBW word after its first store.
        auto second=value?(first|4U):(*out.fields.pbw.field808&~4U);
        out.fields.pbw.field808=second;
        out.effects.word808_writes[out.effects.word808_write_count++]=second;
    }
    std::uint16_t prefix(const std::optional<motion_request::Binding>& binding,
                         std::uint8_t bit) {
        if(!binding)return 0;
        if(!context.events.resolve)
            throw Error{phase,ErrorCode::unresolved_prefix,"Event resolver",binding};
        const auto resolved=context.events.resolve(context.events.context,*binding);
        if(const auto* error=std::get_if<motion_clock::ResolutionError>(&resolved))
            throw Error{phase,ErrorCode::unresolved_prefix,"Event binding",binding,*error};
        const auto& group=std::get<motion_clock::EventGroup>(resolved);
        if(group.identity!=*binding)
            throw Error{phase,ErrorCode::prefix_identity,"Event resolved identity",binding};
        if(!group.prefix)
            throw Error{phase,ErrorCode::unknown_prefix,"Event prefix",binding};
        return (*group.prefix)[bit];
    }
};
}
Result apply(const player_controller_step::PlayerFields& player,
    const player_fields::Fields& fields,const player_position_bridge::Fields& position,
    std::span<const motion_pending::Layer> layers,const Context& context) {
    try {
        Operation op{{player,fields,{}},context};
        const auto first=op.lookup(1);
        bool second=false;
        if(first==0x236U)second=op.need(fields.pbw.field34,"PBW34")==0;
        else if(first==0x246U)second=op.need(fields.pbw.field34,"PBW34")==1;
        if(second) {
            const auto value=op.lookup(7)&0xfffU;
            const bool special=value==0x56U&&(player.pbw.field31==6||player.pbw.field31==5);
            op.flags(special?false:op.predicate());
        }
        if(first==0x238U)op.flags(op.predicate());
        op.phase=Phase::heading;
        if(op.need(position.fieldd44,"PlayerD44")==1) {
            op.out.player.pw.fieldb0=op.out.player.pbw.field24;
            op.out.effects.copied_heading=true;
        }
        op.out.player.pw.fieldb0&=0xffffU;
        op.phase=Phase::prefix;
        if(layers.empty())op.fail(ErrorCode::layer_extent,"Layer0 current mask");
        const auto mask=layers[0].animation.layer.masks[0];
        op.out.fields.pw.field97c=0;
        op.out.fields.pw.field97e=0;
        for(std::uint8_t bit=0;bit<32;++bit) {
            ++op.out.effects.bits_examined;
            if(!(mask&(Word{1}<<bit)))continue;
            const auto& binding=layers[0].animation.layer.bindings[0];
            const auto value=op.prefix(binding,bit);
            op.out.effects.prefix_reads[op.out.effects.prefix_read_count++]={bit,value,binding};
            if(op.out.effects.prefix_store_count==2)break;
            if(op.out.effects.prefix_store_count++==0)op.out.fields.pw.field97c=value;
            else op.out.fields.pw.field97e=value;
        }
        return std::move(op.out);
    }catch(Error& error){return std::move(error);}
    catch(const std::bad_alloc&){return Error{Phase::prefix,ErrorCode::capacity,"Event resource allocation"};}
    catch(const std::length_error&){return Error{Phase::prefix,ErrorCode::capacity,"Event resource extent"};}
}
}
