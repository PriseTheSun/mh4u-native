#pragma once
#include "action_motion.hpp"
#include "scene.hpp"
#include <memory>

namespace mh4u {
struct ActionEventSource {
    std::filesystem::path binary;
    motion_events::ExpectedPackage expected;
};
// Composition for inspection only. Scene/GL ownership stays with the viewer;
// this nonmovable owner keeps Package alive for its Player and controls time.
class ActionScene {
public:
    static constexpr double ticks_per_second=60; // Authored preview policy, not recovered game timing.
    static constexpr std::uint32_t max_capture_ticks=100000;
    ActionScene(const std::filesystem::path& binary,const std::filesystem::path& paired_root,
                const model_motion::ExpectedPackage&,std::size_t action,bool repeat,
                const std::optional<ActionEventSource>& events=std::nullopt);
    ActionScene(const ActionScene&)=delete;
    ActionScene& operator=(const ActionScene&)=delete;
    const std::filesystem::path& model_path() const { return model_path_; }
    const action_motion::Frame& current() const { return player_->current(); }
    const model_motion::ActionInfo& action() const { return player_->action(); }
    std::uint64_t ticks() const { return ticks_; }
    bool repeat() const { return policy_.repeat; }
    bool events_bound() const { return player_->events_bound(); }
    void tick();
    void reset();
    // Recheck the owner-validated 13-file identity after the scene loader reads it.
    void verify_loaded_scene(const Scene&) const;
    void evaluate(const Scene&,SceneFrame&) const;
    Matrix model_matrix() const;
    // Actual transformed geometry bounds, not a guessed world/collision placement.
    Mesh transformed_bounds(const SceneFrame&) const;
private:
    std::filesystem::path binary_,root_,model_path_;
    model_motion::ExpectedPackage expected_;
    model_motion::Package package_;
    std::optional<motion_events::Package> events_;
    model_motion::ActionIdentity identity_;
    action_motion::DiagnosticPolicy policy_;
    std::unique_ptr<action_motion::Player> player_;
    std::uint64_t ticks_=0;
};
} // namespace mh4u
