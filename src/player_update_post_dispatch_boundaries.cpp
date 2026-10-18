#include "player_update_post_dispatch.hpp"
#include "picosha2.h"
#include <bit>
#include <cfenv>
#include <iostream>
#include <stdexcept>

namespace {
namespace post=mh4u::player_update_post_dispatch;
namespace table=mh4u::player_policy_table;
using Word=std::uint32_t;
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
template<class T,class V>T value(V result){
    if(const auto* out=std::get_if<T>(&result))return *out;
    throw std::runtime_error("Unexpected result variant");
}
table::Table decode(const std::vector<std::uint8_t>& bytes){
    return value<table::Table>(table::Table::decode(bytes,picosha2::hash256_hex_string(bytes),{bytes.size()}));
}
void put(std::vector<std::uint8_t>& bytes,std::size_t at,std::uint16_t word){
    bytes.at(at)=static_cast<std::uint8_t>(word);bytes.at(at+1)=static_cast<std::uint8_t>(word>>8);
}
void query_boundaries(){
    table::State state;
    state.timer140_bits=0x3f800000;
    auto error=value<post::Error>(post::policy_has_action(state,0x80,0xff));
    check(error.field=="PW100","Unknown table cannot become a known nonnull pointer");
    state.table=table::Absent{};
    check(!value<post::PolicyQuery>(post::policy_has_action(state,0x80,0xff)).matched,"Null table before timer");
    std::vector<std::uint8_t> bytes(50*22+1);
    for(std::size_t i=0;i<50;++i){bytes[i*22]=0x80;put(bytes,i*22+2,static_cast<std::uint16_t>(i));}
    bytes.back()=0xff;state.table=decode(bytes);state.timer140_bits=0;
    const auto& owner=std::get<table::Table>(state.table);
    check(owner.records().size()==48&&owner.coverage().termination==table::Termination::cap,"Interpreter cap changed");
    auto last=value<post::PolicyQuery>(post::policy_has_action(state,0x80,49));
    check(last.matched&&last.records==50,"Uncapped scanner lost record50");
    auto absent=value<post::PolicyQuery>(post::policy_has_action(state,0x80,99));
    check(!absent.matched&&absent.records==50,"Scanner must reach sentinel");
    check(!value<post::PolicyQuery>(post::policy_has_action(state,0x10080,49)).matched,"Kind argument narrowed");
    check(!value<post::PolicyQuery>(post::policy_has_action(state,0x80,0x10031)).matched,"Action argument narrowed");
    check(std::holds_alternative<table::Error>(table::Table::decode(bytes,picosha2::hash256_hex_string(bytes))),"Default carrier budget changed");
    auto tail=value<table::ScanRecord>(owner.scan_record(50));
    check(tail.first==0xff&&!tail.flags06&&!tail.flags0e&&!tail.action,"Sentinel fields read");
    check(std::holds_alternative<table::Error>(owner.scan_record(SIZE_MAX)),"Scan offset overflow");
    for(Word timer:{0x3f800000U,0x7f800000U}){
        state.timer140_bits=timer;
        auto out=value<post::PolicyQuery>(post::policy_has_action(state,0x80,49));
        check(!out.matched&&out.records==0,"Positive timer must suppress scan");
    }
    for(Word timer:{0U,0x80000000U,0xbf800000U,0xff800000U,0x7fc01234U,0xff801234U}){
        state.timer140_bits=timer;
        check(value<post::PolicyQuery>(post::policy_has_action(state,0x80,49)).matched,"Unordered/negative timer wrongly suppresses scan");
    }
    state.timer140_bits=0;
    // Only the49th record is partial: the interpreter's48 records remain
    // valid, while the query may consume individual fields of this next row.
    bytes.resize(48*22+8);bytes[48*22]=0x89;put(bytes,48*22+2,0xf8);put(bytes,48*22+6,0);
    state.table=decode(bytes);
    auto partial=value<post::PolicyQuery>(post::policy_has_action(state,0x89,0xf8));
    check(partial.matched&&partial.records==49,"Partial row hit read unneeded fields");
    error=value<post::Error>(post::policy_has_action(state,0x89,0xf7));
    check(error.record==49&&std::holds_alternative<table::Error>(error.cause),"Missing terminator treated as false");
    put(bytes,48*22+6,0x8000);state.table=decode(bytes);
    error=value<post::Error>(post::policy_has_action(state,0x99,0xf8));
    check(error.record==48&&error.field=="Policy flags0e","Conditional flag read reordered after kind mismatch");
    bytes.resize(48*22+16);put(bytes,48*22+14,0x4000);state.table=decode(bytes);
    for(Word timer:{0x3f800000U,0x7f800000U,0x7fc01234U,0xff801234U}){
        state.timer144_bits=timer;error=value<post::Error>(post::policy_has_action(state,0x89,0xf8));
        check(error.record==49,"Positive/unordered secondary timer must skip candidate");
    }
    for(Word timer:{0U,0x80000000U,0xbf800000U,0xff800000U}){
        state.timer144_bits=timer;check(value<post::PolicyQuery>(post::policy_has_action(state,0x89,0xf8)).matched,"Ordered nonpositive secondary timer skip");
    }
    bytes.resize(48*22+1);bytes.back()=0x89;state.table=decode(bytes);
    error=value<post::Error>(post::policy_has_action(state,0x99,0xf8));
    check(error.field=="Policy flags06"&&error.record==48,"Missing flags06 not reported lazily");
    bytes.back()=0xff;state.table=decode(bytes);
    check(!value<post::PolicyQuery>(post::policy_has_action(state,0x89,0xf8)).matched,"One-byte sentinel requires full row");
}
void eligibility_boundaries(){
    mh4u::player_controller_step::PlayerFields player;
    mh4u::player_fields::Fields fields;
    player.pbw.field31=13;player.pw.field1f4=0xffffffff;
    for(Word selector:{3U,31U,32U,0xffffffffU})
        check(!mh4u::player_transition::type13_flag(player.pbw,player.pw,selector),"Shared type13 predicate shifted an invalid selector");
    player.pbw.field31=0;player.pw.field1f4=0;
    for(Word selector:{0U,36U,48U,0xffffffffU,0x10025U})
        check(!value<bool>(post::eligible(player,fields,selector)),"Out-of-range selector consumed fields");
    auto error=value<post::Error>(post::eligible(player,fields,37));
    check(std::holds_alternative<mh4u::player_selection::Error>(error.cause),"Unknown ID list treated as absent");
    player.selection_pbw.fields2fc[0]=95;
    check(!value<bool>(post::eligible(player,fields,37)),"First ID hit failed to short-circuit unknown tail");
    player.selection_pbw.fields2fc[0]=94;
    check(!value<bool>(post::eligible(player,fields,42)),"Selector42 first ID short circuit");
    for(auto& id:player.selection_pbw.fields2fc)id=0;
    player.pw.fieldec=0x08000000;player.pbw.field31=13;player.pw.field1f4=0x01800000;
    check(!value<bool>(post::eligible(player,fields,37)),"Type13 dual mask must precede unknown PW800");
    player.pw.field1f4=0x01000000;
    error=value<post::Error>(post::eligible(player,fields,37));check(error.field=="PW800","Single type13 mask rejected early");
    player.selection_pw.field800=0x200;
    check(!value<bool>(post::eligible(player,fields,41)),"PW800 flag not applied");
    player.selection_pw.field800=0;
    check(value<bool>(post::eligible(player,fields,37)),"Selector37 eligibility");
    error=value<post::Error>(post::eligible(player,fields,42));check(error.field=="PW80f","Selector42 required byte");
    for(unsigned i=0;i<256;++i){fields.pw.field80f=static_cast<std::uint8_t>(i);check(value<bool>(post::eligible(player,fields,47))==(i<2),"Selector42 unsigned byte range");}
    fields.pw.field80f=0;player.pw.fieldec=0;
    check(!value<bool>(post::eligible(player,fields,37))&&!value<bool>(post::eligible(player,fields,42)),"Eligibility final bit");
}
void phase_boundaries(){
    mh4u::player_controller_step::State state;
    std::array<mh4u::motion_pending::Layer,2> layers{};
    mh4u::player_fields::Fields fields;
    state.player.pbw.field371=3;state.player.pbw.field39c=0x3f000000;
    state.player.pw.fieldc4=0x40000000;state.player.selection_pw.field594=1;
    constexpr auto binding=post::Virtual10c::uplayer_b25b68;
    auto out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));
    check(out.effects.virtual_returned&&out.effects.base_bits==0x3f800000&&out.fields.pw.fieldc0_bits==0x3f000000,"State3 base speed");
    check(std::bit_cast<Word>(out.layers[0].animation.layer.speed)==0x3f000000&&std::bit_cast<Word>(out.layers[1].animation.layer.speed)==0x40000000,"Layer speed stores");
    auto error=value<post::Error>(post::post_phase(state,layers,fields,{}));check(error.phase==post::Phase::virtual_call,"Missing virtual accepted");
    error=value<post::Error>(post::post_phase(state,layers,fields,static_cast<post::Virtual10c>(7)));check(error.code==post::ErrorCode::unsupported_virtual,"Wrong concrete virtual accepted");
    state.player.pbw.field371=1;
    error=value<post::Error>(post::post_phase(state,layers,fields,binding));check(error.field=="PW10","Consumed base unknown");
    state.player.pw.fieldsc[1]=1;
    out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));
    check(out.effects.base_bits==0&&!out.fields.pw.field10_bits,"Overwritten numeric load required knowledge");
    state.player.pw.fieldsc={1,0};fields.pw.field10_bits=0x40000000;
    out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));
    check(out.effects.base_bits==0x3e4ccccd&&out.effects.primary_bits==0x3dcccccd&&out.effects.secondary_bits==0x3ecccccd,"Rate operand order/constants");
    state.player.pw.fieldsc={0,0};
    state.player.selection_pw.field594=0;state.player.selection_pbw.field6fa=0xff;
    out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));
    check(out.state.player.selection_pbw.field6fa==0&&out.effects.query_count==0,"FF gate did not skip unknown table");
    state.player.selection_pbw.field6fa=4;
    auto before=state;const auto original_fields=fields;
    error=value<post::Error>(post::post_phase(state,layers,fields,binding));
    check(error.field=="PW100"&&state.player==before.player&&state.motion==before.motion&&fields==original_fields&&layers[0].animation.layer.speed==0,"Late failure published rate changes");
    std::vector<std::uint8_t> bytes(23);bytes[0]=0x80;put(bytes,2,0xff);bytes.back()=0xff;state.player.pw.policy.table=decode(bytes);
    for(unsigned action_state:{1U,3U,5U,7U,12U}){
        state.player.pbw.field371=static_cast<std::uint8_t>(action_state);
        out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));
        check(out.effects.query_count==1&&out.state.player.selection_pbw.field6fa==((action_state==7||action_state==12)?4:1),"Flag store state exclusions");
    }
    state.player.pw.fieldec=0x08000000;state.player.pw.field334=0;
    out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));
    check(out.effects.eligibility_selector==0&&out.effects.eligible==false&&!out.fields.pw.field339,"Zero mode wrongly read PW339");
    state.player.pw.field334=0x10000000;
    error=value<post::Error>(post::post_phase(state,layers,fields,binding));check(error.field=="PW339","PW339 gate missing");
    fields.pw.field339=2;
    out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));check(!out.effects.eligible,"PW339 skip consumed IDs");
    fields.pw.field339=1;for(auto& id:state.player.selection_pbw.fields2fc)id=0;state.player.selection_pw.field800=0;
    state.player.pbw.field371=5;
    out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));
    check(out.effects.eligible==true&&out.state.player.pw.fieldec==0x08000000&&out.fields.pw.field339==1,"State5 eligibility preserves flags");
    state.player.pbw.field371=3;
    out=value<post::PostOutput>(post::post_phase(state,layers,fields,binding));
    check(out.state.player.pw.fieldec==0&&out.fields.pw.field339==0,"Eligibility stores");
    mh4u::motion_command::Limits limits;limits.storage_layers=1;
    error=value<post::Error>(post::post_phase(state,layers,fields,binding,limits));check(error.code==post::ErrorCode::capacity,"Storage limit");
    error=value<post::Error>(post::post_phase(state,{},fields,binding));check(error.phase==post::Phase::primary_speed,"Missing layer0");
    error=value<post::Error>(post::post_phase(state,std::span(layers).first(1),fields,binding));check(error.phase==post::Phase::secondary_speed,"Missing layer1");
    const int rounding=std::fegetround();check(std::fesetround(FE_UPWARD)==0,"Set rounding");
    auto unsupported=post::post_phase(state,layers,fields,binding);
    check(std::fesetround(rounding)==0,"Restore rounding");
    error=value<post::Error>(unsupported);check(error.phase==post::Phase::primary_speed&&std::holds_alternative<mh4u::binary32_arithmetic::Error>(error.cause),"Numeric environment cause");
}
}
int main(int argc,char**){try{
    check(argc==1,"No arguments expected");query_boundaries();eligibility_boundaries();phase_boundaries();
    std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
