#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <variant>

namespace mh4u::notification_queue {
using Word=std::uint32_t;
struct Unknown {bool operator==(const Unknown&)const=default;};
struct Absent {bool operator==(const Absent&)const=default;};
struct PoolIndex {std::uint8_t value;bool operator==(const PoolIndex&)const=default;};
// Caller-owned equality identity for a known nonnull reference outside the pool.
// A token is not an emulated address and is never dereferenced. The tag itself
// establishes nonnullness; token zero is not the Absent alternative.
struct Opaque {std::uint64_t identity;bool operator==(const Opaque&)const=default;};
using Slot=std::variant<Unknown,Absent,PoolIndex,Opaque>;
using Record=std::array<std::optional<std::uint8_t>,148>;
struct State {
    std::array<Record,16> pool{};
    std::array<Slot,16> slots{};
    std::optional<Word> counter{};
    bool operator==(const State&)const=default;
};
static_assert(std::is_trivially_copyable_v<State> && std::is_nothrow_move_assignable_v<State>);
struct Request {
    Word arg0{},arg1{},arg2{},arg3{};
    std::array<Word,3> payload{}; // Raw words: no numeric interpretation.
};
enum class Phase {pool,slots,counter,cleanup};
enum class ErrorCode {unknown_byte,unknown_slot,invalid_index,unknown_counter};
struct Error {Phase phase;ErrorCode code;std::optional<std::size_t> index{};};
struct Effects {
    std::uint8_t pool_examined{},slots_examined{},cleanup_examined{};
    std::optional<PoolIndex> selected{};
    bool cleanup_attempted{};
    std::optional<std::uint8_t> cleanup_slot{};
};
struct Output {State queue;std::optional<PoolIndex> inserted{};Effects effects{};};
using Result=std::variant<Output,Error>;
// Complete b8ba58 plus reached b81a1c/fill. No count/slot/pool invariant is
// inferred. A full queue may remove the first alias of a free pool record and
// still report no insertion. Partial opaque record bytes are preserved.
// Fixed, disjoint source bindings and exclusive synchronous ownership only;
// no initialization, draining, allocation or display consumer is implied.
[[nodiscard]] Result enqueue(const State&,const Request&) noexcept;
}
