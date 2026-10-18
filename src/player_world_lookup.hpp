#pragma once
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace mh4u::player_world_lookup {
using Word=std::uint32_t;
struct Unknown {bool operator==(const Unknown&)const=default;};
struct Absent {bool operator==(const Absent&)const=default;};
struct Index {std::size_t value;bool operator==(const Index&)const=default;};
using Link=std::variant<Unknown,Absent,Index>;
struct ResourceReference {
    std::string sha256;
    std::uint32_t buffer_offset{};
    bool operator==(const ResourceReference&)const=default;
};
// Raw ARM snapshots retain their word. Native resources carry identity and
// loaded-buffer offset instead of fabricating a source or host address.
using Field28=std::variant<Word,ResourceReference>;
enum class RecordUpdateBinding {item_pop_5aa4cc};
enum class AuxiliaryPresence {absent,present};
struct Record {
    // Explicit construction/snapshot facts. A lookup-only projection does not
    // imply a virtual target or the absence of attached auxiliary objects.
    std::optional<RecordUpdateBinding> update_binding;
    std::optional<AuxiliaryPresence> auxiliary10,auxiliary14;
    Link next=Unknown{}; // Source+8; order remains significant on equal rank.
    Link previous=Unknown{}; // Source+c; owned by the same native list.
    std::optional<std::uint8_t> field19,field2f,field54,field61,field78;
    std::optional<std::uint8_t> field4,field1a,field2e,field55,field56,field57,
        field60,field62,field63,field64,field79;
    std::optional<std::uint16_t> field30;
    std::optional<std::uint16_t> field2c,field32;
    std::array<std::optional<Word>,3> fields34{},fields40{};
    std::array<std::optional<Word>,3> fields68{},fields7c{};
    std::array<std::optional<Word>,2> fields8c{};
    std::optional<Word> field4c_bits,field50_bits;
    std::optional<Word> field1c_bits,field20_bits,field24,field58_bits,field5c_bits,
        field74,field88;
    std::optional<Field28> field28; // Copied without dereferencing.
    bool operator==(const Record&)const=default;
};
// Capacity belongs to constructor5bae4c/allocator5b8a1c's real128-slot pool.
// Source lookup has no cycle guard; malformed native links fail explicitly.
struct Manager {
    static constexpr std::size_t capacity=128;
    std::array<Record,capacity> records{};
    Link head=Unknown{}; // Source manager+4a1c. Unknown is not an empty list.
    std::optional<Word> field4a20,field4a24; // Linked count and allocator counter.
    bool operator==(const Manager&)const=default;
};
struct Context {
    const Manager* manager=nullptr; // Borrowed immutable owner, null means Unknown.
    std::optional<Word> area_mask14; // *(fcf45c)+14, distinct from spatial mask+18.
};
struct Request {
    std::optional<Word> area;
    std::array<std::optional<Word>,3> point{};
    bool copy_field28=true;
};
struct Match {
    std::size_t index{};
    std::array<Word,4> vector34{};
    std::array<Word,3> words40{};
    std::optional<Field28> field28;
    Word status{};
    bool operator==(const Match&)const=default;
};
struct Output {
    Word returned=0xffff;
    std::optional<Match> match;
    std::size_t visited{};
};
enum class ErrorCode {unknown_field,invalid_link,cycle,floating_environment};
struct Error {ErrorCode code;std::string_view field;std::optional<std::size_t> index{};};
using Result=std::variant<Output,Error>;
// Complete5b8f14→5ba61c, with real linked traversal and source priority table.
// Geometry uses nonfused binary32, RNE and gradual underflow, including source
// unordered/infinite/overflow decisions. FPSCR flags/traps are not emulated.
// Missing output on a miss preserves caller-owned destinations.
[[nodiscard]] Result query(const Context&,const Request&);
struct PlayerOutput {player_fields::Fields fields;Output lookup;};
using PlayerResult=std::variant<PlayerOutput,Error>;
// b140b0..b14104 caller publication: PBW9/XYZ, PW5a8=match status or0 on miss.
// Stack outputs are returned, not persisted as a second Player state.
[[nodiscard]] PlayerResult apply(const player_controller_step::PlayerFields&,
    const player_fields::Fields&,const Context&);
} // namespace mh4u::player_world_lookup
