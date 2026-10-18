#include "motion_bank.hpp"
namespace mh4u::motion_bank {
Result replace(const motion_request::Model& model,std::span<const motion_pending::Layer> layers,
    std::size_t active_count,std::uint32_t bank,const motion_request::Bank& replacement,Limits limits){
    if(bank>=model.banks.size())return Error{ErrorCode::bank_extent,"Bank index exceeds the sixteen selector banks"};
    if(active_count>layers.size())return Error{ErrorCode::active_layer_extent,"Active count exceeds supplied layer storage"};
    if(layers.size()>limits.layers)return Error{ErrorCode::resource_limit,"Layer copy limit"};
    for(const auto& layer:layers)if(layer.animation.tracks.joints.size()>limits.joints_per_layer)
        return Error{ErrorCode::resource_limit,"Joint binding copy limit"};
    if(std::holds_alternative<motion_request::Unknown>(replacement))
        return Error{ErrorCode::unknown_replacement,"Replacement pointer presence is unknown"};
    if(const auto* resource=std::get_if<motion_request::Resource>(&replacement);resource&&!resource->identity)
        return Error{ErrorCode::resource_identity,"Replacement requires a stable nonzero identity"};
    Output out{model,{layers.begin(),layers.end()}};
    out.model.banks[bank]=replacement;
    for(std::size_t i=0;i<active_count;++i){
        auto& layer=out.layers[i].animation.layer;
        if(((layer.selector>>8)&15U)==bank)layer.flags=2;
    }
    return out;
}
} // namespace mh4u::motion_bank
