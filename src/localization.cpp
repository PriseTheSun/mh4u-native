#include "localization.hpp"
#include "picosha2.h"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <set>

namespace mh4u::localization {
namespace {
constexpr std::string_view magic = "MH4U-NATIVE-TEXT 1\n";
constexpr std::string_view source_profile = "mh4u-lmd-0001010b-contiguous-v1";
struct ParseFailure { Error error; };
[[noreturn]] void fail(ErrorCode code, std::string message, std::size_t at = 0) {
    throw ParseFailure{{code, std::move(message), at}};
}
bool hash_valid(std::string_view value) {
    return value.size() == 64 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}
bool name_valid(std::string_view value) {
    return !value.empty() && value.size() <= 128 && std::all_of(value.begin(), value.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
               (c >= '0' && c <= '9') || c == '_';
    });
}
bool locale_valid(std::string_view value) { return value == "pt-BR" || value == "en"; }
void validate_expected(const ExpectedCatalog& expected, const Limits& limits) {
    if (!limits.package_bytes || !limits.documents || !limits.fragments ||
        !limits.text_bytes || !limits.metadata_bytes)
        fail(ErrorCode::resource_limit, "Catalog limits must be positive");
    if (!locale_valid(expected.locale) || !hash_valid(expected.package_sha256) || expected.documents.empty())
        fail(ErrorCode::invalid_expected_identity, "Expected locale, SHA-256 or document set is invalid");
    if (expected.documents.size() > limits.documents)
        fail(ErrorCode::resource_limit, "Expected document count exceeds local limit");
    std::set<std::string> names;
    std::size_t total = 0;
    for (const auto& doc : expected.documents) {
        if (!name_valid(doc.document) || !hash_valid(doc.source_sha256) || !doc.fragment_count)
            fail(ErrorCode::invalid_expected_identity, "Expected document identity is invalid");
        if (!names.insert(doc.document).second)
            fail(ErrorCode::duplicate_identity, "Expected document name appears twice");
        if (doc.fragment_count > limits.fragments - total)
            fail(ErrorCode::resource_limit, "Expected fragments exceed aggregate limit");
        total += doc.fragment_count;
    }
}
void validate_utf8(std::string_view bytes) {
    for (std::size_t i = 0; i < bytes.size();) {
        const auto at = i;
        const auto first = static_cast<unsigned char>(bytes[i++]);
        if (first < 0x80) continue;
        unsigned count{}, code{}, minimum{};
        if (first >= 0xc2 && first <= 0xdf) { count = 1; code = first & 31; minimum = 0x80; }
        else if (first >= 0xe0 && first <= 0xef) { count = 2; code = first & 15; minimum = 0x800; }
        else if (first >= 0xf0 && first <= 0xf4) { count = 3; code = first & 7; minimum = 0x10000; }
        else fail(ErrorCode::invalid_utf8, "Invalid UTF-8 leading byte", at);
        if (count > bytes.size() - i) fail(ErrorCode::invalid_utf8, "Truncated UTF-8 sequence", at);
        for (unsigned n = 0; n < count; ++n) {
            const auto next = static_cast<unsigned char>(bytes[i++]);
            if ((next & 0xc0) != 0x80) fail(ErrorCode::invalid_utf8, "Invalid UTF-8 continuation", i - 1);
            code = (code << 6) | (next & 63);
        }
        if (code < minimum || code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            fail(ErrorCode::invalid_utf8, "Non-scalar or overlong UTF-8 sequence", at);
    }
}
std::uint32_t decimal(std::string_view value, std::size_t at) {
    if (value.empty() || value.size() > 10 || (value.size() > 1 && value.front() == '0'))
        fail(ErrorCode::malformed_package, "Non-canonical unsigned decimal", at);
    if (!std::all_of(value.begin(), value.end(), [](char c) { return c >= '0' && c <= '9'; }))
        fail(ErrorCode::malformed_package, "Non-decimal value", at);
    std::uint32_t result{};
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), result);
    if (error != std::errc{} || end != value.data() + value.size())
        fail(ErrorCode::malformed_package, "Unsigned decimal overflow", at);
    return result;
}
class Reader {
public:
    explicit Reader(std::string_view value) : bytes(value), offset(magic.size()) {}
    std::string_view field(std::size_t limit) {
        const auto start = offset;
        const auto colon = bytes.find(':', offset);
        if (colon == std::string_view::npos || colon - offset > 10)
            fail(ErrorCode::malformed_package, "Missing or oversized field length", offset);
        const auto length = decimal(bytes.substr(offset, colon - offset), offset);
        if (length > limit) fail(ErrorCode::resource_limit, "Field exceeds local limit", start);
        offset = colon + 1;
        if (length >= bytes.size() - offset)
            fail(ErrorCode::malformed_package, "Truncated length-prefixed field", start);
        const auto result = bytes.substr(offset, length);
        offset += length;
        if (bytes[offset++] != ',') fail(ErrorCode::malformed_package, "Field terminator is not a comma", offset - 1);
        return result;
    }
    std::uint32_t number() { const auto at = offset; return decimal(field(10), at); }
    std::string_view bytes;
    std::size_t offset;
};
}

std::string_view error_name(ErrorCode code) noexcept {
    switch (code) {
    case ErrorCode::io: return "io";
    case ErrorCode::resource_limit: return "resource_limit";
    case ErrorCode::invalid_expected_identity: return "invalid_expected_identity";
    case ErrorCode::package_hash_mismatch: return "package_hash_mismatch";
    case ErrorCode::unsupported_variant: return "unsupported_variant";
    case ErrorCode::invalid_utf8: return "invalid_utf8";
    case ErrorCode::malformed_package: return "malformed_package";
    case ErrorCode::duplicate_identity: return "duplicate_identity";
    case ErrorCode::source_identity_mismatch: return "source_identity_mismatch";
    case ErrorCode::missing_document: return "missing_document";
    case ErrorCode::missing_fragment: return "missing_fragment";
    }
    return "unknown_error";
}

Result<Catalog> Catalog::decode(std::string_view bytes, const ExpectedCatalog& expected, Limits limits) {
    try {
        validate_expected(expected, limits);
        if (bytes.size() > limits.package_bytes) fail(ErrorCode::resource_limit, "Package exceeds local byte limit");
        const auto digest = picosha2::hash256_hex_string(bytes.begin(), bytes.end());
        if (digest != expected.package_sha256)
            fail(ErrorCode::package_hash_mismatch, "Package SHA-256 differs from selected manifest/header");
        validate_utf8(bytes);
        if (!bytes.starts_with(magic)) fail(ErrorCode::unsupported_variant, "Unsupported native text package version");
        Reader reader(bytes);
        Catalog result;
        result.locale_ = reader.field(16);
        if (!locale_valid(result.locale_)) fail(ErrorCode::unsupported_variant, "Unsupported target locale", reader.offset);
        if (result.locale_ != expected.locale)
            fail(ErrorCode::source_identity_mismatch, "Target locale differs from selected manifest", reader.offset);
        if (reader.field(16) != "en" || reader.field(128) != source_profile)
            fail(ErrorCode::unsupported_variant, "Unsupported source locale or producer profile", reader.offset);
        const auto count = reader.number();
        if (!count || count > limits.documents) fail(ErrorCode::resource_limit, "Document count outside local limits", reader.offset);
        if (count != expected.documents.size()) fail(ErrorCode::source_identity_mismatch, "Document set differs from manifest", reader.offset);
        std::set<std::string> seen;
        for (std::uint32_t d = 0; d < count; ++d) {
            DocumentIdentity identity;
            identity.document = reader.field(128);
            identity.source_sha256 = reader.field(64);
            identity.fragment_count = reader.number();
            if (!name_valid(identity.document) || !hash_valid(identity.source_sha256))
                fail(ErrorCode::malformed_package, "Invalid document name or SHA-256", reader.offset);
            if (!seen.insert(identity.document).second)
                fail(ErrorCode::duplicate_identity, "Document name appears twice", reader.offset);
            const auto chosen = std::find_if(expected.documents.begin(), expected.documents.end(), [&](const auto& doc) {
                return doc.document == identity.document;
            });
            if (chosen == expected.documents.end() || *chosen != identity)
                fail(ErrorCode::source_identity_mismatch, "Document name/hash/count differs from manifest", reader.offset);
            if (!result.identities_.empty() && identity.document < result.identities_.back().document)
                fail(ErrorCode::malformed_package, "Documents must be in lexical name order", reader.offset);
            if (!identity.fragment_count || identity.fragment_count > limits.fragments - result.fragment_count_)
                fail(ErrorCode::resource_limit, "Fragment count exceeds aggregate limit", reader.offset);
            Document doc;
            doc.metadata = reader.field(limits.metadata_bytes);
            if (doc.metadata.empty()) fail(ErrorCode::malformed_package, "Missing opaque producer metadata", reader.offset);
            // No reserve from untrusted counts. Each bounded record is checked first.
            for (std::uint32_t index = 0; index < identity.fragment_count; ++index) {
                const auto actual_index = reader.number();
                if (actual_index < index) fail(ErrorCode::duplicate_identity, "Duplicate or repeated fragment index", reader.offset);
                if (actual_index != index) fail(ErrorCode::malformed_package, "Fragments must retain contiguous source indices", reader.offset);
                Fragment fragment;
                fragment.source_offset = reader.number();
                fragment.original = reader.field(limits.text_bytes);
                const auto present = reader.field(1);
                const auto translated = reader.field(limits.text_bytes);
                if (present != "0" && present != "1") fail(ErrorCode::unsupported_variant, "Unknown translation presence tag", reader.offset);
                if (fragment.original.find('\0') != std::string::npos || translated.find('\0') != std::string_view::npos)
                    fail(ErrorCode::unsupported_variant, "Embedded NUL is outside the source producer profile", reader.offset);
                if (present == "0") {
                    if (!translated.empty()) fail(ErrorCode::malformed_package, "Absent translation has nonempty payload", reader.offset);
                } else {
                    if (translated == fragment.original) fail(ErrorCode::malformed_package, "Identical translation must use English fallback", reader.offset);
                    if (result.locale_ == "en") fail(ErrorCode::unsupported_variant, "English source package cannot contain translations", reader.offset);
                    fragment.translation = std::string(translated);
                    ++result.translated_count_;
                }
                doc.fragments.push_back(std::move(fragment));
            }
            result.fragment_count_ += identity.fragment_count;
            result.identities_.push_back(std::move(identity));
            result.documents_.push_back(std::move(doc));
        }
        if (reader.offset != bytes.size()) fail(ErrorCode::malformed_package, "Unexpected trailing package bytes", reader.offset);
        result.package_sha256_ = digest;
        return result;
    } catch (const ParseFailure& failure) { return failure.error; }
}

Result<Catalog> Catalog::load(const std::filesystem::path& path, const ExpectedCatalog& expected, Limits limits) {
    try {
        validate_expected(expected, limits);
        std::error_code status;
        if (!std::filesystem::is_regular_file(path, status) || status)
            return Error{ErrorCode::io, "Catalog path is not a readable regular file", 0};
        const auto size = std::filesystem::file_size(path, status);
        if (status) return Error{ErrorCode::io, "Cannot read catalog file size", 0};
        if (size > limits.package_bytes) return Error{ErrorCode::resource_limit, "Catalog file exceeds local byte limit", 0};
        std::ifstream input(path, std::ios::binary);
        if (!input) return Error{ErrorCode::io, "Cannot open catalog", 0};
        std::string bytes(static_cast<std::size_t>(size), '\0');
        input.read(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!input || input.peek() != std::char_traits<char>::eof())
            return Error{ErrorCode::io, "Catalog size changed or read was incomplete", 0};
        return decode(bytes, expected, limits);
    } catch (const ParseFailure& failure) { return failure.error; }
}

Result<std::size_t> Catalog::locate(const DocumentIdentity& identity) const {
    const auto at = std::lower_bound(identities_.begin(), identities_.end(), identity.document,
        [](const auto& left, const auto& right) { return left.document < right; });
    if (at == identities_.end() || at->document != identity.document)
        return Error{ErrorCode::missing_document, "Document is absent from this catalog", 0};
    if (*at != identity) return Error{ErrorCode::source_identity_mismatch, "Lookup document identity differs from loaded source", 0};
    return static_cast<std::size_t>(at - identities_.begin());
}
Result<TextView> Catalog::lookup(const DocumentIdentity& identity, std::uint32_t index, Language language) const {
    if (language != Language::english && language != Language::translated)
        return Error{ErrorCode::unsupported_variant, "Unsupported language selection", 0};
    const auto location = locate(identity);
    if (const auto* error = std::get_if<Error>(&location)) return *error;
    const auto& fragments = documents_[std::get<std::size_t>(location)].fragments;
    if (index >= fragments.size()) return Error{ErrorCode::missing_fragment, "Fragment index is absent from source", 0};
    const auto& fragment = fragments[index];
    const bool translated = language == Language::translated && fragment.translation.has_value();
    return TextView{translated ? std::string_view(*fragment.translation) : std::string_view(fragment.original),
                    fragment.original, translated ? std::string_view(locale_) : std::string_view("en"),
                    fragment.source_offset, translated};
}
Result<std::string_view> Catalog::producer_metadata(const DocumentIdentity& identity) const {
    const auto location = locate(identity);
    if (const auto* error = std::get_if<Error>(&location)) return *error;
    return std::string_view(documents_[std::get<std::size_t>(location)].metadata);
}
} // namespace mh4u::localization
