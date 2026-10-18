#pragma once
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include "player_reference.hpp"
#include <array>
#include <optional>
#include <variant>

namespace mh4u::player_object_query {
struct Unknown {bool operator==(const Unknown&)const=default;};
struct Absent {bool operator==(const Absent&)const=default;};
struct ObjectIndex {std::uint32_t value;bool operator==(const ObjectIndex&)const=default;};
struct DataIndex {std::uint32_t value;bool operator==(const DataIndex&)const=default;};
struct OverrideIndex {std::uint32_t value;bool operator==(const OverrideIndex&)const=default;};
using OverrideLink=std::variant<Unknown,Absent,OverrideIndex>;
using Slot=std::variant<Unknown,Absent,ObjectIndex>;
using DataLink=std::variant<Unknown,Absent,DataIndex>;
// Root Object and D=*(Object+e28) have distinct identities: different roots
// may share one D, and multiple slots may reference the same root.
struct Object {DataLink data;bool operator==(const Object&)const=default;};
struct DataFields {
    std::optional<std::uint8_t> field140,field34c,field290,field1c0,field14c;
    std::optional<std::uint32_t> field1b4;
    // Per-component knowledge: the heading paths consume only X and Z.
    std::array<std::optional<std::uint32_t>,3> position_bits{}; // D+198/19c/1a0
    std::optional<std::uint8_t> field349{};
    std::optional<std::uint16_t> field1c4{};
    // Consumed by the separate68da18 owner; no resource/radius is guessed here.
    std::optional<std::uint32_t> field78b8_bits{},field1bc_bits{};
    std::optional<std::uint8_t> field78e5{},field278{},field27c{},field1ca{},field1cb{},field797e{};
    OverrideLink field580{};
    std::optional<std::uint8_t> field530{}; // 9547a0, consumed by686f08.
    bool operator==(const DataFields&)const=default;
};
struct Collection {
    static constexpr std::size_t capacity=32; // At most32 reachable roots/D from32 slots.
    std::array<Slot,capacity> slots{};
    std::array<Object,capacity> objects{};
    std::array<DataFields,capacity> data{};
    bool operator==(const Collection&)const=default;
};
// Synchronous borrowed singleton snapshot; no ARM address or inferred empty
// collection. Known-null is distinct from unknown. Links are checked lazily.
using Manager=std::variant<Unknown,Absent,const Collection*>;
enum class ErrorCode {unknown_field,absent_manager,absent_data,invalid_index,
    unsupported_availability,unsupported_position,reference};
struct Error {
    ErrorCode code;std::string_view field;
    std::optional<std::size_t> slot;
    std::optional<player_reference::Error> reference;
    std::optional<std::size_t> object{},data{};
};
struct ResolvedData {DataIndex index;const DataFields* fields;};
struct ResolvedObject {ObjectIndex index;ResolvedData data;};
// Borrowed resolution is the sole owner of graph interpretation. Null slots
// are ordinary skips; null root data is a consumed dereference failure.
[[nodiscard]] std::variant<const Collection*,Error> resolve_manager(const Manager&) noexcept;
[[nodiscard]] std::variant<ResolvedData,Error> resolve(const Collection&,ObjectIndex) noexcept;
[[nodiscard]] std::variant<std::optional<ResolvedObject>,Error> resolve_slot(const Collection&,std::size_t) noexcept;
struct Output {
    bool matched{};
    std::uint8_t slots_examined{},object_queries{},direction_queries{};
};
using Result=std::variant<Output,Error>;
// Shared original91f214(object,Player,0), local availability profile only.
// Callers retain their own order relative to mask/enabled/other source reads.
[[nodiscard]] std::variant<bool,Error> availability(
    const player_controller_step::PlayerFields&,const DataFields&) noexcept;
// b34268 ->6823a8/911e48/91a2a8. PW9a8 can return before reading Manager.
// The collection is never modified. No unknown fact becomes a false result.
[[nodiscard]] Result query(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const Manager&) noexcept;
struct ScanEffects {
    std::uint8_t slots_examined{},enabled_queries{},object_queries{},
        availability_queries{},direction_queries{},mask_stores{};
    bool operator==(const ScanEffects&)const=default;
};
struct ScanOutput {
    Collection collection;
    bool matched{};
    std::optional<ObjectIndex> matched_object;
    std::optional<DataIndex> matched_data;
    ScanEffects effects;
};
using ScanResult=std::variant<ScanOutput,Error>;
// Complete686fc0 ->911e48/9368c4 in the shared local91f214 profile.
// A successful false result preserves D349 stores in the returned candidate;
// any typed failure discards that candidate. Source singleton/root/D bindings
// remain immutable throughout this synchronous profile, but D fields may alias.
[[nodiscard]] ScanResult scan(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const Manager&) noexcept;
struct StatusScanOutput {
    bool matched{};
    std::uint8_t slots_examined{},enabled_queries{},status_queries{},availability_queries{};
};
// 686f08: slot ->911e48 ->D1b4 bit0 ->9547a0 ->91f214. Read-only;
// shared91f214 still rejects the unrecovered remote-availability path.
[[nodiscard]] std::variant<StatusScanOutput,Error> scan_status(
    const player_controller_step::PlayerFields&,const Manager&) noexcept;
} // namespace mh4u::player_object_query
