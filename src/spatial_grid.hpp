#pragma once
#include "collision_resource.hpp"
#include "motion_transform_math.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>

namespace mh4u::spatial_grid {
using Word=std::uint32_t;
using Bits4=std::array<Word,4>;
struct PositionXZ {
    // Y/W are not consumed by346a5c. Z is unnecessary when the X gates reject.
    std::optional<Word> x,z;
};
struct Setup {
    // The source345b9c stores +0 in every W and in the unused Y step/reciprocal.
    // Bounds and origin Y are copied raw; no normalization or axis conversion.
    Bits4 bbox_min,bbox_max,cell_size,reciprocal,origin;
    std::uint16_t nx{},nz{};
    bool operator==(const Setup&)const=default;
};
struct Cell {
    std::string_view source_sha256;
    std::uint32_t group{},index{};
    std::int16_t x{},z{};
    const collision_resource::Cell* source{};
    // All group-local IDs in source order, including repeats. This is candidate
    // enumeration, not a collision decision or a callback execution result.
    std::span<const std::uint16_t> pair_ids;
};
enum class ErrorCode {
    resource,unknown_operand,nonfinite_operand,numeric_overflow,
    floating_environment,unsupported_arithmetic,coordinate_domain
};
struct Error {
    ErrorCode code;std::string_view field;
    std::variant<std::monostate,collision_resource::Error,motion_transform_math::Error> cause;
};
template<class T>using Result=std::variant<T,Error>;
// Prepared retains the immutable Resource owner. Cell/source views borrow its
// storage and stay valid while this Prepared or any Resource copy remains alive.
// Setup models only the numeric compressed1/index1 branch, without allocator,
// stream, callback, query-counter or object-manager state.
class Prepared {
public:
    static Result<Prepared> prepare(collision_resource::Resource,std::uint32_t group);
    const Setup& setup()const noexcept{return setup_;}
    const collision_resource::Resource& resource()const noexcept{return resource_;}
    const collision_resource::Grid& source_grid()const noexcept{return grid_;}
    // Inclusive float bounds, then SUB/MUL/truncating S32/SXTH in source order.
    // An upper-edge coordinate may convert to nx/nz and is a miss, not clamped.
    // Errors and misses never expose an invalid/partial Cell.
    [[nodiscard]] Result<std::optional<Cell>> lookup(const PositionXZ&)const;
private:
    Prepared(collision_resource::Resource resource,collision_resource::Grid grid,Setup setup)
        :resource_(std::move(resource)),grid_(grid),setup_(setup){}
    collision_resource::Resource resource_;
    collision_resource::Grid grid_;
    Setup setup_;
};
} // namespace mh4u::spatial_grid
