#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace mh4u::collision_resource {
using Bits3=std::array<std::uint32_t,3>;
using Bits4=std::array<std::uint32_t,4>;
enum class ErrorCode {io,invalid_expected,hash_mismatch,unsupported_format,malformed_resource,resource_limit,index_out_of_range,unsupported_grid,unresolved_grid};
struct Error {ErrorCode code;std::string message;std::size_t byte_offset{};std::optional<std::uint32_t> group;};
template<class T>using Result=std::variant<T,Error>;
struct Limits {
 std::size_t bytes=64U*1024U*1024U,records=2U*1024U*1024U;
 std::size_t grid_cells=4U*1024U*1024U,grid_indices=16U*1024U*1024U;
 std::size_t representation_bytes=128U*1024U*1024U;
}; // Adjustable allocation/work policies, not inferred original game maxima.
struct Range {std::uint32_t first{},count{};bool operator==(const Range&)const=default;};
struct Section {std::uint32_t offset{},count{},stride{};bool operator==(const Section&)const=default;};
struct Header {
 std::uint32_t magic{},version{},global_tree_type{},group_tree_type{};
 std::uint16_t group_count{},attribute_count{};
 std::uint32_t pair_count{},face_count{},vertex_count{};
 std::array<std::uint32_t,2> unknown20;
 Bits4 bbox_min,bbox_max;
};
struct Group {
 std::uint32_t source_offset{};Bits4 bbox_min,bbox_max;
 std::array<std::uint32_t,3> source_fields20; // Source bytes; loader's pointer fields are separate.
 Range pairs,faces,vertices;std::uint32_t index_id_raw{};
};
struct Pair {
 std::uint32_t source_offset{};
 std::uint16_t face_a_local{},face_b_local{}; // FFFF means absent second face.
 std::array<std::uint8_t,4> order_raw;
 std::uint16_t attribute_id{};
};
struct Face {
 std::uint32_t source_offset{};Bits3 normal_bits;
 std::array<std::uint16_t,3> vertex_local;
 std::uint16_t attribute_id{};std::uint32_t field14{};
 std::array<std::uint8_t,3> adjacency_raw;
 std::uint8_t field1b{};std::uint32_t field1c{};
};
struct Vertex {std::uint32_t source_offset{};Bits4 position_bits;};
struct Attribute {std::uint32_t source_offset{};std::array<std::uint32_t,8> words;};
struct Cell {std::uint16_t count{},unknown{};std::uint32_t start{};};
struct PairBounds {std::array<std::uint32_t,7> words;}; // Exact28source bytes; no added runtime padding.
struct Grid {
 std::uint32_t group{},source_offset{},version{};Bits4 bbox_min,bbox_max;
 std::uint16_t nx{},nz{};std::uint8_t index_kind{},compressed{};
 Section cells_source,indices_source,bounds_source;
 std::span<const Cell> cells;
 std::span<const std::uint16_t> pair_ids; // Group-local; repeated indices remain ordered.
 std::span<const PairBounds> pair_bounds;
};
struct ResolvedFace {
 std::uint32_t id{};const Face* face{};
 std::array<std::uint32_t,3> vertex_ids;
 std::array<const Vertex*,3> vertices;
};
struct ResolvedPair {
 std::string_view source_sha256;std::uint32_t group_id{},pair_id{};
 const Group* group{};const Pair* pair{};const Attribute* attribute{};
 ResolvedFace first;std::optional<ResolvedFace> second;
};
// Immutable source-bound SBC2010091000 facts. Copies share lifetime; all views
// borrow storage and require any Resource copy to stay alive. SHA is content
// identity; IDs and byte offsets remain source-local. No synthetic instance ID.
// Parsing does not normalize floating bits or decode collision semantics.
class Resource {
public:
 static Result<Resource> load(const std::filesystem::path&,std::string_view expected_sha256,Limits={});
 static Result<Resource> decode(std::string_view bytes,std::string_view expected_sha256,Limits={});
 std::string_view sha256()const noexcept;
 std::span<const std::byte> bytes()const noexcept;
 const Header& header()const noexcept;
 std::array<Section,5> sections()const noexcept; // groups,pairs,faces,vertices,attributes
 std::span<const Group> groups()const noexcept;
 std::span<const Pair> pairs()const noexcept;
 std::span<const Face> faces()const noexcept;
 std::span<const Vertex> vertices()const noexcept;
 std::span<const Attribute> attributes()const noexcept;
 Result<ResolvedPair> resolve_pair(std::uint32_t group,std::uint32_t local_pair)const;
 // Eager parse, O(1) borrowed lookup. Unsupported/malformed grids never become
 // empty success. Core facts remain usable; a failed section leaves subsequent
 // section offsets unknown. Only compressed GRCO kind1 is currently supported.
 Result<Grid> grid(std::uint32_t group)const;
 Result<std::span<const std::uint16_t>> cell_pairs(std::uint32_t group,std::uint32_t cell)const;
 std::uint32_t core_end_offset()const noexcept;
 std::span<const std::byte> acceleration_bytes()const noexcept;
 // Known only after every supported group section; contents remain unparsed.
 std::optional<std::uint32_t> global_tail_offset()const noexcept;
private:
 struct Data;
 explicit Resource(std::shared_ptr<const Data> data):data_(std::move(data)){}
 static Result<Resource> decode_owned(std::string,std::string_view,Limits);
 std::shared_ptr<const Data> data_;
};
} // namespace mh4u::collision_resource
