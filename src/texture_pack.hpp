#pragma once
#include "assets.hpp"
#include <map>
#include <string>

namespace mh4u {
struct TextureReplacement { Image pixels; std::string encoded_sha256; };
struct TexturePack {
    std::string name;
    // Keys identify encoded original PNG bytes, including images embedded in a GLB.
    std::map<std::string,TextureReplacement> replacements;
};
// Reads and validates every replacement once; no resource is changed on disk.
TexturePack load_texture_pack(const std::filesystem::path& manifest);
}
