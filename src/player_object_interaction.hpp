#pragma once
#include "binary32_arithmetic.hpp"
#include "player_object_query.hpp"
#include <span>

namespace mh4u::player_object_interaction {
namespace oq=player_object_query;
struct EntryIndex {std::uint32_t value;bool operator==(const EntryIndex&)const=default;};
struct RecordIndex {std::uint32_t value;bool operator==(const RecordIndex&)const=default;};
struct ParameterIndex {std::uint32_t value;bool operator==(const ParameterIndex&)const=default;};
template<class Index>using Link=std::variant<oq::Unknown,oq::Absent,Index>;
// f3a020 -> table[kind] -> Entry -> Record. Indices preserve pointer aliasing
// without exposing ARM addresses. All views are borrowed for this call only.
struct Entry {Link<RecordIndex> record;};
struct Record {Link<ParameterIndex> field4c,fieldb4;};
struct Parameters {std::optional<std::uint32_t> field30_bits,field80_bits;};
struct Override {std::optional<std::uint32_t> field_e4_bits;};
struct Registry {
    std::span<const Link<EntryIndex>> table;
    std::span<const Entry> entries;
    std::span<const Record> records;
    std::span<const Parameters> parameters;
    std::span<const Override> overrides;
};
using Resources=std::variant<oq::Unknown,oq::Absent,const Registry*>;
enum class ErrorCode {unknown_field,null_binding,invalid_index,arithmetic,object_query};
struct Error {
    ErrorCode code;std::string_view field;
    std::optional<std::size_t> slot,object,data,index;
    std::optional<binary32_arithmetic::Error> arithmetic;
    std::optional<oq::Error> object_query;
};
struct Effects {
    std::uint8_t slots_examined{},enabled_queries{},category_queries{},
        provider_queries{},sphere_queries{};
    bool operator==(const Effects&)const=default;
};
struct Output {std::optional<oq::ObjectIndex> selected;Effects effects;};
using Result=std::variant<Output,Error>;
// Complete68da18 for a present Player: ordered32-slot search and the actual
// provider lookup, 937f94 override and20c044 sphere test. No field is mutated.
// Reached unknown/null/invalid references are errors, never false candidates.
// Source D797e is loaded by91d878 but its value is unused by6881d8: this
// semantic projection intentionally does not require that dead byte known.
// Registry/Collection bindings remain immutable throughout the synchronous
// call; no loader, resource type, lifetime or initial registry is inferred.
[[nodiscard]] Result query(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const oq::Manager&,const Resources&) noexcept;
} // namespace mh4u::player_object_interaction
