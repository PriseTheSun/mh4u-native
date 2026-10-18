#include "joint_output.hpp"
#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>

namespace mh4u::joint_output {
namespace {
namespace math=motion_transform_math;
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view message,std::size_t j=0){throw Failure{{code,message,j}};}
template<class T>T checked(math::Result<T> result,std::size_t joint=0){
    if(auto* value=std::get_if<T>(&result))return *value;
    const auto error=std::get<math::Error>(result);ErrorCode code=ErrorCode::numeric_domain;
    switch(error.code){
    case math::ErrorCode::floating_environment:code=ErrorCode::floating_environment;break;
    case math::ErrorCode::nonfinite_operand:code=ErrorCode::nonfinite_operand;break;
    case math::ErrorCode::numeric_overflow:code=ErrorCode::numeric_overflow;break;
    case math::ErrorCode::missing_template:code=ErrorCode::missing_template;break;
    case math::ErrorCode::degenerate_quaternion:break;
    }
    fail(code,error.message,joint);
}
Bits3 xyz(const Bits4& value){return {value[0],value[1],value[2]};}
bool ieee_equal(std::uint32_t a,std::uint32_t b){
    const auto aa=a&0x7fffffffU,bb=b&0x7fffffffU;
    if(aa>0x7f800000U||bb>0x7f800000U)return false;
    return a==b||(aa==0&&bb==0);
}
const Matrix& parent_matrix(const Parent& parent,std::uint32_t index,const Limits& limits){
    if(index&0x80000000U)return parent.world;
    if(parent.getter==Getter::unknown)fail(ErrorCode::unknown_getter,"Parent virtual getter is unknown");
    if(parent.getter!=Getter::uplayer_d02258)fail(ErrorCode::unsupported_getter,"Only source uPlayer getter d02258 is supported");
    if(std::holds_alternative<Unknown>(parent.mapping))fail(ErrorCode::unknown_mapping,"Parent mapping pointer is unknown");
    if(std::holds_alternative<Null>(parent.mapping))fail(ErrorCode::null_mapping,"Parent mapping pointer is null");
    const auto map=std::get<std::span<const std::uint8_t>>(parent.mapping);
    if(map.size()>limits.mapping)fail(ErrorCode::resource_limit,"Parent mapping exceeds caller limit");
    const auto reduced=index&0xffU;
    if(reduced>=map.size())fail(ErrorCode::mapping_extent,"Parent mapping does not cover reduced index");
    const auto mapped=map[reduced];if(mapped==0xff)return parent.world;
    if(std::holds_alternative<Unknown>(parent.joints))fail(ErrorCode::unknown_parent_joints,"Parent joint pointer is unknown");
    if(std::holds_alternative<Null>(parent.joints))fail(ErrorCode::null_parent_joints,"Parent joint pointer is null");
    const auto joints=std::get<std::span<const Matrix>>(parent.joints);
    if(joints.size()>limits.parent_joints)fail(ErrorCode::resource_limit,"Parent joint storage exceeds caller limit");
    if(mapped>=joints.size())fail(ErrorCode::parent_extent,"Parent joint index exceeds storage");
    return joints[mapped];
}
void null_callback(Callback callback,std::size_t index){
    if(callback==Callback::unknown)fail(ErrorCode::unknown_callback,"Consumed joint callback pointer is unknown",index);
    if(callback!=Callback::null)fail(ErrorCode::unsupported_callback,"Joint callback is outside the recovered profile",index);
}
void rebuild(State& state){
    const auto count=state.joints.size();
    if(count>256)fail(ErrorCode::resource_limit,"Dirty rebuild supports at most 256 representable order indices");
    for(auto& joint:state.joints){joint.depth=0xffffffffU;joint.metadata.mapping98&=~0x30000U;}
    std::vector<std::size_t> path;path.reserve(count);
    for(std::size_t start=0;start<count;++start){
        std::size_t current=start;
        while(state.joints[current].depth==0xffffffffU){
            auto& joint=state.joints[current];
            if(joint.metadata.mapping98&0x20000U)fail(ErrorCode::cycle,"Same-owner joint hierarchy contains a cycle",current);
            joint.metadata.mapping98|=0x20000U;
            const auto parent=(joint.metadata.flags94>>8)&0xffU;
            if(parent==0xff){joint.depth=0;joint.metadata.mapping98&=~0x20000U;break;}
            if(joint.owner==Owner::unknown)fail(ErrorCode::unknown_owner,"Non-root joint owner pointer is unknown",current);
            if(joint.owner==Owner::null)fail(ErrorCode::null_owner,"Non-root joint owner pointer is null",current);
            if(joint.owner!=Owner::self)fail(ErrorCode::foreign_owner,"Dirty rebuild requires the resolved same owner",current);
            if(parent>=count)fail(ErrorCode::parent_extent,"Dirty joint parent exceeds canonical storage",current);
            path.push_back(current);current=parent;
        }
        while(!path.empty()){
            const auto child=path.back();path.pop_back();auto& joint=state.joints[child];
            null_callback(joint.callback,child);
            const auto parent=(joint.metadata.flags94>>8)&0xffU;
            joint.depth=state.joints[parent].depth+1U;
            joint.metadata.mapping98&=~0x20000U;
        }
    }
    if(count){
        if(std::holds_alternative<Unknown>(state.order))fail(ErrorCode::unknown_order,"Rebuilt order storage is unknown");
        if(std::holds_alternative<Null>(state.order))fail(ErrorCode::null_order,"Rebuilt order storage is null");
        auto& order=std::get<std::vector<std::uint8_t>>(state.order);
        if(order.size()<count)fail(ErrorCode::order_extent,"Rebuilt order storage is too small");
        for(std::size_t i=0;i<count;++i)order[i]=static_cast<std::uint8_t>(i);
        // Source 95b39c exact pair swaps. A stable sort changes ties after swaps.
        for(std::size_t i=0;i<count;++i)for(std::size_t j=i+1;j<count;++j)
            if(state.joints[order[i]].depth>state.joints[order[j]].depth)std::swap(order[i],order[j]);
    }
    state.dirty=0;
}
SelectedCallback select(State& state){
    const auto flags=state.flags238;
    if(flags&0x08000000U)return SelectedCallback::null;
    if(flags&0x04000000U)return SelectedCallback::unsupported_95a7bc;
    if(!(flags&0x40U))return SelectedCallback::normal_9608d8;
    state.flags104|=0x40U;
    if(flags&0x100000U)return SelectedCallback::normal_9608d8;
    return flags&0x4000U?SelectedCallback::unsupported_95ba60:SelectedCallback::unsupported_95c24c;
}
}
Result apply(const State& source,const Parameters& parameters,Limits limits){
    try{
        if(source.joints.size()>limits.joints||source.joints.size()>std::numeric_limits<std::uint32_t>::max())
            fail(ErrorCode::resource_limit,"Canonical joint storage exceeds caller/source count limits");
        if(const auto* order=std::get_if<std::vector<std::uint8_t>>(&source.order);order&&order->size()>limits.order)
            fail(ErrorCode::resource_limit,"Order storage exceeds caller limit");
        Output out{source,false,std::nullopt,0};auto& state=out.state;
        if(std::holds_alternative<Unknown>(parameters.parent))fail(ErrorCode::unknown_parent,"Attachment pointer is unknown");
        const auto* parent=std::get_if<Parent>(&parameters.parent);
        if(!parent||((parent->flags4&7U)!=1U&&(parent->flags4&7U)!=2U)){
            out.attachment_cleared=true;state.world=state.local_rt;
        }else{
            state.world=checked(math::matrix_product(state.local_rt,parent_matrix(*parent,state.attachment_index,limits)));
            state.world=checked(math::attachment_basis(state.world,state.attachment_mode>>16,parameters.reference_basis));
        }
        state.world=checked(math::scale_rows(state.world,state.scale));
        if(!parameters.reference_basis||!parameters.reference_translation)
            fail(ErrorCode::missing_template,"World comparison templates are unknown");
        bool same=true;for(std::size_t i=0;i<12;++i)same&=ieee_equal(state.world[i],(*parameters.reference_basis)[i]);
        state.flags104=same?state.flags104&~0xc0U:state.flags104|0xc0U;
        same=true;for(std::size_t i=0;i<4;++i)same&=ieee_equal(state.world[i+12],(*parameters.reference_translation)[i]);
        state.flags104=same?state.flags104&~0x100U:state.flags104|0x100U;
        if(std::holds_alternative<Unknown>(parameters.model))fail(ErrorCode::unknown_model,"rModel pointer presence is unknown");
        if(std::holds_alternative<joint_pose::PresentModel>(parameters.model)){
            if(state.dirty)rebuild(state);
            out.selected=select(state);
            const auto count=state.joints.size();
            const std::vector<std::uint8_t>* order=nullptr;
            if(count){
                if(std::holds_alternative<Unknown>(state.order))fail(ErrorCode::unknown_order,"Traversal order pointer is unknown");
                if(std::holds_alternative<Null>(state.order))fail(ErrorCode::null_order,"Traversal order pointer is null");
                order=&std::get<std::vector<std::uint8_t>>(state.order);
                if(order->size()<count)fail(ErrorCode::order_extent,"Traversal order storage is too small");
            }
            for(std::size_t i=0;i<count;++i){
                const auto index=(*order)[i];
                if(index>=count)fail(ErrorCode::order_extent,"Traversal index exceeds canonical joints",index);
                auto& joint=state.joints[index];if(joint.metadata.flags94&0x20U)continue;
                if(*out.selected==SelectedCallback::normal_9608d8){
                    auto local=checked(math::rt_matrix(joint.pose.rotation,xyz(joint.pose.translation)),index);
                    local=checked(math::scale_rows(local,xyz(joint.pose.scale)),index);
                    const auto parent_index=(joint.metadata.flags94>>8)&0xffU;
                    if(parent_index!=0xff&&parent_index>=count)fail(ErrorCode::parent_extent,"Output parent exceeds canonical storage",index);
                    const auto& base=parent_index==0xff?state.world:state.joints[parent_index].matrix;
                    joint.matrix=checked(math::matrix_product(local,base),index);++out.updated_joints;
                }else if(*out.selected!=SelectedCallback::null)
                    fail(ErrorCode::unsupported_selected_callback,"Selected joint transformation callback is outside profile",index);
                if(state.flags238&0x01000000U)null_callback(joint.callback,index);
            }
            state.flags230|=1U;
        }
        if((state.flags4&7U)!=2U||((~((state.flags4&0xffffU)>>10))&0x12U)!=0)state.fieldc4c=2;
        return out;
    }catch(const Failure& failure){return failure.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Joint output allocation failed"};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Joint output capacity exceeded"};}
}
} // namespace mh4u::joint_output
