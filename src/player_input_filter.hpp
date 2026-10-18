#pragma once
#include "player_input_block.hpp"
#include <string_view>

namespace mh4u::player_input_filter {
using Block=player_input_block::Block;
// Transient facts outside the 196-byte input block. These are raw source
// values; project_facts borrows them from the existing canonical owners.
struct Facts {
    std::optional<std::uint8_t> global2b,global2f,globald72,pbw30,pbw33,pbw371;
    std::optional<std::uint16_t> pbw372;
    std::optional<std::uint32_t> pw13c_bits,pbw464_bits,pwe8;
    std::optional<std::uint8_t> pw9dc,pw9de,pw9dd;
};
struct Unknown {};
struct Absent {};
struct Mode2Object {std::optional<std::uint8_t> field984c,field982f;};
struct Manager {
    std::optional<std::uint8_t> ad24;
    std::optional<std::uint32_t> ad50,ad64;
    std::optional<std::int8_t> ad20;
};
struct Pad {
    std::optional<std::uint16_t> field31e,field31c,field320,field32c,field32e,field338,field33a;
};
struct Context {
    std::optional<std::int8_t> object20,object21; // Object at fb5edc.
    std::variant<Unknown,Absent,Mode2Object> mode2=Unknown{}; // Pointer edbe90.
    Manager manager; // Same fbeffc object for both source helpers.
    std::optional<std::uint8_t> byte108b9b8,byte1083850,byte109bd29;
    Pad pad;
};
struct Effects {
    bool local{},cleared{},restored{},suppressed{},lane6_preserved{};
    bool operator==(const Effects&)const=default;
};
enum class ErrorCode {unknown_field,projection};
struct Error {
    ErrorCode code;std::string_view field;
    std::optional<player_input_block::Error> projection{};
};
struct BlockOutput {Block block;Effects effects;};
using BlockResult=std::variant<BlockOutput,Error>;
// Complete b33638 helper closure, disjoint objects and immutable snapshots.
// Unknown bytes can be preserved or defined by stores. No FPSCR flags/traps,
// pointer aliases, physical-device acquisition or complete b1407c scheduler.
BlockResult apply_block(const Block&,const Facts&,const Context&) noexcept;
Facts project_facts(const player_controller_step::PlayerFields&,const player_fields::Fields&) noexcept;
struct Output {player_controller_step::PlayerFields player;player_fields::Fields fields;Effects effects;};
using Result=std::variant<Output,Error>;
// Canonical wrapper calls the same core, then publishes transactionally.
Result apply(const player_controller_step::PlayerFields&,const player_fields::Fields&,const Context&) noexcept;
} // namespace mh4u::player_input_filter
