#pragma once
#include "motion_application.hpp"
#include "motion_clock.hpp"
#include "motion_pending.hpp"
#include "joint_update.hpp"
#include <optional>
#include <span>
#include <variant>

namespace mh4u::model_animation_frame {
struct State {
    motion_pending::State animation; // Sole mutable owner of layers, poses and transition snapshots.
    motion_application::Model model;
};
struct Parameters {
    motion_pending::Parameters pending; // Includes the sole model+14 delta and explicit templates.
    std::span<const joint_update::Metadata> metadata;
};
struct Resolvers {
    motion_pending::ActionResolver actions;
    motion_initialization::Resolver tracks;
    motion_clock::EventResolver events;
};
struct Limits {
    std::size_t layers=8,joints=256,snapshots=256,bindings_per_layer=256;
    motion_pending::Limits pending;
    motion_application::Limits application;
    joint_update::Limits joint_update;
    motion_clock::Limits clock;
};
enum class ErrorCode {resource_limit,unknown_model,pending,preparation,application,restoration,joint_update,clock};
struct Error {
    ErrorCode code;std::string_view message;
    std::optional<std::size_t> layer{};
    std::optional<motion_pending::Error> pending{};
    std::optional<motion_transform_math::Error> preparation{};
    std::optional<motion_application::Error> application{};
    std::optional<joint_pose::Error> restoration{};
    std::optional<joint_update::Error> joint_update{};
    std::optional<motion_clock::Error> clock{};
};
struct Output {
    State state;
    bool pending_found{},restored_joints{};
    std::optional<joint_update::Leaf> joint_leaf{};
    std::size_t initializer_calls{},prepared_layers{},model_samples{},joint_samples{},loop_iterations{},event_queries{};
};
using Result=std::variant<Output,Error>;
// Complete numeric orchestration9573b8 and95b790: pending -> attribute merge ->
// prepared frames -> model application -> joint pose -> clock. Canonical owners
// implement the called functions. Owns no second sampler, parser or timer.
// Local owned snapshot vectors are explicit present tables; source counts are
// their declared active extents. Model presence remains explicit. Parent/world
// matrix construction and95b490/95b940 are subsequent caller stages, not hidden
// here. Finite preparation arithmetic; raw unconsumed state remains unchanged.
// Errors publish no partial state. Resolver side effects remain caller-owned.
[[nodiscard]] Result step(const State&,std::span<const std::uint8_t> bone_to_joint,
    const Parameters&,Resolvers,Limits={});
}
