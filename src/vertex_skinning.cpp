#include "vertex_skinning.hpp"
#include <bit>
#include <cmath>
#include <limits>
#ifdef __FAST_MATH__
#error "vertex_skinning requires explicit ordered binary32 arithmetic"
#endif
namespace mh4u::vertex_skinning {
namespace {
struct Failure {Error error;};
struct Arithmetic {
    Stage stage=Stage::input;std::optional<std::size_t> influence;
    std::size_t component{};std::uint8_t palette_index{};
    [[noreturn]]void fail(ErrorCode code,std::string_view why)const{throw Failure{{code,why,stage,influence,component,palette_index}};}
    float input(float x)const{if(!std::isfinite(x))fail(ErrorCode::nonfinite_operand,"Consumed binary32 operand is nonfinite");return x;}
    float output(float x)const{if(!std::isfinite(x))fail(ErrorCode::numeric_overflow,"Nonfinite binary32 intermediate");return x;}
    float mul(float a,float b)const{input(a);input(b);volatile float x=a*b;return output(x);}
    float add(float a,float b)const{input(a);input(b);volatile float x=a+b;return output(x);}
    template<std::size_t N>float dot(const std::array<float,N>& a,const std::array<float,N>& b)const{
        float sum=mul(a[0],b[0]);for(std::size_t i=1;i<N;++i)sum=add(sum,mul(a[i],b[i]));return sum;
    }
};
float value(std::uint32_t bits){return std::bit_cast<float>(bits);}
std::uint32_t bits(float value){return std::bit_cast<std::uint32_t>(value);}
}
Result skin(const model_resource::Vertex& vertex,std::span<const Palette> palette,const View3& view,Limits limits){
    static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
    Arithmetic a;
    try {
        if(vertex.influence_count!=2&&vertex.influence_count!=4)a.fail(ErrorCode::unsupported_influences,"VS18 CPU profile requires two or four influences");
        if(palette.size()>limits.palette_entries)a.fail(ErrorCode::palette_limit,"Palette exceeds declared work bound");
        // Reuse the existing ordered-float owner for the environment probe.
        const auto environment=motion_transform_math::multiply(0x3f800000U,0x3f800000U);
        if(std::holds_alternative<motion_transform_math::Error>(environment))a.fail(ErrorCode::floating_environment,"Nearest-even binary32 and gradual underflow required");
        std::array<float,12> rows{};a.stage=Stage::blend;
        for(std::size_t i=0;i<vertex.influence_count;++i){
            a.influence=i;a.palette_index=vertex.joints[i];
            if(a.palette_index>=palette.size())a.fail(ErrorCode::invalid_palette_index,"Influence index outside supplied palette, including zero weight");
            const float weight=a.mul(value(weight_scale_bits),static_cast<float>(vertex.weights[i]));
            for(std::size_t k=0;k<12;++k){a.component=k;const float term=a.mul(weight,value(palette[a.palette_index][k]));rows[k]=i?a.add(rows[k],term):term;}
        }
        a.influence.reset();a.palette_index=0;
        const std::array<float,4> position{value(vertex.position_bits[0]),value(vertex.position_bits[1]),value(vertex.position_bits[2]),1.0F};
        const std::array<float,3> normal{static_cast<float>(vertex.normal[0]),static_cast<float>(vertex.normal[1]),static_cast<float>(vertex.normal[2])};
        std::array<float,3> skin_normal{},view_normal{};Output out{};
        for(std::size_t r=0;r<3;++r){a.component=r;a.stage=Stage::position;out.position_model[r]=bits(a.dot<4>({rows[r*4],rows[r*4+1],rows[r*4+2],rows[r*4+3]},position));
            a.stage=Stage::normal_skin;skin_normal[r]=a.dot<3>({rows[r*4],rows[r*4+1],rows[r*4+2]},normal);out.normal_skin[r]=bits(skin_normal[r]);}
        a.stage=Stage::normal_view;
        for(std::size_t r=0;r<3;++r){a.component=r;view_normal[r]=a.dot<3>({value(view[r*3]),value(view[r*3+1]),value(view[r*3+2])},skin_normal);out.normal_view[r]=bits(view_normal[r]);}
        a.stage=Stage::normal_length;a.component=0;const float square=a.dot<3>(view_normal,view_normal);
        if(!(square>0.0F))a.fail(ErrorCode::degenerate_normal,"Normal squared length is zero, including binary32 underflow");
        volatile float length=std::sqrt(square);a.output(length);volatile float reciprocal=1.0F/length;a.output(reciprocal);
        a.stage=Stage::normal_unit;for(std::size_t i=0;i<3;++i){a.component=i;out.normal_unit[i]=bits(a.mul(view_normal[i],reciprocal));}
        return out;
    }catch(const Failure& failure){return failure.error;}
}
} // namespace mh4u::vertex_skinning
