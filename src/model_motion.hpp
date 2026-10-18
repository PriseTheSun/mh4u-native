#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::model_motion {
using Vector = std::array<double,3>;
using Quaternion = std::array<double,4>; // Raw XYZW, never normalized here.
enum class ErrorCode {
    io, resource_limit, invalid_expected, package_hash_mismatch,
    binding_mismatch, unsupported_format, malformed_package,
    duplicate_identity, duplicate_component, nonfinite_value,
    invalid_timeline, missing_action, action_identity_mismatch, invalid_sample_frame
};
struct Error { ErrorCode code; std::string message; std::size_t byte_offset{}; };
std::string_view error_name(ErrorCode) noexcept;
template<class T> using Result = std::variant<T,Error>;
struct ActionIdentity {
    std::uint32_t entry_id{},slot{};
    std::string lmt_sha256;
    bool operator==(const ActionIdentity&) const = default;
};
struct ActionInfo {
    ActionIdentity identity;
    std::uint32_t frame_count{};
    std::int32_t loop_frame_raw{};
    std::uint32_t presence_mask{}; // bit 0 translation, bit 1 rotation.
    std::uint32_t gltf_clip_index{}; // Within the hash-bound joint glTF.
    bool operator==(const ActionInfo&) const = default;
};
struct AssetBinding {
    std::string relative_path,sha256;
    bool operator==(const AssetBinding&) const = default;
};
struct ExpectedPackage {
    // Use an independently selected generated identity header/manifest.
    std::string package_sha256;
    AssetBinding gltf,curves;
    std::vector<ActionInfo> actions;
    std::vector<AssetBinding> dependencies; // Complete producer-owned content inventory, excluding glTF/curves above.
};
struct Limits {
    std::size_t package_bytes=8U*1024U*1024U;
    std::size_t bound_asset_bytes=64U*1024U*1024U; // Aggregate across all bindings.
    std::size_t actions=64;
    std::size_t keys=250000; // Aggregate, checked before allocation.
    std::size_t path_bytes=1024;
    std::size_t asset_bindings=64; // Includes the primary glTF and curves.
};
struct Sample {
    std::optional<Vector> translation;
    std::optional<Quaternion> rotation;
};

// No LMT decoding, model application, clocks, loops or missing-track defaults.
// Queries use finite source frames in [0,N-1], inclusive, without clamping.
class Package {
public:
    // load verifies binary and every bound content file under the explicit root.
    static Result<Package> load(const std::filesystem::path& binary,
                                const ExpectedPackage&,const std::filesystem::path& paired_asset_root,
                                Limits limits={});
    // decode validates serialized bindings but does not read bound asset files.
    static Result<Package> decode(std::string_view,const ExpectedPackage&,Limits limits={});
    Result<Sample> sample(const ActionIdentity&,double source_frame) const;
    const std::vector<ActionInfo>& actions() const noexcept { return info_; }
    const AssetBinding& gltf_binding() const noexcept { return gltf_; }
    const AssetBinding& curve_binding() const noexcept { return curves_; }
    const std::vector<AssetBinding>& dependencies() const noexcept { return dependencies_; }
private:
    struct Key { double frame{}; std::array<double,4> value{}; };
    struct Channel { std::uint32_t interpolation{}; std::vector<Key> keys; };
    struct Action { std::optional<Channel> translation,rotation; };
    std::vector<ActionInfo> info_;
    std::vector<Action> actions_;
    AssetBinding gltf_,curves_;
    std::vector<AssetBinding> dependencies_;
};
} // namespace mh4u::model_motion
