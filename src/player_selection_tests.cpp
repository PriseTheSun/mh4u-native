#include "player_selection.hpp"
#include <cfenv>
#include <iostream>
#include <stdexcept>
#ifdef MH4U_PLAYER_SELECTION_GENERATED
#include "player_selection_fixtures.hpp"
#include "player_transition_constants.hpp"
#endif
namespace {
namespace ps=mh4u::player_selection;
namespace pt=mh4u::player_transition;
std::size_t assertions=0;
void check(bool good,const char* why){++assertions;if(!good)throw std::runtime_error(why);}
template<class T,class... U>T value(std::variant<U...> result){
    if(!std::holds_alternative<T>(result))throw std::runtime_error("Unexpected result variant");
    return std::get<T>(std::move(result));
}
void synthetic(){
    ps::State s;s.pw={0x7fc00001U,4,0x7f800000U,44,55,0,1,0x80000000U};
    s.pbw={255,8,9};s.transition.global.field5d=1;s.transition.pbw.field31=255;
    const auto initial=s;
    const auto done=value<ps::Completed>(ps::begin_common(s,{3,-1,2147483647,0xffffffff,2,0x7fc00001}));
    auto expected=s;expected.pw.field176=0;expected.pw.field1b0=0;expected.pw.field5b2=0;
    expected.pw.field5b4=0;expected.pw.fielda10=0;expected.pbw.field370=3;
    check(done.state==expected&&!done.transition,"pl_state3 ran a transition or changed unrelated fields");
    check(s==initial,"Input state mutated");
    for(auto invalid:{2U,4U,257U,259U,0xffffffffU}){
        const auto e=value<ps::Error>(ps::begin_common(s,{invalid,0,0,0,0,0}));
        check(e.code==ps::ErrorCode::unsupported_pl_state,"Full-width branch was narrowed before comparison");
    }
    check(value<ps::Error>(ps::begin_common(s,{0,0,0,0,0,0})).code==ps::ErrorCode::unknown_selection_context,"State0 invented missing context");
    auto p=value<ps::PendingAnimation>(ps::begin_wrapper(s,ps::Wrapper::b1d114,{1,-16777217,2147483647,0x7fc00001}));
    check(p.request()==ps::AnimationRequest{24,0,0xcb800000,0x4f000000},"Signed32 rounding or mode0 wrapper differs");
    auto p2=value<ps::PendingAnimation>(ps::begin_wrapper(s,ps::Wrapper::b23b74,{1,16777219,-2147483647,0xffffffff}));
    check(p2.request()==ps::AnimationRequest{24,2,0x4b800002,0xcf000000},"Mode2 wrapper or ties-even differs");
    check(p.before_request().pbw.field376==8&&p.before_request().pbw.field6fa==9,"Continuation ran before request completion");
    check(p.before_request().transition==s.transition,"Prefix changed transition-owned state");
    s.transition.pbw.field31=2;
    check(value<ps::PendingAnimation>(ps::begin_common(s,{1,0,0,0,1,0})).before_request().pw.field170==0,"Type2 clear omitted");
    auto bad=value<ps::Error>(ps::begin_wrapper(s,static_cast<ps::Wrapper>(77),{1,0,0,0}));
    check(bad.code==ps::ErrorCode::invalid_wrapper,"Unknown wrapper accepted");
    const int old=std::fegetround();std::fesetround(FE_DOWNWARD);
    check(value<ps::Error>(ps::begin_common(s,{1,1,1,0,0,0})).code==ps::ErrorCode::unsupported_float_environment,"Unsupported rounding accepted");
    check(std::holds_alternative<ps::Completed>(ps::begin_common(s,{3,1,1,0,0,0})),"Integer-only prefix spuriously required conversion");
    std::fesetround(old);
}
#ifdef MH4U_PLAYER_SELECTION_GENERATED
void original(){
    const auto constants=value<pt::Constants>(pt::Constants::decode(pt::generated::constants_bytes,pt::generated::expected_constants()));
    std::size_t returns=0,splits=0,chains=0,post_branch_changes=0;
    for(const auto& c:selection_oracle::cases){
        auto result=c.entry==0?ps::begin_common(c.before,c.arguments):
            ps::begin_wrapper(c.before,c.entry==1?ps::Wrapper::b1d114:ps::Wrapper::b23b74,
                {c.arguments.pl_state,c.arguments.interpolation,c.arguments.frame,c.arguments.extra_bits});
        if(c.kind==0){
            const auto out=value<ps::Completed>(std::move(result));
            check(out.state==c.after&&!out.transition,"Complete prefix disagrees with ARM");++returns;continue;
        }
        const auto pending=value<ps::PendingAnimation>(std::move(result));
        check(pending.request()==c.animation,"Animation request disagrees with original call registers");
        ps::State post;
        if(c.kind==1){
            check(pending.before_request()==c.prefix_after,"Full prefix state differs from ARM");
            post=c.post;++splits;
            if(pending.request().selector==8&&bool(post.pw.field594)!=bool(pending.before_request().pw.field594))++post_branch_changes;
        }else{
            // Only the original mode>=3 router is proven to return unchanged.
            check(pending.request().mode>2,"A real animation dependency was skipped");
            post=pending.before_request();++chains;
        }
        const auto unmodified=post;
        const auto out=value<ps::Completed>(ps::resume(pending,post,constants));
        check(post==unmodified,"Post-request input was mutated");
        check(out.state==c.after,"Full declared suffix/chain state differs from ARM");
        check(out.transition.has_value(),"Transition observation lost");
        const auto& effect=*out.transition;
        check(effect.arguments.state_arg==c.state_arg&&effect.arguments.action_arg==c.action_arg&&effect.arguments.flags_arg==c.flags_arg,"Transition call arguments differ");
        check(effect.effective_flags==c.effective_flags,"Effective flags differ");
        check(effect.returned_dispatch_call.has_value()==c.dispatch_called,"Dispatch presence differs");
        if(c.dispatch_called)check(effect.returned_dispatch_call==pt::ObservedDispatchCall{c.dispatch_selector,c.dispatch_flags},"Returned dispatch arguments differ");
        // The public b235f8 operation uses the same original suffix snapshot;
        // expected state/arguments are retained ARM facts, not a native oracle.
        const auto direct=value<ps::Completed>(ps::direct_transition(post,{c.state_arg,c.action_arg,c.flags_arg},constants));
        check(direct.state==c.after&&post==unmodified,"Direct transition disagrees with original suffix or mutates input");
        check(direct.transition.has_value(),"Direct transition observation missing");
        check(direct.transition->effective_flags==c.effective_flags&&
              direct.transition->returned_dispatch_call==effect.returned_dispatch_call,"Direct transition lost flags/returned dispatch");
        // The prior prefix remains visible; rejected suffix publishes no changes.
        auto rejected=post;rejected.transition.global.field5d=1;const auto saved=rejected;
        const auto error=value<ps::Error>(ps::resume(pending,rejected,constants));
        check(error.code==ps::ErrorCode::transition_rejected&&error.transition_code==pt::ErrorCode::optional_dispatch_body,"Transition rejection lost its owning error");
        check(rejected==saved,"Rejected continuation altered input");
        const auto direct_error=value<ps::Error>(ps::direct_transition(rejected,{c.state_arg,c.action_arg,c.flags_arg},constants));
        check(direct_error.code==error.code&&direct_error.transition_code==error.transition_code&&direct_error.message==error.message,
              "Direct transition replaced owning failure");
        check(rejected==saved,"Rejected direct transition published prefix clear");
    }
    check(returns==12&&splits==108&&chains==72&&post_branch_changes==72,"Original fixture coverage changed");
    std::cout<<"{\"return_cases\":"<<returns<<",\"split_cases\":"<<splits<<",\"real_no_request_chains\":"<<chains<<",\"post_request_branch_changes\":"<<post_branch_changes<<",\"full_state_mismatches\":0}\n";
}
#endif
}
int main(){try{synthetic();
#ifdef MH4U_PLAYER_SELECTION_GENERATED
original();
#endif
std::cout<<"{\"status\":\"passed\",\"assertions\":"<<assertions<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
