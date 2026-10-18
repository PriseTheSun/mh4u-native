#include "motion_transition.hpp"
#include <array>
#include <bit>
#include <cfenv>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace mt=mh4u::motion_transition;
namespace jp=mh4u::joint_pose;
namespace ju=mh4u::joint_update;
namespace mp=mh4u::motion_pending;
namespace mq=mh4u::motion_request;
namespace math=mh4u::motion_transform_math;
std::size_t checks{};
void check(bool ok,const char* message){++checks;if(!ok)throw std::runtime_error(message);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
jp::Bits4 vec(float x,float y,float z,float w){return {bits(x),bits(y),bits(z),bits(w)};}
mt::Output success(mt::Result result){check(std::holds_alternative<mt::Output>(result),"expected successful transition");return std::get<mt::Output>(result);}
mt::Error error(mt::Result result,mt::ErrorCode code){
    check(std::holds_alternative<mt::Error>(result),"expected transition error");
    auto out=std::get<mt::Error>(result);check(out.code==code,"wrong transition error");return out;
}
mt::Parameters params(){return {vec(0,0,0,1),{bits(3),bits(-4),bits(5)},0xabcdef11};}
jp::Joint joint(){return {{0x7fc12345U,0x80000000U,0x12345678U},vec(0,0,0,1),vec(2,4,6,9),vec(1,3,5,8)};}
mp::Snapshot snapshot(){return {0,0x87654321U,vec(0,0,0,-1),vec(3,7,11,13),vec(4,8,12,17)};}
void run(){
    std::vector<jp::Joint> joints{joint()};std::vector<ju::Metadata> metadata{{0xff00,0xabc300ff}};
    std::vector<mp::Snapshot> snapshots{snapshot()};mq::Layer a,b;
    std::array<mt::LayerView,2> layers{{{a},{b}}};a.field60=0.5F;b.field60=1.0F;
    auto p=params();const auto original=joints;const auto snapshots_before=snapshots;
    // A stale owner and absent auxiliary arrays are not read if no flags&100.
    auto out=success(mt::apply(joints,{},{},layers,p));
    check(!out.blend_layer_found&&out.blended_joints==0,"absent transition state");
    check(out.accumulated_flags==0,"absent transition clears accumulated flags");
    check(out.joints==original,"absent transition preserves joint bytes");
    check(out.local_rt==jp::Matrix{bits(1),0,0,0,0,bits(1),0,0,0,0,bits(1),0,bits(3),bits(-4),bits(5),bits(1)},"RT is built even without transitions");
    a.flags=0x100;b.flags=0;
    out=success(mt::apply(joints,metadata,snapshots,layers,p));
    check(out.blend_layer_found&&out.blended_joints==1,"active transition count");
    check(out.accumulated_flags==p.accumulated_flags,"active transition preserves accumulated flags");
    check(out.joints[0].rotation==vec(0,0,0,1),"opposite hemisphere preserves rotation");
    check(out.joints[0].translation==vec(2,5,8,0),"translation blend and zero padding");
    check(out.joints[0].scale==vec(3,6,9,0),"scale blend and zero padding");
    check(out.joints[0].reference_translation==joints[0].reference_translation,"reference translation is opaque");
    check(joints==original&&snapshots==snapshots_before,"borrowed joints and snapshots unchanged after success");
    // Any flag100 opens the pass, then the owner can choose an unflagged layer.
    snapshots[0].layer_index=1;b.selector=0xffff;b.flags=2;b.attributes=0xffffffff;
    out=success(mt::apply(joints,metadata,snapshots,layers,p));
    check(out.joints[0].translation==vec(3,7,11,0),"owner does not require selector or flag100");
    check(out.blended_joints==1,"disabled layer still supplies transition weight");
    snapshots[0].layer_index=0;
    for(float zero:{0.0F,-0.0F}){
        a.field60=zero;joints[0].rotation[0]=0x7fc12345U;
        out=success(mt::apply(joints,metadata,snapshots,layers,p));
        check(out.joints==joints&&out.blended_joints==0,"signed zero skips all pose arithmetic/padding");
    }
    joints=original;a.field60=-0.5F;
    out=success(mt::apply(joints,metadata,snapshots,layers,p));
    check(out.joints[0].translation==vec(0,1,2,0),"negative weight extrapolates");
    check(out.joints[0].scale==vec(1,2,3,0),"scale is not clamped");
    a.field60=1.5F;
    out=success(mt::apply(joints,metadata,snapshots,layers,p));
    check(out.joints[0].translation==vec(4,9,14,0),"weight above one extrapolates");
    for(std::uint32_t flags:{1U,0x40U,0x41U}){
        metadata[0].flags94=flags;snapshots[0].layer_index=0xfffffffeU;
        out=success(mt::apply(joints,metadata,snapshots,layers,p));
        check(out.joints==joints&&out.blended_joints==0,"skip flags precede owner lookup");
    }
    metadata[0].flags94=0;snapshots[0].layer_index=0xffffffffU;
    out=success(mt::apply(joints,{},snapshots,layers,p));
    check(out.joints==joints,"owner sentinel precedes metadata read");
    out=success(mt::apply({}, {}, {},layers,p));
    check(out.accumulated_flags==p.accumulated_flags&&out.blend_layer_found,"zero joints preserve active accumulated flags");
    error(mt::apply(joints,metadata,{},layers,p),mt::ErrorCode::snapshot_extent);
    snapshots[0]=snapshot();
    error(mt::apply(joints,{},snapshots,layers,p),mt::ErrorCode::metadata_extent);
    snapshots[0].layer_index=2;
    auto e=error(mt::apply(joints,metadata,snapshots,layers,p),mt::ErrorCode::layer_extent);
    check(e.joint==0&&e.layer==2,"owner error identity retained");
    snapshots[0]=snapshot();a.field60=1;
    snapshots[0].rotation=vec(0,0,0,0);
    e=error(mt::apply(joints,metadata,snapshots,layers,p),mt::ErrorCode::arithmetic);
    check(e.component==mt::Component::rotation&&e.arithmetic&&e.arithmetic->code==math::ErrorCode::degenerate_quaternion,"zero quaternion is not repaired");
    snapshots[0]=snapshot();snapshots[0].translation[1]=0x7f800000U;
    e=error(mt::apply(joints,metadata,snapshots,layers,p),mt::ErrorCode::arithmetic);
    check(e.component==mt::Component::translation,"late translation error component");
    check(joints==original,"late error does not publish earlier quaternion");
    snapshots[0]=snapshot();snapshots[0].scale[2]=0x7fc12345U;
    e=error(mt::apply(joints,metadata,snapshots,layers,p),mt::ErrorCode::arithmetic);
    check(e.component==mt::Component::scale,"late scale error component");
    snapshots[0]=snapshot();a.field60=std::bit_cast<float>(0x7fc12345U);
    e=error(mt::apply(joints,metadata,snapshots,layers,p),mt::ErrorCode::arithmetic);
    check(e.component==mt::Component::rotation,"NaN weight does not compare equal to zero");
    a.field60=.5F;
    auto limited=mt::Limits{};limited.joints=0;
    error(mt::apply(joints,metadata,snapshots,layers,p,limited),mt::ErrorCode::resource_limit);
    limited=mt::Limits{};limited.layers=1;
    error(mt::apply(joints,metadata,snapshots,layers,p,limited),mt::ErrorCode::resource_limit);
    // The first joint succeeds before the second errors; both caller objects remain intact.
    joints.push_back(joint());metadata.push_back(metadata[0]);snapshots.push_back(snapshot());snapshots[1].scale[1]=0x7f800000U;
    const auto before_late=joints;const auto before_snapshots=snapshots;
    e=error(mt::apply(joints,metadata,snapshots,layers,p),mt::ErrorCode::arithmetic);
    check(e.joint==1&&e.component==mt::Component::scale,"late joint error position");
    check(joints==before_late&&snapshots==before_snapshots,"whole call remains transactional");
    p.model_rotation=vec(0,0,2,0);
    out=success(mt::apply({}, {}, {}, {},p));
    check(out.local_rt[0]==bits(-7)&&out.local_rt[5]==bits(-7)&&out.local_rt[10]==bits(1),"model quaternion is not normalized");
    p.model_translation={0x7fc12345U,0x7f800000U,0x80000000U};
    out=success(mt::apply({}, {}, {}, {},p));
    check(out.local_rt[12]==p.model_translation[0]&&out.local_rt[13]==p.model_translation[1]&&out.local_rt[14]==p.model_translation[2],"model translation is copied raw");
    p.model_rotation[0]=0x7f800000U;
    e=error(mt::apply({}, {}, {}, {},p),mt::ErrorCode::arithmetic);
    check(e.component==mt::Component::model_rt,"model RT errors precede layer scan");
    p=params();const auto rounding=std::fegetround();check(std::fesetround(FE_UPWARD)==0,"set rounding mode");
    auto bad_environment=mt::apply({}, {}, {}, {},p);
    check(std::fesetround(rounding)==0,"restore rounding mode");
    e=error(std::move(bad_environment),mt::ErrorCode::arithmetic);
    check(e.component==mt::Component::model_rt&&e.arithmetic&&e.arithmetic->code==math::ErrorCode::floating_environment,"RT consumes environment even with no joints");
}
}
int main(){try{run();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
