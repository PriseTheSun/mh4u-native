#pragma once
// Test-only comparison of the canonical actor state. Float fields compare raw
// words so inactive NaN payloads are preserved rather than compared numerically.
#include "player_animation.hpp"
#include <array>
#include <bit>
namespace mh4u::test_player_actor_state {
namespace pa=player_animation;namespace mp=motion_pending;
inline std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
inline bool layer_equal(const mp::Layer& x,const mp::Layer& y){const auto& a=x.animation;const auto& b=y.animation;
 if(a.tracks!=b.tracks||a.prediction_rotation!=b.prediction_rotation||a.prediction_translation!=b.prediction_translation||a.previous_rotation!=b.previous_rotation||a.base_rotation!=b.base_rotation||a.previous_translation!=b.previous_translation||a.base_translation!=b.base_translation||x.rotation_98!=y.rotation_98||x.translation_c8!=y.translation_c8)return false;
 const auto& l=a.layer;const auto& r=b.layer;if(l.selector!=r.selector||l.flags!=r.flags||l.field08!=r.field08||l.attributes!=r.attributes||l.bindings!=r.bindings||l.masks!=r.masks||l.previous_masks!=r.previous_masks||l.interpolation_curve_bits!=r.interpolation_curve_bits)return false;
 const std::array ll{l.requested_frame,l.offset,l.interpolation,l.remaining_interpolation,l.weight,l.frame,l.previous_frame,l.end_frame,l.field58,l.speed,l.field60,l.prepared_frame,l.prepared_next_frame,l.previous_step};
 const std::array rr{r.requested_frame,r.offset,r.interpolation,r.remaining_interpolation,r.weight,r.frame,r.previous_frame,r.end_frame,r.field58,r.speed,r.field60,r.prepared_frame,r.prepared_next_frame,r.previous_step};for(std::size_t i=0;i<ll.size();++i)if(bits(ll[i])!=bits(rr[i]))return false;return true;
}
inline bool hierarchy_equal(const mh4u::model_animation_output::Hierarchy& a,const mh4u::model_animation_output::Hierarchy& b){if(a.local_rt!=b.local_rt||a.world!=b.world||a.flags4!=b.flags4||a.flags104!=b.flags104||a.flags230!=b.flags230||a.attachment_index!=b.attachment_index||a.attachment_mode_high16!=b.attachment_mode_high16||a.dirty!=b.dirty||a.fieldc4c!=b.fieldc4c||a.joints!=b.joints||a.order.index()!=b.order.index())return false;if(const auto* x=std::get_if<std::vector<std::uint8_t>>(&a.order))return *x==std::get<std::vector<std::uint8_t>>(b.order);return true;}
inline bool state_equal(const pa::State& a,const pa::State& b){if(a.controller.player!=b.controller.player||a.controller.motion!=b.controller.motion||a.frame.model!=b.frame.model||!hierarchy_equal(a.hierarchy,b.hierarchy))return false;
 const auto& x=a.frame.animation;const auto& y=b.frame.animation;if(x.joints!=y.joints||x.snapshots!=y.snapshots||x.calculation_flags!=y.calculation_flags||x.accumulated_flags!=y.accumulated_flags||x.layers.size()!=y.layers.size())return false;
 for(std::size_t i=0;i<x.layers.size();++i)if(!layer_equal(x.layers[i],y.layers[i]))return false;for(std::size_t i=0;i<6;++i)if(!layer_equal(a.inactive[i],b.inactive[i]))return false;return true;
}

} // namespace mh4u::test_player_actor_state
