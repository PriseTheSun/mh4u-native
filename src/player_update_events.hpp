#pragma once
#include "player_update_statistics.hpp"
#include "player_position_fields.hpp"
#include "motion_clock.hpp"

namespace mh4u::player_update_events {
using Word=std::uint32_t;
struct Context {
    const player_update_statistics::Registry& players;
    motion_clock::EventResolver events;
    // Actual lookup work, not a bound on the source ID or row-table capacity.
    std::size_t registry_work_limit=256;
};
struct Lookup {
    std::uint8_t id;
    Word selector,source_offset,value;
    bool operator==(const Lookup&)const=default;
};
struct PrefixRead {
    std::uint8_t bit;
    std::uint16_t value;
    std::optional<motion_request::Binding> binding;
    bool operator==(const PrefixRead&)const=default;
};
struct Effects {
    std::array<Lookup,2> lookups{};
    std::array<Word,2> word808_writes{};
    std::array<PrefixRead,3> prefix_reads{};
    std::uint8_t lookup_count{},word808_write_count{},predicate_calls{},
        prefix_read_count{},prefix_store_count{},bits_examined{};
    bool copied_heading{};
    bool operator==(const Effects&)const=default;
};
enum class Phase {lookup,flags,heading,prefix};
enum class ErrorCode {unknown_field,absent_registry,registry_extent,capacity,
    layer_extent,unresolved_prefix,prefix_identity,unknown_prefix};
struct Error {
    Phase phase;
    ErrorCode code;
    std::string_view field;
    std::optional<motion_request::Binding> binding{};
    std::optional<motion_clock::ResolutionError> resolution{};
};
struct Output {
    player_controller_step::PlayerFields player;
    player_fields::Fields fields;
    Effects effects;
};
using Result=std::variant<Output,Error>;
// Original b14724 through BEFORE b148f0. af7378 uses the same registry
// singleton as statistics; it reads indexed words, not the Player scan.
// The current layer0 binding/mask comes from the returned model candidate.
// A null prefix yields a selected numeric0; a present prefix is resolved from
// its retained immutable identity. Even the third active bit reads before
// exiting. No clock, bank reassignment, parser or external publication occurs.
[[nodiscard]] Result apply(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const player_position_bridge::Fields&,
    std::span<const motion_pending::Layer>,const Context&);
}
