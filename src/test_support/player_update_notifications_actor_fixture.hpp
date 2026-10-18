#pragma once
// Explicit shared diagnostic inputs; no inferred world queue startup.
#include "player_update_status_actor_fixture.hpp"
namespace mh4u::test_notifications_actor_fixture {
namespace na=native_actor;namespace nq=notification_queue;
constexpr std::uint32_t one=0x3f800000;
using test_status_actor_fixture::status_inputs;
inline nq::State queue_seed(){
 nq::State q;q.counter=0;
 for(auto& record:q.pool){record.fill(std::uint8_t{0x95});record[0]=0;record[2].reset();}
 for(auto& slot:q.slots)slot=nq::Absent{};
 return q;
}
inline void notification_inputs(na::PlayerInitial& seed){
 status_inputs(seed);seed.shared_fields.pbw.field34c=100;
 seed.shared_fields.pw.field1fc=seed.shared_fields.pw.field200=0;
 seed.shared_fields.pw.field586=0x4d5;seed.shared_fields.pw.field2c8_bits=one;
 seed.controller.player.pw.fieldec|=0x08000000U;
}
}
