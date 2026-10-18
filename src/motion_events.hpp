#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::motion_events {
enum class ErrorCode {
    io, resource_limit, invalid_expected, package_hash_mismatch, binding_mismatch,
    unsupported_format, malformed_package, duplicate_identity, duplicate_group,
    missing_action, action_identity_mismatch, empty_slot, events_absent,
    events_rejected, invalid_group, group_unavailable, invalid_interval,
    unsupported_accumulator
};
struct Error { ErrorCode code; std::string message; std::size_t byte_offset{}; };
std::string_view error_name(ErrorCode) noexcept;
template<class T> using Result=std::variant<T,Error>;
enum class ActionState : std::uint32_t { empty_slot=0, events_absent=1, parsed=2, rejected=3 };
enum class GroupState : std::uint32_t { unavailable=0, absent=1, present=2 };
struct SourceInfo {
    std::uint32_t entry_id{}; std::string lmt_sha256; std::uint32_t slot_count{};
    bool operator==(const SourceInfo&) const=default;
};
struct ActionIdentity {
    std::uint32_t entry_id{},slot{}; std::string lmt_sha256;
    bool operator==(const ActionIdentity&) const=default;
};
struct ActionInfo {
    ActionIdentity identity;
    std::optional<std::uint32_t> frame_count; // No action header for empty_slot.
    ActionState state{};
    std::array<GroupState,4> groups{};
    std::string rejection_code;
    bool operator==(const ActionInfo&) const=default;
};
struct ExpectedPackage {
    // Independently selected pin, never read from the untrusted binary itself.
    std::string package_sha256,inventory_sha256;
    std::vector<SourceInfo> sources;
    std::vector<ActionInfo> actions;
};
struct Limits {
    std::size_t package_bytes=8U*1024U*1024U;
    std::size_t sources=8,actions=4096,records=250000,rejection_code_bytes=96;
};
struct Record { std::uint32_t mask{},duration{}; bool operator==(const Record&) const=default; };
struct Group {
    std::array<std::uint16_t,32> prefix_raw{}; // Source u16 table; gameplay meanings remain unassigned.
    std::vector<Record> records;
    std::uint64_t duration_sum{};
    bool kernel_supported{}; // False when cumulative signed overflow is possible.
};
struct PrefixHit {
    std::uint32_t bit_index{};
    std::uint16_t value{}; // Zero is a selected source value, not absence.
    bool operator==(const PrefixHit&) const=default;
};
class Package {
public:
    static Result<Package> load(const std::filesystem::path&,const ExpectedPackage&,Limits={});
    static Result<Package> decode(std::string_view,const ExpectedPackage&,Limits={});
    // Strict inclusive source frame interval. Does not clamp, sort or loop.
    Result<std::uint32_t> range(const ActionIdentity&,std::uint32_t group,
                                std::int64_t begin,std::int64_t end) const;
    // Raw signed kernel inputs: negative/reversed/out-of-N values are preserved.
    Result<std::uint32_t> kernel_range(const ActionIdentity&,std::uint32_t group,
                                       std::int32_t begin,std::int32_t end) const;
    // Returned pointer is valid while this Package remains alive and unmoved.
    Result<const Group*> group(const ActionIdentity&,std::uint32_t group) const;
    // Lowest active bit maps through the selected group's raw u16 prefix.
    // Present table + mask0 => nullopt. Missing/rejected tables remain errors.
    // No clock, callback, transition predicate or output-zero fallback.
    Result<std::optional<PrefixHit>> first_prefix(const ActionIdentity&,
                                                  std::uint32_t group,std::uint32_t mask) const;
    const std::vector<ActionInfo>& actions() const noexcept { return info_; }
    const std::vector<SourceInfo>& sources() const noexcept { return sources_; }
    const std::string& inventory_sha256() const noexcept { return inventory_sha_; }
private:
    Result<std::size_t> find(const ActionIdentity&) const;
    std::vector<SourceInfo> sources_;
    std::vector<ActionInfo> info_;
    std::vector<std::array<Group,4>> groups_;
    std::string inventory_sha_;
};
} // namespace mh4u::motion_events
