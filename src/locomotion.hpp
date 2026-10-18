#pragma once
#include "collision.hpp"
#include <array>
#include <cstddef>
#include <vector>

namespace mh4u::locomotion {
// Diagnostic geometry, not recovered MH4U movement constants or physics.
struct Parameters {
    double radius{};
    double height{};
    double slope_min_y{0.8};
    double max_step_height{}; // This continuous-heightfield profile requires zero.
    double skin{0.001};
    double max_displacement{4096}; // Reject, rather than silently truncate, longer input.
    std::size_t max_contacts{8}; // 1..16; unconsumed motion is discarded safely.
};
struct State { collision::Point position; double yaw{}; };
struct Input { double dx{},dz{},yaw_delta{}; };
enum class BarrierKind { floor_boundary,wall };
struct Contact {
    BarrierKind kind{};
    // Original floor triangle or first triangle of the wall rectangle.
    std::size_t triangle_id{};
    collision::TriangleSource source;
    collision::Point normal; // Horizontal normal pointing away from the barrier.
};
struct StepResult {
    State state;
    double applied_dx{},applied_dz{};
    bool blocked{},contact_limit_reached{};
    std::array<Contact,16> contacts{};
    std::size_t contact_count{},segment_tests{};
    std::size_t support_triangle{};
};
struct BuildStats { std::size_t floor_triangles{},wall_triangles{},floor_boundaries{},wall_segments{}; };

class World {
public:
    // Owns both immutable evaluated worlds. Accepts an upward, continuous,
    // nonoverlapping heightfield and full-height vertical rectangle walls only.
    // Its swept footprint is a horizontal disk; height verifies wall coverage.
    // There are no ceilings, jumping, stairs, dynamics or general 3D capsules.
    World(collision::World floor,collision::World walls,Parameters);
    State create_state(double x,double z,double yaw=0) const;
    StepResult step(const State&,Input) const;
    const Parameters& parameters() const { return parameters_; }
    const BuildStats& stats() const { return stats_; }
private:
    struct Segment { collision::Point a,b; Contact contact; };
    collision::World floor_,walls_;
    Parameters parameters_;
    BuildStats stats_;
    std::vector<Segment> segments_;
    double floor_min_y_{},floor_max_y_{};
    collision::Hit support(double x,double z) const;
    void validate_clearance(double x,double z) const;
};
}
