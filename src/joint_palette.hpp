#pragma once
#include "motion_transform_math.hpp"
#include "joint_output.hpp"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::joint_palette {
using Matrix=motion_transform_math::Matrix;
using Bits3=motion_transform_math::Bits3;
using Bits12=motion_transform_math::Palette;
struct Unknown {bool operator==(const Unknown&)const=default;};
struct Null {bool operator==(const Null&)const=default;};
struct Present {bool operator==(const Present&)const=default;};
using Presence=std::variant<Unknown,Null,Present>;
// Caller-issued immutable snapshot identities, never target/process addresses.
struct InputIdentity {
    std::uint64_t resource{},pose{},context{};
    std::uint32_t palette{};
    bool operator==(const InputIdentity&)const=default;
};
// Borrow the canonical hierarchy owner directly; no extraction or per-frame copy.
// The owner must remain alive and unmodified for the synchronous operation.
class JointMatrices {
    std::variant<std::span<const Matrix>,std::span<const joint_output::Joint>> view_;
public:
    JointMatrices():view_(std::span<const Matrix>{}){}
    explicit JointMatrices(std::span<const Matrix> value):view_(value){}
    explicit JointMatrices(std::span<const joint_output::Joint> value):view_(value){}
    [[nodiscard]] std::size_t size()const noexcept {
        return std::visit([](const auto& value){return value.size();},view_);
    }
    [[nodiscard]] const Matrix& operator[](std::size_t index)const noexcept {
        if(const auto* value=std::get_if<std::span<const Matrix>>(&view_))return (*value)[index];
        return std::get<std::span<const joint_output::Joint>>(view_)[index].matrix;
    }
};
struct PaletteView {std::span<const std::uint8_t> indices;}; // Owner already narrows source count.
struct ResourceView {
    std::uint64_t identity{};
    std::optional<std::uint8_t> auxiliary214;
    std::span<const Matrix> source_matrices;
    std::span<const PaletteView> palettes;
};
using ResourceReference=std::variant<Unknown,Null,ResourceView>;
struct Location {
    std::uint64_t storage{}; // Nonzero, stable, unique live allocation token.
    std::uint32_t offset{}; // From a registered nonnull 16-byte-aligned origin.
    bool operator==(const Location&)const=default;
};
struct Region {
    Location base;
    std::uint32_t extent{}; // Bytes accessible from base; bounded by caller policy.
    bool operator==(const Region&)const=default;
};
using Buffer=std::variant<Unknown,Null,Region>;
using Pointer=std::variant<Unknown,Null,Location>;
// Address and tag are separate facts. Address offset must be divisible by16.
struct TaggedLocation {
    Location address;
    std::uint8_t low_bits{};
    bool operator==(const TaggedLocation&)const=default;
};
using Binding=std::variant<Unknown,Null,TaggedLocation>;
struct CommandCursor {
    Region buffer;
    std::uint32_t cursor{},end{}; // Relative to buffer.base, not pointers.
    bool operator==(const CommandCursor&)const=default;
};
using Command=std::variant<Unknown,Null,CommandCursor>;
struct Context {
    std::uint64_t identity{};
    std::uint32_t flags4c{},flags188{},flags1bc{};
    Command command=Unknown{};
    Pointer command638=Unknown{};
    std::optional<std::uint32_t> descriptor_word;
    std::optional<Bits3> offset;
    std::array<Binding,3> bindings{Unknown{},Unknown{},Unknown{}}; //438/43c/440.
    bool operator==(const Context&)const=default;
};
struct Manager {
    Buffer normal=Unknown{},history=Unknown{};
    std::optional<std::uint32_t> stride;
    std::optional<std::uint8_t> local_lock,global_lock;
    std::span<const std::uint32_t> dirty_words;
};
using ManagerReference=std::variant<Unknown,Null,Manager>;
struct ModelView {
    std::uint64_t pose_identity{};
    std::uint32_t joint_count{},palette_count{},flags104{},word154{};
    std::optional<std::uint32_t> history_selector;
    std::optional<std::uint8_t> parent_sharing;
    Presence parent=Unknown{};
    std::span<const std::uint32_t> handles;
    JointMatrices joint_matrices;
};
struct Limits {
    std::size_t matrices=256,palettes=256,indices_per_palette=24,total_indices=6144;
    std::size_t dirty_words=64,patches=257;
    std::uint32_t buffer_bytes=16U*1024U*1024U;
}; // Work/capacity policy; not universal original validation.
enum class ErrorCode {
    resource_limit,source_extent,joint_extent,palette_extent,handle_extent,dirty_extent,
    unknown_resource,null_resource,unknown_auxiliary,unsupported_auxiliary,
    unknown_parent_sharing,unknown_parent,unsupported_parent,unknown_history,unsupported_history,
    unknown_manager,null_manager,unknown_buffer,unknown_stride,unknown_lock,unsupported_lock,
    unknown_command,null_command,unknown_descriptor,unsupported_material,allocation_required,
    invalid_storage,invalid_alignment,address_overflow,buffer_extent,unknown_context_offset,
    unknown_binding,invalid_binding,arithmetic
};
struct Error {
    ErrorCode code;std::string_view message;
    std::size_t palette{},entry{};
    std::optional<motion_transform_math::Error> arithmetic;
};
template<class T>using Result=std::variant<T,Error>;
struct Generated {
    InputIdentity identity;
    std::vector<std::uint8_t> source_indices;
    std::vector<Bits12> values;
};
// Pure source959d00 formula. All B elements and first3 columns of W consumed.
// No MOD parsing here; identity is supplied by the immutable resource/pose owner.
[[nodiscard]] Result<Generated> generate(std::span<const Matrix> source,
    JointMatrices joints,std::span<const std::uint8_t> indices,
    const Bits3& context_offset,InputIdentity,Limits={});
enum class WriteKind {model_word154,palette};
struct Write {
    Location destination;
    std::vector<std::uint32_t> words; // Serialize LE in order; zero-length write allowed.
    WriteKind kind;
    std::optional<InputIdentity> input;
    std::vector<std::uint8_t> source_indices;
};
struct DirtyWrite {std::uint32_t bank{},value{};};
enum class ReservationKind {invalid_handle,absent_buffer,already_marked,writable};
struct Reservation {
    std::uint32_t handle{},bank{},slot{},mask{};
    ReservationKind kind;
    std::optional<Location> destination;
};
struct Prepared {
    Context context;
    std::uint32_t model_flags104{};
    std::vector<DirtyWrite> dirty_writes;
    std::vector<Reservation> reservations;
    std::vector<Write> writes; // Ordered command/palette patches. Aliases are retained.
};
// Complete normal959d00 plan, including admitted1a5c10 and6be688. Owns no GPU
// or raw buffer pointer. Caller applies the complete plan transactionally to
// the same registered allocations/snapshot identities; never replay on newer
// dirty words. Inputs/outputs must not alias immutable source/model tables.
// Unsupported paths reject when consumed. Joint_count0 skips resource/manager.
[[nodiscard]] Result<Prepared> prepare(const ModelView&,const ResourceReference&,
    const Context&,const ManagerReference&,Limits={});
// Original6bdc44 normal buffer branch, separately from palette preparation.
// Same address preserves existing tag. History buffer/fallback outside profile.
// Both buffers null preserve bindings without consuming handle/stride.
[[nodiscard]] Result<Context> bind(const Context&,const ManagerReference&,
    std::uint32_t handle,Limits={});
} // namespace mh4u::joint_palette
