#include "joint_pose.hpp"
#include <bit>
#include <cfenv>
#include <cmath>
#include <limits>
#include <new>
#include <stdexcept>

#ifdef __FAST_MATH__
#error "joint_pose requires ordered binary32 operations without fast-math"
#endif
namespace mh4u::joint_pose {
namespace {
struct Failure {Error error;};
[[noreturn]]void fail(ErrorCode code,std::string_view message,std::size_t index=0){throw Failure{{code,message,index}};}
float value(std::uint32_t bits){return std::bit_cast<float>(bits);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
struct Math {
    std::size_t index{};
    Math(){
        static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<float>::digits==24);
        if(std::fegetround()!=FE_TONEAREST)fail(ErrorCode::floating_environment,"Nearest-even arithmetic required");
        volatile float tiny=value(1),twice=tiny+tiny;
        if(bits(twice)!=2)fail(ErrorCode::floating_environment,"Gradual underflow required");
    }
    float input(float x){if(!std::isfinite(x))fail(ErrorCode::nonfinite_operand,"Consumed matrix operand is nonfinite",index);return x;}
    float result(float x){if(!std::isfinite(x))fail(ErrorCode::numeric_overflow,"Binary32 intermediate is nonfinite",index);return x;}
    float add(float a,float b){input(a);input(b);volatile float r=a+b;return result(r);}
    float sub(float a,float b){input(a);input(b);volatile float r=a-b;return result(r);}
    float mul(float a,float b){input(a);input(b);volatile float r=a*b;return result(r);}
    float div(float a,float b){input(a);input(b);if(b==0)fail(ErrorCode::numeric_domain,"Zero quaternion extraction divisor",index);volatile float r=a/b;return result(r);}
    float sqrt(float a){input(a);if(a<0)fail(ErrorCode::numeric_domain,"Negative quaternion extraction radicand",index);volatile float r=std::sqrt(a);return result(r);}
};
Bits4 vector(const Bits3& b){return {b[0],b[1],b[2],0};}
Bits4 rotation(const Matrix& source,Math& math){
    auto m=[&](unsigned i){return value(source[i]);};
    const float trace=math.add(math.add(m(0),m(5)),m(10));
    Bits4 out{};
    if(trace>0){
        const float root=math.sqrt(math.add(trace,1));
        const float factor=math.div(.5F,root);
        out[3]=bits(math.mul(root,.5F));
        out[0]=bits(math.mul(math.sub(m(6),m(9)),factor));
        out[1]=bits(math.mul(math.sub(m(8),m(2)),factor));
        out[2]=bits(math.mul(math.sub(m(1),m(4)),factor));
    }else{
        unsigned i=m(0)<m(5)?1:0;
        if(m(5*i)<m(10))i=2;
        constexpr std::array<unsigned,3> next{1,2,0};
        const auto j=next[i],k=next[j];
        const float root=math.sqrt(math.add(math.sub(math.sub(m(5*i),m(5*j)),m(5*k)),1));
        const float factor=math.div(.5F,root);
        out[i]=bits(math.mul(root,.5F));
        out[3]=bits(math.mul(math.sub(m(4*j+k),m(4*k+j)),factor));
        out[j]=bits(math.mul(math.add(m(4*i+j),m(4*j+i)),factor));
        out[k]=bits(math.mul(math.add(m(4*i+k),m(4*k+i)),factor));
    }
    return out;
}
} // namespace
std::variant<Bits4,Error> matrix_rotation(const Matrix& source){
    try{Math math;return rotation(source,math);}
    catch(const Failure& failure){return failure.error;}
}
Result restore(std::span<const Joint> previous,std::uint32_t flags,
    const ModelReference& model,const Templates& templates,Limits limits){
    if(previous.size()>limits.joints||previous.size()>std::numeric_limits<std::uint32_t>::max())
        return Error{ErrorCode::resource_limit,"Joint count exceeds caller/u32 bound",0};
    const bool copy=(flags&0x10000000U)!=0;
    std::span<const Matrix> matrices;
    if(!copy){
        if(std::holds_alternative<Unknown>(model))return Error{ErrorCode::unknown_model,"rModel presence is unknown",0};
        if(std::holds_alternative<Null>(model))return Error{ErrorCode::null_model,"Original rModel+6c read would dereference null",0};
        const auto& table=std::get<PresentModel>(model).matrices;
        if(std::holds_alternative<Unknown>(table))return Error{ErrorCode::unknown_table,"rModel+6c table pointer is unknown",0};
        if(std::holds_alternative<Null>(table)){
            if(!previous.empty())return Error{ErrorCode::null_table,"Original matrix access would dereference null",0};
        }else{
            matrices=std::get<std::span<const Matrix>>(table);
            if(matrices.size()<previous.size())return Error{ErrorCode::table_extent,"Matrix table does not cover all joints",matrices.size()};
        }
    }
    if(previous.empty())return Restored{};
    if(!templates.scale)return Error{ErrorCode::missing_template,"Scale template10e6380 is unknown",0};
    if(copy&&(!templates.translation||!templates.rotation))
        return Error{ErrorCode::missing_template,"Consumed translation/rotation template is unknown",0};
    // Structural validation precedes allocation; no output is published until
    // every joint has been restored, including any late numeric failure.
    try{
        Restored out{{previous.begin(),previous.end()}};
        if(copy)for(auto& joint:out.joints){
            joint.translation=vector(*templates.translation);joint.rotation=*templates.rotation;
            joint.scale=vector(*templates.scale);
        }else{
            Math math;
            for(std::size_t i=0;i<out.joints.size();++i){
                math.index=i;auto& joint=out.joints[i];
                joint.translation=vector(joint.reference_translation);
                joint.rotation=rotation(matrices[i],math);
                joint.scale=vector(*templates.scale);
            }
        }
        return out;
    }catch(const Failure& failure){return failure.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Joint allocation failed",0};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Joint allocation exceeds container capacity",0};}
}
} // namespace mh4u::joint_pose
