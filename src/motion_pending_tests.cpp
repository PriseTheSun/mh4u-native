#include "motion_pending.hpp"
#include <bit>
#include <cfenv>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace mp=mh4u::motion_pending;
namespace mi=mh4u::motion_initialization;
namespace mb=mh4u::motion_binding;
namespace jp=mh4u::joint_pose;
namespace mq=mh4u::motion_request;
namespace {
std::size_t checks=0;
void check(bool v,const char* message){++checks;if(!v)throw std::runtime_error(message);}
float f(std::uint32_t b){return std::bit_cast<float>(b);}
const mp::Output& good(const mp::Result& r){check(std::holds_alternative<mp::Output>(r),"expected pending result");return std::get<mp::Output>(r);}
const mp::Error& error(const mp::Result& r,mp::ErrorCode code){check(std::holds_alternative<mp::Error>(r),"expected pending rejection");const auto& e=std::get<mp::Error>(r);check(e.code==code,"wrong pending rejection");return e;}
mp::Parameters parameters(){
    mp::Parameters p{};p.model_reference=jp::PresentModel{jp::Null{}};
    p.initialization.templates.identity_10e6560=jp::Bits4{0,0,0,0x3f800000};
    p.initialization.templates.previous_translation_10e6370=jp::Bits3{10,20,30};
    p.scale_10e6380=jp::Bits3{40,50,60};return p;
}
mp::State state(){
    mp::State s;s.layers.resize(3);s.joints.resize(1);s.snapshots.resize(1);s.calculation_flags=0x3f123456;s.accumulated_flags=0x40000000;
    s.joints[0]={{0x111,0x222,0x333},{1,2,3,4},{5,6,7,8},{9,10,11,12}};
    s.snapshots[0]={2,0x20,{101,102,103,104},{105,106,107,108},{109,110,111,112}};
    for(unsigned i=0;i<3;++i){auto& a=s.layers[i].animation;a.layer.flags=2;a.layer.selector=static_cast<std::uint16_t>(0x400+i);a.layer.weight=1;a.layer.interpolation=1;a.layer.remaining_interpolation=1;a.layer.masks={31,32,33,34};
        a.layer.bindings[0]=mq::Binding{99,4,i,0};a.tracks.joints.resize(1);
        const mb::Channel old{mb::TrackReference{99,4,i,0},0x7fa00000U+i,mb::BufferReference{99,0x400+i},0x80000000};
        a.tracks.translation=a.tracks.rotation=old;for(auto& c:a.tracks.joints[0].channels)c=old;a.tracks.joints[0].flags=0x77;
        s.layers[i].rotation_98={51,52,53,54};s.layers[i].translation_c8={61,62,63,64};
    }return s;
}
struct ResolverState {mutable std::vector<std::uint32_t> calls;bool fail{};std::optional<mb::Action> action;};
mp::ActionResolution resolve(const void* context,std::uint32_t bank,std::uint32_t slot){
    auto& r=*static_cast<const ResolverState*>(context);r.calls.push_back((bank<<8)|slot);
    if(r.fail)return mp::ActionResolutionError{73,"Explicit unknown bank in test"};
    return r.action;
}
mp::ActionResolver resolver(ResolverState& r){return {&r,resolve};}
void boundaries(){
    auto s=state();auto p=parameters();ResolverState r;
    auto result=mp::apply(s,{},p,resolver(r),{});const auto& out=good(result);
    check(out.pending_found&&!out.restored_joints&&out.initializer_calls==3,"pending processing count");
    check(r.calls==std::vector<std::uint32_t>{0x402,0x401,0x400},"action resolutions must descend by layer");
    check(out.state.joints==s.joints,"active branch restored joints");
    for(unsigned i=0;i<3;++i){const auto& a=out.state.layers[i].animation;const auto& before=s.layers[i].animation;
        check(a.layer.flags==0x100,"pending flags clearing and transition bit");check(a.layer.masks==before.layer.masks,"pending reset erased event masks");
        check(!a.layer.bindings[0]&&!a.tracks.translation.track&&!a.tracks.rotation.track,"pending references not cleared");
        check(a.tracks.translation.frame_bits==before.tracks.translation.frame_bits&&a.tracks.translation.key_data==before.tracks.translation.key_data&&a.tracks.translation.weight_bits==0,"root reset changed unconsumed cursor or kept weight");
        check(a.tracks.joints[0].flags==0,"joint flags were not reset");
        for(unsigned k=0;k<3;++k){const auto& c=a.tracks.joints[0].channels[k];const auto& old=before.tracks.joints[0].channels[k];check(!c.track&&c.frame_bits==old.frame_bits&&c.key_data==old.key_data&&c.weight_bits==0,"joint reset is not selective");}
        check(out.state.layers[i].translation_c8==jp::Bits4{10,20,30,0},"additional translation reset/padding");
    }
    check(s.layers[0].animation.layer.flags==2&&s.layers[0].animation.tracks.translation.track.has_value(),"input state mutated");
    auto quiet=s;for(auto& l:quiet.layers)l.animation.layer.flags=0;
    quiet.snapshots.clear();quiet.layers[0].animation.tracks.joints.clear();
    auto quiet_result=mp::apply(quiet,{},mp::Parameters{}, {},{});const auto& idle=good(quiet_result);
    check(!idle.pending_found&&idle.state.calculation_flags==quiet.calculation_flags&&idle.state.layers[0].animation.tracks.joints.empty(),"no-pending path consumed unrelated dependencies");
    auto absent=s;for(auto& l:absent.layers)l.animation.layer.selector=0xffff;
    auto missing=p;missing.model_reference=jp::Unknown{};error(mp::apply(absent,{},missing,{},{}),mp::ErrorCode::unknown_model);
    missing.model_reference=jp::Null{};absent.snapshots.clear();auto null_result=mp::apply(absent,{},missing,{},{});const auto& nul=good(null_result);
    check(nul.pending_found&&!nul.restored_joints&&nul.state.layers[0].animation.layer.flags==2,"null model consumed pending layers");
    check(nul.state.calculation_flags==(absent.calculation_flags&~0x20000000U)&&nul.state.accumulated_flags==absent.accumulated_flags,"null model early return flags");
    absent=s;for(auto& l:absent.layers)l.animation.layer.selector=0xffff;absent.layers[1].animation.layer.flags=0x125;
    auto reset_result=mp::apply(absent,{},p,{},{});const auto& reset=good(reset_result);
    check(reset.restored_joints&&reset.initializer_calls==0,"no-active path did not restore joints");
    check(reset.state.snapshots[0].rotation==s.joints[0].rotation&&reset.state.snapshots[0].translation==jp::Bits4{9,10,11,0}&&reset.state.snapshots[0].scale==jp::Bits4{5,6,7,0},"snapshot did not precede joint reset");
    check(reset.state.snapshots[0].layer_index==2&&reset.state.snapshots[0].flags==0x20,"highest pending owner/flags");
    check(reset.state.layers[1].animation.layer.flags==0x125,"intermediate nonpending flags changed");
    check(reset.state.joints[0].rotation==*p.initialization.templates.identity_10e6560&&reset.state.joints[0].translation==jp::Bits4{10,20,30,0},"joint reset not delegated");
    auto broken=s;broken.snapshots.clear();error(mp::apply(broken,{},p,resolver(r),{}),mp::ErrorCode::snapshot_extent);
    broken=s;broken.layers[2].animation.tracks.joints.clear();check(error(mp::apply(broken,{},p,resolver(r),{}),mp::ErrorCode::binding_extent).layer==2,"binding error layer");
    missing=p;missing.initialization.templates.identity_10e6560.reset();check(error(mp::apply(s,{},missing,resolver(r),{}),mp::ErrorCode::missing_template).layer==2,"template error layer");
    error(mp::apply(s,{},p,{},{}),mp::ErrorCode::unresolved_action);r.fail=true;
    auto unknown=mp::apply(s,{},p,resolver(r),{});check(error(unknown,mp::ErrorCode::unresolved_action).resolution->code==73,"resolution diagnostic lost");r.fail=false;
    mq::Action header{1,0,true,{}};r.action.emplace(mb::Action{17,0,0,header,0,{}});
    error(mp::apply(s,{},p,resolver(r),{}),mp::ErrorCode::action_identity);r.action.reset();
    missing=p;missing.scale_10e6380.reset();auto failed_reset=mp::apply(absent,{},missing,{},{});check(error(failed_reset,mp::ErrorCode::restoration).restoration->code==jp::ErrorCode::missing_template,"restoration error lost");
    check(absent.joints==s.joints&&absent.snapshots==s.snapshots,"failed reset changed caller state");
    mp::Limits limits;limits.layers=2;error(mp::apply(s,{},p,{}, {},limits),mp::ErrorCode::resource_limit);limits={};limits.joints=0;error(mp::apply(s,{},p,{}, {},limits),mp::ErrorCode::resource_limit);
    limits={};limits.initialization.binding.joints=0;error(mp::apply(s,{},p,resolver(r),{},limits),mp::ErrorCode::initialization);
    // NaN/Inf/subnormal bit gates must not become finite-only policy. No
    // arithmetic follows when every action is explicitly absent.
    for(auto remaining:{0U,0x80000000U,1U,0x80000001U,0x7f800000U,0x7fc01234U}){
        auto raw=s;for(auto& l:raw.layers)l.animation.layer.remaining_interpolation=f(remaining);
        auto sampled=mp::apply(raw,{},p,resolver(r),{});const auto& o=good(sampled);
        const auto expected=(remaining&0x7fffffffU)?0x100:0;check(o.state.layers[0].animation.layer.flags==expected,"remaining raw-bit predicate");
    }
    auto sticky=s;for(auto& l:sticky.layers){l.animation.layer.remaining_interpolation=0;l.animation.layer.flags=0x102;}
    auto sticky_result=mp::apply(sticky,{},p,resolver(r),{});check(good(sticky_result).state.layers[0].animation.layer.flags==0x100,"zero remaining cleared sticky100");
    for(auto interp:{0x34000000U,0x34000001U,0x7f800000U,0x7fc01234U,0xff800000U})
        for(auto weight:{0U,0x80000000U,1U,0x3f800000U,0x7f800000U,0x7fc01234U,0xbf800000U}){
            auto gates=s;gates.layers.resize(2);auto& first=gates.layers[0].animation.layer;first.flags=0;first.weight=0;
            auto& pending=gates.layers[1].animation.layer;pending.interpolation=f(interp);pending.weight=f(weight);pending.remaining_interpolation=0;
            gates.snapshots[0].flags=0;gates.snapshots[0].layer_index=1;
            auto gate_result=mp::apply(gates,{},p,resolver(r),{});const auto& observed=good(gate_result).state.snapshots[0];
            const bool blend=interp!=0x34000000U&&interp!=0xff800000U&&(weight==1||weight==0x3f800000U||weight==0x7f800000U);
            check(observed.rotation==(blend?gates.joints[0].rotation:gates.snapshots[0].rotation),"raw IEEE interpolation/weight gate changed");
            check(observed.translation==gates.snapshots[0].translation&&observed.scale==gates.snapshots[0].scale,"zero remaining copied translation/scale");
        }
    const int old=std::fegetround();std::fesetround(FE_DOWNWARD);auto raw_round=mp::apply(s,{},p,resolver(r),{});std::fesetround(old);
    check(std::holds_alternative<mp::Output>(raw_round),"copy-only pending imposed arithmetic environment");
}
}
int main(){try{boundaries();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
