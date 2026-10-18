#pragma once
#include "binary32_arithmetic.hpp"
#include "player_manager_update.hpp"
#include "player_position_fields.hpp"
#include "player_spatial_acquisition.hpp"

namespace mh4u::player_contact_update {
namespace pc=player_controller_step;
namespace pf=player_fields;
namespace pm=player_manager_update;
namespace pr=player_reference;
namespace sq=spatial_query;
using Word=std::uint32_t;
using Bits3=sq::Bits3;
struct Context {
    std::optional<Word> delta_bits;
    pm::Manager manager=pm::Unknown{};
    pr::Reference reference=pr::Unknown{};
    player_spatial_acquisition::Context spatial;
    // Projected from the same admission external globals, never a second seed.
    std::optional<Word> global_c8c,global_c90;
    std::optional<Bits3> null_reference_template; // source10e6370, only when null.
    // Only a proven stack producer may supply these isolated-leaf raw copies.
    // No attribute-field producer is established by the complete caller.
    // Earlier stack contents remain opaque; composition leaves both Unknown.
    std::array<std::optional<Word>,2> opaque_attribute_tail{};
};
enum class CallKind {integrate,position,attribute_mask,wrapper_flags,local_control,
    height_wrapper,height_query,area_match,radius,manager_selector};
struct Effects {
    std::array<CallKind,24> calls{};
    std::uint8_t call_count{};
    std::optional<Word> scaled_delta,attribute_mask,wrapper_flags;
    std::optional<sq::HeightResult> height;
    bool contact_written{},permission_written{};
    std::optional<bool> radius_result;
};
struct Output {
    pc::PlayerFields player;
    pf::Fields fields;
    player_position_bridge::Fields position;
    Effects effects;
};
enum class Phase {integration,spatial,permission,radius};
enum class ErrorCode {unknown_field,absent_manager,invalid_identity,arithmetic,
    mask,query,capacity};
struct Error {
    Phase phase;ErrorCode code;std::string_view field;
    std::variant<std::monostate,binary32_arithmetic::Error,
        player_spatial_acquisition::Error,sq::Error> cause{};
};
using Result=std::variant<Output,Error>;
using RadiusResult=std::variant<bool,Error>;
// Complete original leaves; integration rounds each operation separately.
// Query uses the one immutable SBC/Scene owner, preserving classification and
// returned grid patches. No external state is changed on success or failure.
[[nodiscard]] Result integrate(const pc::PlayerFields&,const pf::Fields&,
    const player_position_bridge::Fields&,const Context&);
[[nodiscard]] Result contact(const pc::PlayerFields&,const pf::Fields&,
    const player_position_bridge::Fields&,const Context&);
[[nodiscard]] Result refresh(const pc::PlayerFields&,const pf::Fields&,
    const player_position_bridge::Fields&,const Context&);
// Source caller b146d8 through beforeb14700: clear6fb, conditional contact,
// then refresh. This does not invoke the following bridge or advance a clock.
[[nodiscard]] Result apply(const pc::PlayerFields&,const pf::Fields&,
    const player_position_bridge::Fields&,const Context&);
// d0ce2c/d0c9f8, also available to compare unequal source radii independently.
// Reference is an explicit snapshot, not a dynamically aliased PBW position.
[[nodiscard]] RadiusResult radius_contains(const Bits3&,Word first_radius,
    Word second_radius,const Context&);
} // namespace mh4u::player_contact_update
