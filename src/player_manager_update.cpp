#include "player_manager_update.hpp"
#include <bit>
#include <utility>

namespace mh4u::player_manager_update {
namespace {
namespace fp=binary32_arithmetic;
namespace pt=player_transition;
struct Failure {Error error;};
bool nan(Word v){return (v&0x7fffffffU)>0x7f800000U;}
bool greater(Word a,Word b){
    if(nan(a)||nan(b))return false;
    if((a&0x7fffffffU)==0&&(b&0x7fffffffU)==0)return false;
    if((a^b)&0x80000000U)return (a>>31)==0;
    return a>>31?a<b:a>b;
}
struct Runner {
    const pc::PlayerFields& original;
    const pf::Fields& original_fields;
    const Context& context;
    Output out;
    Phase phase=Phase::collection;
    Runner(const pc::PlayerFields& p,const pf::Fields& f,const Context& c):original(p),original_fields(f),context(c),out{p,f,{},{},{}}{}
    [[noreturn]] void fail(ErrorCode code,std::string_view field){throw Failure{{phase,code,field,{}}};}
    template<class T>T need(const std::optional<T>& v,std::string_view field){if(!v)fail(ErrorCode::unknown_field,field);return *v;}
    void call(CallKind k,Word arg=0,bool foreign=false){
        if(out.effects.call_count==out.effects.calls.size())fail(ErrorCode::capacity,"observed calls");
        out.effects.calls[out.effects.call_count++]={k,arg,foreign};
    }
    Word math(fp::Result r,std::string_view field){
        if(auto* e=std::get_if<fp::Error>(&r))throw Failure{{phase,ErrorCode::arithmetic,field,*e}};
        return std::get<Word>(r);
    }
    Word angle(Word x,Word z){
        call(CallKind::angle);
        auto result=pr::angular_core(x,z);
        if(auto* e=std::get_if<pr::Error>(&result))throw Failure{{phase,ErrorCode::reference,"568b54",*e}};
        return std::get<Word>(result);
    }
    ManagerState& manager(){
        if(out.manager)return *out.manager;
        if(std::holds_alternative<Unknown>(context.manager))fail(ErrorCode::unknown_field,"manager1057294");
        if(std::holds_alternative<Absent>(context.manager))fail(ErrorCode::absent_manager,"manager1057294");
        const auto* ptr=std::get<ManagerView>(context.manager).state;
        if(!ptr)fail(ErrorCode::invalid_identity,"manager state");
        out.manager=*ptr;return *out.manager;
    }
    bool local(const pc::PlayerFields& p,bool foreign=false){call(CallKind::local_control,0,foreign);return pt::local_control(out.player.global,p.pbw);}
    bool group(const pc::PlayerFields& p,Word selector,bool foreign=false){call(CallKind::type_group,selector,foreign);return pt::type_group(p.pbw,selector);}
    bool action97(){call(CallKind::action97);return out.player.pbw.field371==0&&(out.player.pbw.field372==97||out.player.pbw.field372==98);}
    bool action145(){call(CallKind::action145,1);return out.player.pbw.field371==0&&out.player.pbw.field372>=145&&out.player.pbw.field372<=147;}
    bool flag(){call(CallKind::flag6f9);return player_selection::flag6f9(need(out.player.selection_pbw.field6f9,"PBW6f9"));}
    Word descriptor(const pf::Fields& f,bool foreign){
        call(CallKind::descriptor,64,foreign);
        const auto kind=need(f.pbw.descriptors[0][0],"PBW1c0.byte0");
        if(kind!=11&&kind!=12)return 0xffffffffU;
        return (need(f.pbw.descriptors[0][1],"PBW1c0.byte1")&64U)?1U:0U;
    }
    Word mode(){if(action97())return 1;if(action145())return 2;(void)group(out.player,0);return 0;}
    void collection(){
        phase=Phase::collection;call(CallKind::selector);
        if(!context.virtual84)fail(ErrorCode::unknown_field,"virtual84");
        if(*context.virtual84!=player_direction_refresh::Virtual84::uplayer_b085bc)fail(ErrorCode::unsupported_binding,"virtual84");
        if(context.layers.empty())fail(ErrorCode::unknown_field,"layer0.selector (Player244)");
        const auto selector=context.layers[0].animation.layer.selector;
        Word relative=0,best=0x4a095440;bool found=false;
        if(selector==38){
            auto rr=oq::resolve_manager(context.objects);
            if(auto* e=std::get_if<oq::Error>(&rr))throw Failure{{phase,ErrorCode::collection,"f32004",*e}};
            const auto& objects=*std::get<const oq::Collection*>(rr);
            for(std::size_t i=0;i<32;++i){
                call(CallKind::slot,Word(i));++out.effects.slots_examined;
                auto slot=oq::resolve_slot(objects,i);
                if(auto* e=std::get_if<oq::Error>(&slot))throw Failure{{phase,ErrorCode::collection,"slot",*e}};
                const auto& item=std::get<std::optional<oq::ResolvedObject>>(slot);
                if(!item)continue;
                call(CallKind::enabled);++out.effects.enabled_queries;
                const auto& data=*item->data.fields;
                if(need(data.field140,"D140")==0)continue;
                const std::array<Word,3> point{need(data.position_bits[0],"D198"),need(data.position_bits[1],"D19c"),need(data.position_bits[2],"D1a0")};
                const std::array<Word,3> origin{need(out.fields.pbw.field10_bits,"PBW10"),need(out.fields.pbw.field14_bits,"PBW14"),need(out.fields.pbw.field18_bits,"PBW18")};
                const Word x=math(fp::sub(point[0],origin[0]),"distance.x");
                const Word y=math(fp::sub(point[1],origin[1]),"distance.y");
                const Word z=math(fp::sub(point[2],origin[2]),"distance.z");
                Word distance=math(fp::mul(x,x),"distance.x2");
                distance=math(fp::add(distance,math(fp::mul(y,y),"distance.y2")),"distance.xy");
                distance=math(fp::add(distance,math(fp::mul(z,z),"distance.z2")),"distance.xyz");
                if(nan(distance)||nan(best)||greater(distance,best))continue;
                best=distance;found=true;++out.effects.accepted_objects;out.effects.selected_object=item->index;
                const Word rx=math(fp::sub(point[0],origin[0]),"heading.x");
                const Word rz=math(fp::sub(point[2],origin[2]),"heading.z");
                relative=(angle(rx,rz)-out.player.pbw.field24+65536U)&0xffffU;
            }
        }
        const Word old=need(out.fields.pw.fieldb4,"PWb4");
        if(!found){
            if(old==0)return;
            const Word sum=old+1024U;
            out.fields.pw.fieldb4=std::uint16_t(sum<=2048?0:old<32768?old-1024:sum);
        }else if(relative>=32768){
            if(relative<0xd556)relative=0xd556;
            const Word candidate=old-2048U;
            out.fields.pw.fieldb4=std::uint16_t(old<32768||std::bit_cast<std::int32_t>(candidate)>std::int32_t(relative)?candidate:relative);
        }else{
            if(relative>0x2aab)relative=0x2aab;
            const Word candidate=old+2048U;
            out.fields.pw.fieldb4=std::uint16_t(old>=32768||candidate<relative?candidate:relative);
        }
    }
    struct ResolvedPlayer {const pc::PlayerFields* player;const pf::Fields* fields;bool foreign;ForeignPlayer identity;};
    ResolvedPlayer target(){
        const auto& link=std::get<ManagerView>(context.manager).player;
        if(std::holds_alternative<Unknown>(link))fail(ErrorCode::unknown_field,"manager.e18");
        if(std::holds_alternative<Absent>(link))fail(ErrorCode::absent_player,"manager.e18");
        if(std::holds_alternative<Caller>(link))return {&out.player,&out.fields,false,{}};
        const auto view=std::get<ForeignPlayer>(link);
        if(!view.player||!view.fields)fail(ErrorCode::invalid_identity,"manager.e18 foreign");
        const bool p_alias=view.player==&original,f_alias=view.fields==&original_fields;
        if(p_alias!=f_alias)fail(ErrorCode::invalid_identity,"partial Player alias");
        if(p_alias)return {&out.player,&out.fields,false,{}};
        return {view.player,view.fields,true,view};
    }
    Word vertical_heading(bool foreign){
        call(CallKind::vertical_heading,0,foreign);
        if(std::holds_alternative<pr::Unknown>(context.reference))fail(ErrorCode::unknown_field,"manager.1c");
        if(std::holds_alternative<pr::Absent>(context.reference))return 0;
        const auto& p=std::get<pr::Points>(context.reference);
        const Word dx=math(fp::sub(std::bit_cast<Word>(p.second_x),std::bit_cast<Word>(p.first_x)),"vertical.dx");
        const Word dz=math(fp::sub(std::bit_cast<Word>(p.second_z),std::bit_cast<Word>(p.first_z)),"vertical.dz");
        const Word first_y=need(p.first_y_bits,"reference44");
        const Word second_y=need(p.second_y_bits,"reference64");
        const Word dy=math(fp::sub(second_y,first_y),"vertical.dy");
        const Word xx=math(fp::mul(dx,dx),"vertical.x2");
        const Word norm=math(fp::add(xx,math(fp::mul(dz,dz),"vertical.z2")),"vertical.xz");
        const Word root=math(fp::sqrt(norm),"vertical.sqrt");
        return angle(dy^0x80000000U,root);
    }
    void vertical(const ResolvedPlayer& p){
        call(CallKind::vertical_update,0,p.foreign);
        if(!local(*p.player,p.foreign))return;
        phase=Phase::vertical;
        const Word heading=vertical_heading(p.foreign);
        call(CallKind::vertical_value,heading,p.foreign);
        Word value=math(fp::from_unsigned_word(heading),"b3f868.convert");
        value=math(fp::mul(value,0x43b40000),"b3f868.360");
        value=math(fp::mul(value,0x37800000),"b3f868.2^-16");
        if(std::bit_cast<std::int32_t>(value)>std::int32_t(0x43340000))value=math(fp::sub(value,0x43b40000),"b3f868.subtract360");
        value^=0x80000000U;const bool positive=greater(value,0);
        if(!group(*p.player,positive?0U:1U,p.foreign))(void)group(*p.player,2,p.foreign);
        Word ratio=math(fp::div(positive?value:value^0x80000000U,0x42700000),"b3f868.divide60");
        if(std::bit_cast<std::int32_t>(ratio)>std::int32_t(0x3f800000))ratio=0x3f800000;
        const Word result=math(fp::mul(ratio,positive?0x42c80000:0xc2c80000),"b3f868.factor");
        if(p.foreign){out.foreign=ForeignPatch{p.identity,result,result};out.effects.foreign_vertical_written=true;}
        else {out.player.pw.fielddc=result;out.fields.pw.fielde0_bits=out.player.pw.fielddc;out.effects.caller_vertical_written=true;}
        phase=Phase::manager;
    }
    void start(Word mode_value){
        call(CallKind::manager_start,mode_value);
        if(mode_value>=3)return;
        auto& m=manager();if(need(m.fields1050[4],"manager1054")!=0)return;
        m.field102c=0x42480000;m.fields1050[1]=0;m.fields1050[4]=1;m.field104c=0;
        m.fields1050[2]=0;m.fields1050[3]=0;m.fields1050[5]=0;m.fields1050[0]=std::uint8_t(mode_value);
        auto selected=[&]{m.fields1050[1]=1;m.field1040=0x41700000;m.field1044=0x42700000;m.field1048=0x3cb60b61;};
        if(mode_value==1){selected();return;}if(mode_value==2)return;
        const auto p=target();call(CallKind::descriptor_predicate,0,p.foreign);
        if(group(*p.player,0,p.foreign)&&descriptor(*p.fields,p.foreign)==1)selected();
        call(CallKind::global_gate,0,p.foreign);
        if(need(out.player.global.field7a,"Global7a")==0)vertical(p);
    }
    void toggle(Word second){
        call(CallKind::toggle,second);
        out.player.pw.field50d^=1U;
        if(out.player.pw.field50d==0)return;
        start(mode());if(second!=0)return;
        if(group(out.player,0)&&descriptor(out.fields,false)==1)return;
        bool use_e6=action97();
        if(!use_e6&&!action145())(void)group(out.player,0);
        if(use_e6)out.fields.pw.fielde6=50;else out.fields.pw.fielde4=50;
    }
    bool excluded(){
        const auto state=out.player.pbw.field371;const auto action=out.player.pbw.field372;
        if(state==11)return true;
        if(state==0){switch(action){case 6:case 13:case 14:case 27:case 28:case 55:case 63:case 137:case 138:case 139:return true;default:return false;}}
        if(state==4){switch(action){case 0:case 26:case 27:case 32:case 34:case 35:case 36:case 37:case 38:case 39:case 40:case 41:case 44:return true;default:return false;}}
        return false;
    }
    Word input(std::size_t i,std::string_view field){return need(player_input_storage::join(out.fields.pbw.words3d8[i]),field);}
    void fallback(){
        if(action97()){out.player.pw.field50d=1;start(mode());}
        else {out.player.pw.field50d=0;out.fields.pw.field50c=0;out.fields.pw.field50e=0;}
    }
    void management(){
        phase=Phase::manager;
        if(!local(out.player))return;
        const auto state=out.player.pbw.field371;if(state==6||state==7||state==8)return;
        call(CallKind::manager_gate);auto& m=manager();
        const bool allowed=need(m.field112b,"manager112b")==0&&need(m.field1203,"manager1203")==0&&
            need(m.field15ff,"manager15ff")==0&&need(m.field15a4,"manager15a4")==0&&need(m.field1645,"manager1645")==0;
        if(!allowed){out.player.pw.field50d=0;out.fields.pw.field50c=0;return;}
        if(flag()&&group(out.player,0)&&(input(3,"PBW3e4")&128U)){
            if(!excluded())toggle(0);
            if(out.player.selection_pbw.field370==2)out.player.pw.field50d=0;
        }else if(flag()){
            if(!group(out.player,0))fallback();
        }else if(action145()){
            if(input(0,"PBW3d8")&512U)toggle(1);
        }else fallback();
        if(out.player.pw.field50d==0)out.fields.pw.field50c=0;
    }
};
Result run(const pc::PlayerFields& p,const pf::Fields& f,const Context& c,bool collection,bool manager){
    try {Runner r(p,f,c);if(collection)r.collection();if(manager)r.management();return std::move(r.out);}
    catch(const Failure& e){return e.error;}
}
}
Result collection_phase(const pc::PlayerFields& p,const pf::Fields& f,const Context& c){return run(p,f,c,true,false);}
Result manager_phase(const pc::PlayerFields& p,const pf::Fields& f,const Context& c){return run(p,f,c,false,true);}
Result apply(const pc::PlayerFields& p,const pf::Fields& f,const Context& c){return run(p,f,c,true,true);}
} // namespace mh4u::player_manager_update
