#pragma once
#include "assets.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mh4u {
using Matrix = std::array<float,16>; // Column-major, column vectors, glTF right-handed coordinates.
using Quaternion = std::array<float,4>; // x,y,z,w.
Matrix identity_matrix();
Matrix matrix_product(const Matrix&,const Matrix&);
Vec3 transform_point(const Matrix&,Vec3);
Vec3 transform_normal(const Matrix&,Vec3);
Quaternion normalized_quaternion(Quaternion);
Quaternion quaternion_slerp(Quaternion,Quaternion,float);
struct Transform { Vec3 translation{}; Quaternion rotation{0,0,0,1}; Vec3 scale{1,1,1}; };
Matrix transform_matrix(const Transform&);
struct SceneNode {
    std::string name;
    int parent=-1;
    Transform rest;
    bool has_matrix=false;
    Matrix matrix=identity_matrix();
};
struct Skin { std::vector<std::size_t> joints; std::vector<Matrix> inverse_bind; };
struct SkinVertex {
    Vertex vertex;
    std::array<std::uint32_t,4> joints{};
    std::array<float,4> weights{};
};
std::array<float,4> normalized_weights(std::array<float,4>);
struct Primitive {
    std::vector<SkinVertex> vertices;
    std::vector<std::uint32_t> indices;
    int material=-1;
    bool skinned=false;
};
struct MeshInstance { std::size_t primitive{},node{}; int skin=-1; };
enum class AlphaMode { opaque,mask,blend };
enum class WrapMode { repeat,clamp,mirror };
struct Material {
    std::string name;
    int image=-1;
    std::array<float,4> color{1,1,1,1};
    AlphaMode alpha=AlphaMode::opaque;
    float alpha_cutoff=0.5f;
    bool double_sided=false;
    WrapMode wrap_s=WrapMode::repeat,wrap_t=WrapMode::repeat;
    bool min_linear=true,mag_linear=true;
};
enum class ChannelPath { translation,rotation,scale };
enum class Interpolation { linear,step,cubic_spline };
struct AnimationChannel {
    std::size_t node{};
    ChannelPath path{};
    Interpolation interpolation=Interpolation::linear;
    std::vector<float> times;
    std::vector<std::array<float,4>> values;
    // Derivatives per second, present only for glTF CUBICSPLINE.
    std::vector<std::array<float,4>> in_tangents,out_tangents;
};
struct Animation { std::string name; std::vector<AnimationChannel> channels; float duration{}; };
struct SceneImage { Image pixels; std::string encoded_sha256; };
struct Scene {
    std::vector<SceneNode> nodes;
    std::vector<std::size_t> hierarchy_order;
    std::vector<Skin> skins;
    std::vector<Primitive> primitives;
    std::vector<MeshInstance> instances;
    std::vector<Material> materials;
    std::vector<SceneImage> images;
    std::vector<Animation> animations;
    std::vector<std::string> notes;
};
struct DrawRange { std::size_t first{},count{}; int material=-1; std::size_t primitive{}; };
struct SceneFrame { Mesh mesh; std::vector<DrawRange> draws; std::vector<Matrix> global_transforms; };
// Validates topology and skin indices once at the owning boundary.
void prepare_scene(Scene&);
float animation_time(float time,float duration,bool loop);
std::vector<Transform> sample_animation(const Scene&,int animation,float time,bool loop);
void evaluate_scene(const Scene&,int animation,float time,bool loop,SceneFrame&);
Scene load_gltf(const std::filesystem::path&);
}
