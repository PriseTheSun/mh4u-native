#pragma once
#include <cstdint>
#include <optional>

namespace mh4u::player_position_bridge {
// Persistent position/bridge facts; other position and model fields retain
// their canonical owners. This declaration is shared without a frame pipeline.
struct Fields {
    std::optional<std::uint8_t> fieldd2c,fieldd44;
    std::optional<std::uint32_t> fieldd30_bits,fieldd34_bits,fieldd38_bits,fieldd64;
    std::optional<std::uint16_t> euler_mode; // low16(model38); hierarchy owns high16.
    bool operator==(const Fields&)const=default;
};
}
