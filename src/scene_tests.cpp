#include "scene.hpp"
#include <cmath>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {
void require(bool valid,const char* message) { if(!valid) throw std::runtime_error(message); }
void near(float actual,float expected,const char* message,float epsilon=1e-4f) { require(std::isfinite(actual)&&std::abs(actual-expected)<=epsilon,message); }
void rejects(const std::function<void()>& operation) {
    try { operation(); } catch(const std::runtime_error&) { return; }
    throw std::runtime_error("Invalid scene state accepted");
}
mh4u::Scene fixture() {
    using namespace mh4u;
    Scene scene; scene.nodes.resize(3);
    scene.nodes[0].rest.translation={10,0,0};
    scene.nodes[1].parent=0; scene.nodes[1].rest.translation={2,0,0};
    // A skinned mesh's node transform must cancel: world skin uses G_joint * inverseBind.
    scene.nodes[2].rest.translation={1000,2000,3000};
    Skin skin; skin.joints={0,1};
    auto a=identity_matrix(),b=identity_matrix(); a[12]=-10; b[12]=-12;
    skin.inverse_bind={a,b}; scene.skins.push_back(skin);
    Primitive primitive; primitive.skinned=true;
    primitive.vertices={{{{12,0,0},{0,0,1},{0,0}},{0,1,0,0},{0.2f,0.2f,0,0}},
                        {{{13,0,0},{0,0,1},{1,0}},{1,0,0,0},{1,0,0,0}},
                        {{{12,1,0},{0,0,1},{0,1}},{0,1,0,0},{0.5f,0.5f,0,0}}};
    primitive.indices={0,1,2}; scene.primitives.push_back(primitive);
    scene.instances.push_back({0,2,0});
    AnimationChannel rotation; rotation.node=1; rotation.path=ChannelPath::rotation;
    rotation.times={0,1}; rotation.values={{0,0,0,2},{0,0,std::sqrt(0.5f),std::sqrt(0.5f)}};
    scene.animations.push_back({"quarter turn",{rotation},0}); prepare_scene(scene); return scene;
}
void mathematical_tests() {
    using namespace mh4u;
    auto scene=fixture(); SceneFrame frame;
    evaluate_scene(scene,-1,0,false,frame);
    near(frame.global_transforms[1][12],12,"Hierarchy composition");
    for(std::size_t v=0;v<3;++v) {
        near(frame.mesh.triangles[v].position.x,scene.primitives[0].vertices[v].vertex.position.x,"Bind pose preservation X");
        near(frame.mesh.triangles[v].position.y,scene.primitives[0].vertices[v].vertex.position.y,"Bind pose preservation Y");
    }
    near(scene.primitives[0].vertices[0].weights[0],0.5f,"Weight normalization");
    evaluate_scene(scene,0,0.5f,false,frame);
    const float s=std::sqrt(0.5f);
    near(frame.mesh.triangles[1].position.x,12+s,"Hierarchical rotational skin X");
    near(frame.mesh.triangles[1].position.y,s,"Hierarchical rotational skin Y");
    near(frame.mesh.triangles[2].position.x,12-s*0.5f,"Two-joint blended X");
    near(frame.mesh.triangles[2].position.y,0.5f+s*0.5f,"Two-joint blended Y");
    near(sample_animation(scene,0,0.5f,false)[1].translation.x,2,"Missing channel must preserve rest component");
    Transform scaled; scaled.scale={2,1,0.5f};
    const auto normal=transform_normal(transform_matrix(scaled),{s,s,0});
    near(normal.x,1/std::sqrt(5.0f),"Inverse transpose X"); near(normal.y,2/std::sqrt(5.0f),"Inverse transpose Y");
    const auto q=quaternion_slerp({0,0,0,1},{0,0,0,-1},0.5f); near(std::abs(q[3]),1,"Antipodal quaternion interpolation");
    near(animation_time(-0.25f,1,true),0.75f,"Negative wrap");
    near(animation_time(1,1,true),0,"Exact loop boundary");
    near(animation_time(2,1,false),1,"Clamp at last frame");
    near(animation_time(5,0,true),0,"Zero-length clip");
    scene.animations[0].channels[0].interpolation=Interpolation::step;
    evaluate_scene(scene,0,0.999f,false,frame); near(frame.mesh.triangles[1].position.x,13,"STEP before final key");
    evaluate_scene(scene,0,1,false,frame); near(frame.mesh.triangles[1].position.y,1,"STEP at exact final key");
    rejects([]{ normalized_weights({0,0,0,0}); });
    rejects([]{ normalized_weights({1,-1,0,0}); });
    rejects([]{ normalized_quaternion({0,0,0,0}); });
    rejects([&]{ auto broken=scene; broken.nodes[0].parent=1; prepare_scene(broken); });
    rejects([&]{ auto broken=scene; broken.animations[0].channels[0].times={0,0}; prepare_scene(broken); });
    rejects([&]{ auto broken=scene; broken.animations[0].channels.push_back(broken.animations[0].channels[0]); prepare_scene(broken); });
    rejects([&]{ auto broken=scene; broken.primitives[0].vertices[0].joints[0]=20; prepare_scene(broken); });
    std::cout<<"Scene math: bind pose, hierarchy, skin blending, inverse-transpose normals, slerp, missing channels, STEP and loops passed\n";
}
void loader_tests() {
    const auto root=std::filesystem::temp_directory_path()/("mh4u-gltf-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    require(std::filesystem::create_directory(root),"Cannot create owned glTF test directory");
    struct Cleanup { std::filesystem::path p; ~Cleanup(){ std::error_code error; std::filesystem::remove_all(p,error); } } cleanup{root};
    const std::array<float,18> geometry{0,0,0,1,0,0,0,1,0,0,0,1,0,0,1,0,0,1};
    const std::array<std::uint32_t,3> indices{0,1,2};
    { std::ofstream file(root/"mesh.bin",std::ios::binary);
      file.write(reinterpret_cast<const char*>(geometry.data()),sizeof(geometry));
      file.write(reinterpret_cast<const char*>(indices.data()),sizeof(indices)); require(bool(file),"Cannot write fixture buffer"); }
    const std::string source=R"({"asset":{"version":"2.0"},"buffers":[{"uri":"mesh.bin","byteLength":84}],
"bufferViews":[{"buffer":0,"byteLength":36},{"buffer":0,"byteOffset":36,"byteLength":36},{"buffer":0,"byteOffset":72,"byteLength":12}],
"accessors":[{"bufferView":0,"componentType":5126,"type":"VEC3","count":3},{"bufferView":1,"componentType":5126,"type":"VEC3","count":3},{"bufferView":2,"componentType":5125,"type":"SCALAR","count":3}],
"materials":[{"pbrMetallicRoughness":{"baseColorFactor":[1,0,0,1]}},{"pbrMetallicRoughness":{"baseColorFactor":[0,1,0,1]}}],
"meshes":[{"primitives":[{"attributes":{"POSITION":0,"NORMAL":1},"indices":2,"material":0},{"attributes":{"POSITION":0,"NORMAL":1},"indices":2,"material":1}]}],
"nodes":[{"translation":[2,0,0],"children":[1]},{"translation":[3,0,0],"mesh":0}],"scenes":[{"nodes":[0]}],"scene":0})";
    const auto write=[&](const std::string& content) { std::ofstream file(root/"scene.gltf"); file<<content; require(bool(file),"Cannot write fixture glTF"); };
    write(source); auto scene=mh4u::load_gltf(root/"scene.gltf"); mh4u::SceneFrame frame;
    mh4u::evaluate_scene(scene,-1,0,false,frame);
    require(frame.draws.size()==2&&frame.draws[1].first==3&&frame.draws[1].material==1,"glTF multiple primitives/materials lost");
    near(frame.mesh.triangles[0].position.x,5,"glTF hierarchy transform"); near(scene.materials[1].color[1],1,"glTF material color");
    auto invalid=source; const auto offset=invalid.find("\"byteOffset\":36"); require(offset!=std::string::npos,"Fixture mutation point missing");
    invalid.replace(offset,std::string("\"byteOffset\":36").size(),"\"byteOffset\":72"); write(invalid);
    rejects([&]{ mh4u::load_gltf(root/"scene.gltf"); });
    invalid=source; const auto count=invalid.find("\"count\":3");
    invalid.replace(count,std::string("\"count\":3").size(),"\"count\":2147483647"); write(invalid);
    rejects([&]{ mh4u::load_gltf(root/"scene.gltf"); });
    invalid=source; const auto uri=invalid.find("mesh.bin"); invalid.replace(uri,8,"../mesh.bin"); write(invalid);
    rejects([&]{ mh4u::load_gltf(root/"scene.gltf"); });
    for(const auto* representation:{"\"componentType\":5122","\"componentType\":5126","\"componentType\":5123,\"normalized\":true"}) {
        invalid=source; const auto component=invalid.find("\"componentType\":5125");
        require(component!=std::string::npos,"Index accessor mutation point missing");
        invalid.replace(component,std::string("\"componentType\":5125").size(),representation); write(invalid);
        rejects([&]{ mh4u::load_gltf(root/"scene.gltf"); });
    }
    std::cout<<"glTF boundary: multi-primitive/material scene, hierarchy, offset overflow, oversized accessor, escaping URI and signed/float/normalized indices passed\n";
    // Exercise the public loader and sampler together: row layout, derivative units,
    // quaternion normalization and rejection of malformed spline data.
    const auto write_spline=[&](const std::vector<float>& times,const std::vector<float>& values,
                                unsigned components,const char* target) {
        { std::ofstream file(root/"spline.bin",std::ios::binary);
          file.write(reinterpret_cast<const char*>(times.data()),times.size()*sizeof(float));
          file.write(reinterpret_cast<const char*>(values.data()),values.size()*sizeof(float));
          require(bool(file),"Cannot write spline fixture"); }
        std::ostringstream json;
        json<<R"({"asset":{"version":"2.0"},"buffers":[{"uri":"spline.bin","byteLength":)"<<(times.size()+values.size())*4
            <<R"(}],"bufferViews":[{"buffer":0,"byteLength":)"<<times.size()*4
            <<R"(},{"buffer":0,"byteOffset":)"<<times.size()*4<<R"(,"byteLength":)"<<values.size()*4
            <<R"(}],"accessors":[{"bufferView":0,"componentType":5126,"type":"SCALAR","count":)"<<times.size()
            <<R"(},{"bufferView":1,"componentType":5126,"type":"VEC)"<<components<<R"(","count":)"<<values.size()/components
            <<R"(}],"nodes":[{}],"scenes":[{"nodes":[0]}],"scene":0,"animations":[{"samplers":[{"input":0,"output":1,"interpolation":"CUBICSPLINE"}],"channels":[{"sampler":0,"target":{"node":0,"path":")"
            <<target<<R"("}}]}]})";
        write(json.str());
    };
    using namespace mh4u;
    std::vector<float> translations{0,0,0, 0,0,0, 3,0,0, -2,0,0, 4,0,0, 0,0,0};
    write_spline({2,4},translations,3,"translation");
    auto spline=load_gltf(root/"scene.gltf");
    near(sample_animation(spline,0,0,false)[0].translation.x,0,"Spline clamps before its first key");
    near(sample_animation(spline,0,2.5f,false)[0].translation.x,1.65625f,"Spline tangent units and row ordering");
    near(sample_animation(spline,0,3,false)[0].translation.x,3.25f,"Spline midpoint differs from linear");
    near(sample_animation(spline,0,9,false)[0].translation.x,4,"Spline clamps after its last key");
    const float s=std::sqrt(0.5f);
    // These unit vertices/tangents describe normalize((1-t)*(0,0,0,2)+t*(0,0,1,1)).
    const std::vector<float> rotations{0,0,0,0, 0,0,0,1, 0,0,0.25f,s-0.75f,
                                     0,0,s-0.25f,-0.25f, 0,0,s,s, 0,0,0,0};
    write_spline({2,4},rotations,4,"rotation"); spline=load_gltf(root/"scene.gltf");
    auto actual=sample_animation(spline,0,2.5f,false)[0].rotation;
    near(actual[2],std::sqrt(0.02f),"Cubic quaternion curve preserves unnormalized endpoint magnitudes");
    near(actual[3],std::sqrt(0.98f),"Cubic quaternion result normalized");
    actual=sample_animation(spline,0,3,false)[0].rotation;
    near(actual[2],1/std::sqrt(10.0f),"Cubic quaternion midpoint");
    near(actual[3],3/std::sqrt(10.0f),"Cubic quaternion midpoint W");
    const std::vector<float> signed_rotations{0,0,0,0, 0,0,0,1, 0,0,0,0,
                                            0,0,0,0, 0,0,s,-s, 0,0,0,0};
    write_spline({2,4},signed_rotations,4,"rotation"); spline=load_gltf(root/"scene.gltf");
    actual=sample_animation(spline,0,3,false)[0].rotation;
    near(actual[2],std::sin(3.14159265358979323846f*3/8),"Spline must not apply automatic hemisphere correction");
    near(actual[3],std::cos(3.14159265358979323846f*3/8),"Spline preserves authored quaternion signs");
    auto malformed=translations;malformed.resize(15);
    write_spline({2,4},malformed,3,"translation"); rejects([&]{load_gltf(root/"scene.gltf");});
    write_spline({2},{0,0,0, 0,0,0, 0,0,0},3,"translation"); rejects([&]{load_gltf(root/"scene.gltf");});
    malformed=translations;malformed[6]=std::numeric_limits<float>::quiet_NaN();
    write_spline({2,4},malformed,3,"translation"); rejects([&]{load_gltf(root/"scene.gltf");});
    malformed=rotations;malformed[7]=2;
    write_spline({2,4},malformed,4,"rotation"); rejects([&]{load_gltf(root/"scene.gltf");});
    std::cout<<"glTF CUBICSPLINE: row layout, per-second tangents, endpoint clamping, quaternion curves/signs and malformed data passed\n";
}
void real_asset(const std::filesystem::path& path) {
    using namespace mh4u;
    const auto scene=load_gltf(path); SceneFrame bind;
    evaluate_scene(scene,-1,0,false,bind);
    // The MH4U exporter explicitly promises a world-space bind pose for this probe.
    float max_bind_error=0; double sum_squared=0; std::size_t count=0;
    for(const auto& instance:scene.instances) {
        require(instance.skin>=0,"Real-actor probe expects skinned geometry");
        const auto& primitive=scene.primitives[instance.primitive];
        for(auto index:primitive.indices) {
            const auto expected=primitive.vertices[index].vertex.position,actual=bind.mesh.triangles[count].position;
            const auto error=std::hypot(actual.x-expected.x,actual.y-expected.y,actual.z-expected.z);
            max_bind_error=std::max(max_bind_error,error); sum_squared+=double(error)*error; ++count;
        }
    }
    require(max_bind_error<0.001f,"Real actor fails exported bind-pose agreement");
    require(!scene.animations.empty(),"Real actor animation missing");
    float max_motion=0;
    for(std::size_t c=0;c<scene.animations.size();++c) {
        SceneFrame first,current; evaluate_scene(scene,static_cast<int>(c),0,false,first);
        for(int sample=1;sample<=4;++sample) {
            evaluate_scene(scene,static_cast<int>(c),scene.animations[c].duration*sample/4,false,current);
            require(first.mesh.triangles.size()==current.mesh.triangles.size(),"Animated topology changed");
            for(std::size_t i=0;i<current.mesh.triangles.size();++i) {
                const auto a=first.mesh.triangles[i].position,b=current.mesh.triangles[i].position;
                max_motion=std::max(max_motion,std::hypot(a.x-b.x,a.y-b.y,a.z-b.z));
                const auto n=current.mesh.triangles[i].normal; near(std::hypot(n.x,n.y,n.z),1,"Real animated normal must stay normalized");
            }
        }
    }
    require(max_motion>0.001f,"Real animation produces no measurable vertex motion");
    std::cout<<"REAL_BIND_MAX_ERROR "<<max_bind_error<<"\nREAL_BIND_RMS_ERROR "<<std::sqrt(sum_squared/count)
             <<"\nREAL_MAX_MOTION "<<max_motion<<"\nREAL_ANIMATIONS "<<scene.animations.size()<<'\n';
}
}
int main(int argc,char** argv) {
    try {
        mathematical_tests();
        loader_tests();
        if(argc==2) { std::u8string value; for(unsigned char c:std::string(argv[1])) value.push_back(static_cast<char8_t>(c)); real_asset(std::filesystem::path(value)); }
        else require(argc==1,"Usage: mh4u-scene-tests [exported-animated-actor.gltf]");
        return 0;
    } catch(const std::exception& error) { std::cerr<<error.what()<<'\n'; return 1; }
}
