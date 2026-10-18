#include "player_status_update.hpp"
#include <cfenv>
#include <iostream>
#include <stdexcept>

namespace {
namespace su=mh4u::player_status_update;
namespace oq=mh4u::player_object_query;
using Player=mh4u::player_controller_step::PlayerFields;
using Fields=mh4u::player_fields::Fields;
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
Fields known(){
    Fields f;auto& p=f.pw;
    p.field17e=0;p.field274=0;p.field275=0;p.field276=0;p.field278_bits=0;
    p.field238=0;p.field240=0;p.field68_bits=0;p.field6c_bits=0;f.pbw.field368_bits=0;
    p.field224=0;p.field288_bits=0;p.field284_bits=0;p.field1a8_bits=0;p.field28c_bits=0;
    p.field4fc=0;p.field268_bits=0;p.field1c0_bits=0;p.field1d0=0;p.field1b4_bits=0;
    p.field2f0_bits=0;p.field1f8=0;p.field1a4_bits=0;p.field1a0_bits=0;
    p.field2a4_bits=0;p.field29c_bits=0;p.field29a=0;p.field2c8_bits=0;p.field2cc_bits=0;
    return f;
}
void reject(const Player& p,const Fields& f,const oq::Manager& m,std::optional<su::Word> delta,
    su::ErrorCode code,su::Phase phase){
    const auto before=f;const auto player=p;
    const auto result=su::apply(p,f,m,delta);const auto* e=std::get_if<su::Error>(&result);
    check(e&&e->code==code&&e->phase==phase,"Exact typed boundary");
    check(f==before&&p==player,"Failure publishes no candidate");
}
su::Output success(const Player& p,const Fields& f,const oq::Manager& m=oq::Unknown{},
    std::optional<su::Word> delta={}){
    const auto before=f;const auto player=p;const auto result=su::apply(p,f,m,delta);
    const auto* out=std::get_if<su::Output>(&result);
    check(out!=nullptr,"Expected success");check(f==before&&p==player,"Success preserves borrowed inputs");return *out;
}
}
int main(){try{
    Player p{};p.selection_pbw.fields2fc.fill(0);
    const auto base=known();const auto initial=success(p,base);
    check(initial.field1f4==0&&initial.fields.pw.field299==0,"Known baseline");
    check(initial.effects.query_count==4&&!initial.effects.scan,"Unselected manager and clock stay unknown");
    // Unknown operands must fail where consumed, without depending on later
    // missing operands or exposing any of the candidate's earlier writes.
    auto missing=base;missing.pw.field17e.reset();reject(p,missing,oq::Unknown{}, {},su::ErrorCode::unknown_field,su::Phase::flags);
    missing=base;missing.pw.field2cc_bits.reset();reject(p,missing,oq::Unknown{}, {},su::ErrorCode::unknown_field,su::Phase::secondary_flags);
    auto remote=p;remote.pbw.field33=1;remote.pw.field1f4=0x12345678;
    auto out=success(remote,Fields{});check(!out.effects.local&&out.field1f4==0x12345678&&out.fields==Fields{},"Remote gate consumes no local fields");
    auto lazy=base;lazy.pw.field288_bits=0x3f800000;lazy.pw.field284_bits.reset();
    lazy.pw.field1a4_bits=0x3f800000;lazy.pw.field1a0_bits.reset();
    lazy.pw.field2a4_bits=0x3f800000;lazy.pw.field29c_bits.reset();lazy.pw.field29a.reset();
    out=success(p,lazy);check(out.field1f4==0x00100000&&*out.fields.pw.field1f8==0x08000800,"Short circuits preserve unknown unused fields");
    p.pbw.field31=13;reject(p,base,oq::Unknown{}, {},su::ErrorCode::unknown_field,su::Phase::flags);p.pbw.field31=0;
    p.pbw.field371=6;p.pbw.field372=26;success(p,base); // switch arm never consumes190/198
    p.pbw.field372=0;reject(p,base,oq::Unknown{}, {},su::ErrorCode::unknown_field,su::Phase::flags);p.pbw.field371=0;
    auto ratio=base;ratio.pw.field29a=1;ratio.pw.field2a0=1;
    reject(p,ratio,oq::Unknown{}, {},su::ErrorCode::unknown_field,su::Phase::secondary_flags);
    ratio.pw.field2a2=0;out=success(p,ratio);check(*out.fields.pw.field299==1,"Zero denominator produces source numeric result");
    p.selection_pbw.fields2fc[0]=39;
    reject(p,base,oq::Unknown{}, {},su::ErrorCode::dependency,su::Phase::object_scan);
    reject(p,base,oq::Absent{}, {},su::ErrorCode::dependency,su::Phase::object_scan);
    oq::Collection collection;collection.slots.fill(oq::Absent{});
    out=success(p,base,&collection);check(out.effects.timer_cleared&&out.fields.pw.field2fc_bits==0&&out.effects.scan->slots_examined==32,"Known empty scan clears unknown timer");
    collection.slots[0]=oq::ObjectIndex{0};collection.objects[0].data=oq::DataIndex{0};
    auto& d=collection.data[0];d.field140=0;
    out=success(p,base,&collection);check(out.effects.scan->status_queries==0,"Disabled object ignores unknown status");
    d.field140=1;d.field1b4=0;
    success(p,base,&collection);d.field1b4=1;
    reject(p,base,&collection,{},su::ErrorCode::dependency,su::Phase::object_scan);
    d.field530=2;p.global.field5d=1;success(p,base,&collection); // no availability consumed
    d.field530=1;reject(p,base,&collection,{},su::ErrorCode::dependency,su::Phase::object_scan);
    p.global.field5d=0;d.field14c=0;p.selection_pbw.field9=0;
    reject(p,base,&collection,{},su::ErrorCode::unknown_field,su::Phase::timer);
    auto timer=base;timer.pw.field2fc_bits=0;reject(p,timer,&collection,{},su::ErrorCode::dependency,su::Phase::timer);
    timer.pw.field2fc_bits=0x43340000;timer.pw.field2f0_bits.reset();
    // A match stops before unknown later slots and before the unused list tail.
    collection.slots[1]=oq::Unknown{};p.selection_pbw.fields2fc[1].reset();
    out=success(p,timer,&collection);check(out.field1f4==0x40000000&&out.effects.query_count==1&&out.effects.scan->slots_examined==1,"Already reached threshold is lazy");
    p.selection_pbw.fields2fc.fill(0);p.selection_pbw.fields2fc[0]=39;
    timer=base;timer.pw.field2fc_bits=0x43330000; //179 +1 reaches actual event path.
    const auto graph=collection;reject(p,timer,&collection,0x3f800000,su::ErrorCode::unsupported_event,su::Phase::event);
    check(collection==graph,"Event failure preserves shared graph");
    timer.pw.field2fc_bits=0x3f800000;out=success(p,timer,&collection,0x3f800000);
    check(out.effects.timer_incremented&&out.fields.pw.field2fc_bits==0x40000000,"Below-threshold increment returns");
    collection.slots[0]=oq::ObjectIndex{32};
    const auto invalid=oq::scan_status(p,&collection);const auto* e=std::get_if<oq::Error>(&invalid);
    check(e&&e->code==oq::ErrorCode::invalid_index&&e->slot==0&&e->object==32,"Graph failure preserves identity");
    // The shared d05ac0 owner preserves unknown on its first source load.
    p.pbw.field371=6;p.pbw.field372=39;
    check(!mh4u::player_status_predicates::status224({},p.pbw),"Status cannot fabricate missing224");
    const auto rounding=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"Set diagnostic rounding");
    reject(p,base,oq::Unknown{}, {},su::ErrorCode::dependency,su::Phase::flags);
    out=success(remote,Fields{});check(!out.effects.local,"Unreached arithmetic ignores rounding");
    check(std::fesetround(rounding)==0,"Restore rounding");
    std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
