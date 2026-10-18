#pragma once
// Explicit finite inspection seed and immutable parameter bytes shared by the
// two persistent update checkpoint tests. This is not recovered bootstrap.
#include "player_actor.hpp"
#include "picosha2.h"
#include <bit>
namespace mh4u::test_update_actor_fixture {
namespace na=native_actor;namespace oq=player_object_query;namespace jp=joint_pose;
namespace pq=player_parameter_query;namespace us=player_update_statistics;
using U=std::uint32_t;
inline constexpr U one=0x3f800000;
inline constexpr jp::Bits4 identity{0,0,0,one};
inline U bits(float f){return std::bit_cast<U>(f);}
inline na::PlayerInitial seed(){
 na::PlayerInitial x{};x.snapshots.resize(19);x.model.rotation=x.model.rotation_delta=identity;x.model.scale=x.model.null_scale={one,one,one,0};
 for(auto& l:x.layers){l.animation.tracks.joints.resize(19);l.animation.layer.speed=1;l.animation.layer.flags=2;l.animation.prediction_rotation=identity;}
 x.layers[0].animation.layer.end_frame=30;x.layers[0].animation.layer.frame=5.25f;x.layers[0].animation.layer.prepared_frame=6.5f;x.layers[1].animation.layer.weight=.375f;
 x.controller.motion.fieldsd48[4]=one;x.controller.motion.fieldsd48[5]=bits(30);
 for(U i=2;i<8;++i){x.layers[i].animation.layer.frame=std::bit_cast<float>(0x7fc00001U+i);x.layers[i].rotation_98={i,0xffffffff,0x80000000,0x7f800000};}
 auto& p=x.controller.player;auto& f=x.shared_fields;
 p.global.field2b=2;p.global.field2f=0;p.global.field5d=0;p.global.field22=0x6a31;p.global.field20=0xc541;p.global.fieldd72=0;
 p.pbw.field30=0;p.pbw.field33=0;p.pbw.field371=1;p.pbw.field372=2;p.selection_pbw.field9=0;p.selection_pbw.field6fa=0x91;p.pw.fielde8=4;p.pw.fieldec=0x80060001;
 f.pbw.field10_bits=f.pbw.field14_bits=f.pbw.field18_bits=0;f.pbw.field54_bits=0;f.pw.field13c_bits=one;f.pw.field1f8=0x1000;f.pw.field6a8=255;f.pw.field6a4_bits=0x7fc12345;
 f.pw.field159=0xa5;f.pw.field15a=0x5a;f.pw.field158=0x91;f.pw.field0a=1;f.pw.field1b8_bits=0x7fa12345;f.pw.field1bc_bits=0xff800000;
 f.pw.field31c=0xffff;f.pw.field224=0;f.pw.field238=0;f.pw.field324_bits=0;
 x.state1_fields.field50_bits=0x7fc12345;x.position_fields.fieldd30_bits=0xff812345;return x;
}
inline na::PlayerBanks banks(){na::PlayerBanks b{};b[0]=na::MotionSource{"c5f8f0c5f86ad43323a3ca7dcf6394a6938363b038fd1bab264e4b2597e0c0d8"};return b;}
inline oq::Collection collection(bool positive=false){
 oq::Collection c;c.slots.fill(oq::Absent{});c.slots[0]=oq::ObjectIndex{0};c.slots[1]=oq::ObjectIndex{1};c.objects[0].data=c.objects[1].data=oq::DataIndex{0};
 auto& d=c.data[0];d.field140=1;d.field1b4=1;d.field14c=0;d.field290=1;d.field349=0;d.position_bits={0,{},bits(positive?-10.0f:10.0f)};
 d.field78b8_bits=0x7fa54321;d.field580=oq::OverrideIndex{0xffffffff};return c;
}
// A finite, explicit inspection seed. These parameter records are caller-owned
// bytes, not a claim of equipment/bootstrap recovered from an original save.
inline na::PlayerInitial continuation_seed(){
 auto x=seed();auto& p=x.controller.player;auto& f=x.shared_fields;
 p.selection_pbw.fields2fc.fill(0);p.selection_pw.field800=0;p.global.fieldsd38.fill(0);
 p.pw.fieldf0=0x30000001;p.pw.fieldf8=0x90000010;
 f.pbw.field34c=f.pbw.field34e=20;f.pw.field274=f.pw.field275=f.pw.field276=0;
 f.pw.field278_bits=f.pw.field284_bits=f.pw.field288_bits=0;f.pw.field262=0;
 f.pw.field1a0_bits=f.pw.field1a4_bits=0;f.pw.fields248_bits.fill(0);f.pw.fields864_bits.fill(0);
 f.pw.fielda15=0;f.pw.field4f8=0xdeadbeef;f.pw.field4fc=0xaabbccdd;
 f.pbw.field93ee=0x91;f.pbw.field93e9=0x92;
 auto& primary=f.pbw.descriptors[0];primary[0]=7;primary[2]=primary[3]=0;primary[18]=primary[19]=0;
 for(std::size_t i=1;i<f.pbw.descriptors.size();++i)f.pbw.descriptors[i][0]=6;
 f.pbw.words9404[7]=0xffffffff;
 // Poison the input block that the prefix clears, while preserving separate
 // partially known descriptors and unconsumed raw query words.
 f.pbw.words3d8[18]={0xa1,{},0xff,{}};f.pbw.words9404[255]=0x7fa12345;
 f.pbw.descriptors[1][27]=0x91;return x;
}
struct Parameters {
    std::array<std::uint8_t,24> record{};
    std::string record_sha;
    std::array<us::BonusRecord,32> bonuses{};
    us::Context context;
    Parameters(const std::array<std::uint8_t,42>& caps,const std::string& caps_sha){
        record[8]=150;record[10]=20;record_sha=picosha2::hash256_hex_string(record);
        for(auto& bonus:bonuses)bonus.id=0;
        context.parameters.records24[7]=pq::Bytes{std::as_bytes(std::span{record}),record_sha,0};
        context.parameters.caps_by_kind=pq::Bytes{std::as_bytes(std::span{caps}),caps_sha,0};
        context.mode2_bonuses=us::BonusTable{bonuses};
    }
    Parameters(const Parameters&)=delete;Parameters& operator=(const Parameters&)=delete;
};
} // namespace mh4u::test_update_actor_fixture
