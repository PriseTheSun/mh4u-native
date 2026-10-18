#pragma once
#include "collision_resource.hpp"
#include "spatial_face_query.hpp"
#include "spatial_grid.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::spatial_query {
using Word=std::uint32_t;
using Bits3=std::array<Word,3>;
// A disengaged optional always means Unknown, including collection presence.
// An engaged empty vector means a collection observed to be empty.
struct NullableIndex {std::optional<bool> present;std::optional<Word> index;};
struct NullableStatus {
    std::optional<bool> present;std::optional<Word> instance,group;
};
struct Filter {
    std::optional<Word> object_mask,area_mask,attribute_mask,excluded_tag,flags;
    std::optional<Word> exclusion_mode;
    NullableIndex excluded_object;
    NullableStatus excluded_status;
    std::optional<bool> extra_c8,extra_c9;
    // cd2f04 +30 and cd2e04 +18: only known-null callbacks are admitted.
    std::optional<bool> face_callback_present,post_callback_present;
};
struct Status {
    std::optional<std::uint8_t> enabled_c,disabled_1f,field20;
    std::optional<bool> pointer24_present,pointer30_present;
};
struct Instance {
    std::optional<std::uint8_t> enabled_5,bypass_pair_box_33;
    std::optional<Word> object_mask,area_mask,exclusion_tag;
    // present==false is the original null resource, not a missing asset.
    std::optional<bool> resource_present,tree_present,grid_present;
    std::optional<collision_resource::Resource> resource;
    std::optional<Word> status_count,grid_state;
    Status status;
};
struct GridState {
    // Mutable source stamps belong to the loaded grid, not the immutable SBC
    // bytes or an instance. Multiple instances may select this same state slot.
    collision_resource::Resource resource;
    Word group{};
    std::optional<Word> counter;
    std::optional<std::uint8_t> depth;
    std::vector<std::optional<Word>> pair_marks;
};
struct Node {
    // Native registry slot, never an ARM address or a persistent game ID.
    std::optional<Word> instance;
    // 3e73f0's vertical container gate consumes only these two bounds.
    std::optional<Word> minimum_x,maximum_z;
};
struct Scene {
    std::vector<Instance> instances;
    std::vector<GridState> grid_states;
    std::array<std::optional<std::vector<Node>>,32> direct;
    // Original family order: 1498, a98, 1e98, 2898. Nonempty branches use
    // different callbacks and currently return unsupported, never a fake miss.
    std::array<std::array<std::optional<bool>,32>,4> other_empty;
    std::array<std::array<std::optional<bool>,32>,2> extra_c8_empty;
    std::optional<bool> extra_c8_enabled,extra_c9_enabled,extra_c9_empty;
    std::optional<Word> extra_tail_count,status_selector_a,status_selector_b;
    std::array<std::optional<Word>,3> classification_masks;
    std::array<std::optional<Word>,3> normal_thresholds;
};
struct Identity {
    Word instance{},group{},pair{},face{},attribute{};
    bool operator==(const Identity&)const=default;
};
struct Candidate {
    Word instance{},group{},pair{};
    bool operator==(const Candidate&)const=default;
};
struct Hit {
    // Keeps source identity and borrowed geometry alive across Scene destruction.
    collision_resource::Resource resource;
    Identity identity;
    spatial_face_query::Replacement replacement;
};
struct Effects {
    std::vector<Candidate> candidates;
    std::vector<Identity> face_calls;
};
struct GridUpdate {Word slot{};GridState state;};
struct Evaluation {
    Word classification{};
    std::optional<Hit> hit;
    Effects effects;
    // Transactional state, including increments on a grid miss and modulo
    // rollover. The caller can commit these only after complete success.
    std::vector<GridUpdate> grid_updates;
};
struct Limits {
    std::size_t nodes=1U<<20,candidates=1U<<20,faces=1U<<21,runtime_words=1U<<20;
    // Separate host budget: zero-pair grids still occupy update slots.
    std::size_t runtime_slots=1U<<20;
}; // Native work/output policy, not a source-game maximum.
enum class ErrorCode {unknown_operand,invalid_identity,unsupported_route,resource,
    grid,face,arithmetic,resource_limit};
enum class GridPlanCode {invalid_slot,source_binding,resource_limit,duplicate_slot};
struct GridPlanError {GridPlanCode code;std::optional<Word> slot;};
struct Error {
    ErrorCode code;std::string_view field;
    std::variant<std::monostate,collision_resource::Error,spatial_grid::Error,
        spatial_face_query::Error,motion_transform_math::Error> cause;
    std::optional<GridPlanError> plan{};
};
template<class T>using Result=std::variant<T,Error>;
// Validates each set against the Scene binding, rejects duplicate slots within
// either set, and returns a sorted owned plan. Later replaces earlier for the
// same slot. Unknown counter/depth/marks are preserved until a query consumes
// them. No Scene mutation; caller commits only after its whole transaction.
[[nodiscard]] Result<std::vector<GridUpdate>> plan_updates(const Scene&,
    std::span<const GridUpdate> earlier,std::span<const GridUpdate> later={},Limits={});
// 1774e8/180c5c, type15, direct untransformed one-group SBC/GRCO instances.
// Traversal, candidates and equal-distance replacement preserve source order.
// Unsupported/Unknown paths publish no partial result or external mutation.
[[nodiscard]] Result<Evaluation> query(const Scene&,const Filter&,
    const spatial_face_query::Vertical&,Limits={},std::span<const GridUpdate> previous={});
struct HeightRequest {
    std::optional<Bits3> position;
    std::optional<bool> previous_present;
    std::optional<Word> previous_y;
    std::optional<Word> area,flags,attribute_mask;
    std::optional<bool> attribute_output_present;
};
// 2b9474 direct SBC branch. Values come from parsed source records, not the
// normalized/intersection plane. Keeps geometry alive independently of Scene.
struct DirectAttribute {
    collision_resource::Resource resource;
    Identity identity;
    std::array<std::uint8_t,6> bytes0_5;
    std::array<Word,3> words2_4_0;
    Bits3 normal_bits;
    Word word24;
    // Original does not write these output lanes. No prior output is supplied
    // to height(), so their values remain Unknown, including after a hit.
    std::optional<Word> opaque28,opaque2c;
};
struct HeightResult {
    Word height_bits{};std::uint8_t status{};Evaluation query;
    std::optional<DirectAttribute> attribute{};
};
// 2bb178 constructs its own filter and downward ray. Requested attributes are
// produced only on a hit; dynamic/alternate/transformed routes remain outside
// the admitted Scene profile. Both query entrypoints retain previous updates
// even when this call does not touch their slots, and read them before Scene.
[[nodiscard]] Result<HeightResult> height(const Scene&,const HeightRequest&,Limits={},
    std::span<const GridUpdate> previous={});
} // namespace mh4u::spatial_query
