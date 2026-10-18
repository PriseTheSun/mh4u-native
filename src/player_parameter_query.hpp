#pragma once
#include "player_fields.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

namespace mh4u::player_parameter_query {
struct Unknown {};
struct Absent {};
// Immutable synchronous view of a source snapshot. The caller binds the
// digest to its owning bytes; this query does not load files or rehash per call.
// source_offset is provenance within that snapshot, never an emulated address.
struct Bytes {
    std::span<const std::byte> bytes;
    std::string_view sha256;
    std::uint32_t source_offset{};
};
using Binding=std::variant<Unknown,Absent,Bytes>;
enum class Table {
    none,records24,records40,primary40,global,
    caps_by_kind,query_caps,special12,special4,special10,growth8,growth_steps
};
struct Resources {
    // Only source categories that reach each pointer table are represented.
    // Each pointee has a caller-declared extent; no record count is inferred.
    std::array<Binding,21> records24{},records40{};
    std::array<Binding,7> primary40{};
    Binding global{}; // Singleton payload, offsets 3304 and 3366 consumed lazily.
    Binding caps_by_kind{},query_caps{}; // f3fc0e and e05f30 snapshots.
    Binding special12{},special4{},special10{}; // f3eda8/f3f900/f3f998.
    Binding growth8{}; // f3f810.
    std::array<Binding,2> growth_steps{}; // f3ed5c, f3ed64.
};
enum class ErrorCode {
    unknown_field,unknown_resource,absent_resource,resource_extent,
    invalid_provenance,unsupported_selector,capacity
};
struct Error {
    ErrorCode code;
    std::string_view field;
    std::optional<std::size_t> descriptor{};
    Table table{Table::none};
    std::uint32_t kind{};
    std::size_t offset{},size{};
};
struct FlagWrite {
    std::uint8_t kind;
    std::uint32_t before,after;
    bool operator==(const FlagWrite&)const=default;
};
struct Effects {
    std::array<FlagWrite,7> writes{};
    std::size_t write_count{};
    bool operator==(const Effects&)const=default;
};
struct Output {
    player_fields::Fields fields;
    std::int32_t value{};
    Effects effects;
};
using Result=std::variant<Output,Error>;
// Complete selected 2f8e04 routes 0..6, with immutable resources throughout
// the call. Selectors 7..18 are explicit unsupported routes; >=19 returns 0.
// Player/PBW is present in this API. Null-player source early return is not a
// fabricated Fields object. No timer, physical control or gameplay is inferred.
[[nodiscard]] Result apply(const player_fields::Fields&,std::uint32_t selector,
    const Resources&) noexcept;
} // namespace mh4u::player_parameter_query
