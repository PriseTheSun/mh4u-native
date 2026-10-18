#pragma once
#include "item_pop_secondary.hpp"

namespace mh4u::item_pop_builder {
using Word=std::uint32_t;
using Resource=item_pop_resources::Resource;
using Limits=item_pop_resources::Limits;
struct Context {
    Word id_counter{}; //5ba7b4 preserves the incoming manager+4a24.
    // Explicit snapshots of10e6370 and10e6270, never inferred BSS values.
    std::array<Word,3> position_template{},auxiliary_template{};
    player_transition::GlobalFields global; // Canonical global owner, including RNG channel1 (+22).
    player_selection::AreaContext area; // Borrowed for this call only.
};
struct PrimarySource {
    item_pop_secondary::Location location;
    std::string set_sha256;
};
struct SlotSource {
    Word group_iteration{},group_index{},record_index{};
    std::optional<PrimarySource> primary;
    std::optional<item_pop_secondary::Selection> secondary;
};
struct Built {
    player_world_lookup::Manager manager;
    player_transition::GlobalFields global;
    std::array<std::optional<SlotSource>,player_world_lookup::Manager::capacity> sources{};
    std::size_t allocated{},missing_members{},failed_secondary{},capacity_skips{},rng_calls{};
};
enum class ErrorCode {wrong_kind,resource_limit,resource_failure,selection_failure,unsupported_descriptor,secondary_failure};
struct Error {
    ErrorCode code;
    std::string_view message;
    std::optional<Word> group_iteration,record_index;
    std::optional<item_pop_resources::Error> resource;
    std::optional<player_selection::Error> selection;
    std::optional<item_pop_secondary::Error> secondary;
};
using Result=std::variant<Built,Error>;
// Entire5ba7b4, including real5b928c semantics, for a fresh/unattached pool.
// Null resources mean known absence: reset occurs first; null Set skips List.
// All source-written numeric/list fields are owned by Record/Manager. Aux10/14
// are absent by this construction profile, so no cleanup/virtual call is faked.
// Unknown mapping or out-of-descriptor remap fails transactionally. Padding,
// vtables and host pointers are not native state. Readiness19 remains0.
[[nodiscard]] Result build_fresh(const Resource* set,const Resource* list,
    const Context&,Limits={});
} // namespace mh4u::item_pop_builder
