#pragma once
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::localization {

enum class ErrorCode {
    io, resource_limit, invalid_expected_identity, package_hash_mismatch,
    unsupported_variant, invalid_utf8, malformed_package, duplicate_identity,
    source_identity_mismatch, missing_document, missing_fragment
};
struct Error {
    ErrorCode code;
    std::string message;
    std::size_t byte_offset{};
};
std::string_view error_name(ErrorCode code) noexcept;
template<class T> using Result = std::variant<T, Error>;

struct DocumentIdentity {
    std::string document;
    std::string source_sha256;
    std::uint32_t fragment_count{};
    bool operator==(const DocumentIdentity&) const = default;
};
struct ExpectedCatalog {
    std::string locale;
    // Pin from the producer's independently selected sidecar/header, not the
    // file being loaded. The original LMD digest alone cannot attest its text.
    std::string package_sha256;
    std::vector<DocumentIdentity> documents;
};
struct Limits {
    std::size_t package_bytes = 8U * 1024U * 1024U;
    std::size_t documents = 64;
    std::size_t fragments = 65536; // aggregate, not per document
    std::size_t text_bytes = 256U * 1024U;
    std::size_t metadata_bytes = 4U * 1024U * 1024U;
};
enum class Language { english, translated };
struct TextView {
    std::string_view text;
    std::string_view original;
    std::string_view locale; // "en" on fallback or explicit English selection
    std::uint32_t source_offset{};
    bool translated{};
};

// Owns all strings. Views remain valid while this object is alive and unmoved.
// Text fragments are data only: no LMD commands, formatting or label execution.
class Catalog {
public:
    static Result<Catalog> load(const std::filesystem::path& path,
                                const ExpectedCatalog& expected, Limits limits = {});
    static Result<Catalog> decode(std::string_view bytes,
                                  const ExpectedCatalog& expected, Limits limits = {});
    Result<TextView> lookup(const DocumentIdentity& document, std::uint32_t index,
                            Language language = Language::translated) const;
    Result<std::string_view> producer_metadata(const DocumentIdentity& document) const;
    const std::vector<DocumentIdentity>& identities() const noexcept { return identities_; }
    std::string_view locale() const noexcept { return locale_; }
    std::string_view package_sha256() const noexcept { return package_sha256_; }
    std::size_t translated_count() const noexcept { return translated_count_; }
    std::size_t fragment_count() const noexcept { return fragment_count_; }

private:
    struct Fragment {
        std::uint32_t source_offset{};
        std::string original;
        std::optional<std::string> translation;
    };
    struct Document {
        std::string metadata; // opaque producer JSON, never interpreted here
        std::vector<Fragment> fragments;
    };
    Result<std::size_t> locate(const DocumentIdentity& identity) const;
    std::string locale_, package_sha256_;
    std::vector<DocumentIdentity> identities_;
    std::vector<Document> documents_;
    std::size_t translated_count_{}, fragment_count_{};
};
} // namespace mh4u::localization
