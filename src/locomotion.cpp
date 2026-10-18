#include "locomotion.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numbers>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>

namespace mh4u::locomotion {
namespace {
using collision::Point;
using collision::Triangle;
using Key=std::pair<double,double>;
using Edge=std::pair<Key,Key>;
constexpr double tolerance=1e-9;
constexpr double coordinate_limit=1e6;
constexpr std::size_t face_limit=2048;
[[noreturn]] void fail(const char* reason) { throw std::runtime_error(std::string("Locomotion: ")+reason); }
void require(bool condition,const char* reason) { if(!condition) fail(reason); }
bool finite(double x) { return std::isfinite(x); }
Key key(Point p) { return {p.x,p.z}; }
Point flat(Key k) { return {k.first,0,k.second}; }
Point sub(Point a,Point b) { return {a.x-b.x,0,a.z-b.z}; }
Point add(Point a,Point b) { return {a.x+b.x,0,a.z+b.z}; }
Point scale(Point p,double s) { return {p.x*s,0,p.z*s}; }
double dot(Point a,Point b) { return a.x*b.x+a.z*b.z; }
double cross(Point a,Point b) { return a.x*b.z-a.z*b.x; }
double length(Point a) { return std::hypot(a.x,a.z); }
Edge edge(Point a,Point b) { auto x=key(a),y=key(b);return x<y?Edge{x,y}:Edge{y,x}; }
double distance(Point p,Point a,Point b) {
    const auto ab=sub(b,a); const auto t=std::clamp(dot(sub(p,a),ab)/dot(ab,ab),0.0,1.0);
    return length(sub(p,add(a,scale(ab,t))));
}
double signed_area(Point a,Point b,Point p) { return cross(sub(b,a),sub(p,a)); }
bool on_interior(Point a,Point b,Point p) {
    if(key(a)==key(p)||key(b)==key(p)) return false;
    return std::abs(signed_area(a,b,p))<=tolerance*length(sub(b,a))&&dot(sub(p,a),sub(p,b))<0;
}
bool crossing(Point a,Point b,Point c,Point d) {
    const auto ab_c=signed_area(a,b,c),ab_d=signed_area(a,b,d);
    const auto cd_a=signed_area(c,d,a),cd_b=signed_area(c,d,b);
    return ((ab_c>0&&ab_d<0)||(ab_c<0&&ab_d>0))&&((cd_a>0&&cd_b<0)||(cd_a<0&&cd_b>0));
}
bool strictly_inside(const Triangle& t,Point p) {
    for(int i=0;i<3;++i) if(signed_area(t.points[i],t.points[(i+1)%3],p)>=-tolerance*length(sub(t.points[(i+1)%3],t.points[i]))) return false;
    return true; // All upward triangles have clockwise XZ projections.
}
double yaw(double angle) { return std::remainder(angle,2*std::numbers::pi); }
struct Sweep { double distance; Point normal; };
// Earliest intersection of the center ray with a segment inflated by radius.
// The two parallel sides plus endpoint circles cover the entire capsule in XZ.
std::optional<Sweep> sweep(Point p,Point direction,double travel,Point a,Point b,double radius) {
    const auto ab=sub(b,a);const auto extent=length(ab);const auto along=scale(ab,1/extent);
    const Point perpendicular{-along.z,0,along.x};
    const double projection=std::clamp(dot(sub(p,a),along),0.0,extent);
    const auto closest=add(a,scale(along,projection));const auto away=sub(p,closest);
    const double start_distance=length(away);
    if(start_distance<=radius+tolerance&&start_distance>0&&dot(away,direction)<-tolerance)
        return Sweep{0,scale(away,1/start_distance)};
    std::optional<Sweep> best;
    const auto accept=[&](double t,Point n) {
        if(t>=0&&t<=travel&&(!best||t<best->distance)) best=Sweep{t,n};
    };
    const double signed_distance=dot(sub(p,a),perpendicular),rate=dot(direction,perpendicular);
    for(double sign:{-1.0,1.0}) {
        if(sign*rate>=0) continue;
        const double t=(sign*radius-signed_distance)/rate;
        const double longitudinal=dot(sub(add(p,scale(direction,t)),a),along);
        if(longitudinal>=0&&longitudinal<=extent) accept(t,scale(perpendicular,sign));
    }
    for(Point endpoint:{a,b}) {
        const auto offset=sub(p,endpoint); const double forward=dot(offset,direction);
        if(forward>=0) continue;
        const double perpendicular_distance=cross(offset,direction);
        const double discriminant=radius*radius-perpendicular_distance*perpendicular_distance;
        if(discriminant<0) continue;
        const double t=-forward-std::sqrt(discriminant);
        const auto normal=sub(add(p,scale(direction,t)),endpoint); const double magnitude=length(normal);
        if(magnitude>0) accept(t,scale(normal,1/magnitude));
    }
    return best;
}
}

World::World(collision::World floor,collision::World walls,Parameters parameters)
    :floor_(std::move(floor)),walls_(std::move(walls)),parameters_(parameters) {
    const auto& p=parameters_;
    require(finite(p.radius)&&finite(p.height)&&finite(p.slope_min_y)&&finite(p.max_step_height)&&finite(p.skin)&&finite(p.max_displacement),"non-finite parameters");
    require(p.skin>=1e-6&&p.skin<=1&&p.radius>8*p.skin&&p.radius<=1e4&&p.height>0&&p.height<=1e4,"radius/height/skin outside supported numeric range");
    require(p.slope_min_y>=0.01&&p.slope_min_y<=1&&p.max_step_height==0,"profile requires slope_min_y in [0.01,1] and max_step_height=0");
    require(p.max_displacement>0&&p.max_displacement<=1e6&&p.max_contacts>=1&&p.max_contacts<=16,"motion budget outside supported range");
    const auto& floors=floor_.triangles(); const auto& walls_data=walls_.triangles();
    require(!floors.empty()&&floors.size()<=face_limit&&walls_data.size()<=face_limit,"profile permits 1..2048 floor and 0..2048 wall triangles");
    require(floor_.stats().degenerate_triangles==0&&walls_.stats().degenerate_triangles==0,"degenerate geometry is outside this profile");
    for(const auto* world:{&floor_,&walls_}) for(const auto& t:world->triangles()) for(Point q:t.points)
        require(std::abs(q.x)<=coordinate_limit&&std::abs(q.y)<=coordinate_limit&&std::abs(q.z)<=coordinate_limit,"geometry exceeds numeric coordinate range");
    floor_min_y_=std::numeric_limits<double>::infinity();floor_max_y_=-floor_min_y_;
    struct FloorEdge { std::size_t count{},triangle{}; Key from; };
    std::map<Key,double> heights;
    std::map<Edge,FloorEdge> edges;
    for(std::size_t i=0;i<floors.size();++i) {
        const auto& t=floors[i];const auto& a=t.points[0];const auto& b=t.points[1];const auto& c=t.points[2];
        const double nx=(b.y-a.y)*(c.z-a.z)-(b.z-a.z)*(c.y-a.y);
        const double ny=-signed_area(a,b,c);
        const double nz=(b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
        require(ny/std::hypot(nx,ny,nz)>=p.slope_min_y,"floor winding or slope violates profile");
        for(int j=0;j<3;++j) {
            const auto v=t.points[j],w=t.points[(j+1)%3];
            floor_min_y_=std::min(floor_min_y_,v.y);floor_max_y_=std::max(floor_max_y_,v.y);
            const auto [where,inserted]=heights.emplace(key(v),v.y);
            require(inserted||where->second==v.y,"floor has multiple heights at one XZ vertex");
            require(length(sub(w,v))>1e-6,"projected edge below supported numeric scale");
            auto [position,new_edge]=edges.try_emplace(edge(v,w),FloorEdge{1,i,key(v)});
            if(!new_edge) {
                require(position->second.count==1&&position->second.from!=key(v),"floor has duplicate faces or nonmanifold edge");
                ++position->second.count;
            }
        }
    }
    // Bounded O(F^2) topology validation: overlapping faces and T-junctions would
    // invalidate support continuity, so refuse them rather than weld or guess.
    for(std::size_t i=0;i<floors.size();++i) for(std::size_t j=i+1;j<floors.size();++j) {
        const auto& a=floors[i];const auto& b=floors[j];
        for(int x=0;x<3;++x) {
            require(!strictly_inside(a,b.points[x])&&!strictly_inside(b,a.points[x]),"floor projected faces overlap");
            for(int y=0;y<3;++y) {
                const auto a0=a.points[x],a1=a.points[(x+1)%3],b0=b.points[y],b1=b.points[(y+1)%3];
                if(edge(a0,a1)==edge(b0,b1)) continue;
                require(!crossing(a0,a1,b0,b1)&&!on_interior(a0,a1,b0)&&!on_interior(a0,a1,b1)&&!on_interior(b0,b1,a0)&&!on_interior(b0,b1,a1),"floor has crossings or T-junctions");
            }
        }
    }
    segments_.reserve(edges.size()+walls_data.size()/2);
    for(const auto& [e,info]:edges) if(info.count==1)
        segments_.push_back({flat(e.first),flat(e.second),{BarrierKind::floor_boundary,info.triangle,floors[info.triangle].source,{}}});
    stats_.floor_boundaries=segments_.size();
    std::map<Edge,std::vector<std::size_t>> rectangles;
    for(std::size_t i=0;i<walls_data.size();++i) {
        const auto& t=walls_data[i];std::set<Key> projected;
        for(auto v:t.points) projected.insert(key(v));
        require(projected.size()==2,"walls must be vertical rectangle pairs, not general triangles");
        const Edge e{*projected.begin(),*projected.rbegin()};
        require(length(sub(flat(e.second),flat(e.first)))>1e-6,"wall below supported numeric scale");
        rectangles[e].push_back(i);
    }
    for(const auto& [e,ids]:rectangles) {
        require(ids.size()==2,"wall rectangle requires exactly two triangles");
        std::set<std::tuple<double,double,double>> corners;
        double low=std::numeric_limits<double>::infinity(),high=-low;
        for(auto id:ids) for(auto v:walls_data[id].points) { corners.emplace(v.x,v.y,v.z);low=std::min(low,v.y);high=std::max(high,v.y); }
        require(corners.size()==4&&low<high,"wall pair is not a rectangle");
        for(auto k:{e.first,e.second}) for(double y:{low,high})
            require(corners.contains({k.first,y,k.second}),"wall rectangle must have constant top and bottom");
        // Four corners alone also permits overlapping halves with opposite
        // diagonal choices. The shared edge must join opposite corners.
        std::vector<Point> shared;
        for(auto a:walls_data[ids[0]].points) for(auto b:walls_data[ids[1]].points)
            if(key(a)==key(b)&&a.y==b.y) shared.push_back(a);
        require(shared.size()==2&&key(shared[0])!=key(shared[1])&&shared[0].y!=shared[1].y,"wall triangles do not cover a rectangle exactly");
        require(low<=floor_min_y_-p.skin&&high>=floor_max_y_+p.height+p.skin,"wall vertical coverage insufficient for configured body height");
        segments_.push_back({flat(e.first),flat(e.second),{BarrierKind::wall,ids.front(),walls_data[ids.front()].source,{}}});
    }
    stats_.floor_triangles=floors.size();stats_.wall_triangles=walls_data.size();stats_.wall_segments=rectangles.size();
}

collision::Hit World::support(double x,double z) const {
    require(finite(x)&&finite(z)&&std::abs(x)<=coordinate_limit&&std::abs(z)<=coordinate_limit,"position outside numeric range");
    const auto result=floor_.raycast({x,floor_max_y_+1,z},{0,-1,0},floor_max_y_-floor_min_y_+2);
    require(result.hit.has_value(),"position has no floor support");
    return *result.hit;
}
void World::validate_clearance(double x,double z) const {
    const Point point{x,0,z};
    for(const auto& segment:segments_)
        require(distance(point,segment.a,segment.b)>=parameters_.radius+parameters_.skin-tolerance,"position overlaps wall or floor boundary clearance");
}
State World::create_state(double x,double z,double angle) const {
    require(finite(angle),"non-finite yaw");
    const auto floor=support(x,z);validate_clearance(x,z);
    return {floor.position,yaw(angle)};
}
StepResult World::step(const State& state,Input input) const {
    require(finite(state.position.y)&&finite(state.yaw)&&finite(input.dx)&&finite(input.dz)&&finite(input.yaw_delta),"non-finite state or motion");
    const auto original=support(state.position.x,state.position.z);
    require(std::abs(original.position.y-state.position.y)<=parameters_.skin,"state must already be supported by this floor");
    validate_clearance(state.position.x,state.position.z);
    Point remaining{input.dx,0,input.dz},position{state.position.x,0,state.position.z};
    require(length(remaining)<=parameters_.max_displacement,"requested displacement exceeds explicit step budget");
    StepResult result;result.state=state;
    for(std::size_t iteration=0;iteration<parameters_.max_contacts;++iteration) {
        const double travel=length(remaining);if(travel<=tolerance) break;
        const auto direction=scale(remaining,1/travel);
        std::optional<Sweep> earliest; const Segment* obstacle=nullptr;
        for(const auto& segment:segments_) {
            ++result.segment_tests;
            const auto hit=sweep(position,direction,travel,segment.a,segment.b,parameters_.radius+parameters_.skin);
            if(hit&&(!earliest||hit->distance<earliest->distance)) {earliest=hit;obstacle=&segment;}
        }
        if(!earliest) {position=add(position,remaining);remaining={};break;}
        result.blocked=true;
        auto contact=obstacle->contact;contact.normal=earliest->normal;
        result.contacts[result.contact_count++]=contact;
        const double advance=std::max(0.0,earliest->distance-parameters_.skin);
        position=add(position,scale(direction,advance));remaining=scale(direction,travel-advance);
        const double into=dot(remaining,earliest->normal);
        if(into<0) remaining=sub(remaining,scale(earliest->normal,into));
    }
    result.contact_limit_reached=length(remaining)>tolerance;
    const auto final=support(position.x,position.z);validate_clearance(position.x,position.z);
    result.state={final.position,yaw(yaw(state.yaw)+yaw(input.yaw_delta))};
    result.applied_dx=position.x-state.position.x;result.applied_dz=position.z-state.position.z;
    result.support_triangle=final.triangle_id;
    return result;
}
}
