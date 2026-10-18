#pragma once
#include "player_world_lookup.hpp"
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
};
enum class ErrorCode {unknown_field,unsupported_virtual,unsupported_auxiliary,
    unsupported_transform,unsupported_state,unsupported_kind,unsupported_spatial,
    unsupported_callback,unsupported_category_helper,invalid_link,cycle,arithmetic,capacity};
struct Error {
    ErrorCode code;
    std::string_view field;
    std::optional<std::size_t> index;
    std::optional<binary32_arithmetic::Error> arithmetic;
};
struct Output {
    player_world_lookup::Manager manager;
    std::size_t visited{};
    bool clock_gated{};
};
using Result=std::variant<Output,Error>;
// Complete5ba580 traversal and returned record5aa4cc paths0/1/2/6/default,
// with explicitly absent auxiliary10/14 and no transform/spatial/callback.
// States3/4/5 require additional source helpers and fail when reached.
// Mutations are candidate-only until success. Source reads next after update;
// this profile does not remove/relink records or retain the borrowed context.
// No scheduler, implicit delta, RNG, or readiness shortcut is introduced.
[[nodiscard]] Result apply(const player_world_lookup::Manager&,const Context&);
} // namespace mh4u::item_pop_update
