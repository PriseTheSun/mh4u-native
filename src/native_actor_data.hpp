#pragma once
// Internal immutable resource owner shared by the legacy inspector and Player.
#include "native_actor.hpp"
#include "joint_output.hpp"
#include "model_resource.hpp"
#include "motion_resource.hpp"

namespace mh4u::native_actor {
struct Actor::Data {
    struct Part {model_resource::Resource model;std::vector<std::uint8_t> joint_map;};
    struct Draw {std::size_t part;model_resource::Geometry geometry;std::uint8_t palette;int material;};
    struct Clip {std::size_t motion;unsigned slot;};
    // Synchronous view of either canonical output owner; no pose extraction.
    struct Matrices {
        const void* data{};std::size_t count{};
        const joint_pose::Matrix& (*at)(const void*,std::size_t){};
        template<class Joint>explicit Matrices(std::span<const Joint> source):data(source.data()),count(source.size()),
            at([](const void* pointer,std::size_t index)->const joint_pose::Matrix&{return static_cast<const Joint*>(pointer)[index].matrix;}){}
        const joint_pose::Matrix& operator[](std::size_t index)const{return at(data,index);}
        std::size_t size()const noexcept{return count;}
    };
    std::vector<Part> parts;
    std::vector<motion_resource::Resource> motions;
    std::vector<Draw> draws;
    std::vector<Clip> clips;
    std::vector<std::uint8_t> bone_map;
    joint_output::State rest;
    std::shared_ptr<const Scene> scene;
    std::vector<std::filesystem::path> source_files;
    float rate=60;
    std::optional<motion_command::Globals> command_globals;
    Result<Frame> render(Frame,Matrices,std::uint64_t pose_revision)const;
};
}
