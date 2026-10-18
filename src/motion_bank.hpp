#pragma once
#include "motion_pending.hpp"
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_bank {
struct Limits {std::size_t layers=8,joints_per_layer=256;}; // Copy policy, adjustable by the caller.
enum class ErrorCode {bank_extent,active_layer_extent,resource_limit,unknown_replacement,resource_identity};
struct Error {ErrorCode code;std::string_view message;};
struct Output {
    motion_request::Model model;
    std::vector<motion_pending::Layer> layers;
};
using Result=std::variant<Output,Error>;
// Numeric table/flag effects of original 9592d4. Copies all supplied layers;
// only [0,active_count) participates. A matching selector overwrites flags with
// 2, including selector FFFF for bank15. Equal old/new identities still mark.
// Existing Channels/event Bindings are preserved, including their old resource
// identities. This does not acquire/release/destroy ARM resources. Model banks
// borrow views: the caller must retain every Resource snapshot referenced by
// either banks or retained bindings for their full lifetime. Unknown old banks
// can be replaced numerically; an Unknown replacement is not a known pointer.
// No parser, sampling, pending consumption or gameplay bank choice occurs here.
[[nodiscard]] Result replace(const motion_request::Model&,
    std::span<const motion_pending::Layer>,std::size_t active_count,
    std::uint32_t bank,const motion_request::Bank&,Limits={});
} // namespace mh4u::motion_bank
