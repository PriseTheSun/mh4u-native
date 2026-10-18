#pragma once
// Test-only projections of retained original observations; no expected policy.
#include "test_support/player_update_continuation_fixture.hpp"
#include "player_update_dispatch.hpp"
namespace mh4u::test_dispatch {
using namespace mh4u::test_continuation;
namespace ds=mh4u::player_update_dispatch;
namespace action=mh4u::player_state1;
struct CallExtra {std::array<U,3>floats{};U command_flags{};std::array<U,5>before{},after{};};
struct Record {Row row;cp::Arenas before_dispatch,after_first;Bytes global_before_dispatch,global_after_first;Bytes wrappers,environment;std::vector<CallExtra>extra;};
Record read_record_contents(Reader&q,U ci){Record v;v.row=read_contents(q,ci);for(const auto&c:v.row.c.calls)check(c.start>0&&c.end<=v.row.c.trace.size(),"all call bounds");for(auto*stage:{&v.before_dispatch,&v.after_first}){for(U i=0;i<3;++i)(*stage)[i]=q.bytes(csize[i]);(stage==&v.before_dispatch?v.global_before_dispatch:v.global_after_first)=q.bytes(csize[3]);}v.wrappers=q.bytes(4096);check(v.wrappers==q.bytes(4096),"original wrappers unchanged");v.environment=q.bytes(64);check(q.u()==v.row.c.calls.size(),"extended call count");for(const auto&call:v.row.c.calls){CallExtra e;for(auto&x:e.floats)x=q.u();e.command_flags=q.u();if(call.pc==0xb66a20){for(auto&x:e.before)x=q.u();for(auto&x:e.after)x=q.u();}v.extra.push_back(e);}return v;}
Record read_record(Reader&r,U ci){const auto n=r.u();check(n<=4*1024*1024,"dispatch record capacity");Reader q{r.take(n)};auto v=read_record_contents(q,ci);q.end();return v;}
struct Motion {
 ma::rq::Model model;mc::Globals globals;mc::Sampling sampling;
 void init(const Record&v,const std::vector<ma::Source>&sources,ma::Context&tracks){const auto&raw=v.row.c.before[0];model.skeleton_present=word(raw,0xf0)!=0;for(U i=0;i<16;++i){auto a=word(raw,0xc04+4*i);if(!a){model.banks[i]=ma::rq::Absent{};continue;}check(a>=0x3500000&&a<=0x3500180&&(a-0x3500000)%128==0,"wrapper identity");auto table=word(v.wrappers,a-0x3500000+0x64);if(!table){model.banks[i]=ma::rq::Resource{0xfffffffffffdULL,std::nullopt};continue;}bool found=false;for(const auto&s:sources)if(s.base==table){model.banks[i]=s.resource.request_resource();found=true;break;}check(found,"LMT table binding");}
 for(U i=0;i<3;++i)globals.fields6c_tables[i]=ma::words<2>(v.environment,8*i);globals.vector10e6370=ma::words<3>(v.environment,24);sampling.templates.base_translation_10e62c0=ma::words<3>(v.environment,36);sampling.templates.identity_10e6560=ma::words<4>(v.environment,48);sampling.resolver={&tracks,ma::Context::resolve};}
};
Raw snapshot(const Raw&base,const cp::Arenas&stage){auto out=base;for(U i=0;i<3;++i)out[i]=stage[i];return out;}
std::array<U,5> dispatch_values(const pcs::State&s,const pf::Fields&f){check(f.pw.field01.has_value(),"observed PW01 known");return{s.player.pbw.field371,s.player.pbw.field372,s.player.pbw.fields5[0],s.player.selection_pbw.field376,*f.pw.field01};}
Raw project(const Case&c,const ad::Output&o,const action::Fields&fields,const tbl::Table&t35,const tbl::Table&t31,const ma::Context&tracks,const ma::Bindings&events,std::span<const TableBinding>bindings={}){auto out=encode(c,o,t35,tracks,events,&t31,bindings);if(fields.field50_bits)put(out[2],0x50,*fields.field50_bits);return out;}
std::vector<U> scoped_calls(const Case&c,U pc,const Call&scope){std::vector<U>out;for(U i=0;i<c.calls.size();++i){const auto&v=c.calls[i];if(v.pc==pc&&v.start>=scope.start&&v.end<=scope.end)out.push_back(i);}return out;}
Case preparation_case(const Case&c){auto out=c;const auto*first=call(c,0xb66a20);check(first,"original first dispatch");std::erase_if(out.calls,[&](const auto&v){return v.start>=first->start;});out.trace.resize(first->start-1);return out;}
}
