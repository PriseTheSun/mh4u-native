#include "image.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>

namespace mh4u {
namespace {
constexpr std::size_t mib=1024U*1024U;
[[noreturn]] void fail(const std::string& reason) { throw std::runtime_error("PNG: "+reason); }
}
std::uint32_t big_endian(const std::uint8_t* p) { return (std::uint32_t(p[0])<<24)|(std::uint32_t(p[1])<<16)|(std::uint32_t(p[2])<<8)|p[3]; }
Image decode_png(const std::vector<std::uint8_t>& file,std::size_t decoded_budget) {
    constexpr std::array<std::uint8_t,8> signature{137,80,78,71,13,10,26,10};
    if(file.size()<33||!std::equal(signature.begin(),signature.end(),file.begin())||std::memcmp(file.data()+12,"IHDR",4)) fail("textura deve ser PNG");
    const auto width=big_endian(file.data()+16),height=big_endian(file.data()+20);
    if(!width||!height||width>16384||height>16384||std::uint64_t(width)*height>std::min(64*mib,decoded_budget)/4) fail("PNG excede limite de 64 MiB RGBA");
    using Surface=std::unique_ptr<SDL_Surface,decltype(&SDL_DestroySurface)>;
    SDL_IOStream* stream=SDL_IOFromConstMem(file.data(),file.size());
    if(!stream) fail(SDL_GetError());
    Surface source(SDL_LoadPNG_IO(stream,true),SDL_DestroySurface);
    if(!source) fail(std::string("PNG inválido: ")+SDL_GetError());
    Surface rgba(SDL_ConvertSurface(source.get(),SDL_PIXELFORMAT_RGBA32),SDL_DestroySurface);
    if(!rgba||rgba->w!=static_cast<int>(width)||rgba->h!=static_cast<int>(height)) fail("decodificação PNG inconsistente");
    Image image{static_cast<int>(width),static_cast<int>(height),std::vector<std::uint8_t>(std::size_t(width)*height*3),std::vector<std::uint8_t>(std::size_t(width)*height)};
    for(std::size_t y=0;y<height;++y) {
        const auto* row=static_cast<const std::uint8_t*>(rgba->pixels)+y*rgba->pitch;
        for(std::size_t x=0;x<width;++x) {
            const auto p=y*width+x; for(int c=0;c<3;++c) image.rgb[p*3+c]=row[x*4+c]; image.alpha[p]=row[x*4+3];
        }
    }
    return image;
}
}
