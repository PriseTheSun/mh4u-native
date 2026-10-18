#pragma once
// Test-only source-offset projection retained from player_update_begin_tests.cpp.
// Expected values are decoded exclusively from original arena observations.
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include <array>
#include <vector>
#include <stdexcept>
namespace mh4u::test_player_checkpoint {
namespace pcs=player_controller_step;namespace pf=player_fields;
using U=std::uint32_t;using Bytes=std::vector<std::uint8_t>;using Arenas=std::array<Bytes,3>;
inline U read(const Bytes& b,U o,U n=4){if(n>4||o>b.size()||n>b.size()-o)throw std::runtime_error("checkpoint read extent");U v{};for(U i=0;i<n;++i)v|=U(b[o+i])<<(8*i);return v;}
struct Input{pcs::PlayerFields p;pf::Fields f;};
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
 #define B32(name,offset) f.pbw.name=read(b,offset)
 B32(field464_bits,0x464);B32(field54_bits,0x54);B32(field10_bits,0x10);B32(field14_bits,0x14);B32(field18_bits,0x18);B32(field1c_bits,0x1c);B32(field354_bits,0x354);B32(field478,0x478);B32(field6e8_bits,0x6e8);B32(field6ec_bits,0x6ec);B32(field6f0_bits,0x6f0);B32(field6f4_bits,0x6f4);
 #undef B32
 #define B8(name,offset) f.pbw.name=std::uint8_t(read(b,offset,1))
 B8(field6fb,0x6fb);B8(field6fc,0x6fc);B8(field93ee,0x93ee);B8(field93f1,0x93f1);B8(field37a,0x37a);B8(field6c0,0x6c0);
 #undef B8
 f.pbw.field3a8=std::uint16_t(read(b,0x3a8,2));f.pbw.field3ac=std::uint16_t(read(b,0x3ac,2));f.pbw.field3b8=std::uint16_t(read(b,0x3b8,2));f.pbw.field3bc=std::uint16_t(read(b,0x3bc,2));for(U i=0;i<19;++i)f.pbw.words3d8[i]=mh4u::player_input_storage::split(read(b,0x3d8+4*i));
 for(U i=0;i<99;++i)f.pbw.input_other[i]=std::uint8_t(read(b,mh4u::player_input_storage::remaining_offsets[i],1));
 #define W32(name,offset) f.pw.name=read(w,offset)
 W32(field13c_bits,0x13c);W32(field14c,0x14c);W32(field154,0x154);W32(field15c_bits,0x15c);W32(field6a4_bits,0x6a4);W32(field1f8,0x1f8);W32(field220_bits,0x220);W32(field838_bits,0x838);W32(field5a8,0x5a8);
 #undef W32
 #define W8(name,offset) f.pw.name=std::uint8_t(read(w,offset,1))
 W8(field9dc,0x9dc);W8(field9de,0x9de);W8(field9dd,0x9dd);W8(field150,0x150);W8(field158,0x158);W8(field159,0x159);W8(field15a,0x15a);W8(field6a8,0x6a8);W8(field7f0,0x7f0);W8(field01,1);W8(field0b,0xb);W8(field589,0x589);W8(field6a9,0x6a9);
 #undef W8
 f.pw.field188=std::int16_t(read(w,0x188,2));f.pw.field224=std::int16_t(read(w,0x224,2));f.pw.field238=std::int16_t(read(w,0x238,2));return x;
}
}
