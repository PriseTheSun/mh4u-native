#include "localization.hpp"
#ifdef MH4U_LOCALIZATION_GENERATED
#include "catalog_identity.hpp"
#endif
#include "picosha2.h"
#include <algorithm>
#include <chrono>
#include <charconv>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace loc = mh4u::localization;
namespace {
std::size_t checks = 0;
void check(bool value, const char* message) {
    ++checks;
    if (!value) throw std::runtime_error(message);
}
template<class T> T value(loc::Result<T> result) {
    if (const auto* error = std::get_if<loc::Error>(&result))
        throw std::runtime_error(std::string(loc::error_name(error->code)) + ": " + error->message);
    return std::get<T>(std::move(result));
}
std::string digest(std::string_view bytes) { return picosha2::hash256_hex_string(bytes.begin(), bytes.end()); }
std::string frame(const std::vector<std::string>& fields) {
    std::string result = "MH4U-NATIVE-TEXT 1\n";
    for (const auto& field : fields) result += std::to_string(field.size()) + ':' + field + ',';
    return result;
}
const std::string source_hash(64, 'a');
loc::ExpectedCatalog expected(std::string_view bytes) {
    return {"pt-BR", digest(bytes), {{"Fixture", source_hash, 3}}};
}
std::vector<std::string> fixture() {
    return {"pt-BR", "en", "mh4u-lmd-0001010b-contiguous-v1", "1",
            "Fixture", source_hash, "3", "{\"controls\":[[159,7,8,9]],\"unknown\":true}",
            "0", "100", "Name\n%s\xee\x80\x80", "1", "Opções\n%s\xee\x80\x80",
            "1", "128", "Fallback", "0", "",
            "2", "150", "Intentionally empty", "1", ""};
}
void rejected(std::string bytes, loc::ErrorCode wanted, loc::Limits limits = {}) {
    const auto result = loc::Catalog::decode(bytes, expected(bytes), limits);
    const auto* error = std::get_if<loc::Error>(&result);
    check(error && error->code == wanted, "Unexpected malformed-package result");
}
void synthetic() {
    auto fields = fixture(); const auto bytes = frame(fields);
    const auto catalog = value(loc::Catalog::decode(bytes, expected(bytes)));
    const loc::DocumentIdentity identity{"Fixture", source_hash, 3};
    auto translated = value(catalog.lookup(identity, 0));
    check(translated.text == fields[12] && translated.original == fields[10] && translated.translated,
          "UTF-8 translation/control bytes changed");
    check(translated.locale == "pt-BR" && translated.source_offset == 100, "Translation identity facts lost");
    auto english = value(catalog.lookup(identity, 0, loc::Language::english));
    check(english.text == fields[10] && !english.translated && english.locale == "en", "Explicit English selection failed");
    auto fallback = value(catalog.lookup(identity, 1));
    check(fallback.text == "Fallback" && !fallback.translated && fallback.locale == "en", "Missing translation did not fall back");
    auto empty = value(catalog.lookup(identity, 2));
    check(empty.translated && empty.text.empty(), "Explicit empty translation confused with absence");
    check(value(catalog.producer_metadata(identity)) == fields[7], "Opaque command metadata changed");
    check(catalog.fragment_count() == 3 && catalog.translated_count() == 2, "Coverage count mismatch");
    check(std::get<loc::Error>(catalog.lookup(identity, 0, static_cast<loc::Language>(99))).code == loc::ErrorCode::unsupported_variant,
          "Unknown language selection silently fell back");
    check(std::get<loc::Error>(catalog.lookup(identity, 3)).code == loc::ErrorCode::missing_fragment, "Unknown fragment should fail");
    auto wrong = identity; wrong.source_sha256 = std::string(64, 'b');
    check(std::get<loc::Error>(catalog.lookup(wrong, 1)).code == loc::ErrorCode::source_identity_mismatch, "Hash mismatch silently fell back");
    wrong = identity; wrong.fragment_count = 4;
    check(std::get<loc::Error>(catalog.lookup(wrong, 1)).code == loc::ErrorCode::source_identity_mismatch, "Count mismatch silently fell back");
    wrong = identity; wrong.document = "Other";
    check(std::get<loc::Error>(catalog.lookup(wrong, 1)).code == loc::ErrorCode::missing_document, "Unknown document silently fell back");
    auto pin = expected(bytes); pin.package_sha256 = std::string(64, '0');
    check(std::get<loc::Error>(loc::Catalog::decode(bytes, pin)).code == loc::ErrorCode::package_hash_mismatch, "Package pin not checked");
    pin = expected(bytes); pin.documents.push_back(pin.documents.front());
    check(std::get<loc::Error>(loc::Catalog::decode(bytes, pin)).code == loc::ErrorCode::duplicate_identity, "Duplicate expected document accepted");
    pin = expected(bytes); pin.documents[0].source_sha256 = "bad";
    check(std::get<loc::Error>(loc::Catalog::decode(bytes, pin)).code == loc::ErrorCode::invalid_expected_identity, "Malformed expected digest accepted");
    auto mutate = [&](std::size_t index, std::string text, loc::ErrorCode code) {
        auto changed = fields; changed[index] = std::move(text); rejected(frame(changed), code);
    };
    mutate(0, "fr", loc::ErrorCode::unsupported_variant);
    mutate(0, "en", loc::ErrorCode::source_identity_mismatch);
    mutate(1, "ja", loc::ErrorCode::unsupported_variant);
    mutate(2, "other-profile", loc::ErrorCode::unsupported_variant);
    mutate(3, "0", loc::ErrorCode::resource_limit);
    mutate(3, "65", loc::ErrorCode::resource_limit);
    mutate(3, "2", loc::ErrorCode::source_identity_mismatch);
    mutate(4, "Other", loc::ErrorCode::source_identity_mismatch);
    mutate(5, std::string(64, 'b'), loc::ErrorCode::source_identity_mismatch);
    mutate(5, std::string(64, 'A'), loc::ErrorCode::malformed_package);
    mutate(6, "4294967296", loc::ErrorCode::malformed_package);
    mutate(7, "", loc::ErrorCode::malformed_package);
    mutate(8, "1", loc::ErrorCode::malformed_package);
    mutate(13, "0", loc::ErrorCode::duplicate_identity);
    mutate(9, "01", loc::ErrorCode::malformed_package);
    mutate(9, "-1", loc::ErrorCode::malformed_package);
    mutate(11, "2", loc::ErrorCode::unsupported_variant);
    mutate(17, "payload without presence", loc::ErrorCode::malformed_package);
    mutate(12, fields[10], loc::ErrorCode::malformed_package);
    mutate(10, std::string("N\0UL", 4), loc::ErrorCode::unsupported_variant);
    for (const auto& invalid : {std::string("\x80"), std::string("\xc0\xaf"), std::string("\xed\xa0\x80"),
                              std::string("\xf4\x90\x80\x80"), std::string("\xe2\x82"), std::string("\xc2x")})
        mutate(10, invalid, loc::ErrorCode::invalid_utf8);
    auto version = bytes; version[version.find(" 1\n") + 1] = '2'; rejected(version, loc::ErrorCode::unsupported_variant);
    auto noncanonical = bytes; noncanonical.insert(noncanonical.find('\n') + 1, "0"); rejected(noncanonical, loc::ErrorCode::malformed_package);
    rejected(bytes + "trailer", loc::ErrorCode::malformed_package);
    auto limit = loc::Limits{}; limit.package_bytes = bytes.size() - 1; rejected(bytes, loc::ErrorCode::resource_limit, limit);
    limit = {}; limit.text_bytes = 3; rejected(bytes, loc::ErrorCode::resource_limit, limit);
    limit = {}; limit.metadata_bytes = 3; rejected(bytes, loc::ErrorCode::resource_limit, limit);
    limit = {}; limit.fragments = 2; rejected(bytes, loc::ErrorCode::resource_limit, limit);
    for (std::size_t end = 0; end < bytes.size(); ++end)
        check(std::holds_alternative<loc::Error>(loc::Catalog::decode(std::string_view(bytes).substr(0, end),
              expected(std::string_view(bytes).substr(0, end)))), "Truncated prefix accepted");
    // Two records with the same name, with an otherwise valid expected set.
    auto duplicate = fields; duplicate[3] = "2";
    duplicate.insert(duplicate.end(), fields.begin() + 4, fields.end());
    const auto repeated = frame(duplicate); pin = expected(repeated);
    pin.documents.push_back({"Other", source_hash, 3});
    check(std::get<loc::Error>(loc::Catalog::decode(repeated, pin)).code == loc::ErrorCode::duplicate_identity,
          "Repeated document accepted");
    auto en_fields = fields; en_fields[0] = "en";
    auto en_bytes = frame(en_fields); pin = expected(en_bytes); pin.locale = "en";
    check(std::get<loc::Error>(loc::Catalog::decode(en_bytes, pin)).code == loc::ErrorCode::unsupported_variant,
          "English-only package accepted translation payloads");
    en_fields[11] = "0"; en_fields[12] = ""; en_fields[21] = "0";
    en_bytes = frame(en_fields); pin = expected(en_bytes); pin.locale = "en";
    auto en_catalog = value(loc::Catalog::decode(en_bytes, pin));
    check(en_catalog.translated_count() == 0 && value(en_catalog.lookup(identity, 0)).text == fields[10],
          "English-only catalog failed");

    // Exercise the public file boundary, including missing file, actual digest
    // mismatch and size rejection before the read allocation.
    const auto root = std::filesystem::temp_directory_path() /
        ("mh4u-localization-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    if (!std::filesystem::create_directory(root)) throw std::runtime_error("Cannot own temporary test directory");
    struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code ignored; std::filesystem::remove_all(path, ignored); } } cleanup{root};
    const auto path = root / "fixture.mhtxt";
    { std::ofstream out(path, std::ios::binary); out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); }
    check(value(loc::Catalog::load(path, expected(bytes))).fragment_count() == 3, "File load failed");
    check(std::get<loc::Error>(loc::Catalog::load(root / "missing", expected(bytes))).code == loc::ErrorCode::io,
          "Missing file accepted");
    check(std::get<loc::Error>(loc::Catalog::load(root, expected(bytes))).code == loc::ErrorCode::io, "Directory accepted");
    limit = {}; limit.package_bytes = bytes.size() - 1;
    check(std::get<loc::Error>(loc::Catalog::load(path, expected(bytes), limit)).code == loc::ErrorCode::resource_limit,
          "Oversize file accepted");
    { std::ofstream out(path, std::ios::binary | std::ios::app); out << 'x'; }
    check(std::get<loc::Error>(loc::Catalog::load(path, expected(bytes))).code == loc::ErrorCode::package_hash_mismatch,
          "Changed file accepted under old identity");
}
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> columns;
    for (std::size_t start = 0;;) {
        const auto at = line.find('\t', start); columns.push_back(line.substr(start, at - start));
        if (at == std::string::npos) return columns;
        start = at + 1;
    }
}
std::string unhex(const std::string& text) {
    if (text.size() % 2) throw std::runtime_error("Odd oracle hex length");
    auto nibble = [](char c) -> unsigned {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        throw std::runtime_error("Invalid oracle hex digit");
    };
    std::string result;
    for (std::size_t i = 0; i < text.size(); i += 2) result += static_cast<char>(nibble(text[i])*16+nibble(text[i+1]));
    return result;
}
void real(const std::filesystem::path& package, const std::string& pin, const std::filesystem::path& oracle,
          std::uint32_t expected_translations) {
#ifdef MH4U_LOCALIZATION_GENERATED
    auto expected_real = loc::generated::expected_catalog();
    check(pin == expected_real.package_sha256, "Caller pin differs from the selected generated catalog");
#else
    loc::ExpectedCatalog expected_real{"pt-BR", pin, {
        {"CharMake", "", 179},
        {"Common", "", 124},
        {"Title", "", 136}}};
#endif
    std::size_t expected_fragments = 0;
    for (const auto& identity : expected_real.documents) expected_fragments += identity.fragment_count;
    // The generated profile pins source identities at compilation. The legacy
    // profile reads them from the independent oracle and retains its caller pin.
    // Neither profile accepts source identity from the package being tested.
    std::ifstream input(oracle, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open real lookup oracle");
    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(input, line)) {
        auto cols = split(line); check(cols.size() == 7, "Real oracle column mismatch");
        check(rows.size() < 65536, "Real oracle exceeds bounded test size");
        rows.push_back(std::move(cols));
    }
    for (auto& identity : expected_real.documents) {
        const auto row = std::find_if(rows.begin(), rows.end(), [&](const auto& r) { return r[0] == identity.document; });
        check(row != rows.end(), "Expected document missing from real oracle");
#ifdef MH4U_LOCALIZATION_GENERATED
        check(identity.source_sha256 == (*row)[1], "Oracle differs from generated source identity");
#else
        identity.source_sha256 = (*row)[1];
#endif
    }
    const auto catalog = value(loc::Catalog::load(package, expected_real));
    std::size_t translated = 0;
    std::set<std::pair<std::string, std::uint32_t>> covered;
    for (const auto& row : rows) {
        const auto it = std::find_if(expected_real.documents.begin(), expected_real.documents.end(), [&](const auto& d) { return d.document == row[0]; });
        check(it != expected_real.documents.end() && it->source_sha256 == row[1], "Oracle source identity conflict");
        const auto index = static_cast<std::uint32_t>(std::stoul(row[2]));
        check(covered.emplace(row[0], index).second, "Oracle repeats a fragment instead of examining complete coverage");
        const auto original = unhex(row[4]), target = unhex(row[5]);
        const auto pt = value(catalog.lookup(*it, index));
        const auto en = value(catalog.lookup(*it, index, loc::Language::english));
        check(pt.text == target && pt.original == original && en.text == original, "Real Python/C++ text mismatch");
        check(pt.source_offset == std::stoul(row[3]) && pt.translated == (row[6] == "1"), "Real source offset/presence mismatch");
        check(!en.translated && en.locale == "en", "Real English mode mismatch");
        if (pt.translated) ++translated;
    }
    check(rows.size() == expected_fragments && catalog.fragment_count() == expected_fragments, "Real fragment coverage mismatch");
    check(translated == expected_translations && catalog.translated_count() == expected_translations,
          "Real translation coverage mismatch");
    std::cout << "REAL_LOOKUPS=" << rows.size() << " REAL_TRANSLATED=" << translated << " REAL_ENGLISH_FALLBACK=" << rows.size()-translated << '\n';
}
}
int main(int argc, char** argv) {
    try {
        synthetic();
        if (argc == 4 || argc == 5) {
            std::uint32_t count = 160; // Existing frozen reference catalog.
            if (argc == 5) {
                const std::string_view text(argv[4]);
                const auto parsed = std::from_chars(text.data(), text.data()+text.size(), count);
#ifdef MH4U_LOCALIZATION_GENERATED
                constexpr std::uint32_t count_limit = 65536;
#else
                constexpr std::uint32_t count_limit = 439;
#endif
                if (parsed.ec != std::errc{} || parsed.ptr != text.data()+text.size() || count > count_limit)
                    throw std::runtime_error("Expected translation count exceeds selected profile limits");
            }
            real(argv[1], argv[2], argv[3], count);
        }
        else if (argc != 1) throw std::runtime_error("Usage: localization-tests [package expected_sha256 oracle.tsv [expected_translations]]");
        std::cout << "LOCALIZATION_CHECKS=" << checks << '\n';
        return 0;
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
