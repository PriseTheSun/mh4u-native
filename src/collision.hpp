#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <vector>

namespace mh4u::collision {
struct Point { double x{},y{},z{}; };
struct TriangleSource {
    std::size_t instance{},node{},primitive{},primitive_triangle{};
};
struct Triangle { std::array<Point,3> points; TriangleSource source; };
enum class Backfaces { include,cull };
struct Hit {
    // Stable ordinal in the evaluated scene, never the BVH's reordered index.
    std::size_t triangle_id{};
    TriangleSource source;
    double distance{};
    Point position,normal;
    std::array<double,3> barycentric{};
    bool front_face{};
};
struct QueryStats { std::size_t boxes_tested{},triangles_tested{}; };
struct QueryResult { std::optional<Hit> hit; QueryStats stats; };
struct BuildStats { std::size_t triangles{},degenerate_triangles{},bvh_nodes{},estimated_storage_bytes{}; };

class World {
public:
    // Owns input geometry. Exact zero-area triangles retain their IDs but are
    // excluded from the BVH. The budget bounds kernel storage, not file loading.
    explicit World(std::vector<Triangle>,std::size_t storage_budget=256U*1024U*1024U);
    QueryResult raycast(Point origin,Point direction,double max_distance,Backfaces=Backfaces::include) const;
    QueryResult segmentcast(Point start,Point end,Backfaces=Backfaces::include) const;
    const BuildStats& stats() const { return stats_; }
    const std::vector<Triangle>& triangles() const { return triangles_; }
private:
    struct Bounds { Point minimum,maximum; };
    struct Geometry { Bounds bounds; Point normal,centroid; };
    struct Node { Bounds bounds; std::size_t first{},count{},left{},right{}; };
    std::vector<Triangle> triangles_;
    std::vector<Geometry> geometry_;
    std::vector<std::size_t> order_;
    std::vector<Node> nodes_;
    BuildStats stats_;
    std::size_t build_node(std::size_t first,std::size_t count);
    bool intersects_box(const Bounds&,Point origin,Point direction,double limit) const;
    std::optional<Hit> intersects_triangle(std::size_t,Point origin,Point direction,double limit,Backfaces) const;
};

// Uses the existing glTF owner and scene evaluator. Refuses animated/skinned
// scenes rather than silently freezing a pose. Node transforms are applied once.
World load_static_gltf(const std::filesystem::path&);
}
