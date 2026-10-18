#include "scene.hpp"
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

// Read-only test adapter around the production loader/sampler. Queries contain
// clip, node and seconds; output adds the sampled TRS. No independent math here.
int main(int argc,char** argv) {
    try {
        if(argc!=2) throw std::runtime_error("Usage: mh4u-animation-samples scene.gltf < queries.txt");
        std::u8string path;for(unsigned char c:std::string(argv[1])) path.push_back(static_cast<char8_t>(c));
        const auto scene=mh4u::load_gltf(std::filesystem::path(path));
        std::cout<<std::setprecision(std::numeric_limits<float>::max_digits10);
        std::size_t count=0;
        while(true) {
            std::cin>>std::ws;if(std::cin.eof()) break;
            long long clip,node;float seconds;
            if(!(std::cin>>clip>>node>>seconds)) throw std::runtime_error("Malformed query; expected clip node seconds");
            if(++count>65536||clip<0||clip>=static_cast<long long>(scene.animations.size())||node<0||node>=static_cast<long long>(scene.nodes.size())||!std::isfinite(seconds))
                throw std::runtime_error("Query outside animation/node/time or 65536-query budget");
            const auto pose=mh4u::sample_animation(scene,static_cast<int>(clip),seconds,false);
            const auto& t=pose[static_cast<std::size_t>(node)];
            std::cout<<clip<<' '<<node<<' '<<seconds<<' '<<t.translation.x<<' '<<t.translation.y<<' '<<t.translation.z;
            for(float q:t.rotation) std::cout<<' '<<q;
            std::cout<<' '<<t.scale.x<<' '<<t.scale.y<<' '<<t.scale.z<<'\n';
            if(!std::cout) throw std::runtime_error("Could not write sampled TRS");
        }
        return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
