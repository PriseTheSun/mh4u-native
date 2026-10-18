#pragma once
#include "motion_binding.hpp"
#include "motion_sampler.hpp"
#include <array>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace mh4u::motion_resource {
enum class ErrorCode {io,invalid_expected,hash_mismatch,unsupported_format,
    unsupported_layout,resource_limit,malformed_resource,identity_exhausted,
    foreign_identity,bank_extent,slot_extent,absent_action,track_extent};
struct Error {ErrorCode code;std::string message;std::size_t byte_offset{};};
template<class T>using Result=std::variant<T,Error>;
struct Limits {
    std::size_t bytes=64U*1024U*1024U;
    std::size_t slots=4096,tracks=32768,event_records=250000;
}; // Aggregate resource policy; not recovered format maxima.
struct EventGroupView {
    std::array<std::uint16_t,32> prefix;
    std::span<const motion_events::Record> records;
    std::uint32_t source_offset{},records_offset{};
};
struct ActionInfo {
    std::uint32_t source_offset{},track_table_offset{},flags{},float_tracks_offset{};
    std::array<std::uint32_t,4> scene_translation_bits{},scene_rotation_bits{};
};
// Owns immutable LMT67 bytes and all parsed facts. Returned views borrow this
// shared snapshot: at least one Resource copy must outlive their use. Copies
// share an identity; each successful independent decode receives a fresh,
// never-reused process-local identity. SHA256 remains the content identity.
class Resource {
public:
    static Result<Resource> load(const std::filesystem::path&,std::string_view expected_sha256,Limits={});
    static Result<Resource> decode(std::string_view,std::string_view expected_sha256,Limits={});
    std::uint64_t identity()const noexcept;
    std::string_view sha256()const noexcept;
    std::size_t byte_size()const noexcept;
    std::size_t slot_count()const noexcept;
    // Always a present table, including a zero-length LMT table. Absent banks
    // and resource+64==null remain distinct caller-owned Model facts.
    motion_request::Resource request_resource()const noexcept;
    Result<std::optional<motion_binding::Action>> binding_action(std::uint32_t bank,std::uint32_t slot)const;
    Result<motion_sampler::TrackView> sampler_track(const motion_binding::TrackReference&)const;
    Result<std::optional<ActionInfo>> action_info(std::uint32_t slot)const;
    // nullopt means action exists with explicit null event base. An empty
    // action slot is an absent_action error, distinct from four empty groups.
    Result<std::optional<std::array<EventGroupView,4>>> event_groups(std::uint32_t slot)const;
private:
    struct Data;
    explicit Resource(std::shared_ptr<const Data> data):data_(std::move(data)){}
    static Result<Resource> decode_owned(std::string,std::string_view,Limits);
    std::shared_ptr<const Data> data_;
};
} // namespace mh4u::motion_resource
