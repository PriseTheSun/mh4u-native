#include "collision.hpp"
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace {
using namespace mh4u::collision;
void require(bool value,const char* reason) { if(!value) throw std::runtime_error(reason); }
void near(double a,double b,const char* reason,double tolerance=1e-10) { require(std::isfinite(a)&&std::abs(a-b)<=tolerance,reason); }
void rejects(const std::function<void()>& operation) {
    try { operation(); } catch(const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid collision input accepted");
}
Triangle triangle(double x=0,double z=0) { return {{{{x,0,z},{x+1,0,z},{x,1,z}}},{7,8,9,10}}; }
const Hit& hit(const QueryResult& result) { require(result.hit.has_value(),"Expected geometric hit"); return *result.hit; }
void analytic_tests() {
    const World world({triangle()});
    auto result=world.raycast({0.25,0.25,2},{0,0,-100},3);
    near(hit(result).distance,2,"Direction must normalize to source-space distance");
    near(hit(result).normal.z,1,"Winding normal");
    near(hit(result).barycentric[0],0.5,"Barycentric A");
    near(hit(result).barycentric[1],0.25,"Barycentric B");
    near(hit(result).position.z,0,"Hit position");
    require(hit(result).front_face&&hit(result).source.node==8,"Facing and source identity");
    require(!world.raycast({0.25,0.25,2},{0,0,-1},1).hit,"Finite distance miss");
    require(!world.raycast({2,2,2},{0,0,-1},10).hit,"Outside triangle miss");
    require(!world.raycast({0.25,0.25,2},{1,0,0},10).hit,"Parallel ray miss");
    require(!world.raycast({0.25,0.25,0},{1,0,0},10).hit,"Coplanar ray has no isolated intersection");
    near(hit(world.raycast({0.25,0.25,0},{0,0,-1},0)).distance,0,"Closed zero-distance ray interval");
    near(hit(world.raycast({0,0,2},{0,0,-1},2)).distance,2,"Vertex and max-distance endpoint inclusive");
    near(hit(world.raycast({0.5,0.5,2},{0,0,-1},2)).distance,2,"Shared-edge endpoint inclusive");
    result=world.raycast({0.25,0.25,-2},{0,0,1},2);
    require(!hit(result).front_face,"Backface must be reported");
    near(hit(result).normal.z,1,"Backface must not flip winding normal");
    require(!world.raycast({0.25,0.25,-2},{0,0,1},2,Backfaces::cull).hit,"Cull backfaces");
    near(hit(world.segmentcast({0.25,0.25,2},{0.25,0.25,0})).distance,2,"Closed segment endpoint");
    require(!world.segmentcast({0.25,0.25,2},{0.25,0.25,1}).hit,"Short segment miss");
    near(hit(world.raycast({0.25,0.25,2},{0,0,-1e300},3)).distance,2,"Huge direction normalization");
    near(hit(world.raycast({0.25,0.25,2},{0,0,-1e-300},3)).distance,2,"Tiny direction normalization");
    const double nan=std::numeric_limits<double>::quiet_NaN(),inf=std::numeric_limits<double>::infinity();
    rejects([&]{world.raycast({nan,0,0},{0,0,1},3);});
    rejects([&]{world.raycast({0,0,0},{0,0,inf},3);});
    rejects([&]{world.raycast({0,0,0},{0,0,0},3);});
    rejects([&]{world.raycast({0,0,0},{0,0,1},-1);});
    rejects([&]{world.raycast({0,0,0},{0,0,1},inf);});
    rejects([&]{world.segmentcast({0,0,0},{0,0,0});});
    rejects([&]{world.segmentcast({0,0,0},{nan,0,0});});
    rejects([&]{world.raycast({0,0,0},{0,0,1},3,static_cast<Backfaces>(99));});
    rejects([&]{auto invalid=triangle(); invalid.points[0].x=nan; World failed({invalid});});
    rejects([&]{World failed({triangle()},1);});
    Triangle flat=triangle(); flat.points[2]=flat.points[1];
    const World skipped({flat,triangle()});
    require(skipped.stats().degenerate_triangles==1&&hit(skipped.raycast({0.25,0.25,2},{0,0,-1},3)).triangle_id==1,"Skipping degenerates must preserve IDs");
    require(!World({flat}).raycast({0,0,1},{0,0,-1},2).hit,"All-degenerate world must miss");
    require(!World({}).raycast({0,0,1},{0,0,-1},2).hit,"Empty world must miss");
    std::cout<<"Analytic: distance, barycentrics, edges, endpoints, coplanar/parallel, backfaces, finite/zero inputs and degenerates passed\n";
}
void bvh_tests() {
    std::vector<Triangle> rows;
    for(std::size_t i=0;i<128;++i) {
        auto t=triangle(10*double(127-i)); t.source.primitive_triangle=i; rows.push_back(t);
    }
    const World world(std::move(rows));
    require(world.stats().bvh_nodes>1,"BVH not built");
    std::size_t triangle_checks=0;
    for(std::size_t i=0;i<128;++i) {
        const auto result=world.raycast({10*double(127-i)+0.25,0.25,4},{0,0,-1},10);
        require(hit(result).triangle_id==i&&hit(result).source.primitive_triangle==i,"BVH reorder changed source identity");
        near(hit(result).distance,4,"BVH analytic grid distance"); triangle_checks+=result.stats.triangles_tested;
    }
    require(triangle_checks<=128*8,"BVH failed to cull spatially disjoint triangles");
    const World nearest({triangle(0,-2),triangle(0,1),triangle(0,0)});
    require(hit(nearest.raycast({0.25,0.25,2},{0,0,-1},20)).triangle_id==1,"Nearest hit depends on input order");
    const World equal(std::vector<Triangle>(33,triangle()));
    require(hit(equal.raycast({0.25,0.25,2},{0,0,-1},3)).triangle_id==0,"Exact equal-distance tie must choose lowest original ID");
    std::cout<<"BVH: 128 analytic grid queries, stable IDs, nearest/ties passed; triangle checks "<<triangle_checks<<" / "<<128*128<<" linear candidates\n";
}
void loader_tests() {
    const auto root=std::filesystem::temp_directory_path()/("mh4u-collision-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(std::filesystem::create_directory(root),"Cannot create fixture directory");
    struct Cleanup { std::filesystem::path path; ~Cleanup(){std::error_code error;std::filesystem::remove_all(path,error);} } cleanup{root};
    const std::array<float,18> data{0,0,0,1,0,0,0,1,0,0,0,1,0,0,1,0,0,1};
    {std::ofstream file(root/"mesh.bin",std::ios::binary);file.write(reinterpret_cast<const char*>(data.data()),sizeof(data));require(bool(file),"Fixture write failed");}
    const std::string source=R"({"asset":{"version":"2.0"},"buffers":[{"uri":"mesh.bin","byteLength":72}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36}],
"accessors":[{"bufferView":0,"componentType":5126,"type":"VEC3","count":3},{"bufferView":1,"componentType":5126,"type":"VEC3","count":3}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1}}]}],
"nodes":[{"translation":[10,0,0],"children":[1,2]},{"translation":[0,0,3],"scale":[-1,2,1],"mesh":0},{"translation":[0,0,1],"mesh":0}],
"scenes":[{"nodes":[0]}],"scene":0})";
    {std::ofstream file(root/"scene.gltf");file<<source;require(bool(file),"Fixture scene write failed");}
    const auto world=load_static_gltf(root/"scene.gltf");
    const auto result=world.raycast({9.75,0.5,5},{0,0,-1},10);
    near(hit(result).distance,2,"glTF parent/local transforms must apply exactly once");
    near(hit(result).normal.z,-1,"Negative scale winding must use transformed geometry");
    // The existing scene owner traverses children from its LIFO pending stack.
    require(hit(result).triangle_id==1&&hit(result).source.instance==1&&hit(result).source.node==1&&hit(result).source.primitive==0,"glTF instance provenance");
    require(!world.raycast({9.75,0.5,5},{0,0,-1},10,Backfaces::cull).hit,"Mirrored glTF backface culling");
    const auto second=world.raycast({10.25,0.25,5},{0,0,-1},10);
    require(hit(second).triangle_id==0&&hit(second).source.instance==0,"Shared primitive instances must have distinct triangle IDs");
    near(hit(second).distance,4,"Second instance world transform");
    std::cout<<"Static glTF: hierarchy, scale/reflection, one-time evaluation and instance provenance passed\n";
}
void real_asset(const std::filesystem::path& path) {
    const auto world=load_static_gltf(path);
    require(world.stats().triangles==86&&world.stats().degenerate_triangles==0,"Expected frozen v00a01-core2 geometric probe");
    std::size_t triangles_tested=0; double max_error=0;
    for(std::size_t i=0;i<world.triangles().size();++i) {
        const auto& p=world.triangles()[i].points;
        const Point center{(p[0].x+p[1].x+p[2].x)/3,(p[0].y+p[1].y+p[2].y)/3,(p[0].z+p[1].z+p[2].z)/3};
        const auto result=world.raycast({center.x,center.y+10,center.z},{0,-1,0},20);
        require(hit(result).triangle_id==i,"Real triangle centroid ray did not recover original face ID");
        const double error=std::abs(hit(result).distance-10); max_error=std::max(max_error,error);
        require(error<1e-8,"Real centroid intersection distance exceeds tolerance");
        require(hit(result).normal.y>0.9,"Real source winding differs from measured upward normals");
        triangles_tested+=result.stats.triangles_tested;
    }
    require(!world.raycast({1e6,1e6,1e6},{0,-1,0},20).hit,"Real mesh outside query must miss");
    require(triangles_tested<86*86,"Real BVH did not reduce triangle candidates");
    std::cout<<"REAL_TRIANGLES 86\nREAL_CENTROID_RAYS 86\nREAL_MAX_DISTANCE_ERROR "<<max_error
             <<"\nREAL_TRIANGLES_TESTED "<<triangles_tested<<"\nREAL_LINEAR_CANDIDATES "<<86*86<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        analytic_tests(); bvh_tests(); loader_tests();
        if(argc==2) { std::u8string utf;for(unsigned char c:std::string(argv[1])) utf.push_back(static_cast<char8_t>(c));real_asset(std::filesystem::path(utf)); }
        else require(argc==1,"Usage: mh4u-collision-tests [v00a01-core2/scene.gltf]");
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n';return 1; }
}
