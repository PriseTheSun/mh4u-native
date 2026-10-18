#pragma once
#include <array>
#include <cstdint>
// Source data 0x1057648..0x1057750 from the pinned EU analysis ELF.
// sha256: 1eb6bf14a7f3e120cea2b613a3d356067adca49221e155afb00e3f90231cf7eb
namespace mh4u::player_reference::detail {
inline constexpr std::array<std::array<std::uint32_t,2>,33> table{{
    {0x00000000U,0x3fa2ebf1U},
    {0x3fa2ebf1U,0x3fa29ab1U},
    {0x4022c351U,0x3fa1f923U},
    {0x4073bfe2U,0x3fa10925U},
    {0x40a2223bU,0x3f9fcd71U},
    {0x40ca1597U,0x3f9e498cU},
    {0x40f1a7faU,0x3f9c81abU},
    {0x410c6432U,0x3f9a7a9bU},
    {0x411fb386U,0x3f98399dU},
    {0x4132bab9U,0x3f95c44bU},
    {0x41457343U,0x3f932075U},
    {0x4157d751U,0x3f905408U},
    {0x4169e1d2U,0x3f8d64ebU},
    {0x417b8e70U,0x3f8a58ecU},
    {0x41866cc7U,0x3f8735a9U},
    {0x418ee021U,0x3f84007dU},
    {0x41972029U,0x3f80be73U},
    {0x419f2c10U,0x3f7ae877U},
    {0x41a70354U,0x3f744c4aU},
    {0x41aea5b6U,0x3f6db032U},
    {0x41b61338U,0x3f671b36U},
    {0x41bd4c11U,0x3f60938fU},
    {0x41c450aeU,0x3f5a1eb2U},
    {0x41cb21a3U,0x3f53c157U},
    {0x41d1bfaeU,0x3f4d7f84U},
    {0x41d82baaU,0x3f475c97U},
    {0x41de668fU,0x3f415b59U},
    {0x41e4716aU,0x3f3b7e02U},
    {0x41ea4d5aU,0x3f35c652U},
    {0x41effb8cU,0x3f303593U},
    {0x41f57d39U,0x3f2accaeU},
    {0x41fad39eU,0x3f258c33U},
    {0x42000000U,0x3f207467U},
}};
} // namespace mh4u::player_reference::detail
