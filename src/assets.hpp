#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>

namespace mh4u {
struct Vec2 { float x{}, y{}; };
struct Vec3 { float x{}, y{}, z{}; };
struct Vertex { Vec3 position; Vec3 normal; Vec2 uv; };
struct Mesh {
    std::vector<Vertex> triangles;
    Vec3 minimum{}, maximum{}, center{};
    float radius{};
    std::size_t source_positions{}, material_groups{};
    bool has_uv{};
};
struct Image {
    int width{}, height{};
    std::vector<std::uint8_t> rgb;
    std::vector<std::uint8_t> alpha;
};
Mesh load_obj(const std::filesystem::path& path);
Image load_ppm(const std::filesystem::path& path);
void apply_pgm_alpha(Image& image, const std::filesystem::path& path);
void write_ppm(const std::filesystem::path& path, const Image& image);
}  // namespace mh4u
