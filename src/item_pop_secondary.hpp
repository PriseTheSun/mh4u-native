#pragma once
#include "item_pop_resources.hpp"
#include "player_selection.hpp"

namespace mh4u::item_pop_secondary {
using Word=std::uint32_t;
using Resource=item_pop_resources::Resource;
using Limits=item_pop_resources::Limits;
template<class T>using Binding=std::variant<player_world_lookup::Unknown,player_world_lookup::Absent,const T*>;
// Borrowed facts. Unknown and known-null remain distinct; pointer alternatives
// must identify a live caller-owned object. No binding survives this call.
struct Environment {
    Binding<player_world_lookup::Manager> singleton_manager;
    Binding<Resource> set,list;
    player_selection::AreaContext area;
};
struct Location {
    Word source_offset{},buffer_offset{};
    bool operator==(const Location&)const=default;
};
struct Selection {
    std::optional<Word> remapped_group,position_index,choice_index;
    std::optional<Location> position,choice;
    std::optional<std::string> set_sha256;
};
enum class ErrorCode {unknown_field,null_binding,foreign_manager,wrong_kind,
    resource_limit,resource_failure,selection_failure,unsupported_descriptor,unsupported_auxiliary};
struct Error {
    ErrorCode code;
    std::string_view field;
    std::optional<item_pop_resources::Error> resource;
    std::optional<player_selection::Error> selection;
};
template<class T>using Result=std::variant<T,Error>;
// Shared interpretation of the singleton/resource boundary, called only when
// source control flow consumes the identity or dereferences that resource.
[[nodiscard]] std::optional<Error> validate_manager(const player_world_lookup::Manager& expected,
    const Binding<player_world_lookup::Manager>&);
[[nodiscard]] Result<const Resource*> resolve_resource(const Binding<Resource>&,item_pop_resources::Kind);
struct Output {
    player_world_lookup::Record record;
    player_transition::GlobalFields global;
    Selection selection;
    bool initialized{};
    std::size_t rng_calls{},missing_members{},work{};
};
// Complete5b928c with known-null auxiliary10 on the success path. A source
// false result retains RNG advances; typed failures publish no candidate.
// Does not allocate/relink records, reset fields or advance IDs. limits.records
// is the remaining shared work allowance supplied by the enclosing operation.
[[nodiscard]] Result<Output> initialize(const player_world_lookup::Record&,
    const player_transition::GlobalFields&,const player_world_lookup::Manager& expected,
    const Environment&,Limits={});
} // namespace mh4u::item_pop_secondary
