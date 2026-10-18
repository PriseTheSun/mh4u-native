#pragma once
// Test-only source-offset projection retained from player_update_begin_tests.cpp.
// Expected values are decoded exclusively from original arena observations.
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include <array>
#include <vector>
#include <stdexcept>
#include <type_traits>
namespace mh4u::test_player_checkpoint {
namespace pcs=player_controller_step;namespace pf=player_fields;
using U=std::uint32_t;using Bytes=std::vector<std::uint8_t>;using Arenas=std::array<Bytes,3>;
inline U read(const Bytes& b,U o,U n=4){if(n>4||o>b.size()||n>b.size()-o)throw std::runtime_error("checkpoint read extent");U v{};for(U i=0;i<n;++i)v|=U(b[o+i])<<(8*i);return v;}
struct Input{pcs::PlayerFields p;pf::Fields f;};
// One test-only offset declaration for every shared optional field. Consumers
// decode source observations and project native outputs through the same map.
template<class Fields,class Visitor>inline void shared_fields(Fields& f,Visitor visit){
 visit(f.pw.field1fc,2,0x1fc,4);visit(f.pw.field200,2,0x200,4);visit(f.pw.field586,2,0x586,2);
 visit(f.pw.field9e8,2,0x9e8,1);
 visit(f.pbw.field368_bits,1,0x368,4);
 visit(f.pw.field17e,2,0x17e,2);
 visit(f.pw.field190,2,0x190,2);visit(f.pw.field198,2,0x198,2);visit(f.pw.field240,2,0x240,2);
 visit(f.pw.field29a,2,0x29a,2);visit(f.pw.field2a0,2,0x2a0,2);visit(f.pw.field2a2,2,0x2a2,2);
 visit(f.pw.field1d0,2,0x1d0,1);visit(f.pw.field265,2,0x265,1);visit(f.pw.field299,2,0x299,1);
 visit(f.pw.field5c_bits,2,0x5c,4);visit(f.pw.field68_bits,2,0x68,4);visit(f.pw.field6c_bits,2,0x6c,4);
 visit(f.pw.field1a8_bits,2,0x1a8,4);visit(f.pw.field1b4_bits,2,0x1b4,4);visit(f.pw.field1c0_bits,2,0x1c0,4);
 visit(f.pw.field268_bits,2,0x268,4);visit(f.pw.field28c_bits,2,0x28c,4);visit(f.pw.field2a4_bits,2,0x2a4,4);visit(f.pw.field29c_bits,2,0x29c,4);
 visit(f.pw.field2c8_bits,2,0x2c8,4);visit(f.pw.field2cc_bits,2,0x2cc,4);visit(f.pw.field2f0_bits,2,0x2f0,4);visit(f.pw.field2fc_bits,2,0x2fc,4);
 visit(f.pw.field940_bits,2,0x940,4);visit(f.pw.field948_bits,2,0x948,4);visit(f.pw.field950_bits,2,0x950,4);

 visit(f.pw.field10_bits,2,0x10,4);visit(f.pw.fieldc0_bits,2,0xc0,4);visit(f.pw.field339,2,0x339,1);visit(f.pw.field80f,2,0x80f,1);
 visit(f.pbw.field37c,1,0x37c,2);visit(f.pbw.field37e,1,0x37e,2);
 visit(f.pbw.field380_bits,1,0x380,4);visit(f.pbw.field384_bits,1,0x384,4);
 visit(f.pbw.field34e,1,0x34e,2);visit(f.pbw.field93e9,1,0x93e9,1);
 for(U i=0;i<7;++i)for(U j=0;j<28;++j)visit(f.pbw.descriptors[i][j],1,(i?0x70+(i-1)*28:0x1c0)+j,1);
 for(U i=0;i<256;++i)visit(f.pbw.words9404[i],1,0x9404+4*i,4);
 for(U i=0;i<40;++i){visit(f.pbw.bonus236[i].id,1,0x236+4*i,2);visit(f.pbw.bonus236[i].value,1,0x238+4*i,2);}
 for(U i=0;i<5;++i){visit(f.pbw.fields388_bits[i],1,0x388+4*i,4);visit(f.pw.fields248_bits[i],2,0x248+4*i,4);visit(f.pw.fields814[i],2,0x814+i,1);visit(f.pw.fields864_bits[i],2,0x864+4*i,4);}
 visit(f.pw.field262,2,0x262,1);visit(f.pw.field805,2,0x805,1);visit(f.pw.field806,2,0x806,1);visit(f.pw.field81c,2,0x81c,1);visit(f.pw.fielda15,2,0xa15,1);
 visit(f.pw.field274,2,0x274,1);visit(f.pw.field275,2,0x275,1);visit(f.pw.field276,2,0x276,1);
 visit(f.pw.field278_bits,2,0x278,4);visit(f.pw.field284_bits,2,0x284,4);visit(f.pw.field288_bits,2,0x288,4);
 visit(f.pw.field1a0_bits,2,0x1a0,4);visit(f.pw.field1a4_bits,2,0x1a4,4);
 visit(f.pw.field294,2,0x294,2);visit(f.pw.field296,2,0x296,2);visit(f.pw.field4f8,2,0x4f8,4);visit(f.pw.field4fc,2,0x4fc,4);
 visit(f.pbw.field464_bits,1,0x464,4);
 visit(f.pbw.field54_bits,1,0x54,4);
 visit(f.pbw.field10_bits,1,0x10,4);
 visit(f.pbw.field14_bits,1,0x14,4);
 visit(f.pbw.field18_bits,1,0x18,4);
 visit(f.pbw.field1c_bits,1,0x1c,4);
 visit(f.pbw.field354_bits,1,0x354,4);
 visit(f.pbw.field478,1,0x478,4);
 visit(f.pbw.field6e8_bits,1,0x6e8,4);
 visit(f.pbw.field6ec_bits,1,0x6ec,4);
 visit(f.pbw.field6f0_bits,1,0x6f0,4);
 visit(f.pbw.field6f4_bits,1,0x6f4,4);
 visit(f.pbw.field6fb,1,0x6fb,1);
 visit(f.pbw.field6fc,1,0x6fc,1);
 visit(f.pbw.field93ee,1,0x93ee,1);
 visit(f.pbw.field93f1,1,0x93f1,1);
 visit(f.pbw.field37a,1,0x37a,1);
 visit(f.pbw.field6c0,1,0x6c0,1);
 visit(f.pw.field13c_bits,2,0x13c,4);
 visit(f.pw.field14c,2,0x14c,4);
 visit(f.pw.field154,2,0x154,4);
 visit(f.pw.field15c_bits,2,0x15c,4);
 visit(f.pw.field6a4_bits,2,0x6a4,4);
 visit(f.pw.field1f8,2,0x1f8,4);
 visit(f.pw.field220_bits,2,0x220,4);
 visit(f.pw.field838_bits,2,0x838,4);
 visit(f.pw.field5a8,2,0x5a8,4);
 visit(f.pw.field9dc,2,0x9dc,1);
 visit(f.pw.field9de,2,0x9de,1);
 visit(f.pw.field9dd,2,0x9dd,1);
 visit(f.pw.field150,2,0x150,1);
 visit(f.pw.field158,2,0x158,1);
 visit(f.pw.field159,2,0x159,1);
 visit(f.pw.field15a,2,0x15a,1);
 visit(f.pw.field6a8,2,0x6a8,1);
 visit(f.pw.field7f0,2,0x7f0,1);
 visit(f.pw.field01,2,1,1);
 visit(f.pw.field0b,2,0xb,1);
 visit(f.pw.field589,2,0x589,1);
 visit(f.pw.field6a9,2,0x6a9,1);
 for(U i=0;i<19;++i)for(U j=0;j<4;++j)visit(f.pbw.words3d8[i][j],1,0x3d8+4*i+j,1);
 for(U i=0;i<99;++i)visit(f.pbw.input_other[i],1,player_input_storage::remaining_offsets[i],1);
 visit(f.pbw.field3a8,1,0x3a8,2);
 visit(f.pbw.field3ac,1,0x3ac,2);
 visit(f.pbw.field3b8,1,0x3b8,2);
 visit(f.pbw.field3bc,1,0x3bc,2);
 visit(f.pbw.field34c,1,0x34c,2);
 visit(f.pw.field188,2,0x188,2);
 visit(f.pw.field224,2,0x224,2);
 visit(f.pw.field238,2,0x238,2);
 visit(f.pw.field0a,2,0xa,1);
 visit(f.pw.field319,2,0x319,1);
 visit(f.pw.field338,2,0x338,1);
 visit(f.pw.field33a,2,0x33a,1);
 visit(f.pw.fielda2c,2,0xa2c,1);
 visit(f.pw.field31c,2,0x31c,4);
 visit(f.pw.field324_bits,2,0x324,4);
 visit(f.pw.field1b8_bits,2,0x1b8,4);
 visit(f.pw.field1bc_bits,2,0x1bc,4);
 visit(f.pw.field328_bits,2,0x328,4);
 visit(f.pw.field32c_bits,2,0x32c,4);
 visit(f.pw.field7fd,2,0x7fd,1);
 visit(f.pw.field7f1,2,0x7f1,1);
 visit(f.pw.field7f4,2,0x7f4,1);
 for(U i=0;i<7;++i){visit(f.pbw.linked_records[i].present,1,0x7e84+792*i,4);visit(f.pbw.linked_records[i].flag,1,0x7e49+792*i,1);}
}
inline Input decode(const Arenas& a){Input x;auto& p=x.p;auto& f=x.f;const auto& b=a[1];const auto& w=a[2];
 for(U i=0;i<3;++i)p.fieldsd10[i]=read(a[0],0xd10+4*i);
 #define PB(name,offset,n) p.pbw.name=static_cast<decltype(p.pbw.name)>(read(b,offset,n))
 PB(field20,0x20,4);PB(field24,0x24,4);PB(field28,0x28,4);PB(field30,0x30,1);PB(field31,0x31,1);PB(field33,0x33,1);PB(field371,0x371,1);PB(field372,0x372,2);PB(field377,0x377,1);PB(field378,0x378,2);PB(field39c,0x39c,4);PB(field3d4,0x3d4,4);PB(field451,0x451,1);
 #undef PB
 for(U i=0;i<3;++i)p.pbw.fields5[i]=std::uint8_t(read(b,5+i,1));
 for(U i=0;i<4;++i)p.pbw.fields2f8[i]=std::uint8_t(read(b,0x2f8+i,1));
 for(U i=0;i<2;++i){p.pbw.angles[i]=std::uint16_t(read(b,0x3c0+2*i,2));p.pbw.magnitudes[i]=std::uint16_t(read(b,0x3c4+2*i,2));}
 #define PW(name,offset,n) p.pw.name=static_cast<decltype(p.pw.name)>(read(w,offset,n))
 PW(field4c,0x4c,4);PW(field54,0x54,4);PW(field58,0x58,4);PW(field79,0x79,1);PW(fieldb0,0xb0,4);PW(fieldbc,0xbc,2);PW(fieldc4,0xc4,4);PW(fielddc,0xdc,4);PW(fielde8,0xe8,4);PW(fieldec,0xec,4);PW(fieldf0,0xf0,4);PW(fieldf4,0xf4,4);PW(fieldf8,0xf8,4);PW(fieldfc,0xfc,4);PW(field17c,0x17c,1);PW(field1f4,0x1f4,4);PW(field261,0x261,1);PW(field334,0x334,4);PW(field33b,0x33b,1);PW(field48c,0x48c,4);PW(field490,0x490,1);PW(field4c2,0x4c2,2);PW(field504,0x504,4);PW(field50d,0x50d,1);PW(field5e4,0x5e4,1);PW(field6c0,0x6c0,1);PW(field9d8,0x9d8,1);
 #undef PW
 for(U i=0;i<2;++i){p.pw.fieldsc[i]=std::uint8_t(read(w,0xc+i,1));p.pw.fields290[i]=std::uint8_t(read(w,0x290+i,1));p.pw.fields35d[i]=std::uint8_t(read(w,0x35d+i,1));p.pw.fields3f5[i]=std::uint8_t(read(w,0x3f5+i,1));}
 for(U i=0;i<4;++i)p.pw.fields488[i]=std::uint8_t(read(w,0x488+i,1));
 p.pw.policy.field104=std::uint16_t(read(w,0x104,2));p.pw.policy.field106=std::uint16_t(read(w,0x106,2));p.pw.policy.field108=std::uint16_t(read(w,0x108,2));for(U i=0;i<48;++i)p.pw.policy.latches[i]=std::uint8_t(read(w,0x10a+i,1));p.pw.policy.timer140_bits=read(w,0x140);p.pw.policy.timer144_bits=read(w,0x144);p.pw.policy.field148=std::uint8_t(read(w,0x148,1));
 p.global={0xa5a5,0x39,0xe2,0xc5,0xd7};
 auto& s=p.selection_pbw;s.field370=std::uint8_t(read(b,0x370,1));s.field376=std::uint8_t(read(b,0x376,1));s.field6fa=std::uint8_t(read(b,0x6fa,1));s.field6f9=std::uint8_t(read(b,0x6f9,1));s.field358=read(b,0x358);s.field9=std::uint8_t(read(b,9,1));for(U i=0;i<20;++i)s.fields2fc[i]=std::uint16_t(read(b,0x2fc+2*i,2));
 auto& q=p.selection_pw;q.field170=read(w,0x170);q.field176=std::uint8_t(read(w,0x176,1));q.field1b0=read(w,0x1b0);q.field5b2=std::uint16_t(read(w,0x5b2,2));q.field5b4=std::uint16_t(read(w,0x5b4,2));q.field594=std::uint8_t(read(w,0x594,1));q.field9a8=std::uint8_t(read(w,0x9a8,1));q.fielda10=read(w,0xa10);q.field7fc=std::uint8_t(read(w,0x7fc,1));q.field800=read(w,0x800);
 shared_fields(f,[&](auto& value,U arena,U offset,U width){using T=typename std::remove_reference_t<decltype(value)>::value_type;value=static_cast<T>(read(a[arena],offset,width));});return x;
}
}
