#include "joint_output.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace jo=mh4u::joint_output;
namespace jp=mh4u::joint_pose;
namespace mt=mh4u::motion_transform_math;
namespace {
std::size_t checks;
void require(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::uint32_t b(float value){return std::bit_cast<std::uint32_t>(value);}
jo::Matrix identity(){return {b(1),0,0,0,0,b(1),0,0,0,0,b(1),0,0,0,0,b(1)};}
jo::State state(std::size_t n=0){
    jo::State s;s.local_rt=s.world=identity();s.scale={b(1),b(1),b(1)};
    s.flags4=0x4802;s.flags104=0x12345678;s.flags230=0x30;s.fieldc4c=0x12345678;
    s.order=std::vector<std::uint8_t>(n);
    for(std::size_t i=0;i<n;++i){jo::Joint j;j.pose={{b(9),b(8),b(7)},{0,0,0,b(1)},{b(1),b(1),b(1),0xdeadbeef},{b(2),b(3),b(4),0xcafebabe}};
        j.metadata={0xff00,0xaabbccdd};j.matrix=identity();j.depth=42;j.callback=jo::Callback::null;j.owner=jo::Owner::self;s.joints.push_back(j);std::get<std::vector<std::uint8_t>>(s.order)[i]=static_cast<std::uint8_t>(i);}
    return s;
}
jo::Parameters params(){jo::Parameters p;p.model=jp::PresentModel{jp::Unknown{}};p.parent=jo::Null{};
    const auto m=identity();mt::Basis base;std::copy_n(m.begin(),12,base.begin());p.reference_basis=base;p.reference_translation=jo::Bits4{0,0,0,b(1)};return p;}
jo::Output ok(const jo::State& s,const jo::Parameters& p){auto r=jo::apply(s,p);if(auto* error=std::get_if<jo::Error>(&r))throw std::runtime_error(std::string(error->message));++checks;return std::get<jo::Output>(std::move(r));}
void error(const jo::State& s,const jo::Parameters& p,jo::ErrorCode code,jo::Limits limits={}){auto r=jo::apply(s,p,limits);require(std::holds_alternative<jo::Error>(r),"expected tagged rejection");require(std::get<jo::Error>(r).code==code,"wrong rejection code");}
void boundary(){
    auto s=state();auto p=params();auto out=ok(s,p);
    require(out.state.fieldc4c==0x12345678,"word c4c must remain unchanged for gate");
    require(out.state.flags230==0x31&&out.selected==jo::SelectedCallback::normal_9608d8,"empty present model still selects callback");
    require(out.attachment_cleared,"null attachment clears pointer");
    s.flags4=2;require(ok(s,p).state.fieldc4c==2,"c4c is full word store");
    p.parent=jo::Unknown{};error(s,p,jo::ErrorCode::unknown_parent);p=params();
    p.model=jp::Unknown{};error(s,p,jo::ErrorCode::unknown_model);p=params();
    p.reference_basis.reset();error(s,p,jo::ErrorCode::missing_template);p=params();
    p.reference_translation.reset();error(s,p,jo::ErrorCode::missing_template);p=params();
    p.model=jp::Null{};s=state(2);s.dirty=3;s.order=jo::Unknown{};s.flags238=0x40;
    out=ok(s,p);require(!out.selected&&out.state.dirty==3&&out.state.joints==s.joints,"null rModel consumes no traversal state");
    require(out.state.flags230==0x30,"null rModel preserves flags230");p=params();
    s=state(1);s.order=jo::Unknown{};error(s,p,jo::ErrorCode::unknown_order);s.order=jo::Null{};error(s,p,jo::ErrorCode::null_order);
    s.order=std::vector<std::uint8_t>{};error(s,p,jo::ErrorCode::order_extent);s.order=std::vector<std::uint8_t>{1};error(s,p,jo::ErrorCode::order_extent);
    s=state(1);s.joints[0].metadata.flags94=0; // self-parent allowed when clean: consumes prior matrix.
    out=ok(s,p);require(out.updated_joints==1,"clean self-parent consumes prior matrix");
    s.joints[0].metadata.flags94=0x0100;error(s,p,jo::ErrorCode::parent_extent);
    s=state(1);s.flags238=0x08000000;s.joints[0].callback=jo::Callback::unknown;s.joints[0].pose.rotation[0]=0x7fc12345;
    out=ok(s,p);require(out.updated_joints==0&&out.state.joints==s.joints,"null selector ignores pose and callback without extra flag");
    s.flags238|=0x01000000;error(s,p,jo::ErrorCode::unknown_callback);s.joints[0].callback=jo::Callback::unsupported;error(s,p,jo::ErrorCode::unsupported_callback);
    s.joints[0].metadata.flags94|=0x20;ok(s,p); // skipped callback is never consumed.
    for(auto flags:{0x04000000U,0x40U,0x4040U}){
        s=state(1);s.flags238=flags;error(s,p,jo::ErrorCode::unsupported_selected_callback);
        s.joints[0].metadata.flags94|=0x20;out=ok(s,p);require(out.updated_joints==0,"skipped unsupported callback allowed");
        s=state();s.flags238=flags;out=ok(s,p);require(out.selected.has_value(),"unsupported empty selector is a fact");
    }
    s=state(1);s.flags238=0x100040;out=ok(s,p);require(out.state.flags104&0x40,"normal special selector sets flag10440");
    s=state(4);s.dirty=7;s.flags238=0x08000000;
    const std::uint32_t parents[]={2,2,3,0xff};
    for(unsigned i=0;i<4;++i)s.joints[i].metadata.flags94=parents[i]<<8;
    s.joints[3].callback=jo::Callback::unknown;s.joints[3].owner=jo::Owner::unknown;
    out=ok(s,p);require(std::get<std::vector<std::uint8_t>>(out.state.order)==std::vector<std::uint8_t>({3,2,0,1}),"source pair sort order");
    require(out.state.dirty==0&&out.state.joints[0].depth==2,"dirty depth and flag cleared");
    for(unsigned i=0;i<4;++i)require(out.state.joints[i].metadata.mapping98==(s.joints[i].metadata.mapping98&~0x30000U),"unrelated mapping bits preserved");
    require(s.dirty==7&&s.joints[0].depth==42,"source state is immutable");
    {auto tied=state(3);tied.dirty=1;tied.flags238=0x08000000;
     tied.joints[0].metadata.flags94=tied.joints[1].metadata.flags94=0x0200;
     const auto sorted=ok(tied,p);
     require(std::get<std::vector<std::uint8_t>>(sorted.state.order)==std::vector<std::uint8_t>({2,1,0}),"exact pair swaps reverse tied siblings, unlike stable sort");}
    for(const auto owner:{jo::Owner::unknown,jo::Owner::null,jo::Owner::other}){
        auto v=s;v.joints[0].owner=owner;error(v,p,owner==jo::Owner::unknown?jo::ErrorCode::unknown_owner:owner==jo::Owner::null?jo::ErrorCode::null_owner:jo::ErrorCode::foreign_owner);
    }
    auto v=s;v.joints[0].callback=jo::Callback::unknown;error(v,p,jo::ErrorCode::unknown_callback);
    v=s;v.joints[0].callback=jo::Callback::unsupported;error(v,p,jo::ErrorCode::unsupported_callback);
    v=s;v.joints[3].metadata.flags94=0;v.joints[3].owner=jo::Owner::self;error(v,p,jo::ErrorCode::cycle);
    v=s;v.joints[0].metadata.flags94=0x0400;error(v,p,jo::ErrorCode::parent_extent);
    s=state(2);s.order=std::vector<std::uint8_t>{0,0};out=ok(s,p);require(out.updated_joints==2&&out.state.joints[1]==s.joints[1],"literal duplicate visitation");
    s=state(256);s.flags238=0x08000000;s.dirty=1;out=ok(s,p);require(out.state.joints.size()==256,"256 source byte indices representable");
    s=state(257);error(s,p,jo::ErrorCode::resource_limit);s.flags238=0x08000000;s.dirty=1;error(s,p,jo::ErrorCode::resource_limit,{257,257,256,256});
    s=state(1);error(s,p,jo::ErrorCode::resource_limit,{0,256,256,256});
    s=state();s.local_rt[12]=0x7fc12345;p.model=jp::Null{};out=ok(s,p);require(out.state.world[12]==0x7fc12345&&(out.state.flags104&0x100),"NaN translation copied and compares unequal");
    s.local_rt[12]=0x80000000;out=ok(s,p);require((out.state.flags104&0x100)==0,"signed zero compares equal");
    p.reference_translation=jo::Bits4{0x7f800000,0,0,b(1)};s.local_rt[12]=0x7f800000;out=ok(s,p);require((out.state.flags104&0x100)==0,"raw infinity equality");
    p=params();s=state();jo::Parent parent{};parent.flags4=3;p.parent=parent;out=ok(s,p);require(out.attachment_cleared,"invalid parent clears before getter/matrices");
    parent.flags4=1;parent.world=identity();p.parent=parent;s.attachment_index=0xffffffffU;s.attachment_mode=3U<<16;ok(s,p);
    s.attachment_index=257;error(s,p,jo::ErrorCode::unknown_getter);parent.getter=jo::Getter::unsupported;p.parent=parent;error(s,p,jo::ErrorCode::unsupported_getter);
    parent.getter=jo::Getter::uplayer_d02258;p.parent=parent;error(s,p,jo::ErrorCode::unknown_mapping);
    parent.mapping=jo::Null{};p.parent=parent;error(s,p,jo::ErrorCode::null_mapping);
    std::array<std::uint8_t,2> mapping={0,0xff};parent.mapping=std::span<const std::uint8_t>(mapping).first(1);p.parent=parent;error(s,p,jo::ErrorCode::mapping_extent);
    parent.mapping=std::span<const std::uint8_t>(mapping);p.parent=parent;ok(s,p); // low8(257)==1, ff bypasses joints pointer.
    mapping[1]=0;p.parent=parent;error(s,p,jo::ErrorCode::unknown_parent_joints);parent.joints=jo::Null{};p.parent=parent;error(s,p,jo::ErrorCode::null_parent_joints);
    std::array<jo::Matrix,1> matrices={identity()};parent.joints=std::span<const jo::Matrix>(matrices).first(0);p.parent=parent;error(s,p,jo::ErrorCode::parent_extent);
    parent.joints=std::span<const jo::Matrix>(matrices);p.parent=parent;ok(s,p);
    error(s,p,jo::ErrorCode::resource_limit,{256,256,256,1});error(s,p,jo::ErrorCode::resource_limit,{256,256,0,256});
    s=state();s.local_rt[0]=0x7f800000;p=params();error(s,p,jo::ErrorCode::nonfinite_operand);
    s=state();s.local_rt[0]=0x7f7fffff;s.scale[0]=b(2);error(s,p,jo::ErrorCode::numeric_overflow);
}
void math_boundaries(){
    const jp::Bits4 q={0,0,0,b(1)};const jp::Bits3 t={0x7fc12345,0x7f800000,0xff800000};
    const auto rt=mt::rt_matrix(q,t);require(std::holds_alternative<mt::Matrix>(rt),"RT copies nonfinite translation");
    for(unsigned i=0;i<3;++i)require(std::get<mt::Matrix>(rt)[12+i]==t[i],"raw RT translation bits");
    auto invalid=q;invalid[0]=0x7fc00001;require(std::holds_alternative<mt::Error>(mt::rt_matrix(invalid,t)),"RT consumes Q");
    auto matrix=identity();matrix[0]=0x7fc12345;
    require(std::get<mt::Matrix>(mt::attachment_basis(matrix,3,std::nullopt))==matrix,"unused mode copies raw");
    require(std::get<mt::Error>(mt::attachment_basis(matrix,0,std::nullopt)).code==mt::ErrorCode::missing_template,"basis must be explicit");
    matrix=identity();matrix[0]=0x33800000;matrix[5]=0x34000000;matrix[10]=0x34800000;
    auto normalized=std::get<mt::Matrix>(mt::attachment_basis(matrix,1,std::nullopt));
    require(normalized[0]==0x33800000&&normalized[5]==b(1)&&normalized[10]==b(1),"source threshold inclusive");
    auto diagonal=std::get<mt::Matrix>(mt::attachment_basis(matrix,2,std::nullopt));require(diagonal==matrix,"diagonal scale norm mode");
    const int old=std::fegetround();require(std::fesetround(FE_DOWNWARD)==0,"set rounding for rejection");
    const auto result=mt::rt_matrix(q,{0,0,0});std::fesetround(old);
    require(std::get<mt::Error>(result).code==mt::ErrorCode::floating_environment,"non-nearest rounding rejected");
#if defined(__aarch64__)
    std::uint64_t fpcr;asm volatile("mrs %0, fpcr":"=r"(fpcr));const auto altered=fpcr|(1ULL<<24);asm volatile("msr fpcr, %0"::"r"(altered));
    const auto fz=mt::rt_matrix(q,{0,0,0});asm volatile("msr fpcr, %0"::"r"(fpcr));
    require(std::get<mt::Error>(fz).code==mt::ErrorCode::floating_environment,"actual FZ rejected");
#elif defined(__SSE__)
    const auto csr=_mm_getcsr();_mm_setcsr(csr|0x8040U);const auto fz=mt::rt_matrix(q,{0,0,0});_mm_setcsr(csr);
    require(std::get<mt::Error>(fz).code==mt::ErrorCode::floating_environment,"actual FTZ/DAZ rejected");
#endif
}
}
int main(){try{boundary();math_boundaries();std::cout<<"joint-output boundaries: "<<checks<<" checks passed\n";return 0;}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
