#pragma once
#include "player_input_storage.hpp"
#include <array>
#include <cstdint>
#include <optional>

namespace mh4u::player_fields {
// b2ac60 only tests the source pointer for presence; it never dereferences it.
// Keep each of the seven records distinct, including unknown presence/flag.
struct LinkedRecord {
    std::optional<bool> present; // PBW+7e84+0x318*i, projected as nonzero.
    std::optional<std::uint8_t> flag; // PBW+7e49+0x318*i.
    bool operator==(const LinkedRecord&)const=default;
};
// Additional source fields only. Shared input, policy, selection and transition
// fields stay in their existing owners; no persistent raw-memory projection.
using Descriptor28=std::array<std::optional<std::uint8_t>,28>;
struct BonusRecord {
    std::optional<std::uint16_t> id;
    std::optional<std::int16_t> value;
    bool operator==(const BonusRecord&)const=default;
};
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
    std::optional<std::int16_t> field34c;
    std::array<LinkedRecord,7> linked_records{};
    // 2f8e04 snapshots PBW1c0, then PBW70+28*i for i=0..5. Byte
    // knowledge is preserved; copies alone do not make unused bytes known.
    std::array<Descriptor28,7> descriptors{};
    // The source index is a raw u8 kind. Do not infer a smaller PBW extent
    // from the currently supported record-provider categories.
    std::array<std::optional<std::uint32_t>,256> words9404{};
    std::optional<std::uint16_t> field37c{},field37e{};
    std::optional<std::uint32_t> field380_bits{},field384_bits{};
    std::optional<std::int16_t> field34e;
    std::array<BonusRecord,40> bonus236{};
    std::array<std::optional<std::uint32_t>,5> fields388_bits{}; // 388..398.
    std::optional<std::uint8_t> field93e9;
    std::optional<std::uint32_t> field368_bits; // b22164; distinct from PBW358.
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
    std::optional<std::uint8_t> field0a,field319,field338,field33a,fielda2c;
    std::optional<std::uint32_t> field31c,field324_bits,field1b8_bits,field1bc_bits;
    std::optional<std::uint32_t> field328_bits,field32c_bits;
    std::optional<std::uint8_t> field7fd,field7f1,field7f4;
    std::optional<std::uint8_t> field262,field805,field806,field81c,fielda15;
    std::optional<std::int8_t> field274,field275,field276;
    std::optional<std::uint32_t> field278_bits,field284_bits,field288_bits;
    std::optional<std::uint32_t> field1a0_bits,field1a4_bits;
    std::array<std::optional<std::uint32_t>,5> fields248_bits{}; // 248..258.
    std::array<std::optional<std::uint8_t>,5> fields814{}; // PW804+[16..20].
    std::array<std::optional<std::uint32_t>,5> fields864_bits{}; // PW824+4*[16..20].
    std::optional<std::int16_t> field294,field296;
    std::optional<std::uint32_t> field4f8,field4fc;
    std::optional<std::uint32_t> field10_bits,fieldc0_bits;
    std::optional<std::uint8_t> field339,field80f;
    // b14610 position/timer phase. Saved position and per-axis increments
    // remain distinct from the canonical PBW current position.
    std::optional<std::uint32_t> field90_bits,field94_bits,field98_bits;
    std::optional<std::uint32_t> fielda0_bits,fielda4_bits,fielda8_bits,fieldb8_bits;
    // b22164 status producer; existing status/input fields above retain ownership.
    std::optional<std::uint16_t> field17e;
    std::optional<std::int16_t> field190,field198,field240,field29a,field2a0,field2a2;
    std::optional<std::int8_t> field1d0;
    std::optional<std::uint8_t> field265,field299;
    std::optional<std::uint32_t> field5c_bits,field68_bits,field6c_bits;
    std::optional<std::uint32_t> field1a8_bits,field1b4_bits,field1c0_bits;
    std::optional<std::uint32_t> field268_bits,field28c_bits,field2a4_bits,field29c_bits;
    std::optional<std::uint32_t> field2c8_bits,field2cc_bits,field2f0_bits,field2fc_bits;
    std::optional<std::uint32_t> field940_bits,field948_bits,field950_bits;
    // b41804 consumes previous status masks and the notification selector.
    // Their producers are separate; begin_update/status never copy or seed them.
    std::optional<std::uint32_t> field1fc,field200;
    std::optional<std::uint16_t> field586;
    // b29f7c material bank selector. Its resource tables have separate owners.
    std::optional<std::uint8_t> field9e8;
    bool operator==(const PwFields&)const=default;
};
struct Fields {PbwFields pbw;PwFields pw;bool operator==(const Fields&)const=default;};
} // namespace mh4u::player_fields
