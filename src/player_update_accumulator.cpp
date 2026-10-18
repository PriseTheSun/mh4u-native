#include "player_update_accumulator.hpp"

namespace mh4u::player_update_accumulator {
Result apply(const player_fields::Fields& fields,std::optional<std::uint32_t> delta_bits){
    // Source loads player+14 before the accumulator at PBW+54.
    if(!delta_bits)return Error{ErrorCode::unknown_field,"player14",{}};
    if(!fields.pbw.field54_bits)return Error{ErrorCode::unknown_field,"PBW54",{}};
    auto summed=motion_transform_math::add(*fields.pbw.field54_bits,*delta_bits);
    if(auto* error=std::get_if<motion_transform_math::Error>(&summed))
        return Error{ErrorCode::arithmetic,"PBW54+player14",*error};
    const auto bits=std::get<std::uint32_t>(summed);
    // The source CMP/GE is signed integer, not a floating comparison. Avoid
    // implementation-defined unsigned-to-signed conversion on portable hosts.
    const bool clear=(bits&0x80000000U)==0&&bits>=0x4b189680U;
    auto out=Output{fields};out.fields.pbw.field54_bits=clear?0U:bits;
    return out;
}
} // namespace mh4u::player_update_accumulator
