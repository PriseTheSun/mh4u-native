#pragma once
#include "player_position_bridge.hpp"
#include "test_support/model_animation_frame_fixture.hpp"
namespace mh4u::test_support::position_bridge {
using namespace mh4u::test_support::model_frame;
namespace bridge=mh4u::player_position_bridge;
namespace mo=mh4u::model_animation_output;
namespace jo=mh4u::joint_output;
namespace pa=mh4u::player_animation;
namespace pcs=mh4u::player_controller_step;
namespace pf=mh4u::player_fields;
using Arenas=std::map<std::string,Bytes>;
void same(const Bytes& a,const Bytes& b,const std::string& label){
    check(a.size()==b.size(),label+" extent");if(a!=b){auto at=std::mismatch(a.begin(),a.end(),b.begin());std::ostringstream s;s<<label<<" byte+0x"<<std::hex<<std::distance(a.begin(),at.first)<<" got "<<unsigned(*at.first)<<" expected "<<unsigned(*at.second);throw std::runtime_error(s.str());}++checks;
}
void same(const Arenas& a,const Arenas& b){check(a.size()==b.size(),"Arena count");for(const auto& [name,bytes]:a)same(bytes,b.at(name),name);}
Raw motion_raw(const Arenas& a,unsigned layers){Raw out{a.at("model"),a.at("joints"),a.at("snapshots"),{}};for(unsigned i=0;i<layers;++i)out.channels.push_back(a.at("channels_"+std::to_string(i)));return out;}
void put_motion(Arenas& a,const Raw& raw){a.at("model")=raw.model;a.at("joints")=raw.joints;a.at("snapshots")=raw.snapshots;for(unsigned i=0;i<raw.channels.size();++i)a.at("channels_"+std::to_string(i))=raw.channels[i];}
struct State {frame::State frame;mo::Hierarchy hierarchy;pcs::State controller;pf::Fields shared;bridge::Fields fields;};
State decode(const Arenas& a,const Context& context,unsigned layers){
    State out;out.frame=frame_state(motion_raw(a,layers),context);const auto& m=a.at("model");const auto& p=a.at("pbw");auto& h=out.hierarchy;
    h.local_rt=words<16>(m,0x70);h.world=words<16>(m,0xb0);h.flags4=word(m,4);h.flags104=word(m,0x104);h.flags230=word(m,0x230);h.attachment_index=word(m,0x34);h.attachment_mode_high16=static_cast<std::uint16_t>(word(m,0x3a,2));h.dirty=static_cast<std::uint8_t>(word(m,0xc49,1));h.fieldc4c=word(m,0xc4c);h.order=a.at("order");
    for(std::size_t i=0;i<out.frame.animation.joints.size();++i){const auto pos=i*168;const auto& j=a.at("joints");const auto owner=word(j,pos+0xa0);h.joints.push_back({{word(j,pos+0x94),word(j,pos+0x98)},words<16>(j,pos+8),word(j,pos+0x90),word(j,pos+0x88)?jo::Callback::unsupported:jo::Callback::null,owner==0x2000000?jo::Owner::self:owner?jo::Owner::other:jo::Owner::null});}
    auto& c=out.controller;c.player.fieldsd10=words<3>(m,0xd10);c.player.pbw.field20=word(p,0x20);c.player.pbw.field24=word(p,0x24);c.player.pbw.field28=word(p,0x28);c.motion.fieldsd48=words<7>(m,0xd48);c.motion.fieldd68=word(m,0xd68);c.motion.fieldsd70=words<4>(m,0xd70);
    // Unrelated canonical fields are typed sentinels, not facts assigned to
    // unobserved source memory. The bridge must preserve them exactly.
    c.player.pw.fieldb0=0xdead1234;c.player.global.field5d=0x5a;c.player.selection_pw.field7fc=0x12345678;
    out.shared.pbw.field10_bits=word(p,0x10);out.shared.pbw.field14_bits=word(p,0x14);out.shared.pbw.field18_bits=word(p,0x18);out.shared.pbw.field1c_bits=word(p,0x1c);out.shared.pw.field154=0xcafe1234;
    out.fields={static_cast<std::uint8_t>(word(m,0xd2c,1)),static_cast<std::uint8_t>(word(m,0xd44,1)),word(m,0xd30),word(m,0xd34),word(m,0xd38),word(m,0xd64),static_cast<std::uint16_t>(word(m,0x38,2))};return out;
}
Arenas encode(const Arenas& before,const State& state,const Context& context,bool scratch){
    auto out=before;put_motion(out,frame_raw(motion_raw(before,static_cast<unsigned>(state.frame.animation.layers.size())),state.frame,context));auto& m=out.at("model");auto& p=out.at("pbw");const auto& h=state.hierarchy;
    put_words(m,0x70,h.local_rt);put_words(m,0xb0,h.world);put(m,4,h.flags4);put(m,0x104,h.flags104);put(m,0x230,h.flags230);put(m,0x34,h.attachment_index);put(m,0x3a,h.attachment_mode_high16,2);put(m,0xc49,h.dirty,1);put(m,0xc4c,h.fieldc4c);
    check(h.joints.size()*168==out.at("joints").size(),"Hierarchy extent");for(std::size_t i=0;i<h.joints.size();++i){const auto pos=i*168;const auto& j=h.joints[i];auto& bytes=out.at("joints");put_words(bytes,pos+8,j.matrix);put(bytes,pos+0x90,j.depth);put(bytes,pos+0x94,j.metadata.flags94);put(bytes,pos+0x98,j.metadata.mapping98);
        const auto expected_callback=word(before.at("joints"),pos+0x88)?jo::Callback::unsupported:jo::Callback::null;const auto pointer=word(before.at("joints"),pos+0xa0);const auto expected_owner=pointer==0x2000000?jo::Owner::self:pointer?jo::Owner::other:jo::Owner::null;
        check(j.callback==expected_callback&&j.owner==expected_owner,"Borrowed callback/owner identity preserved");}
    const auto* order=std::get_if<std::vector<std::uint8_t>>(&h.order);check(order&&order->size()==out.at("order").size(),"Present order preserved");out.at("order")=*order;
    const auto& c=state.controller;put_words(m,0xd10,c.player.fieldsd10);put_words(m,0xd48,c.motion.fieldsd48);put(m,0xd68,c.motion.fieldd68);put_words(m,0xd70,c.motion.fieldsd70);
    put(p,0x20,c.player.pbw.field20);put(p,0x24,c.player.pbw.field24);put(p,0x28,c.player.pbw.field28);
    const auto& shared=state.shared.pbw;check(shared.field10_bits&&shared.field14_bits&&shared.field18_bits&&shared.field1c_bits,"Projected PBW lanes known");put(p,0x10,*shared.field10_bits);put(p,0x14,*shared.field14_bits);put(p,0x18,*shared.field18_bits);put(p,0x1c,*shared.field1c_bits);
    const auto& f=state.fields;check(f.fieldd2c&&f.fieldd44&&f.fieldd30_bits&&f.fieldd34_bits&&f.fieldd38_bits&&f.fieldd64&&f.euler_mode,"Projected aux fields known");put(m,0xd2c,*f.fieldd2c,1);put(m,0xd44,*f.fieldd44,1);put(m,0xd30,*f.fieldd30_bits);put(m,0xd34,*f.fieldd34_bits);put(m,0xd38,*f.fieldd38_bits);put(m,0xd64,*f.fieldd64);put(m,0x38,*f.euler_mode,2);
    if(scratch){for(unsigned i=0;i<3;++i){put(m,0xd00+4*i,word(p,0x10+4*i));put(m,0xd20+4*i,word(p,0x20+4*i));}put(m,0xd0c,0);}
    return out;
}
} // namespace mh4u::test_support::position_bridge
