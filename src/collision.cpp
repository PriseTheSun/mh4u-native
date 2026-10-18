#include "collision.hpp"
#include "scene.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>

namespace mh4u::collision {
namespace {
constexpr double epsilon=64*std::numeric_limits<double>::epsilon();
constexpr std::size_t leaf_size=8;
[[noreturn]] void fail(const char* reason) { throw std::runtime_error(std::string("Collision: ")+reason); }
bool finite(Point p) { return std::isfinite(p.x)&&std::isfinite(p.y)&&std::isfinite(p.z); }
Point add(Point a,Point b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
Point subtract(Point a,Point b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
Point scale(Point a,double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(Point a,Point b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
Point cross(Point a,Point b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
double length(Point p) { return std::hypot(p.x,p.y,p.z); }
double component(Point p,int axis) { return axis==0?p.x:axis==1?p.y:p.z; }
Point minimum(Point a,Point b) { return {std::min(a.x,b.x),std::min(a.y,b.y),std::min(a.z,b.z)}; }
Point maximum(Point a,Point b) { return {std::max(a.x,b.x),std::max(a.y,b.y),std::max(a.z,b.z)}; }
Point unit(Point direction) {
    if(!finite(direction)) fail("non-finite direction");
    const double largest=std::max({std::abs(direction.x),std::abs(direction.y),std::abs(direction.z)});
    if(largest==0) fail("zero direction");
    // Scaling first accepts finite directions with very large/small magnitudes.
    direction={direction.x/largest,direction.y/largest,direction.z/largest};
    return scale(direction,1/length(direction));
}
Point point(Vec3 p) { return {p.x,p.y,p.z}; }
}

World::World(std::vector<Triangle> triangles,std::size_t storage_budget):triangles_(std::move(triangles)) {
    constexpr std::size_t per_triangle=sizeof(Triangle)+sizeof(Geometry)+sizeof(std::size_t)+2*sizeof(Node);
    if(triangles_.size()>storage_budget/per_triangle) fail("kernel storage budget exceeded");
    stats_.triangles=triangles_.size(); stats_.estimated_storage_bytes=triangles_.size()*per_triangle;
    geometry_.reserve(triangles_.size()); order_.reserve(triangles_.size());
    for(std::size_t i=0;i<triangles_.size();++i) {
        const auto& p=triangles_[i].points;
        for(const auto& v:p) if(!finite(v)) fail("non-finite triangle");
        const auto e1=subtract(p[1],p[0]),e2=subtract(p[2],p[0]),n=cross(e1,e2);
        const double magnitude=length(n);
        if(!finite(e1)||!finite(e2)||!finite(n)||!std::isfinite(magnitude)) fail("triangle exceeds arithmetic range");
        Geometry geometry{{minimum(p[0],minimum(p[1],p[2])),maximum(p[0],maximum(p[1],p[2]))},
                          magnitude>0?scale(n,1/magnitude):Point{},
                          add(add(scale(p[0],1.0/3),scale(p[1],1.0/3)),scale(p[2],1.0/3))};
        if(!finite(geometry.normal)||!finite(geometry.centroid)) fail("triangle exceeds arithmetic range");
        geometry_.push_back(geometry);
        if(magnitude==0) ++stats_.degenerate_triangles;
        else order_.push_back(i);
    }
    if(!order_.empty()) { nodes_.reserve(order_.size()*2); build_node(0,order_.size()); }
    stats_.bvh_nodes=nodes_.size();
}

std::size_t World::build_node(std::size_t first,std::size_t count) {
    Bounds box=geometry_[order_[first]].bounds;
    Point low=geometry_[order_[first]].centroid,high=low;
    for(std::size_t i=first+1;i<first+count;++i) {
        const auto& item=geometry_[order_[i]];
        box.minimum=minimum(box.minimum,item.bounds.minimum); box.maximum=maximum(box.maximum,item.bounds.maximum);
        low=minimum(low,item.centroid); high=maximum(high,item.centroid);
    }
    const auto node=nodes_.size(); nodes_.push_back({box,first,count,0,0});
    if(count<=leaf_size) return node;
    const auto extent=subtract(high,low);
    int axis=0; if(extent.y>extent.x) axis=1; if(extent.z>component(extent,axis)) axis=2;
    const auto middle=first+count/2;
    std::nth_element(order_.begin()+first,order_.begin()+middle,order_.begin()+first+count,
        [&](std::size_t a,std::size_t b) {
            const auto x=component(geometry_[a].centroid,axis),y=component(geometry_[b].centroid,axis);
            return x==y?a<b:x<y;
        });
    const auto left=build_node(first,middle-first),right=build_node(middle,first+count-middle);
    nodes_[node].left=left; nodes_[node].right=right; nodes_[node].count=0;
    return node;
}

bool World::intersects_box(const Bounds& box,Point origin,Point direction,double limit) const {
    double low=0,high=limit;
    for(int axis=0;axis<3;++axis) {
        const double o=component(origin,axis),d=component(direction,axis);
        const double a=component(box.minimum,axis),b=component(box.maximum,axis);
        if(d==0) { if(o<a||o>b) return false; continue; }
        double entry=(a-o)/d,exit=(b-o)/d;
        if(entry>exit) std::swap(entry,exit);
        // Outward rounding protects a closed AABB boundary from one-ulp division.
        entry=std::nextafter(entry,-std::numeric_limits<double>::infinity());
        exit=std::nextafter(exit,std::numeric_limits<double>::infinity());
        low=std::max(low,entry); high=std::min(high,exit);
        if(high<low) return false;
    }
    return true;
}

std::optional<Hit> World::intersects_triangle(std::size_t id,Point origin,Point direction,double limit,Backfaces backfaces) const {
    const auto& triangle=triangles_[id]; const auto& p=triangle.points;
    const auto e1=subtract(p[1],p[0]),e2=subtract(p[2],p[0]);
    const auto cross_direction=cross(direction,e2);
    const double determinant=dot(e1,cross_direction);
    const double parallel_threshold=epsilon*length(e1)*length(e2);
    if(!std::isfinite(determinant)||!std::isfinite(parallel_threshold)) fail("intersection exceeds arithmetic range");
    if(backfaces==Backfaces::cull?determinant<=parallel_threshold:std::abs(determinant)<=parallel_threshold) return {};
    const auto offset=subtract(origin,p[0]);
    const double u=dot(offset,cross_direction)/determinant;
    const auto perpendicular=cross(offset,e1);
    const double v=dot(direction,perpendicular)/determinant;
    const double distance=dot(e2,perpendicular)/determinant;
    if(!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(distance)) fail("query exceeds intersection arithmetic range");
    if(u < -epsilon||v < -epsilon||u+v > 1+epsilon||distance<0||distance>limit) return {};
    const auto position=add(origin,scale(direction,distance));
    if(!finite(position)) fail("hit position exceeds arithmetic range");
    return Hit{id,triangle.source,distance,position,geometry_[id].normal,{1-u-v,u,v},determinant>0};
}

QueryResult World::raycast(Point origin,Point direction,double max_distance,Backfaces backfaces) const {
    if(!finite(origin)||!std::isfinite(max_distance)||max_distance<0) fail("origin/distance must be finite; distance must be nonnegative");
    if(backfaces!=Backfaces::include&&backfaces!=Backfaces::cull) fail("unknown backface policy");
    direction=unit(direction);
    QueryResult result;
    if(nodes_.empty()) return result;
    // Median splits have logarithmic depth; DFS needs at most depth+1 entries.
    std::array<std::size_t,std::numeric_limits<std::size_t>::digits+1> pending{};
    std::size_t size=1; pending[0]=0;
    while(size) {
        const auto& node=nodes_[pending[--size]];
        ++result.stats.boxes_tested;
        const double limit=result.hit?result.hit->distance:max_distance;
        if(!intersects_box(node.bounds,origin,direction,limit)) continue;
        if(node.count) {
            for(std::size_t i=node.first;i<node.first+node.count;++i) {
                ++result.stats.triangles_tested;
                auto hit=intersects_triangle(order_[i],origin,direction,result.hit?result.hit->distance:max_distance,backfaces);
                if(hit&&(!result.hit||hit->distance<result.hit->distance||
                         (hit->distance==result.hit->distance&&hit->triangle_id<result.hit->triangle_id))) result.hit=hit;
            }
        } else {
            if(size+2>pending.size()) fail("BVH depth invariant violated");
            pending[size++]=node.right; pending[size++]=node.left;
        }
    }
    return result;
}

QueryResult World::segmentcast(Point start,Point end,Backfaces backfaces) const {
    if(!finite(start)||!finite(end)) fail("segment endpoints must be finite");
    const auto direction=subtract(end,start); const auto distance=length(direction);
    if(!finite(direction)||!std::isfinite(distance)||distance==0) fail("segment must have finite nonzero length");
    return raycast(start,direction,distance,backfaces);
}

World load_static_gltf(const std::filesystem::path& path) {
    const auto scene=load_gltf(path);
    if(!scene.animations.empty()||!scene.skins.empty()) fail("static collision refuses animation/skin data");
    for(const auto& instance:scene.instances)
        if(instance.skin>=0||scene.primitives[instance.primitive].skinned) fail("static collision refuses skinned instances");
    SceneFrame frame; evaluate_scene(scene,-1,0,false,frame);
    std::vector<Triangle> triangles; triangles.reserve(frame.mesh.triangles.size()/3);
    std::size_t cursor=0;
    for(std::size_t i=0;i<scene.instances.size();++i) {
        const auto& instance=scene.instances[i]; const auto& primitive=scene.primitives[instance.primitive];
        for(std::size_t face=0;face<primitive.indices.size()/3;++face) {
            triangles.push_back({{point(frame.mesh.triangles[cursor].position),point(frame.mesh.triangles[cursor+1].position),
                                  point(frame.mesh.triangles[cursor+2].position)},
                                 {i,instance.node,instance.primitive,face}});
            cursor+=3;
        }
    }
    if(cursor!=frame.mesh.triangles.size()) fail("evaluated triangle identity mapping disagrees with scene");
    return World(std::move(triangles));
}
}
