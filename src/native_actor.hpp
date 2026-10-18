#pragma once
#include "scene.hpp"
#include "model_animation_frame.hpp"
#include "motion_command.hpp"
#include <filesystem>
#include <memory>
#include <span>
#include <variant>

namespace mh4u::native_actor {
class Player;
enum class ErrorCode {io,recipe,identity,resource,hierarchy,binding,pose,palette,vertex,capacity,time};
struct Error {ErrorCode code;std::string message;std::size_t part{},primitive{},vertex{};};
template<class T>using Result=std::variant<T,Error>;
struct Frame {
    SceneFrame scene;
    float source_frame{};
    std::size_t samples{},palettes{};
    std::uint64_t tick{};
    std::array<std::uint32_t,4> event_masks{};
    bool ended{};
};
// Native MOD/LMT playback through9573b8,95b490 and95b940. Inspection
// pacing (one source time unit per tick), initial runtime flags, hierarchy,
// startup templates and camera remain explicit policies, not recovered bootstrap.
// Seeking replays bounded ticks; LMT loop/clamp semantics belong to the clock.
// Geometry and raw skin weights never pass through glTF conversion.
class Actor {
public:
    static Result<Actor> load(const std::filesystem::path& recipe);
    std::shared_ptr<const Scene> presentation()const noexcept;
    std::size_t joint_count()const noexcept;
    std::size_t part_count()const noexcept;
    float inspection_rate()const noexcept;
    bool supports_transitions()const noexcept;
    // Canonical local files actually consumed by the validated loader, including
    // the recipe. Immutable alongside the resources; no second recipe parser.
    std::span<const std::filesystem::path> source_files()const noexcept;
    Result<Frame> evaluate(int clip,float seconds,bool loop);
    Result<Frame> seek(int clip,float seconds,bool loop);
    // Executes the original command on the current canonical pose/channels.
    // The caller chooses the action and blend duration; no input policy is inferred.
    Result<Frame> change_action(int clip,bool loop,float interpolation=4);
private:
    friend class Player;
    struct Data;
    explicit Actor(std::shared_ptr<const Data>);
    std::shared_ptr<const Data> data_;
    Result<Frame> evaluate_impl(int clip,float seconds,bool loop,bool change,float interpolation,bool reset=false);
    int current_clip_=-2;
    bool current_loop_{};
    std::uint64_t next_tick_{};
    model_animation_frame::State current_{};
    motion_command::Fields command_fields_{};
    joint_pose::Matrix current_local_rt_{};
    std::size_t current_samples_{};
    std::uint64_t pose_revision_{};
};
}
