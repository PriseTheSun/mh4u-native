#pragma once
#include "item_pop_secondary.hpp"
#include "binary32_arithmetic.hpp"

namespace mh4u::item_pop_update {
using Word=std::uint32_t;
// Snapshot of the object at105718c; distinct from model/player clock fields.
struct Clock {
    std::optional<Word> field50,field5c_bits;
    std::optional<std::uint8_t> field57;
};
struct Context {
    Clock clock;
    std::optional<std::uint8_t> category_e250; // Object atfbeffc, notfb6b7c.
    item_pop_secondary::Environment environment;
    item_pop_resources::Limits limits;
};
enum class ErrorCode {unknown_field,unsupported_virtual,unsupported_auxiliary,
    unsupported_transform,unsupported_kind,unsupported_spatial,
    unsupported_callback,unsupported_category_helper,invalid_link,cycle,arithmetic,capacity,
    dependency,resource_failure,resource_limit,secondary_failure};
struct Error {
    ErrorCode code;
    std::string_view field;
    std::optional<std::size_t> index;
    std::optional<binary32_arithmetic::Error> arithmetic;
    std::optional<item_pop_secondary::Error> secondary;
    std::optional<item_pop_resources::Error> resource;
};
struct Effects {
    std::array<std::size_t,2> rng_calls{};
    std::size_t removed{},primary_renewed{},secondary_renewed{},secondary_failed{};
    bool operator==(const Effects&)const=default;
};
struct Output {
    player_world_lookup::Manager manager;
    player_transition::GlobalFields global;
    std::size_t visited{};
    bool clock_gated{};
    Effects effects;
};
using Result=std::variant<Output,Error>;
// Complete5ba580 traversal and returned record5aa4cc states0..6/default,
// with explicitly absent auxiliaries and no transform/spatial/callback.
// States3/4/5 consume singleton/resources/RNG only on reached source branches.
// Mutations are candidate-only; next is read after the record call, including
// removal. No scheduler, implicit delta, seed or readiness shortcut exists.
[[nodiscard]] Result apply(const player_world_lookup::Manager&,
    const player_transition::GlobalFields&,const Context&);
} // namespace mh4u::item_pop_update
