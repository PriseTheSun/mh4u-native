#include "player_update_begin.hpp"

namespace mh4u::player_update_begin {
Result apply(const player_controller_step::PlayerFields& player,
    const player_fields::Fields& fields) noexcept {
    const auto missing=[](std::string_view field)->Result{return Error{ErrorCode::unknown_field,field};};
    if(!fields.pbw.field10_bits)return missing("PBW10");
    if(!fields.pbw.field14_bits)return missing("PBW14");
    if(!fields.pbw.field18_bits)return missing("PBW18");
    if(!player.selection_pbw.field9)return missing("PBW9");
    if(!fields.pw.field1f8)return missing("PW1f8");
    Output out{player,fields};
    auto& p=out.player;auto& f=out.fields;
    // Source store order. Distinct typed owners rule out raw arena aliasing.
    f.pbw.field6e8_bits=fields.pbw.field10_bits;
    f.pbw.field6ec_bits=fields.pbw.field14_bits;
    f.pbw.field6f0_bits=fields.pbw.field18_bits;
    f.pbw.field6f4_bits=0;
    f.pw.field0b=0;
    f.pbw.field37a=player.selection_pbw.field9;
    f.pw.field589=0;
    f.pbw.field93f1=0;
    p.selection_pbw.field6fa=0;
    f.pbw.field6fc=0;
    f.pw.field1f8=*fields.pw.field1f8&~std::uint32_t{0x1000};
    f.pw.field6a8=0;
    f.pw.field6a9=0;
    p.pw.fielde8&=~std::uint32_t{4};
    f.pbw.field6c0=0;
    f.pw.field01=0;
    return out;
}
} // namespace mh4u::player_update_begin
