#include "action_scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace mh4u {
namespace {
model_motion::Package verified(const std::filesystem::path& binary,
                              const model_motion::ExpectedPackage& expected,const std::filesystem::path& root) {
    auto loaded=model_motion::Package::load(binary,expected,root);
    if(const auto* error=std::get_if<model_motion::Error>(&loaded))
        throw std::runtime_error("Motion package: "+std::string(model_motion::error_name(error->code))+": "+error->message);
    return std::move(std::get<model_motion::Package>(loaded));
}
}
ActionScene::ActionScene(const std::filesystem::path& binary,const std::filesystem::path& root,
                        const model_motion::ExpectedPackage& expected,std::size_t index,bool repeat,
                        const std::optional<ActionEventSource>& events):
    binary_(binary),root_(root),expected_(expected),package_(verified(binary,expected,root)),
    policy_{0,1,repeat,{{0,0,0},{0,0,0,1},{1,1,1},{1,1,1}}} {
    if(index>=package_.actions().size())throw std::runtime_error("Motion action outside package");
    if(events) {
        auto loaded=motion_events::Package::load(events->binary,events->expected);
        if(const auto* error=std::get_if<motion_events::Error>(&loaded))
            throw std::runtime_error("Motion events: "+std::string(motion_events::error_name(error->code))+": "+error->message);
        events_.emplace(std::move(std::get<motion_events::Package>(loaded)));
    }
    identity_=package_.actions()[index].identity;
    const auto& relative=package_.gltf_binding().relative_path;
    model_path_=root/std::filesystem::path(std::u8string(relative.begin(),relative.end()));
    reset();
}
void ActionScene::reset() {
    // Construct first: failure cannot leave a half-reset player.
    auto candidate=std::make_unique<action_motion::Player>(package_,identity_,policy_,events_?&*events_:nullptr);
    player_=std::move(candidate);ticks_=0;
}
void ActionScene::tick() {
    if(ticks_==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("Motion tick counter exhausted");
    player_->tick();++ticks_;
}
void ActionScene::verify_loaded_scene(const Scene& scene) const {
    (void)verified(binary_,expected_,root_);
    if(scene.animations.size()!=package_.actions().size())throw std::runtime_error("Paired clip count mismatch");
    for(const auto& a:package_.actions())
        if(a.gltf_clip_index>=scene.animations.size()||scene.animations[a.gltf_clip_index].duration!=float(a.frame_count-1))
            throw std::runtime_error("Paired source-frame timeline mismatch");
}
void ActionScene::evaluate(const Scene& scene,SceneFrame& frame) const {
    // The Player clock has already advanced; only its explicitly returned sample
    // frame identifies the joint pose belonging to the current model transform.
    evaluate_scene(scene,static_cast<int>(action().gltf_clip_index),current().joint_source_frame,false,frame);
}
Matrix ActionScene::model_matrix() const {
    const auto& model=current().state.pre.model;
    // The motion owner supplies the validated raw-Q convention. Here only
    // scale and row-major -> column-major presentation layout are composed.
    const auto rotation=root_motion::rotation_basis_raw(model.rotation);
    Matrix result=identity_matrix();
    for(std::size_t column=0;column<3;++column)for(std::size_t row=0;row<3;++row)
        result[column*4+row]=static_cast<float>(rotation[row][column]*model.scale[column]);
    for(std::size_t axis=0;axis<3;++axis)result[12+axis]=model.position[axis];
    for(float value:result)if(!std::isfinite(value))throw std::runtime_error("Nonfinite inspection model transform");
    // null_scale belongs to source motion application. Player already consumes
    // it there; applying it again to rendered vertices would double interpret it.
    return result;
}
Mesh ActionScene::transformed_bounds(const SceneFrame& frame) const {
    if(frame.mesh.triangles.empty())throw std::runtime_error("Empty motion geometry");
    const auto matrix=model_matrix();
    Mesh bounds;bounds.minimum=bounds.maximum=transform_point(matrix,frame.mesh.triangles.front().position);
    for(const auto& vertex:frame.mesh.triangles) {
        const auto point=transform_point(matrix,vertex.position);
        bounds.minimum={std::min(bounds.minimum.x,point.x),std::min(bounds.minimum.y,point.y),std::min(bounds.minimum.z,point.z)};
        bounds.maximum={std::max(bounds.maximum.x,point.x),std::max(bounds.maximum.y,point.y),std::max(bounds.maximum.z,point.z)};
    }
    bounds.center={(bounds.minimum.x+bounds.maximum.x)/2,(bounds.minimum.y+bounds.maximum.y)/2,(bounds.minimum.z+bounds.maximum.z)/2};
    bounds.radius=std::hypot(bounds.maximum.x-bounds.center.x,bounds.maximum.y-bounds.center.y,bounds.maximum.z-bounds.center.z);
    if(!std::isfinite(bounds.radius)||bounds.radius<=0)throw std::runtime_error("Degenerate motion bounds");
    return bounds;
}
} // namespace mh4u
