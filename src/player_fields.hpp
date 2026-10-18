#pragma once
#include <array>
#include <cstdint>
#include <optional>

namespace mh4u::player_fields {
// Additional source fields only. Shared input, policy, selection and transition
// fields stay in their existing owners; no persistent raw-memory projection.
struct PbwFields {
    std::optional<std::uint16_t> field3a8,field3ac,field3b8,field3bc;
    std::array<std::optional<std::uint32_t>,19> flags3d8{}; // word0 is pt.pbw.field3d4
    std::optional<std::uint32_t> field14_bits,field478;
    std::optional<std::uint8_t> field6fb,field6fc,field93ee,field93f1;
    std::optional<std::uint32_t> field10_bits,field18_bits,field354_bits;
    bool operator==(const PbwFields&)const=default;
};
struct PwFields {
    std::optional<std::uint32_t> field14c,field154,field15c_bits,field6a4_bits;
    std::optional<std::uint8_t> field150,field158,field159,field15a,field6a8;
    std::optional<std::int16_t> field188,field224,field238;
    std::optional<std::uint32_t> field1f8,field220_bits,field838_bits;
    std::optional<std::uint8_t> field7f0;
    bool operator==(const PwFields&)const=default;
};
struct Fields {PbwFields pbw;PwFields pw;bool operator==(const Fields&)const=default;};
} // namespace mh4u::player_fields
