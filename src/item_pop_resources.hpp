#pragma once
#include "player_world_lookup.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::item_pop_resources {
using Word=std::uint32_t;
enum class Kind {set,list};
enum class ErrorCode {io,invalid_expected,hash_mismatch,unsupported_format,malformed_resource,uninitialized_range,resource_limit,wrong_kind,index_out_of_range};
enum class OffsetSpace {source,loaded};
struct Error {ErrorCode code;std::string message;std::size_t byte_offset{};std::optional<Word> group{},record{};OffsetSpace offset_space=OffsetSpace::source;};
template<class T>using Result=std::variant<T,Error>;
struct Limits {std::size_t bytes=16U*1024U*1024U,loaded_bytes=32U*1024U*1024U,records=1U<<20,representation_bytes=96U*1024U*1024U;};
struct Segment {Word buffer_offset{},source_offset{},size{};bool operator==(const Segment&)const=default;};
struct Descriptor {Word source_offset{},buffer_offset{},relative_offset{},count{};bool operator==(const Descriptor&)const=default;};
struct Primary {
 Word source_offset{},buffer_offset{};std::array<std::byte,36> raw{};
 Word member_word{};std::array<Word,3> position_bits{};Word radius_bits{},height_bits{};
 std::array<std::uint16_t,2> words18{};std::array<std::uint8_t,8> bytes1c{};
 bool operator==(const Primary&)const=default;
};
struct Position {
 Word source_offset{},buffer_offset{};std::array<std::byte,28> raw{};
 std::array<Word,3> position_bits{};Word radius_bits{},height_bits{};
 std::array<std::uint16_t,2> words14{};std::array<std::uint8_t,4> bytes18{};
 bool operator==(const Position&)const=default;
};
struct Choice {
 Word source_offset{},buffer_offset{};std::array<std::byte,6> raw{};
 std::uint8_t weight{},member{};std::array<std::uint8_t,2> counts{},flags{};
 bool operator==(const Choice&)const=default;
};
struct PositionSet {Descriptor descriptor;std::span<const Position> records;};
struct ChoiceSet {Descriptor descriptor;std::span<const Choice> records;};
struct Group {
 std::array<Descriptor,3> descriptors;
 std::span<const Primary> primary;
 // Getter23dd98 returns zero when the secondary table counts differ.
 Word secondary_count{};
 std::span<const PositionSet> positions;
 std::span<const ChoiceSet> choices;
};
struct Member {
 Descriptor descriptor;
 player_world_lookup::ResourceReference reference;
 std::optional<Word> source_offset; // Unknown if pointer lies in an unloaded gap.
};
// Source-bound IPS/IPL2003. No host or emulated addresses. Copies share immutable
// lifetime; spans require any Resource copy to remain alive. Segment gaps are not
// initialized to zero. Unsupported dynamic selection is not a parser operation.
class Resource {
public:
 static Result<Resource> load(const std::filesystem::path&,std::string_view expected_sha256,Limits={});
 static Result<Resource> decode(std::string_view,std::string_view expected_sha256,Limits={});
 Kind kind()const noexcept;
 std::string_view sha256()const noexcept;
 std::span<const std::byte> bytes()const noexcept;
 std::span<const Word> header()const noexcept;
 std::span<const Segment> segments()const noexcept;
 Word allocation_size()const noexcept;
 Word group_count()const noexcept;
 Result<Group> group(Word index)const;
 Result<std::span<const Descriptor>> members(unsigned table)const;
 // An out-of-table index is the original null result, not an error.
 Result<std::optional<Member>> member(unsigned table,Word index)const;
 Result<std::span<const std::byte>> loaded_bytes(Word offset,Word size)const;
private:
 struct Data;std::shared_ptr<const Data> data_;
 explicit Resource(std::shared_ptr<const Data> d):data_(std::move(d)){}
 static Result<Resource> decode_owned(std::string,std::string_view,Limits);
};
} // namespace mh4u::item_pop_resources
