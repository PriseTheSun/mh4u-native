#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace mh4u::net {
using Digest=std::array<std::uint8_t,32>;
struct Content { Digest digest; std::string hex; std::vector<std::string> clips; };
Content verify_manifest(const std::filesystem::path&);
struct WorldIdentity {
    Content content;
    std::string document; // Canonical recipe/resource inventory; hashed locally.
    std::size_t file_count{};
    std::uint64_t byte_count{};
};
// Includes the recipe, static core glTF scenes and either a core glTF actor or
// the native actor loader's validated MOD/LMT/texture/command dependencies.
// Path roles are explicit, and all files must stay inside recipe.parent_path().
WorldIdentity identify_world(const std::filesystem::path& recipe,
    const std::array<std::filesystem::path,4>& scenes);
std::filesystem::path utf8_path(const std::string&);
}
