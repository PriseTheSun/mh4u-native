#pragma once
// Shared explicit manager inputs for persistent checkpoint tests.
#include "player_actor.hpp"
namespace mh4u::test_manager_actor_fixture {
namespace pm=player_manager_update;namespace na=native_actor;
inline pm::ManagerState manager_seed(){
 pm::ManagerState m;m.field112b=m.field1203=m.field15ff=m.field15a4=m.field1645=0;
 m.field102c=m.field1040=m.field1044=m.field1048=m.field104c=0x7fa54321;
 m.fields1050.fill(0xa5);m.fields1050[4]=0;return m;
}
inline void manager_inputs(na::PlayerInitial& x){
 auto& p=x.controller.player;auto& f=x.shared_fields;
 p.pbw.field31=4;p.selection_pbw.field6f9=1;p.pw.field50d=0;p.global.field7a=0;
 f.pw.field13c_bits=0;f.pbw.field464_bits=0;f.pw.field9dc=f.pw.field9de=f.pw.field9dd=0;
 for(auto& word:f.pbw.words3d8)word.fill(0);
 f.pbw.words3d8[3]={128,0,0,0};
 f.pw.fieldb4=0;f.pw.field50c=0xa5;f.pw.field50e=0x5a91;f.pw.fielde4=0x9523;f.pw.fielde6=0xabcd;
 f.pw.fielde0_bits=0x7fa12345;f.pw.fieldb8_bits=0;p.pw.fielddc=0x7fa98765;
}
}
