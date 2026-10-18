#pragma once
#include "collision.hpp"
#include <filesystem>

namespace mh4u {
// A local, explicit diagnostic scene recipe. Values are authored inspection
// parameters, not recovered game defaults or an original spawn definition.
struct WorldConfig {
    std::filesystem::path stage,actor,floor,walls;
    collision::Point spawn;
    double heading{},radius{},height{},speed{},min_normal_y{},actor_ground_offset{};
    bool operator==(const WorldConfig&) const;
};
WorldConfig load_world_config(const std::filesystem::path&);
}
