#include "collision.hpp"
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
using mh4u::collision::Point;
double number(const std::string& value) {
    char* end=nullptr; const auto result=std::strtod(value.c_str(),&end);
    if(end==value.c_str()||*end||!std::isfinite(result)) throw std::runtime_error("Expected a finite number: "+value);
    return result;
}
std::string quote(const std::string& value) {
    std::string result="\""; const char* hex="0123456789abcdef";
    for(unsigned char c:value) {
        if(c=='"'||c=='\\') { result+='\\'; result+=static_cast<char>(c); }
        else if(c<32) { result+="\\u00"; result+=hex[c>>4]; result+=hex[c&15]; }
        else result+=static_cast<char>(c);
    }
    return result+'"';
}
void point(Point p) { std::cout<<'['<<p.x<<','<<p.y<<','<<p.z<<']'; }
void help() {
    std::cout<<"mh4u-collision-query --scene FILE --origin X Y Z --direction X Y Z --max-distance D [--backfaces include|cull]\n"
               "mh4u-collision-query --scene FILE --origin X Y Z --end X Y Z [--backfaces include|cull]\n"
               "Queries static glTF geometry only; does not interpret original collision attributes. JSON on stdout.\n";
}
}
int main(int argc,char** argv) {
    try {
        std::string path; std::optional<Point> origin,direction,end; std::optional<double> distance;
        auto backfaces=mh4u::collision::Backfaces::include; bool policy_seen=false;
        auto argument=[&](int& i)->std::string { if(++i>=argc) throw std::runtime_error("Missing option value"); return argv[i]; };
        auto vector=[&](int& i) { const auto x=number(argument(i)),y=number(argument(i)),z=number(argument(i)); return Point{x,y,z}; };
        for(int i=1;i<argc;++i) {
            const std::string option=argv[i];
            if(option=="--help") { help(); return 0; }
            if(option=="--scene"&&!path.size()) path=argument(i);
            else if(option=="--origin"&&!origin) origin=vector(i);
            else if(option=="--direction"&&!direction) direction=vector(i);
            else if(option=="--end"&&!end) end=vector(i);
            else if(option=="--max-distance"&&!distance) distance=number(argument(i));
            else if(option=="--backfaces"&&!policy_seen) {
                const auto value=argument(i); policy_seen=true;
                if(value=="cull") backfaces=mh4u::collision::Backfaces::cull;
                else if(value!="include") throw std::runtime_error("Backface policy must be include or cull");
            } else throw std::runtime_error("Unknown or repeated option: "+option);
        }
        if(path.empty()||!origin||(end&&(direction||distance))||(!end&&(!direction||!distance)))
            throw std::runtime_error("Specify --scene, --origin, and either --end or --direction with --max-distance");
        std::u8string utf; for(unsigned char c:path) utf.push_back(static_cast<char8_t>(c));
        const auto world=mh4u::collision::load_static_gltf(std::filesystem::path(utf));
        const auto result=end?world.segmentcast(*origin,*end,backfaces):world.raycast(*origin,*direction,*distance,backfaces);
        const auto& stats=world.stats(); std::cout<<std::setprecision(17);
        std::cout<<"{\"schema\":\"mh4u-collision-query-v1\",\"scene\":"<<quote(path)
                 <<",\"mode\":"<<quote(end?"segment":"ray")<<",\"backfaces\":"<<quote(backfaces==mh4u::collision::Backfaces::include?"include":"cull")
                 <<",\"triangles\":"<<stats.triangles<<",\"degenerate_triangles\":"<<stats.degenerate_triangles
                 <<",\"bvh_nodes\":"<<stats.bvh_nodes<<",\"boxes_tested\":"<<result.stats.boxes_tested
                 <<",\"triangles_tested\":"<<result.stats.triangles_tested<<",\"hit\":";
        if(!result.hit) std::cout<<"null";
        else {
            const auto& hit=*result.hit;
            std::cout<<"{\"triangle_id\":"<<hit.triangle_id<<",\"instance\":"<<hit.source.instance<<",\"node\":"<<hit.source.node
                     <<",\"primitive\":"<<hit.source.primitive<<",\"primitive_triangle\":"<<hit.source.primitive_triangle
                     <<",\"distance\":"<<hit.distance<<",\"position\":"; point(hit.position);
            std::cout<<",\"normal\":"; point(hit.normal);
            std::cout<<",\"barycentric\":["<<hit.barycentric[0]<<','<<hit.barycentric[1]<<','<<hit.barycentric[2]
                     <<"],\"front_face\":"<<(hit.front_face?"true":"false")<<'}';
        }
        std::cout<<"}\n"; return 0;
    } catch(const std::exception& error) { std::cerr<<"{\"error\":"<<quote(error.what())<<"}\n"; return 1; }
}
