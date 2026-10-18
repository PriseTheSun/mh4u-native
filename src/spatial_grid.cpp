#include "spatial_grid.hpp"
#include <bit>
#include <limits>

#ifdef __FAST_MATH__
#error "spatial_grid requires ordered binary32 arithmetic"
#endif

namespace mh4u::spatial_grid {
namespace {
namespace math=motion_transform_math;
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view field){throw Failure{{code,field,{}}};}
Word arithmetic(math::Result<Word> result,std::string_view field){
    if(const auto* error=std::get_if<math::Error>(&result)){
        ErrorCode code=ErrorCode::unsupported_arithmetic;
        if(error->code==math::ErrorCode::floating_environment)code=ErrorCode::floating_environment;
        else if(error->code==math::ErrorCode::nonfinite_operand)code=ErrorCode::nonfinite_operand;
        else if(error->code==math::ErrorCode::numeric_overflow)code=ErrorCode::numeric_overflow;
        throw Failure{{code,field,*error}};
    }
    return std::get<Word>(result);
}
Word sub(Word a,Word b,std::string_view field){return arithmetic(math::add(a,b^0x80000000U),field);}
Word mul(Word a,Word b,std::string_view field){return arithmetic(math::multiply(a,b),field);}
Word div(Word a,Word b,std::string_view field){return arithmetic(math::divide(a,b),field);}
Word finite(Word word,std::string_view field){
    if((word&0x7f800000U)==0x7f800000U)fail(ErrorCode::nonfinite_operand,field);
    return word;
}
// Finite IEEE comparisons performed on bits preserve subnormal/zero semantics
// even when the host FP environment would flush them. An outside miss does not
// require arithmetic. Actual SUB/MUL/DIV use the shared guarded FP owner.
bool less(Word a,Word b){
    if((a&0x7fffffffU)==0&&(b&0x7fffffffU)==0)return false;
    const auto order=[](Word x){return (x&0x80000000U)?~x:(x|0x80000000U);};
    return order(a)<order(b);
}
std::int16_t narrow(Word coordinate,std::string_view field){
    // VCVT.S32.F32 truncates toward zero, independent of the FPSCR rounding
    // setting. Saturation/NaN are excluded; never use an out-of-range C++ cast.
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    finite(coordinate,field);
    const float value=std::bit_cast<float>(coordinate);
    if(value < -2147483648.0F || value >= 2147483648.0F)fail(ErrorCode::coordinate_domain,field);
    const auto integer=static_cast<std::int32_t>(value);
    const auto low=static_cast<std::uint32_t>(integer)&0xffffU;
    const auto extended=low<0x8000U?static_cast<std::int32_t>(low):static_cast<std::int32_t>(low)-65536;
    return static_cast<std::int16_t>(extended);
}
}

Result<Prepared> Prepared::prepare(collision_resource::Resource resource,std::uint32_t group){
    const auto parsed=resource.grid(group);
    if(const auto* error=std::get_if<collision_resource::Error>(&parsed))return Error{ErrorCode::resource,"GRCO",*error};
    const auto grid=std::get<collision_resource::Grid>(parsed);
    try{
        // 345bc8..345c28: Z subtract first, X subtract second, X division,
        // Z division, X reciprocal, Z reciprocal. Counts are u16 source facts,
        // exactly representable by the original VCVT.F32.U32 conversion.
        const auto dz=sub(grid.bbox_max[2],grid.bbox_min[2],"setup extent Z");
        const auto dx=sub(grid.bbox_max[0],grid.bbox_min[0],"setup extent X");
        const auto nx=std::bit_cast<Word>(static_cast<float>(grid.nx));
        const auto nz=std::bit_cast<Word>(static_cast<float>(grid.nz));
        const auto sx=div(dx,nx,"setup cell X");
        const auto sz=div(dz,nz,"setup cell Z");
        const auto rx=div(0x3f800000U,sx,"setup reciprocal X");
        const auto rz=div(0x3f800000U,sz,"setup reciprocal Z");
        Setup setup{{grid.bbox_min[0],grid.bbox_min[1],grid.bbox_min[2],0},
                    {grid.bbox_max[0],grid.bbox_max[1],grid.bbox_max[2],0},
                    {sx,0,sz,0},{rx,0,rz,0},
                    {grid.bbox_min[0],grid.bbox_min[1],grid.bbox_min[2],0},grid.nx,grid.nz};
        return Prepared{std::move(resource),grid,setup};
    }catch(const Failure& error){return error.error;}
}

Result<std::optional<Cell>> Prepared::lookup(const PositionXZ& position)const{
    try{
        // 346a70..346ab8: inclusive X then Z gates. Bounds Y/W are never read.
        if(!position.x)fail(ErrorCode::unknown_operand,"position X");
        const auto x=finite(*position.x,"position X");
        if(less(x,setup_.bbox_min[0])||less(setup_.bbox_max[0],x))return std::optional<Cell>{};
        if(!position.z)fail(ErrorCode::unknown_operand,"position Z");
        const auto z=finite(*position.z,"position Z");
        if(less(z,setup_.bbox_min[2])||less(setup_.bbox_max[2],z))return std::optional<Cell>{};
        const auto dx=sub(x,setup_.bbox_min[0],"cell delta X");
        const auto dz=sub(z,setup_.bbox_min[2],"cell delta Z");
        const auto sx=mul(dx,setup_.reciprocal[0],"cell coordinate X");
        const auto sz=mul(dz,setup_.reciprocal[2],"cell coordinate Z");
        const auto ix=narrow(sx,"cell index X");
        const auto iz=narrow(sz,"cell index Z");
        // 3487ac..3487bc checks only signed upper bounds. Valid prepared
        // source bounds/counts cannot produce a negative narrowed coordinate;
        // refuse that unsafe address domain explicitly instead of indexing it.
        if(ix>=setup_.nx||iz>=setup_.nz)return std::optional<Cell>{};
        if(ix<0||iz<0)fail(ErrorCode::coordinate_domain,"negative cell address");
        const auto index=std::uint32_t(iz)*setup_.nx+std::uint32_t(ix);
        const auto candidates=resource_.cell_pairs(grid_.group,index);
        if(const auto* error=std::get_if<collision_resource::Error>(&candidates))return Error{ErrorCode::resource,"cell candidates",*error};
        return std::optional<Cell>{Cell{resource_.sha256(),grid_.group,index,ix,iz,
            &grid_.cells[index],std::get<std::span<const std::uint16_t>>(candidates)}};
    }catch(const Failure& error){return error.error;}
}
} // namespace mh4u::spatial_grid
