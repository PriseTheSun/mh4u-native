#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace mh4u::player_input_storage {
using Byte=std::optional<std::uint8_t>;
using Word=std::array<Byte,4>;
constexpr Word split(std::optional<std::uint32_t> value) noexcept {
    Word out{};
    if(value)for(unsigned i=0;i<4;++i)out[i]=std::uint8_t(*value>>(i*8));
    return out;
}
constexpr std::optional<std::uint32_t> join(const Word& bytes) noexcept {
    std::uint32_t value=0;
    for(unsigned i=0;i<4;++i){if(!bytes[i])return {};value|=std::uint32_t(*bytes[i])<<(i*8);}
    return value;
}
inline constexpr std::size_t first=0x3a0,end=0x464,size=end-first;
// Existing fields have their own canonical owners. Only their complement
// lives in PbwFields::input_other; no shadow copy of these aliases is stored.
constexpr bool separately_owned(std::size_t offset) noexcept {
    return (offset>=0x3a8&&offset<0x3aa)||(offset>=0x3ac&&offset<0x3ae)||
        (offset>=0x3b8&&offset<0x3ba)||(offset>=0x3bc&&offset<0x3be)||
        (offset>=0x3c0&&offset<0x3c8)||(offset>=0x3d4&&offset<0x424)||offset==0x451;
}
inline constexpr auto remaining_offsets=[]{
    std::array<std::uint16_t,99> result{};std::size_t index=0;
    for(std::size_t offset=first;offset<end;++offset)
        if(!separately_owned(offset))result[index++]=std::uint16_t(offset);
    return result;
}();
static_assert(remaining_offsets.back()==0x463);
} // namespace mh4u::player_input_storage
