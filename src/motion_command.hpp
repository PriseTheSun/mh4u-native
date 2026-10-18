#pragma once
#include "motion_pending.hpp"
#include "motion_query.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_command {
// Player auxiliary fields only. Canonical animation storage stays with the
// caller's motion_pending::State; +6c/+70 belong to Layer.interpolation_curve_bits.
struct Fields {
    std::array<std::uint32_t,7> fieldsd48{}; // d48,d4c,d50,d54,d58,d5c,d60.
    std::uint32_t fieldd68{};
    std::array<std::uint32_t,4> fieldsd70{};
    bool operator==(const Fields&)const=default;
};
struct Globals {
    // Current runtime snapshots of104e048,104e050,104e058; no implicit BSS zero.
    std::array<std::optional<std::array<std::uint32_t,2>>,3> fields6c_tables{};
    std::optional<std::array<std::uint32_t,3>> vector10e6370;
};
struct Sampling {
    motion_initialization::Templates templates;
    motion_initialization::Resolver resolver;
};
struct Limits {
    std::size_t storage_layers=8,joint_bindings=2048; // Aggregate copied storage policy.
    motion_query::Limits query;
};
enum class Entry {dispatch,restart,if_changed,variant,common,layer};
struct Command {
    Entry entry{Entry::dispatch};
    std::uint32_t mode{},layer{},selector{},flags{};
    float interpolation{},start_frame{},speed{};
};
struct ObservedRequest {
    motion_request::Request request;
    std::uint32_t command_flags{};
    bool accepted{};
};
struct Output {
    std::vector<motion_pending::Layer> layers;
    Fields fields;
    std::array<ObservedRequest,2> requests{};
    std::uint8_t request_count{};
    std::size_t queries{},samples{};
};
enum class ErrorCode {entry,layer_domain,nonfinite_input,floating_environment,
    unknown_vector,unknown_table,request_failed,resource_limit,query_extent,query_failed,arithmetic};
struct Error {
    ErrorCode code;std::string_view message;
    std::optional<motion_request::Error> request_error;
    std::optional<motion_query::Error> query_error;
    std::optional<motion_transform_math::Error> arithmetic;
    std::size_t query_index{};
};
using Result=std::variant<Output,Error>;
// Shared pre-copy boundary for command consumers. Counts all stored joint
// bindings, including inactive layers, without allocating or interpreting them.
[[nodiscard]] std::optional<Error> validate_storage(
    std::span<const motion_pending::Layer>,const Limits&) noexcept;
// Full b27c1c/b0c060/b0c198/b0c170/b07480/b04e4c, concrete b085bc/9575dc,
// and live958884. active_count is original+234, separate from storage capacity;
// only a consumed query requires its active prefix to fit the borrowed storage.
// Both queries evolve the same canonical model-channel cursors. Setter changes
// do not rebind tracks or infer bank placement; pending/binding remain separate.
// +3b8 is storage[1].animation.layer.weight, never another Fields member.
// Raw unconsumed state is preserved. All output/copies are bounded and
// transactional; resolver side effects are caller-owned. Resources must outlive
// this call. Unknown dispatch modes and unchanged selectors precede source gates.
[[nodiscard]] Result apply(std::span<const motion_pending::Layer> storage,
    std::uint32_t active_count,const Fields&,const motion_request::Model&,
    const Command&,const Globals&,const Sampling& = {},Limits = {});
} // namespace mh4u::motion_command
