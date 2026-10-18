#pragma once
// Shared explicit diagnostic status inputs; never source startup defaults.
#include "player_actor.hpp"
namespace mh4u::test_status_actor_fixture {
namespace na=native_actor;
inline void status_inputs(na::PlayerInitial& seed){
 auto& f=seed.shared_fields;auto& p=seed.controller.player;
 // Explicit test context, not constructor defaults or inferred scheduler state.
 f.pw.field17e=0;f.pw.field238=f.pw.field240=0;
 f.pw.field68_bits=f.pw.field6c_bits=f.pbw.field368_bits=0;
 f.pw.field1a8_bits=f.pw.field28c_bits=f.pw.field268_bits=f.pw.field1c0_bits=0;
 f.pw.field1d0=0;f.pw.field1b4_bits=f.pw.field2f0_bits=0;
 f.pw.field29c_bits=f.pw.field2a4_bits=0;f.pw.field29a=0;
 f.pw.field2c8_bits=f.pw.field2cc_bits=0;f.pw.field2fc_bits=0;
 p.selection_pbw.fields2fc[0]=39;
}
}
