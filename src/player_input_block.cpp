#include "player_input_block.hpp"

namespace mh4u::player_input_block {
namespace {
constexpr auto base=player_input_storage::first;
template<class T>void put(Block& out,std::size_t offset,std::optional<T> value) noexcept {
    for(std::size_t i=0;i<sizeof(T);++i)
        out[offset-base+i]=value?player_input_storage::Byte{std::uint8_t(*value>>(8*i))}:std::nullopt;
}
template<class T>std::optional<T> get(const Block& b,std::size_t offset) noexcept {
    std::uint32_t value=0;
    for(std::size_t i=0;i<sizeof(T);++i){const auto byte=b[offset-base+i];if(!byte)return {};value|=std::uint32_t(*byte)<<(8*i);}
    return T(value);
}
}
Block project(const player_controller_step::PlayerFields& p,const player_fields::Fields& f) noexcept {
    Block b{};
    for(std::size_t i=0;i<player_input_storage::remaining_offsets.size();++i)
        b[player_input_storage::remaining_offsets[i]-base]=f.pbw.input_other[i];
    put(b,0x3a8,f.pbw.field3a8);put(b,0x3ac,f.pbw.field3ac);
    put(b,0x3b8,f.pbw.field3b8);put(b,0x3bc,f.pbw.field3bc);
    for(unsigned i=0;i<2;++i){put<std::uint16_t>(b,0x3c0+2*i,p.pbw.angles[i]);put<std::uint16_t>(b,0x3c4+2*i,p.pbw.magnitudes[i]);}
    put<std::uint32_t>(b,0x3d4,p.pbw.field3d4);
    for(unsigned i=0;i<19;++i)for(unsigned j=0;j<4;++j)b[0x3d8-base+4*i+j]=f.pbw.words3d8[i][j];
    put<std::uint8_t>(b,0x451,p.pbw.field451);
    return b;
}
Result publish(const Block& b,const player_controller_step::PlayerFields& p,const player_fields::Fields& f) noexcept {
    for(const auto [offset,width]:std::array<std::pair<std::size_t,std::size_t>,4>{{{0x3c0,4},{0x3c4,4},{0x3d4,4},{0x451,1}}})
        for(std::size_t i=0;i<width;++i)if(!b[offset-base+i])return Error{ErrorCode::unknown_required_alias,offset+i};
    for(const std::size_t offset:{0x3a8,0x3ac,0x3b8,0x3bc})
        if(b[offset-base].has_value()!=b[offset-base+1].has_value())return Error{ErrorCode::partial_optional_alias,offset};
    Output out{p,f};auto& x=out.player;auto& y=out.fields;
    for(std::size_t i=0;i<player_input_storage::remaining_offsets.size();++i)
        y.pbw.input_other[i]=b[player_input_storage::remaining_offsets[i]-base];
    y.pbw.field3a8=get<std::uint16_t>(b,0x3a8);y.pbw.field3ac=get<std::uint16_t>(b,0x3ac);
    y.pbw.field3b8=get<std::uint16_t>(b,0x3b8);y.pbw.field3bc=get<std::uint16_t>(b,0x3bc);
    for(unsigned i=0;i<2;++i){x.pbw.angles[i]=*get<std::uint16_t>(b,0x3c0+2*i);x.pbw.magnitudes[i]=*get<std::uint16_t>(b,0x3c4+2*i);}
    x.pbw.field3d4=*get<std::uint32_t>(b,0x3d4);
    for(unsigned i=0;i<19;++i)for(unsigned j=0;j<4;++j)y.pbw.words3d8[i][j]=b[0x3d8-base+4*i+j];
    x.pbw.field451=*get<std::uint8_t>(b,0x451);
    return out;
}
} // namespace mh4u::player_input_block
