#include "locomotion.hpp"
#include <chrono>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <string>

namespace {
using mh4u::collision::Point;
using mh4u::collision::Triangle;
using mh4u::locomotion::World;
using mh4u::locomotion::Parameters;
using mh4u::locomotion::BarrierKind;
void require(bool ok,const char* reason) {if(!ok) throw std::runtime_error(reason);}
void near(double a,double b,const char* reason,double epsilon=1e-7) {require(std::isfinite(a)&&std::abs(a-b)<=epsilon,reason);}
void rejects(const std::function<void()>& operation) {try{operation();}catch(const std::runtime_error&){return;}throw std::runtime_error("Expected unsupported input to fail");}
void patch(std::vector<Triangle>& out,double x0,double z0,double x1,double z1,double rise_x=0,double rise_z=0) {
    const auto p=[&](double x,double z){return Point{x,rise_x*x+rise_z*z,z};};
    out.push_back({{p(x0,z0),p(x0,z1),p(x1,z1)},{0,0,0,out.size()}});
    out.push_back({{p(x0,z0),p(x1,z1),p(x1,z0)},{0,0,0,out.size()}});
}
void wall(std::vector<Triangle>& out,Point a,Point b,double low=-100,double high=100) {
    Point a0{a.x,low,a.z},a1{a.x,high,a.z},b0{b.x,low,b.z},b1{b.x,high,b.z};
    out.push_back({{a0,a1,b1},{0,1,0,out.size()}});
    out.push_back({{a0,b1,b0},{0,1,0,out.size()}});
}
Parameters params() {return {1,5,0.8,0,0.001,4096,8};}
World make(std::vector<Triangle> floor,std::vector<Triangle> walls={},Parameters p=params()) {return {mh4u::collision::World(std::move(floor)),mh4u::collision::World(std::move(walls)),p};}
void analytic() {
    std::vector<Triangle> floor,walls;patch(floor,-100,-100,100,100);wall(walls,{0,0,-30},{0,0,30});
    auto world=make(floor,walls);
    const auto start=world.create_state(-10,0);
    const auto hit=world.step(start,{60,0,0});
    require(hit.blocked&&hit.contact_count==1&&hit.contacts[0].kind==BarrierKind::wall,"High-speed wall crossing must be blocked");
    near(hit.state.position.x,-1.002,"Analytic wall clearance");near(hit.state.position.z,0,"Wall hit changes only intended coordinate");
    require(hit.contacts[0].source.node==1&&hit.contacts[0].triangle_id==0,"Wall source identity preserved");
    const auto slide=world.step(start,{60,12,0.5});
    require(slide.blocked,"Diagonal wall hit");near(slide.state.position.z,12,"Tangential motion survives wall slide",0.01);
    near(slide.state.yaw,0.5,"Explicit yaw update");
    const auto away=world.step(hit.state,{-4,0,0});require(!away.blocked,"Moving away from wall must remain possible");
    near(away.applied_dx,-4,"Away displacement");
    auto walked=start;for(int i=0;i<120;++i) walked=world.step(walked,{0.5,0,0}).state;
    near(walked.position.x,hit.state.position.x,"Small steps and single fast wall sweep agree",0.002);
    const auto endcap=world.step(world.create_state(-10,30.5),{30,0,0});
    require(endcap.blocked&&endcap.contacts[0].normal.z>0,"Endpoint circle must intercept near-miss against segment line");
    const auto floor_edge=world.step(world.create_state(-90,60),{-500,0,0});
    require(floor_edge.blocked&&floor_edge.contacts[0].kind==BarrierKind::floor_boundary,"Floor exit blocked over full path");
    near(floor_edge.state.position.x,-98.998,"Floor boundary radius");
    std::vector<Triangle> corner;wall(corner,{0,0,-50},{0,0,0});wall(corner,{-50,0,0},{0,0,0});
    auto corner_world=make(floor,corner);const auto corner_hit=corner_world.step(corner_world.create_state(-10,-10),{50,50,0});
    require(corner_hit.blocked&&corner_hit.contact_count>=2,"Two-wall corner resolves both contacts");
    require(corner_hit.state.position.x<=-1.001&&corner_hit.state.position.z<=-1.001,"Corner cannot leak between walls");
    auto limited=params();limited.max_contacts=1;auto limited_world=make(floor,corner,limited);
    require(limited_world.step(limited_world.create_state(-10,-10),{50,50,0}).contact_limit_reached,"Exhausted contact budget reported");
    std::vector<Triangle> holed;
    for(int x=0;x<3;++x)for(int z=0;z<3;++z)if(x!=1||z!=1)patch(holed,-30+20*x,-30+20*z,-10+20*x,-10+20*z);
    auto hole=make(holed);const auto stopped=hole.step(hole.create_state(-20,0),{40,0,0});
    require(stopped.blocked,"Sweep cannot jump a floor hole to supported endpoint");near(stopped.state.position.x,-11.002,"Hole edge analytic clearance");
    rejects([&]{hole.create_state(0,0);});
    std::vector<Triangle> slope;patch(slope,-40,-40,40,40,0.1,0.2);auto inclined=make(slope);
    const auto up=inclined.step(inclined.create_state(-20,-20),{30,25,0});near(up.state.position.y,2,"Support follows actual floor plane");
    std::cout<<"ANALYTIC passed: tunneling, sliding, endpoint circle, corner, budget, floor boundary/hole, inclined support, provenance\n";
}
void malformed() {
    std::vector<Triangle> floor,walls;patch(floor,-100,-100,100,100);wall(walls,{0,0,-30},{0,0,30});
    auto world=make(floor,walls);auto initial=world.create_state(-10,0);const double nan=std::numeric_limits<double>::quiet_NaN();
    rejects([&]{world.create_state(0,0);});rejects([&]{world.create_state(1000,0);});rejects([&]{world.create_state(nan,0);});
    rejects([&]{world.step(initial,{nan,0,0});});rejects([&]{world.step(initial,{1e8,0,0});});
    rejects([&]{auto altered=initial;altered.position.y+=5;world.step(altered,{});});
    rejects([&]{auto p=params();p.max_step_height=1;make(floor,walls,p);});
    rejects([&]{auto p=params();p.height=200;make(floor,walls,p);});
    rejects([&]{auto p=params();p.radius=nan;make(floor,walls,p);});
    rejects([&]{auto p=params();p.max_contacts=17;make(floor,walls,p);});
    rejects([&]{auto bad=walls;bad.pop_back();make(floor,bad);});
    rejects([&]{auto bad=walls;bad[0].points[1].x+=0.1;make(floor,bad);});
    rejects([&]{auto bad=walls;bad[0].points[1].y-=0.1;make(floor,bad);});
    rejects([&]{auto duplicate=floor;duplicate.insert(duplicate.end(),floor.begin(),floor.end());make(duplicate);});
    rejects([&]{auto overlapping=floor;patch(overlapping,-50,-50,50,50);make(overlapping);});
    rejects([&]{auto overhang=floor;auto copy=floor;for(auto& t:copy)for(auto& p:t.points)p.y+=20;overhang.insert(overhang.end(),copy.begin(),copy.end());make(overhang);});
    rejects([&]{std::vector<Triangle> bad;patch(bad,0,0,10,10);patch(bad,10,0,20,5);make(bad);});
    rejects([&]{std::vector<Triangle> steep;patch(steep,-10,-10,10,10,2);make(steep);});
    rejects([&]{std::vector<Triangle> oversized(2049,floor.front());make(oversized);});
    std::cout<<"REJECTIONS passed: invalid state/parameters, unsupported walls, discontinuities/overlap/T-junctions, slope, resource limits\n";
}
void real(const std::filesystem::path& floor_path,const std::filesystem::path& wall_path) {
    const auto before=std::chrono::steady_clock::now();
    auto p=params();p.radius=22;p.height=175;
    World world(mh4u::collision::load_static_gltf(floor_path),mh4u::collision::load_static_gltf(wall_path),p);
    const auto ready=std::chrono::steady_clock::now();
    require(world.stats().floor_triangles==86&&world.stats().wall_triangles==90&&world.stats().floor_boundaries==8&&world.stats().wall_segments==45,"Expected frozen village geometry");
    const auto start=world.create_state(-61.14208710193634,11963.744791666666);
    near(start.position.y,-17.212244669596355,"Real diagnostic centroid support",1e-6);
    require(world.step(start,{}).support_triangle==30,"Real support must retain source face30");
    std::size_t blocked=0,contacts=0,queries=0;
    for(int i=0;i<64;++i) {
        const double angle=2*std::numbers::pi*i/64;
        const auto result=world.step(start,{4000*std::cos(angle),4000*std::sin(angle),angle});
        require(result.blocked,"Closed real wall loop must stop every radial fast motion");
        require(std::hypot(result.applied_dx,result.applied_dz)<1500,"Real radial motion escaped enclosed walls");
        // Public state validation rechecks independent BVH floor support and
        // clearance against every wall and floor boundary at the final position.
        world.create_state(result.state.position.x,result.state.position.z,result.state.yaw);
        ++blocked;contacts+=result.contact_count;queries+=result.segment_tests;
    }
    auto state=start;
    for(int i=0;i<1000;++i) {
        const double angle=i*0.173;
        const auto result=world.step(state,{9*std::cos(angle),9*std::sin(angle),0.01});
        state=result.state;queries+=result.segment_tests;
    }
    const auto after=std::chrono::steady_clock::now();
    std::cout<<"REAL_FLOOR_TRIANGLES 86\nREAL_WALL_TRIANGLES 90\nREAL_FLOOR_BOUNDARIES 8\nREAL_WALL_SEGMENTS 45\nREAL_SPAWN_SUPPORT_FACE 30\nREAL_RADIAL_SWEEPS "<<blocked
             <<"\nREAL_RADIAL_CONTACTS "<<contacts<<"\nREAL_CONTINUOUS_STEPS 1000\nREAL_SEGMENT_TESTS "<<queries
             <<"\nREAL_LOAD_BUILD_MS "<<std::chrono::duration<double,std::milli>(ready-before).count()
             <<"\nREAL_MOVEMENT_MS "<<std::chrono::duration<double,std::milli>(after-ready).count()<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        analytic();malformed();
        if(argc==3) {
            const auto path=[](const char* value){std::u8string text;for(unsigned char c:std::string(value))text.push_back(static_cast<char8_t>(c));return std::filesystem::path(text);};
            real(path(argv[1]),path(argv[2]));
        } else require(argc==1,"Usage: mh4u-locomotion-tests [floor.gltf walls.gltf]");
        return 0;
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
