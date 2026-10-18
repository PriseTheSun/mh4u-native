#pragma once
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include <variant>

namespace mh4u::player_input_block {
using Block=std::array<player_input_storage::Byte,player_input_storage::size>;
// Transient source-layout projection, not a second persistent input state.
Block project(const player_controller_step::PlayerFields&,const player_fields::Fields&) noexcept;
struct Output {player_controller_step::PlayerFields player;player_fields::Fields fields;};
enum class ErrorCode {unknown_required_alias,partial_optional_alias};
struct Error {ErrorCode code;std::size_t offset;};
using Result=std::variant<Output,Error>;
// Publish into the same canonical owners. Byte-granular extras preserve
// partial knowledge. Existing scalar aliases retain their width/knownness:
// malformed projections cannot silently drop known lanes or invent values.
Result publish(const Block&,const player_controller_step::PlayerFields&,
    const player_fields::Fields&) noexcept;
} // namespace mh4u::player_input_block
