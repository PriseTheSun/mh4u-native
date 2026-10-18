#pragma once
#include "assets.hpp"
#include <cstddef>

namespace mh4u {
// Checks decoded RGBA allocation against the caller's remaining budget before SDL decoding.
Image decode_png(const std::vector<std::uint8_t>&,std::size_t decoded_budget=64U*1024U*1024U);
}
