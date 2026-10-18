#pragma once
#include "test_support/player_update_dispatch_effects.hpp"
#include "player_update_post_dispatch.hpp"
namespace mh4u::test_post {
using namespace mh4u::test_dispatch;
namespace post=mh4u::player_update_post_dispatch;
std::size_t post_queries{},post_records{},post_rescaled{},eligibility_calls{};
struct PostRecord {Record original;std::array<std::array<Bytes,4>,4> stages;Bytes diagnostic;U extent{},initial_s17{},factor{};};
PostRecord read_post_contents(Reader&q,U ci){PostRecord v;v.original=read_record_contents(q,ci);for(auto&stage:v.stages)for(U i=0;i<4;++i)stage[i]=q.bytes(csize[i]);v.diagnostic=q.bytes(4096);v.extent=q.u();check(v.extent>0&&v.extent<=4096,"diagnostic table extent");v.initial_s17=q.u();v.factor=q.u();check(v.initial_s17==0x3f800000,"observed caller initial s17");return v;}
PostRecord read_post(Reader&r,U ci){const auto n=r.u();check(n<=4*1024*1024,"post record capacity");Reader q{r.take(n)};auto v=read_post_contents(q,ci);q.end();return v;}
void post_effects(const Record&v,const post::Effects&e,std::optional<U>factor){const auto&c=v.row.c;check(e.virtual_returned&&calls(c,0xb25b68)==1,"concrete virtual original");const auto*p=call(c,0xb05f84);const auto*s=call(c,0xb061f0);check(p&&s&&p->end<s->start,"speed calls source order");auto ix=[&](const Call*c0){return std::size_t(c0-c.calls.data());};check(e.primary_bits==v.extra[ix(p)].floats[0]&&e.secondary_bits==v.extra[ix(s)].floats[0],"raw original speed arguments");if(factor)check(e.base_bits==*factor,"original live factor s17");check(e.primary_rescaled==bool(pcs_count(c,0xb05fc8,p)),"rescale branch original");post_rescaled+=e.primary_rescaled;check(e.query_count==calls(c,0xb2a75c),"scanner call count");U qi=0;for(const auto&c0:c.calls)if(c0.pc==0xb2a75c){const auto&q=e.policy_queries[qi++];const auto records=pcs_count(c,0xb2a790,&c0);check(q.kind==c0.args[1]&&q.action==c0.args[2]&&q.matched==bool(c0.value)&&q.records==records,"scanner original arguments result examined records");++post_queries;post_records+=records;}const auto*el=call(c,0xb345b8);check(e.eligible.has_value()==bool(el)&&e.eligibility_selector.has_value()==bool(el),"eligibility call presence");if(el){check(*e.eligible==bool(el->value)&&*e.eligibility_selector==el->args[1],"original eligibility argument result");++eligibility_calls;}}
}
