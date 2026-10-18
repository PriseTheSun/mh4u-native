#pragma once
#include "player_animation_types.hpp"
#include "player_controller_step.hpp"
#include "player_fields.hpp"
#include "player_orientation.hpp"
#include "player_position_fields.hpp"

namespace mh4u::player_position_bridge {
// Additional persistent facts only. PBW, d10..18, d4c..60 and model TRS retain
// their existing owners; d00/d20 copies are scratch during this operation.
enum class OrientationCallback {b05328};
enum class OutputCallbacks {standard_95b490_95b940};
struct Context {
    const player_animation::FrameContext& frame;
    std::optional<OrientationCallback> orientation;
    std::optional<OutputCallbacks> publication;
};
// A caller with an optional source delta binds the existing FrameContext only
// when the bridge reaches that operand. d64&8 consumes neither frame resources
// nor delta/templates. The legacy Context and this form share one body.
struct SourceContext {
    const player_animation::FrameResources& frame;
    std::optional<std::uint32_t> delta_bits;
    const motion_initialization::Templates& templates;
    std::optional<OrientationCallback> orientation;
    std::optional<OutputCallbacks> publication;
};
struct Effects {
    bool early_return{},interpolated_position{},publication_skipped{};
    std::optional<player_animation::FrameEffects> animation;
};
struct Output {
    model_animation_frame::State frame;
    model_animation_output::Hierarchy hierarchy;
    player_controller_step::State controller;
    player_fields::Fields shared;
    Fields fields;
    Effects effects;
};
enum class ErrorCode {unknown_field,unsupported_callback,unsupported_route,layer_extent,resource_limit,arithmetic,orientation,animation,publication};
struct Error {
    ErrorCode code;std::string_view field;
    std::optional<motion_transform_math::Error> arithmetic;
    std::optional<player_orientation::Error> orientation;
    std::optional<model_animation_frame::Error> animation;
    std::optional<model_animation_output::Error> publication;
};
using Result=std::variant<Output,Error>;
// Complete b07c08 -> admitted b06f8c -> b07bb8. Explicit bounded active layers
// (not an inferred Player bootstrap), finite consumed arithmetic and concrete
// callbacks. d64&8 returns before orientation/frame; d64&2 skips publication.
// Active b06b38/d00628 routes fail explicitly. Borrowed inputs stay unchanged;
// only a successful combined result may be committed by the caller.
[[nodiscard]] Result apply(const model_animation_frame::State&,
    const model_animation_output::Hierarchy&,const player_controller_step::State&,
    const player_fields::Fields&,const Fields&,const Context&,player_animation::Limits={});
[[nodiscard]] Result apply(const model_animation_frame::State&,
    const model_animation_output::Hierarchy&,const player_controller_step::State&,
    const player_fields::Fields&,const Fields&,const SourceContext&,player_animation::Limits={});
}
