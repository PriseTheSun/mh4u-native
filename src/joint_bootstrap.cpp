#include "joint_bootstrap.hpp"
#include <numeric>
namespace mh4u::joint_bootstrap {
Result initialize(std::span<const model_resource::JointRecord> records,
                  std::span<const joint_pose::Matrix> matrices,
                  std::span<const std::uint32_t> prior,
                  const std::optional<joint_pose::Bits3>& scale,Limits limits) {
    const auto n=records.size();
    if(n>limits.joints||n>255)return Error{ErrorCode::resource_limit,"Joint count exceeds bounded source profile",n,{}};
    if(matrices.size()!=n||prior.size()!=n)return Error{ErrorCode::extent,"Records, matrices and prior metadata extents differ",0,{}};
    if(n&&!scale)return Error{ErrorCode::missing_scale,"Scale template10e6380 is not known",0,{}};
    Output out;out.depth90.resize(n);std::vector<std::uint8_t> children(n);
    for(std::size_t i=0;i<n;++i){
        const auto parent=records[i].fields[1];
        if(parent!=255){
            if(parent>=n)return Error{ErrorCode::parent_extent,"Parent lies outside joint storage",i,{}};
            ++children[parent];
        }
        std::size_t index=i,depth=0;
        while(records[index].fields[1]!=255){
            index=records[index].fields[1];
            if(index>=n)return Error{ErrorCode::parent_extent,"Ancestor lies outside joint storage",i,{}};
            if(++depth>=n)return Error{ErrorCode::parent_cycle,"Parent graph contains a cycle",i,{}};
        }
        out.depth90[i]=static_cast<std::uint32_t>(depth);
    }
    out.joints.reserve(n);out.metadata.reserve(n);out.parameter8c.reserve(n);
    out.matrices.assign(matrices.begin(),matrices.end());out.order.resize(n);
    for(std::size_t i=0;i<n;++i){
        auto q=joint_pose::matrix_rotation(matrices[i]);
        if(auto* error=std::get_if<joint_pose::Error>(&q))return Error{ErrorCode::matrix,"Original matrix rotation outside supported arithmetic domain",i,*error};
        const auto& r=records[i];joint_pose::Joint j;
        j.reference_translation={r.parameter_bits[2],r.parameter_bits[3],r.parameter_bits[4]};
        j.translation={j.reference_translation[0],j.reference_translation[1],j.reference_translation[2],0};
        j.rotation=std::get<joint_pose::Bits4>(q);j.scale={(*scale)[0],(*scale)[1],(*scale)[2],0};
        out.joints.push_back(j);
        out.metadata.push_back({std::uint32_t(r.fields[0])<<24|std::uint32_t(r.fields[1])<<8,
            (prior[i]&0xfffe0000U)|std::uint32_t(children[i])<<8|r.fields[2]});
        out.parameter8c.push_back(r.parameter_bits[1]);out.order[i]=static_cast<std::uint8_t>(i);
    }
    // Match source swap order exactly. Equal-depth entries may move indirectly;
    // a stable_sort would produce a different order for some input forests.
    for(std::size_t i=0;i<n;++i)for(std::size_t j=i+1;j<n;++j)
        if(out.depth90[out.order[i]]>out.depth90[out.order[j]])std::swap(out.order[i],out.order[j]);
    return out;
}
}
