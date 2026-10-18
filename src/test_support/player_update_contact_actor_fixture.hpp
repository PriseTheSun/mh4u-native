#pragma once
// Shared explicit contact inputs for persistent checkpoint tests.
#include "player_actor.hpp"
namespace mh4u::test_contact_actor_fixture {
namespace na=native_actor;
inline void contact_inputs(na::PlayerInitial& seed,bool integrate=false){
 auto& p=seed.controller.player;auto& f=seed.shared_fields;
 p.pw.field6c0=integrate?1:0;p.pw.fieldsc[0]=p.pw.fieldsc[1]=0;
 f.pbw.field01=0xa5;f.pbw.field6fb=0x73;
 f.pw.fields18_bits.fill(0);f.pw.fields28_bits.fill(0);
 seed.position_fields.fieldd2c=0x39;seed.position_fields.fieldd44=0x91;
}
}
