#include "root_motion.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <string>
#include <vector>
namespace {
using namespace mh4u::root_motion;
struct Case {
    const char* name;TickState input;AdvanceParameters parameters;PreUpdateScope scope;
    EventPlan plan;std::vector<EventSample> answers;AdvanceState expected;float progress;
};
#include "root_motion_events_fixtures.inc"
std::size_t assertions=0;
void check(bool condition,const std::string& message) { ++assertions;if(!condition)throw std::runtime_error(message); }
template<class F>void rejected(F fn,ErrorCode code) {
    try{fn();}catch(const Error& e){check(e.code()==code,"Wrong rejection");return;}
    throw std::runtime_error("Expected rejection");
}
template<std::size_t N>void near(const std::array<float,N>& a,const std::array<float,N>& b) {
    for(std::size_t i=0;i<N;++i)check(std::abs(a[i]-b[i])<=1e-5f,"Numeric ARM mismatch");
}
void run(const Case& c) {
    constexpr ResetTemplates reset{{0,0,0},{0,0,0,1}};
    // Planner observation can be compared separately with absent tables.
    const auto advance=advance_one_layer(c.input.pre.clock,c.parameters,{PreUpdate::declared_unchanged,0,{false,false,false,false}},{32});
    check(advance.event_plan.preserve_masks==c.plan.preserve_masks,"Freeze plan");
    check(advance.event_plan.count==c.plan.count,"Request range count");
    for(std::size_t i=0;i<c.plan.count;++i)check(advance.event_plan.ranges[i]==c.plan.ranges[i],"ARM signed range mismatch");
    const auto pending=plan_tick_with_events(c.input,c.parameters,{1,1,1},{},reset,c.scope,{32});
    const auto requests=pending.requests();
    check(requests.size()==c.answers.size(),"Presence/request count");
    for(std::size_t i=0;i<requests.size();++i)check(requests[i]==c.answers[i].request,"ARM request identity mismatch");
    const auto result=pending.finish(c.answers);const auto& a=result.state.pre.clock;const auto& b=c.expected;
    check(a.frame==b.frame&&a.previous_frame==b.previous_frame&&a.previous_step==b.previous_step,"Clock mismatch");
    check(a.internal_flags==b.internal_flags&&a.previous_flags==b.previous_flags&&a.model_attributes==b.model_attributes,"Flags/attributes mismatch");
    near(a.previous_translation,b.previous_translation);near(a.previous_rotation,b.previous_rotation);
    check(a.event_masks==b.event_masks,"ARM current masks mismatch");
    check(a.previous_event_masks==b.previous_event_masks,"ARM previous masks mismatch");
    check(result.progress==c.progress,"ARM progress mismatch");
    check(!result.applied&&!result.prediction_written,"Gate-clear changed motion");
    auto answers=c.answers;std::reverse(answers.begin(),answers.end());
    check(pending.finish(answers).state.pre.clock.event_masks==b.event_masks,"Answer order changed masks");
    if(!answers.empty()) {
        auto missing=answers;missing.pop_back();rejected([&]{pending.finish(missing);},ErrorCode::missing_event_sample);
        auto wrong=answers;wrong[0].request.group=4;rejected([&]{pending.finish(wrong);},ErrorCode::event_request_mismatch);
        if(answers.size()>1){auto duplicate=answers;duplicate[1]=duplicate[0];rejected([&]{pending.finish(duplicate);},ErrorCode::duplicate_event_sample);}
    }
    answers.push_back({{0,0,{0,0}},1});rejected([&]{pending.finish(answers);},ErrorCode::unexpected_event_sample);
    auto joints=c.scope;joints.original_joint_count=1;
    rejected([&]{plan_tick_with_events(c.input,c.parameters,{1,1,1},{},reset,joints,{32});},ErrorCode::unsupported_pre_update_scope);
}
}
int main() {
    try {for(const auto& c:cases){try{run(c);}catch(const std::exception& e){throw std::runtime_error(std::string(c.name)+": "+e.what());}}
        std::cout<<"{\"cases\":"<<cases.size()<<",\"assertions\":"<<assertions<<",\"mismatches\":0}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
