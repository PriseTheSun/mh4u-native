#pragma once
#include "player_status_update.hpp"
#include <algorithm>
#include <span>
#include <vector>
namespace mh4u::test_status_effects {
using U=std::uint32_t;
struct Call{U pc,value;std::array<U,4>args;};
struct Counts{std::size_t scans{},timers{},clears{};};
template<class Check> Counts compare(const player_status_update::Effects&e,std::span<const Call>calls,std::span<const U>trace,std::span<const U>write_pcs,Check check){
 auto find=[&](U pc)->const Call*{auto i=std::find_if(calls.begin(),calls.end(),[&](const auto&v){return v.pc==pc;});return i==calls.end()?nullptr:&*i;};
 auto count=[&](U pc){return std::count_if(calls.begin(),calls.end(),[&](const auto&v){return v.pc==pc;});};
 const auto*l=find(0xd04924),*d=find(0xd05ac0),*s=find(0x686f08);check(l&&e.local==bool(l->value),"local original result");check(e.status224==(d&&d->value!=0),"status224 original result");U qi{};for(const auto&v:calls)if(v.pc==0xd048c4){check(qi<e.list_queries.size()&&e.list_queries[qi++]==v.args[1],"list source order");}check(qi==e.query_count,"list source count");check(e.scan.has_value()==bool(s),"scan source presence");if(s){const auto&v=*e.scan;check(v.matched==bool(s->value),"scan source result");check(v.slots_examined==std::count(trace.begin(),trace.end(),0x686f2c)&&v.enabled_queries==count(0x911e48)&&v.status_queries==count(0x9547a0)&&v.availability_queries==count(0x91f214),"scan observed counts");}
 check(e.timer_incremented==bool(find(0xb04c24)),"timer original call");check(e.timer_cleared==std::any_of(write_pcs.begin(),write_pcs.end(),[](U pc){return pc==0xb22470;}),"timer clear original store");return {std::size_t(s!=nullptr),std::size_t(e.timer_incremented),std::size_t(e.timer_cleared)};
}
}
