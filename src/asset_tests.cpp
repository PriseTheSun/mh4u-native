#include "assets.hpp"
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>

int main() {
    const auto root=std::filesystem::temp_directory_path()/"mh4u-resource-lab-asset-tests";
    if(!std::filesystem::create_directory(root)) { std::cerr<<"Test directory already exists\n"; return 1; }
    struct Cleanup { std::filesystem::path p; ~Cleanup(){ std::error_code ignored; std::filesystem::remove_all(p,ignored); } } cleanup{root};
    auto write=[&](const char* name,const std::string& value){ std::ofstream f(root/name,std::ios::binary); f.write(value.data(),value.size()); return root/name; };
    auto reject=[&](const std::function<void()>& task){ try { task(); } catch(const std::runtime_error&) { return; } throw std::runtime_error("Malformed input was accepted"); };
    try {
        const std::string vertices="v 0 0 0\nv 1 0 0\nv 0 1 0\n";
        auto triangle=mh4u::load_obj(write("triangle.obj",vertices+"f -3 -2 -1\n"));
        if(triangle.triangles.size()!=3 || triangle.triangles[0].normal.z!=1) throw std::runtime_error("Negative index/normal mismatch");
        reject([&]{mh4u::load_obj(write("bad-index.obj",vertices+"f 1 2 4\n"));});
        reject([&]{mh4u::load_obj(write("nonfinite.obj","v nan 0 0\n"));});
        reject([&]{mh4u::load_obj(write("quad.obj",vertices+"f 1 2 3 1\n"));});
        std::string ppm="P6\n# comment\n1 1\n255\n"; ppm+=char(10); ppm+=char(32); ppm+=char(255);
        auto image=mh4u::load_ppm(write("pixel.ppm",ppm));
        if(image.rgb!=std::vector<std::uint8_t>{10,32,255}) throw std::runtime_error("Whitespace-valued P6 pixels lost");
        reject([&]{mh4u::load_ppm(write("large.ppm","P6\n999999 999999\n255\n"));});
        reject([&]{mh4u::load_ppm(write("truncated.ppm","P6\n2 2\n255\nxyz"));});
        mh4u::apply_pgm_alpha(image,write("alpha.pgm","P2\n1 1\n15\n8\n"));
        if(image.alpha!=std::vector<std::uint8_t>{136}) throw std::runtime_error("PGM depth rescaling mismatch");
        reject([&]{mh4u::apply_pgm_alpha(image,write("wrong-size.pgm","P2\n2 1\n255\n0 255\n"));});
        if(image.alpha!=std::vector<std::uint8_t>{136}) throw std::runtime_error("Rejected alpha modified prior image");
        std::cout<<"9 asset boundary checks passed\n";
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 1; }
}
