#pragma once
#include "motion_transform_math.hpp"
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::model_resource {
using Matrix=motion_transform_math::Matrix;
enum class ErrorCode {io,invalid_expected,hash_mismatch,unsupported_format,
    unsupported_layout,resource_limit,malformed_resource,table_extent,identity_exhausted};
struct Error {ErrorCode code;std::string message;std::size_t byte_offset{};};
template<class T>using Result=std::variant<T,Error>;
struct Limits {
    std::size_t bytes=64U*1024U*1024U,joints=256,palettes=4096,primitives=65535;
    std::size_t geometry_vertices=1U*1024U*1024U,geometry_indices=4U*1024U*1024U,geometry_triangles=2U*1024U*1024U;
}; // Caller-adjustable allocation policies, not inferred game maxima.
struct Palette {
    std::uint32_t descriptor{},source_offset{}; // Count is descriptor low16.
    std::span<const std::uint8_t> joints;
};
struct Primitive {
    std::uint32_t source_offset{},flags0{},flags4{},format8{},field10{},input_layout_hash{};
    std::uint16_t source_group{},loaded_group{};
    std::uint16_t visibility_index{},material_index{};
    std::uint8_t palette_index{};
};
enum class VertexLayout {obj2wt,obj4wt,obj4wt_t};
struct Vertex {
    std::array<std::uint32_t,3> position_bits;
    std::array<std::uint32_t,2> uv_bits;
    std::array<std::int8_t,4> normal; // All four signed source bytes; no normalization.
    std::array<std::uint8_t,4> joints{},weights{}; // Palette indices, even for zero weights.
    std::uint8_t influence_count{}; //2 or4; slots beyond this count are absent, not source facts.
    std::optional<std::array<std::int8_t,4>> auxiliary; //Raw bytes28..31 only for stride36.
    bool operator==(const Vertex&)const=default;
};
using Triangle=std::array<std::uint32_t,3>; //Local vertex indices, expanded strip winding.
struct Geometry {
    std::uint64_t resource_identity{};
    std::uint32_t primitive_index{},vertex_source_offset{},index_source_offset{},vertex_start{};
    std::uint8_t stride{};
    VertexLayout layout{};
    std::span<const Vertex> vertices;
    std::span<const std::uint16_t> raw_indices; //Includes0xffff restart tokens.
    std::span<const Triangle> triangles;
    std::size_t restart_count{},degenerate_triangles{};
};
struct JointRecord {
    std::array<std::uint8_t,4> fields;
    std::array<std::uint32_t,5> parameter_bits;
    bool operator==(const JointRecord&)const=default;
};
struct Skeleton {
    std::span<const Matrix> local_matrices,source_skin_matrices;
    std::span<const std::byte> joint_records,remap;
    std::span<const Palette> palettes;
    std::span<const JointRecord> joint_records_parsed;
};
// Immutable SHA-bound MOD230 geometry facts. File bytes and runtime-derived
// group IDs are distinct. No input-layout handle relocation, GPU or rest-pose math.
// Views borrow shared storage; retain any Resource copy while using them.
// N==0 preserves an absent skeleton, even when the header declares palettes;
// loader6806c0 skips those file reads. Palette/header fields remain raw bits.
class Resource {
public:
    static Result<Resource> load(const std::filesystem::path&,std::string_view expected_sha256,Limits={});
    static Result<Resource> decode(std::string_view,std::string_view expected_sha256,Limits={});
    std::uint64_t identity()const noexcept; // Fresh per successful decode, shared by copies.
    std::string_view sha256()const noexcept;
    std::span<const std::byte> bytes()const noexcept;
    std::uint16_t joint_count()const noexcept;
    std::uint32_t declared_palette_count()const noexcept;
    std::uint32_t skeleton_source_offset()const noexcept;
    std::uint32_t primitives_source_offset()const noexcept;
    // nullopt is the loader's zero-joint branch, not an empty initialized table.
    std::optional<Skeleton> skeleton()const noexcept;
    std::span<const Primitive> primitives()const noexcept;
    Result<std::span<const std::byte>> primitive_source_record(std::size_t)const;
    // O(1) borrowed view, prepared once at load. Unsupported/malformed geometry
    // is a per-primitive error; it does not invalidate other source facts.
    // No Q, weight merging/normalization, normal normalization or UV conversion.
    Result<Geometry> geometry(std::size_t primitive)const;
private:
    struct Data;
    explicit Resource(std::shared_ptr<const Data> data):data_(std::move(data)){}
    static Result<Resource> decode_owned(std::string,std::string_view,Limits);
    std::shared_ptr<const Data> data_;
};
} // namespace mh4u::model_resource
