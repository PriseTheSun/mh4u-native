#pragma once
#include "player_input_storage.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace mh4u::player_fields {
// Additional source fields only. Shared input, policy, selection and transition
// fields stay in their existing owners; no persistent raw-memory projection.
struct PbwFields {
    std::optional<std::uint16_t> field3a8,field3ac,field3b8,field3bc;
    // b27928 writes individual bytes within these source words. Keep partial
    // knowledge; policy consumes a word only after all four lanes are known.
    std::array<player_input_storage::Word,19> words3d8{};
    std::array<player_input_storage::Byte,99> input_other{};
    std::optional<std::uint32_t> field14_bits,field478;
    std::optional<std::uint8_t> field6fb,field6fc,field93ee,field93f1;
    std::optional<std::uint32_t> field10_bits,field18_bits,field354_bits;
    std::optional<std::uint32_t> field1c_bits; // PBW position W, published by b07bb8.
    // Source previous position, distinct from current10/14/18 and model TRS.
    std::optional<std::uint32_t> field6e8_bits,field6ec_bits,field6f0_bits;
    std::optional<std::uint32_t> field6f4_bits;
    std::optional<std::uint32_t> field54_bits; // Raw accumulator updated by b09ccc.
    std::optional<std::uint8_t> field37a,field6c0; // PBW6c0 is distinct from pt.pw6c0.
    std::optional<std::uint32_t> field464_bits;
    bool operator==(const PbwFields&)const=default;
};
struct PwFields {
    std::optional<std::uint32_t> field14c,field154,field15c_bits,field6a4_bits;
    std::optional<std::uint8_t> field150,field158,field159,field15a,field6a8;
    std::optional<std::int16_t> field188,field224,field238;
    std::optional<std::uint32_t> field1f8,field220_bits,field838_bits;
    std::optional<std::uint8_t> field7f0;
    std::optional<std::uint32_t> field5a8;
    std::optional<std::uint8_t> field01,field0b,field589,field6a9;
    std::optional<std::uint32_t> field13c_bits;
    std::optional<std::uint8_t> field9dc,field9de,field9dd;
    bool operator==(const PwFields&)const=default;
};
struct Fields {PbwFields pbw;PwFields pw;bool operator==(const Fields&)const=default;};
} // namespace mh4u::player_fields
