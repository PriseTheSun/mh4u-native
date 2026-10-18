#pragma once
#include "player_parameter_query.hpp"
#include "player_input_filter.hpp"
#include "binary32_arithmetic.hpp"

namespace mh4u::player_update_statistics {
using Word=std::uint32_t;
using Unknown=player_parameter_query::Unknown;
using Absent=player_parameter_query::Absent;
using BonusRecord=player_fields::BonusRecord;
struct CurrentPlayer {};
struct PlayerIndex {std::uint32_t value;bool operator==(const PlayerIndex&)const=default;};
using PlayerLink=std::variant<Unknown,Absent,CurrentPlayer,PlayerIndex>;
struct PlayerView {
    const player_transition::PbwFields* pbw{};
    std::span<const BonusRecord> bonus236;
};
struct PlayerRegistry {
    std::optional<std::int32_t> count; // fb83e8+aa8; slots begin at a98.
    // Reaching index4 consumes the count word itself in the source layout.
    // This logical-reference profile rejects that overlap, but count>4 is
    // valid when a prior slot returns. This is not a source capacity claim.
    std::span<const PlayerLink> slots;
    std::span<const PlayerView> players;
};
using Registry=std::variant<Unknown,Absent,const PlayerRegistry*>;
struct BonusTable {std::span<const BonusRecord> records;};
using Mode2Bonuses=std::variant<Unknown,Absent,BonusTable>; // fcf7ec+cc50,32 records.
struct RemoteBuffs {
    std::array<std::optional<std::uint8_t>,3> ids; // PBW2c pointee+176..178.
    std::array<std::optional<std::int16_t>,3> values; // +17a..17e.
};
using Remote=std::variant<Unknown,Absent,const RemoteBuffs*>;
struct Context {
    player_parameter_query::Resources parameters;
    Mode2Bonuses mode2_bonuses=Unknown{};
    Registry players=Unknown{};
    Remote remote_buffs=Unknown{};
    std::optional<Word> clock2c_bits;
    std::optional<Word> manager803c,manager8040,manager8044,managerad5c;
    // Work budget, not an asserted source array maximum. Checked lazily while
    // scanning so a known first match does not require later entries.
    std::size_t registry_work_limit=256;
};
struct QueryObservation {
    Word selector;
    std::int32_t result;
    player_parameter_query::Effects effects;
};
struct Effects {
    std::array<QueryObservation,7> queries{};
    std::size_t query_count{},bonus_lookups{},registry_searches{},registry_slots{},buff_queries{};
    std::array<std::uint8_t,2> helper_flags{};
    bool local{};
};
enum class Phase {first_query,second_query,derived_fields,five_parameters};
enum class ErrorCode {unknown_field,absent_reference,reference_extent,capacity,dependency,division_zero,unsupported_registry_overlap};
using Cause=std::variant<std::monostate,player_parameter_query::Error,
    player_selection::Error,binary32_arithmetic::Error>;
struct Error {Phase phase;ErrorCode code;std::string_view field;Cause cause;};
struct Output {player_fields::Fields fields;Effects effects;};
using Result=std::variant<Output,Error>;
// b142d0 through the returned b0c86c, stopping before b14384. The two local
// helpers and all five unconditional parameter queries execute their actual
// resource operations. Shared ManagerAD50/AD64 are borrowed from the prefix.
// All referenced resources stay immutable during this synchronous call.
// No publication, scheduling, physical input mapping or clock tick occurs.
[[nodiscard]] Result apply(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const player_input_filter::Manager&,const Context&) noexcept;
} // namespace mh4u::player_update_statistics
