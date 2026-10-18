#include "player_update_events.hpp"
#include <iostream>
#include <stdexcept>

namespace {
namespace ev=mh4u::player_update_events;
namespace stats=mh4u::player_update_statistics;
namespace clock=mh4u::motion_clock;
using U=std::uint32_t;
std::size_t checks{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
template<class T>T take(ev::Result result){
    if(auto* value=std::get_if<T>(&result))return std::move(*value);
    throw std::runtime_error("Unexpected events result");
}
struct Resolver {
    mh4u::motion_request::Binding expected{17,2,200,0};
    std::array<std::uint16_t,32> values{};
    std::size_t calls{};
    unsigned failure{},failure_call{};
    static clock::EventResolution resolve(const void* ptr,const mh4u::motion_request::Binding& id){
        auto& self=*static_cast<Resolver*>(const_cast<void*>(ptr));
        ++self.calls;check(id==self.expected,"Retained binding identity changed");
        if(!self.failure_call||self.calls==self.failure_call){
            if(self.failure==1)return clock::ResolutionError{91,"explicit unavailable resource"};
            if(self.failure==2)return clock::EventGroup{{18,2,200,0},{},self.values};
            if(self.failure==3)return clock::EventGroup{id,{},{}};
            if(self.failure==4)throw std::bad_alloc{};
            if(self.failure==5)throw std::length_error("test resource");
        }
        return clock::EventGroup{id,{},self.values}; // Empty records, present prefix.
    }
};
void run(){
    mh4u::player_controller_step::PlayerFields p{};
    mh4u::player_fields::Fields f{};
    mh4u::player_position_bridge::Fields position{};position.fieldd44=0;
    std::array<mh4u::motion_pending::Layer,2> layers{};
    std::array<stats::IndexedRow,256> rows{};for(auto& row:rows)row.selector1=0;
    stats::PlayerRegistry registry{{},{},{},rows};
    stats::Registry binding=&registry;
    ev::Context context{binding,{}};
    p.pw.fieldb0=0xabcd1234;p.pbw.field24=0x9876fedc;
    f.pbw.field808=0xdeadbeef;f.pw.field97c=0xa55a;f.pw.field97e=0x5aa5;
    const auto player_before=p;const auto fields_before=f;
    const auto call=[&](){return ev::apply(p,f,position,layers,context);};
    auto out=take<ev::Output>(call());
    auto expected_p=p;auto expected_f=f;expected_p.pw.fieldb0=0x1234;expected_f.pw.field97c=expected_f.pw.field97e=0;
    check(out.player==expected_p&&out.fields==expected_f,"Unexpected unconsumed field write");
    check(out.effects.lookup_count==1&&out.effects.lookups[0]==ev::Lookup{0,1,0x234,0},"Initial indexed lookup");
    check(!out.effects.predicate_calls&&!out.effects.prefix_read_count&&out.effects.bits_examined==32,"Lazy zero mask");
    // count/slots/Global5a are deliberately unknown: indexed af7378 ignores them.
    for(U id=0;id<256;++id){p.pbw.field33=std::uint8_t(id);out=take<ev::Output>(call());
        check(out.effects.lookups[0].source_offset==0x234+672*id&&out.effects.lookups[0].id==id,"Full u8 source ID extent");}
    p.pbw.field33=0;
    for(U first:{0U,0x10236U,0x10238U,0x10246U}){rows[0].selector1=first;f.pbw.field34.reset();f.pbw.field808.reset();p.selection_pbw.field6f9.reset();
        out=take<ev::Output>(call());check(!out.effects.word808_write_count&&!out.effects.predicate_calls,"Lookup result narrowed or unrelated fields consumed");}
    for(U first:{0x236U,0x246U})for(unsigned gate=0;gate<3;++gate){
        rows[0].selector1=first;f.pbw.field34=std::uint8_t(gate);rows[0].selector7=0xf1234056;p.pbw.field31=5;
        f.pbw.field808=0xaabbccfd;p.selection_pbw.field6f9.reset();out=take<ev::Output>(call());
        const bool reached=gate==(first==0x236?0U:1U);
        check(out.effects.lookup_count==(reached?2:1)&&!out.effects.predicate_calls,"Selector7 exact gates/special branch");
        check(out.fields.pbw.field808==(reached?0xaabbccfbU:0xaabbccfdU),"Special branch flags");
        if(reached)check(out.effects.word808_writes==std::array<U,2>{0xaabbccff,0xaabbccfb}&&out.effects.lookups[1].source_offset==0x24c,"Ordered stores/indexed selector7");
    }
    rows[0].selector1=0x238;f.pbw.field34.reset();rows[0].selector7.reset();
    for(unsigned value:{0U,1U,2U,255U}){
        p.selection_pbw.field6f9=std::uint8_t(value);f.pbw.field808=0x87654327;
        out=take<ev::Output>(call());const bool true_leaf=value==1;
        check(out.fields.pbw.field808==(true_leaf?0x87654325U:0x87654323U),"b07e80 must compare byte==1");
        check(out.effects.predicate_calls==1&&out.effects.lookup_count==1,"238 predicate/query count");
        check(out.effects.word808_writes[0]==(true_leaf?0x87654325U:0x87654327U),"First flag store preserved");
    }
    rows[0].selector1=0x236;f.pbw.field34=0;rows[0].selector7=0x57;p.selection_pbw.field6f9=1;p.pbw.field31=6;
    out=take<ev::Output>(call());check(out.effects.predicate_calls==1&&out.fields.pbw.field808==0x87654325U,"Non56 selector7 must reach predicate");
    rows[0].selector7=0x56;p.pbw.field31=4;out=take<ev::Output>(call());check(out.effects.predicate_calls==1,"56 with other PBW31 must reach predicate");
    rows[0].selector1=0;position.fieldd44=1;out=take<ev::Output>(call());
    check(out.effects.copied_heading&&out.player.pw.fieldb0==0xfedc,"Copy candidate heading then truncate");
    position.fieldd44=2;out=take<ev::Output>(call());check(!out.effects.copied_heading&&out.player.pw.fieldb0==0x1234,"D44 exact1 gate");
    // Null pointer with selected bits publishes zero values; third read precedes exit.
    layers[0].animation.layer.masks[0]=(1U<<0)|(1U<<4)|(1U<<31);
    out=take<ev::Output>(call());check(out.effects.prefix_read_count==3&&out.effects.prefix_store_count==2&&out.effects.bits_examined==32,"Null-prefix scan boundary");
    check(!out.effects.prefix_reads[2].binding&&out.effects.prefix_reads[2].bit==31&&out.fields.pw.field97c==0&&out.fields.pw.field97e==0,"Null prefix is selected zero");
    Resolver resolver;resolver.values[0]=0;resolver.values[4]=0xffff;resolver.values[31]=0x71;
    context.events={&resolver,Resolver::resolve};layers[0].animation.layer.bindings[0]=resolver.expected;
    out=take<ev::Output>(call());check(resolver.calls==3&&out.fields.pw.field97c==0&&out.fields.pw.field97e==0xffff,"Present prefix including zero and empty records");
    check(out.effects.prefix_reads[2]==ev::PrefixRead{31,0x71,resolver.expected},"Third active value read without third store");
    layers[0].animation.layer.masks[0]=1U<<31;resolver.calls=0;out=take<ev::Output>(call());
    check(resolver.calls==1&&out.fields.pw.field97c==0x71&&out.fields.pw.field97e==0&&out.effects.prefix_store_count==1,"Single high bit publishes only first value");
    layers[0].animation.layer.masks[0]=7;resolver.calls=0;out=take<ev::Output>(call());check(resolver.calls==3&&out.effects.bits_examined==3,"Immediate third active exit");
    layers[0].animation.layer.masks[0]=0;layers[0].animation.layer.previous_masks[0]=~0U;resolver.calls=0;resolver.failure=1;
    out=take<ev::Output>(call());check(resolver.calls==0&&!out.effects.prefix_read_count,"Previous masks/unknown resource must be ignored for current mask0");
    const auto fail=[&](ev::ErrorCode code,const char* field){auto e=take<ev::Error>(call());check(e.code==code&&e.field==field,"Typed events failure");};
    binding=stats::Unknown{};fail(ev::ErrorCode::unknown_field,"Registry");binding=stats::Absent{};fail(ev::ErrorCode::absent_registry,"Registry");
    binding=static_cast<const stats::PlayerRegistry*>(nullptr);fail(ev::ErrorCode::absent_registry,"Registry");binding=&registry;
    registry.indexed_rows={};fail(ev::ErrorCode::registry_extent,"Registry indexed row");registry.indexed_rows=rows;
    rows[0].selector1.reset();fail(ev::ErrorCode::unknown_field,"Registry selector1");rows[0].selector1=0x236;f.pbw.field34.reset();fail(ev::ErrorCode::unknown_field,"PBW34");
    f.pbw.field34=0;rows[0].selector7.reset();fail(ev::ErrorCode::unknown_field,"Registry selector7");rows[0].selector7=0x57;p.selection_pbw.field6f9.reset();fail(ev::ErrorCode::unknown_field,"PBW6f9");
    p.selection_pbw.field6f9=1;f.pbw.field808.reset();fail(ev::ErrorCode::unknown_field,"PBW808");f.pbw.field808=7;
    context.registry_work_limit=0;fail(ev::ErrorCode::capacity,"Registry lookup work");context.registry_work_limit=1;fail(ev::ErrorCode::capacity,"Registry lookup work");context.registry_work_limit=2;
    position.fieldd44.reset();fail(ev::ErrorCode::unknown_field,"PlayerD44");position.fieldd44=0;
    auto e=take<ev::Error>(ev::apply(p,f,position,{},context));check(e.code==ev::ErrorCode::layer_extent,"Late layer extent failure");
    layers[0].animation.layer.masks[0]=7;
    // Fail precisely on the third read, after two candidate flag and prefix stores.
    const auto saved_p=p;const auto saved_f=f;const auto saved_position=position;
    resolver.failure_call=3;
    for(unsigned kind=1;kind<=5;++kind){resolver.calls=0;resolver.failure=kind;
        const auto result=take<ev::Error>(call());
        const auto wanted=kind==1?ev::ErrorCode::unresolved_prefix:kind==2?ev::ErrorCode::prefix_identity:kind==3?ev::ErrorCode::unknown_prefix:ev::ErrorCode::capacity;
        check(result.code==wanted&&resolver.calls==3,"Late third-prefix failure must stay transactional");
        check(p==saved_p&&f==saved_f&&position==saved_position,"Partial events state escaped");
        if(kind==1)check(result.resolution&&result.resolution->code==91&&result.binding==resolver.expected,"Resolver cause and identity preserved");
    }
    context.events={};fail(ev::ErrorCode::unresolved_prefix,"Event resolver");
    check(player_before.pw.fieldb0==0xabcd1234&&fields_before.pw.field97c==0xa55a,"Initial snapshots independent");
}
}
int main(int argc,char**){try{check(argc==1,"usage: player-update-events-boundaries");run();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}
catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
