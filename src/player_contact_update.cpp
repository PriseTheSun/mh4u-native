#include "player_contact_update.hpp"
#include <bit>
#include <new>
#include <stdexcept>

namespace mh4u::player_contact_update {
namespace {
namespace fp=binary32_arithmetic;
namespace ac=player_spatial_acquisition;
namespace pt=player_transition;
struct Failure {Error error;};
[[noreturn]] void fail(Phase phase,ErrorCode code,std::string_view field){
    throw Failure{{phase,code,field,{}}};
}
template<class T>T need(const std::optional<T>& x,Phase phase,std::string_view field){
    if(!x)fail(phase,ErrorCode::unknown_field,field);
    return *x;
}
Word value(fp::Result r,Phase phase,std::string_view field){
    if(const auto* e=std::get_if<fp::Error>(&r))throw Failure{{phase,ErrorCode::arithmetic,field,*e}};
    return std::get<Word>(r);
}
Word add(Word a,Word b,Phase p,std::string_view f){return value(fp::add(a,b),p,f);}
Word mul(Word a,Word b,Phase p,std::string_view f){return value(fp::mul(a,b),p,f);}
Word sub(Word a,Word b,Phase p,std::string_view f){return value(fp::sub(a,b),p,f);}
bool nan(Word x){return (x&0x7fffffffU)>0x7f800000U;}
bool ordered_le(Word a,Word b){
    if(nan(a)||nan(b))return false;
    if((a&0x7fffffffU)==0&&(b&0x7fffffffU)==0)return true;
    const auto ordered=[](Word x){return (x&0x80000000U)?~x:(x^0x80000000U);};
    return ordered(a)<=ordered(b);
}
void call(Effects& e,CallKind c){
    if(e.call_count==e.calls.size())fail(Phase::permission,ErrorCode::capacity,"helper trace");
    e.calls[e.call_count++]=c;
}
Word mask(const pc::PlayerFields& p,const pf::Fields& f,Effects& e){
    call(e,CallKind::attribute_mask);auto r=ac::attribute_mask(p,f);
    if(const auto* error=std::get_if<ac::Error>(&r))throw Failure{{Phase::spatial,ErrorCode::mask,"b4620c",*error}};
    return std::get<Word>(r);
}
Word flags(const pc::PlayerFields& p,const Context& c,Effects& e,bool named){
    if(named)call(e,CallKind::wrapper_flags);
    auto r=ac::wrapper_flags(p,c.spatial);
    if(const auto* error=std::get_if<ac::Error>(&r))throw Failure{{Phase::spatial,ErrorCode::mask,"b43194 flags",*error}};
    call(e,CallKind::local_control);return std::get<Word>(r);
}
void integrate_value(Output& out,const Context& c){
    constexpr auto phase=Phase::integration;
    call(out.effects,CallKind::integrate);
    auto dt=need(c.delta_bits,phase,"Player+14");
    if(out.player.pw.fieldsc[1]!=0)dt=mul(0x3f000000U,dt,phase,"b6ab74 scale0.5");
    else if(out.player.pw.fieldsc[0]==1)dt=mul(0x3e4ccccdU,dt,phase,"b6ab74 scale0.2");
    out.effects.scaled_delta=dt;
    call(out.effects,CallKind::position);
    const auto dt2=mul(dt,dt,phase,"b162a8 dt squared");
    auto& a=out.fields.pw.fields28_bits;auto& v=out.fields.pw.fields18_bits;
    const auto ax=need(a[0],phase,"pw+28"),az=need(a[2],phase,"pw+30");
    const auto h=mul(dt2,0x3f000000U,phase,"b162a8 half dt squared");
    const auto vx=need(v[0],phase,"pw+18"),vy=need(v[1],phase,"pw+1c"),vz=need(v[2],phase,"pw+20");
    const auto vxdt=mul(vx,dt,phase,"b162a8 velocity X"),vydt=mul(vy,dt,phase,"b162a8 velocity Y"),vzdt=mul(vz,dt,phase,"b162a8 velocity Z");
    const auto ay=need(a[1],phase,"pw+2c");
    const auto axh=mul(ax,h,phase,"b162a8 acceleration X"),ayh=mul(ay,h,phase,"b162a8 acceleration Y"),azh=mul(az,h,phase,"b162a8 acceleration Z");
    const auto dx=add(vxdt,axh,phase,"b162a8 displacement X"),dy=add(vydt,ayh,phase,"b162a8 displacement Y");
    const auto oldx=need(out.fields.pbw.field10_bits,phase,"pbw+10");
    const auto dz=add(vzdt,azh,phase,"b162a8 displacement Z");
    out.fields.pbw.field10_bits=add(oldx,dx,phase,"b162a8 position X");
    out.fields.pbw.field14_bits=add(need(out.fields.pbw.field14_bits,phase,"pbw+14"),dy,phase,"b162a8 position Y");
    out.fields.pbw.field18_bits=add(need(out.fields.pbw.field18_bits,phase,"pbw+18"),dz,phase,"b162a8 position Z");
    const auto axdt=mul(ax,dt,phase,"b6ab74 acceleration X"),aydt=mul(ay,dt,phase,"b6ab74 acceleration Y"),azdt=mul(az,dt,phase,"b6ab74 acceleration Z");
    v[0]=add(vx,axdt,phase,"b6ab74 velocity X");
    v[1]=add(vy,aydt,phase,"b6ab74 velocity Y");
    v[2]=add(vz,azdt,phase,"b6ab74 velocity Z");
}
bool selector(const Context& c,Effects& effects){
    call(effects,CallKind::manager_selector);
    if(std::holds_alternative<pm::Unknown>(c.manager))fail(Phase::radius,ErrorCode::unknown_field,"manager1057294");
    if(std::holds_alternative<pm::Absent>(c.manager))fail(Phase::radius,ErrorCode::absent_manager,"manager1057294");
    const auto* m=std::get<pm::ManagerView>(c.manager).state;
    if(!m)fail(Phase::radius,ErrorCode::invalid_identity,"manager1057294 state");
    if(need(m->field112b,Phase::radius,"manager+112b"))return false;
    if(need(m->field1203,Phase::radius,"manager+1203"))return false;
    if(need(m->field15a4,Phase::radius,"manager+15a4"))return false;
    if(need(m->field1645,Phase::radius,"manager+1645"))return false;
    if(need(m->fields1050[4],Phase::radius,"manager+1054")==0)return false;
    return need(m->fields1050[5],Phase::radius,"manager+1055")!=0;
}
bool radius_value(const Bits3& point,Word r0,Word r1,const Context& c,Effects& effects){
    constexpr auto phase=Phase::radius;
    call(effects,CallKind::radius);const bool alternate=selector(c,effects);
    if(std::holds_alternative<pr::Unknown>(c.reference))fail(phase,ErrorCode::unknown_field,"manager+1c");
    const auto r=alternate?r1:r0;
    const auto r2=mul(r,r,phase,"d0ce2c radius squared");
    Bits3 reference;
    if(const auto* p=std::get_if<pr::Points>(&c.reference)){
        reference[0]=std::bit_cast<Word>(p->first_x);
        reference[1]=need(p->first_y_bits,phase,"reference+44");
        reference[2]=std::bit_cast<Word>(p->first_z);
    }else reference=need(c.null_reference_template,phase,"template10e6370");
    const auto dx=sub(reference[0],point[0],phase,"d0ce2c delta X");
    const auto dy=sub(reference[1],point[1],phase,"d0ce2c delta Y");
    const auto dz=sub(reference[2],point[2],phase,"d0ce2c delta Z");
    auto distance=mul(dx,dx,phase,"d0ce2c X squared");
    distance=add(distance,mul(dy,dy,phase,"d0ce2c Y squared"),phase,"d0ce2c XY sum");
    distance=add(distance,mul(dz,dz,phase,"d0ce2c Z squared"),phase,"d0ce2c XYZ sum");
    return ordered_le(distance,r2);
}
void refresh_value(Output& out,const Context& c){
    constexpr auto phase=Phase::permission;
    out.fields.pw.field9e9=0;
    auto candidate=need(out.fields.pbw.field01,phase,"pbw+1");
    const auto& p=out.player;
    if(p.global.field2b==1){
        call(out.effects,CallKind::area_match);
        const auto area=need(p.selection_pbw.field9,phase,"pbw+9");
        const auto selected=need(c.global_c90,phase,"global+c90");
        if(area!=selected)candidate=0;
        call(out.effects,CallKind::local_control);
        if(!pt::local_control(p.global,p.pbw)){
            const auto x=need(out.fields.pbw.field10_bits,phase,"pbw+10");
            const auto y=need(out.fields.pbw.field14_bits,phase,"pbw+14");
            const auto offset_y=add(y,0x42c80000U,phase,"b3c244 Y+100");
            const auto z=need(out.fields.pbw.field18_bits,phase,"pbw+18");
            const bool inside=radius_value({x,offset_y,z},0x43160000U,0x43160000U,c,out.effects);
            out.effects.radius_result=inside;
            if(inside){out.fields.pw.field9e9=1;candidate=0;}
        }
    }
    call(out.effects,CallKind::local_control);
    if(pt::local_control(p.global,p.pbw)){
        const auto mode=need(c.global_c8c,phase,"global+c8c");
        bool exempt=false;
        if(mode==19)exempt=need(c.global_c90,phase,"global+c90")==0;
        if(!exempt&&p.pw.field50d!=0)candidate=0;
    }
    out.position.fieldd2c=candidate;out.effects.permission_written=true;
}
void contact_value(Output& out,const Context& c){
    integrate_value(out,c);
    const auto first_mask=mask(out.player,out.fields,out.effects);
    const auto first_flags=flags(out.player,c,out.effects,false);
    call(out.effects,CallKind::height_wrapper);
    const auto area=need(out.player.selection_pbw.field9,Phase::spatial,"pbw+9");
    const auto second_mask=mask(out.player,out.fields,out.effects);
    const auto second_flags=flags(out.player,c,out.effects,true);
    const auto attributes=first_mask|second_mask,wrapper=first_flags|second_flags;
    const Bits3 current{need(out.fields.pbw.field10_bits,Phase::spatial,"pbw+10"),
        need(out.fields.pbw.field14_bits,Phase::spatial,"pbw+14"),
        need(out.fields.pbw.field18_bits,Phase::spatial,"pbw+18")};
    // 2ba230 transports a previous-position pointer; 2bb178 consumes only Y.
    const auto previous_y=need(out.fields.pbw.field6ec_bits,Phase::spatial,"pbw+6ec");
    if(!c.spatial.scene)fail(Phase::spatial,ErrorCode::unknown_field,"spatial scene");
    call(out.effects,CallKind::height_query);
    auto result=sq::height(*c.spatial.scene,{current,true,previous_y,area,wrapper,attributes,true},
        c.spatial.query_limits,c.spatial.previous_grid_updates);
    if(const auto* e=std::get_if<sq::Error>(&result))throw Failure{{Phase::spatial,ErrorCode::query,"2ba230/2bb178",*e}};
    out.effects.attribute_mask=attributes;out.effects.wrapper_flags=wrapper;
    out.effects.height=std::get<sq::HeightResult>(std::move(result));
    const auto& height=*out.effects.height;
    if(!ordered_le(current[1],height.height_bits)||height.status==0)return;
    if(!height.attribute)fail(Phase::spatial,ErrorCode::invalid_identity,"height attribute output");
    const auto& a=*height.attribute;
    out.player.selection_pbw.field370=0;out.player.selection_pw.fielda10=0;
    out.fields.pbw.field14_bits=height.height_bits;out.fields.pbw.field468_bits=height.height_bits;
    for(std::size_t i=0;i<6;++i)out.fields.pbw.fields470[i]=a.bytes0_5[i];
    out.fields.pbw.field478=a.words2_4_0[0];out.fields.pbw.field47c=a.words2_4_0[1];out.fields.pbw.field480=a.words2_4_0[2];
    for(std::size_t i=0;i<3;++i)out.fields.pbw.fields488_bits[i]=a.normal_bits[i];
    out.fields.pbw.field494_bits=0; // b3150c stores its own +0 literal.
    out.fields.pbw.field498=c.opaque_attribute_tail[0]?c.opaque_attribute_tail[0]:a.opaque28;
    out.fields.pbw.field49c=c.opaque_attribute_tail[1]?c.opaque_attribute_tail[1]:a.opaque2c;
    for(auto& v:out.fields.pw.fields18_bits)v=0;
    for(auto& a0:out.fields.pw.fields28_bits)a0=0;
    out.player.pw.field6c0=0;out.effects.contact_written=true;
}
enum class Operation {integration,contact,refresh,caller};
Result execute(const pc::PlayerFields& p,const pf::Fields& f,const player_position_bridge::Fields& position,const Context& c,Operation operation){
    try{
        Output out{p,f,position,{}};
        if(operation==Operation::integration)integrate_value(out,c);
        else if(operation==Operation::contact)contact_value(out,c);
        else if(operation==Operation::refresh)refresh_value(out,c);
        else{
            out.fields.pbw.field6fb=0;
            if(out.player.pw.field6c0!=0)contact_value(out,c);
            refresh_value(out,c);
        }
        return out;
    }catch(const Failure& e){return e.error;}
    catch(const std::bad_alloc&){return Error{Phase::spatial,ErrorCode::capacity,"contact allocation",{}};}
    catch(const std::length_error&){return Error{Phase::spatial,ErrorCode::capacity,"contact extent",{}};}
}
}
Result integrate(const pc::PlayerFields& p,const pf::Fields& f,const player_position_bridge::Fields& q,const Context& c){return execute(p,f,q,c,Operation::integration);}
Result contact(const pc::PlayerFields& p,const pf::Fields& f,const player_position_bridge::Fields& q,const Context& c){return execute(p,f,q,c,Operation::contact);}
Result refresh(const pc::PlayerFields& p,const pf::Fields& f,const player_position_bridge::Fields& q,const Context& c){return execute(p,f,q,c,Operation::refresh);}
Result apply(const pc::PlayerFields& p,const pf::Fields& f,const player_position_bridge::Fields& q,const Context& c){return execute(p,f,q,c,Operation::caller);}
RadiusResult radius_contains(const Bits3& p,Word first,Word second,const Context& c){
    try{Effects effects;return radius_value(p,first,second,c,effects);}catch(const Failure& e){return e.error;}
}
} // namespace mh4u::player_contact_update
