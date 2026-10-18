#include "joint_pose.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
using namespace mh4u::joint_pose;
using Bytes=std::vector<unsigned char>;
std::size_t checks=0;
void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
Bytes bytes(std::istream& in,std::size_t size){Bytes out(size);in.read(reinterpret_cast<char*>(out.data()),static_cast<std::streamsize>(size));if(!in)throw std::runtime_error("Truncated oracle");return out;}
std::uint32_t word(const Bytes& b,std::size_t p){if(p>b.size()||b.size()-p<4)throw std::runtime_error("Oracle bounds");std::uint32_t out=0;for(unsigned i=0;i<4;++i)out|=std::uint32_t(b[p+i])<<(8*i);return out;}
std::uint32_t read(std::istream& in){return word(bytes(in,4),0);}
Bits3 three(const Bytes& b,std::size_t p){return {word(b,p),word(b,p+4),word(b,p+8)};}
Bits4 four(const Bytes& b,std::size_t p){return {word(b,p),word(b,p+4),word(b,p+8),word(b,p+12)};}
std::vector<Joint> joints(const Bytes& b){
    check(b.size()%168==0,"Joint record alignment");std::vector<Joint> out;
    for(std::size_t p=0;p<b.size();p+=168)out.push_back({three(b,p+0x48),four(b,p+0x58),four(b,p+0x68),four(b,p+0x78)});
    return out;
}
void equal(const std::vector<Joint>& out,const std::vector<Joint>& expected){
    check(out.size()==expected.size(),"Joint count");
    for(std::size_t i=0;i<out.size();++i){
        check(out[i].reference_translation==expected[i].reference_translation,"Reference translation preserved");
        for(unsigned k=0;k<4;++k){check(out[i].rotation[k]==expected[i].rotation[k],"Quaternion bits");check(out[i].scale[k]==expected[i].scale[k],"Scale/padding bits");check(out[i].translation[k]==expected[i].translation[k],"Translation/padding bits");}
    }
}
ModelReference model(std::uint32_t presence,std::uint32_t table,std::span<const Matrix> matrices){
    if(presence==0)return Unknown{};
    if(presence==1)return Null{};
    if(table==0)return PresentModel{Unknown{}};
    if(table==1)return PresentModel{Null{}};
    return PresentModel{matrices};
}
std::size_t oracle(const char* path){
    std::ifstream in(path,std::ios::binary);const auto magic=bytes(in,8);check(std::string_view(reinterpret_cast<const char*>(magic.data()),magic.size())=="MH4UJP01","Oracle identity");
    const auto count=read(in);check(count<=100000,"Case resource bound");
    for(unsigned i=0;i<count;++i){
        const auto flags=read(in),n=read(in),presence=read(in),table=read(in),m=read(in),mask=read(in);
        check(n<=256&&m<=256&&presence<=2&&table<=2&&mask<=7,"Fixture structural bounds");const auto raw_templates=bytes(in,40),before=bytes(in,n*168),raw_matrices=bytes(in,m*64),after=bytes(in,n*168);
        Templates templates;if(mask&1)templates.translation=three(raw_templates,0);if(mask&2)templates.rotation=four(raw_templates,12);if(mask&4)templates.scale=three(raw_templates,28);
        std::vector<Matrix> matrices(m);for(unsigned j=0;j<m;++j)for(unsigned k=0;k<16;++k)matrices[j][k]=word(raw_matrices,64*j+4*k);
        const auto previous=joints(before),expected=joints(after);const auto result=restore(previous,flags,model(presence,table,matrices),templates);
        const auto* out=std::get_if<Restored>(&result);
        if(!out)throw std::runtime_error("Rejected ARM case "+std::to_string(i)+": "+std::string(std::get<Error>(result).message));
        try{equal(out->joints,expected);equal(previous,joints(before));}
        catch(const std::exception& e){throw std::runtime_error("ARM case "+std::to_string(i)+": "+e.what());}
    }
    check(in.peek()==std::char_traits<char>::eof(),"Trailing oracle bytes");return count;
}
void error(const Result& result,ErrorCode code){const auto* e=std::get_if<Error>(&result);check(e&&e->code==code,"Expected typed error");}
Matrix identity(){Matrix out{};out[0]=out[5]=out[10]=out[15]=0x3f800000;return out;}
void boundaries(){
    std::vector<Joint> previous(2);previous[0]={{0x7fa12345,0x80000000,1},{1,2,3,4},{5,6,7,8},{9,10,11,12}};previous[1]=previous[0];const auto unchanged=previous;
    std::vector<Matrix> matrices{identity(),identity()};ModelReference reference=PresentModel{std::span<const Matrix>(matrices)};
    Templates templates{Bits3{1,0x7fa12345,0xff800000},Bits4{0x7f800000,0x80000000,1,0x7fa12345},Bits3{0x80000001,0xff800000,0x7fa12345}};
    auto r=std::get<Restored>(restore(previous,0x10000000,Unknown{},templates));
    check(r.joints[0].rotation==*templates.rotation&&r.joints[0].translation==Bits4{1,0x7fa12345,0xff800000,0},"Template raw bits and positive-zero padding");
    auto no_scale=templates;no_scale.scale.reset();error(restore(previous,0x10000000,Unknown{},no_scale),ErrorCode::missing_template);
    auto no_translation=templates;no_translation.translation.reset();error(restore(previous,0x10000000,reference,no_translation),ErrorCode::missing_template);
    auto no_rotation=templates;no_rotation.rotation.reset();error(restore(previous,0x10000000,reference,no_rotation),ErrorCode::missing_template);
    error(restore(previous,0,Unknown{},templates),ErrorCode::unknown_model);error(restore(previous,0,Null{},templates),ErrorCode::null_model);
    error(restore(previous,0,PresentModel{Unknown{}},templates),ErrorCode::unknown_table);error(restore(previous,0,PresentModel{Null{}},templates),ErrorCode::null_table);
    error(restore(previous,0,PresentModel{std::span<const Matrix>(matrices).first(1)},templates),ErrorCode::table_extent);
    error(restore(previous,0,reference,templates,{1}),ErrorCode::resource_limit);
    error(restore({},0,Unknown{},{}),ErrorCode::unknown_model);error(restore({},0,Null{},{}),ErrorCode::null_model);error(restore({},0,PresentModel{Unknown{}},{}),ErrorCode::unknown_table);
    check(std::get<Restored>(restore({},0,PresentModel{Null{}},{})).joints.empty(),"Zero count consumes known null table pointer without dereference");
    check(std::get<Restored>(restore({},0,PresentModel{std::span<const Matrix>{}},{})).joints.empty(),"Empty present table distinct from null");
    check(std::get<Restored>(restore({},0x10000000,Unknown{},{})).joints.empty(),"Zero template branch consumes no model or templates");
    Templates scale_only{{},{},templates.scale};r=std::get<Restored>(restore(previous,0,reference,scale_only));
    check(r.joints[0].translation==Bits4{0x7fa12345,0x80000000,1,0},"Reference raw bits preserved without numeric interpretation");
    matrices[1][0]=0x7fc12345;const auto late=restore(previous,0,reference,scale_only);error(late,ErrorCode::nonfinite_operand);check(std::get<Error>(late).joint_index==1,"Numeric error identifies late joint");equal(previous,unchanged);
    matrices[1]=identity();matrices[1][3]=matrices[1][7]=matrices[1][11]=matrices[1][12]=0x7f800000;
    check(std::holds_alternative<Restored>(restore(previous,0,reference,scale_only)),"Unconsumed matrix components may be nonfinite");
    matrices[0][0]=matrices[0][5]=0x7f7fffff;error(restore(previous,0,reference,scale_only),ErrorCode::numeric_overflow);matrices[0]=identity();
    const int rounding=std::fegetround();std::fesetround(FE_DOWNWARD);
    const auto bad_round=restore(previous,0,reference,scale_only),copy_round=restore(previous,0x10000000,Unknown{},templates),empty_round=restore({},0,PresentModel{Null{}},{});
    std::fesetround(rounding);error(bad_round,ErrorCode::floating_environment);check(std::holds_alternative<Restored>(copy_round)&&std::holds_alternative<Restored>(empty_round),"Unconsumed float environment bypassed");
#if defined(__aarch64__)
    struct Control{std::uint64_t old;Control(){asm volatile("mrs %0, fpcr":"=r"(old));const auto flush=old|(std::uint64_t(1)<<24);asm volatile("msr fpcr, %0"::"r"(flush));}~Control(){asm volatile("msr fpcr, %0"::"r"(old));}};
#elif defined(__SSE__)
    struct Control{unsigned old;Control(){asm volatile("stmxcsr %0":"=m"(old));const unsigned flush=old|(1U<<15)|(1U<<6);asm volatile("ldmxcsr %0"::"m"(flush));}~Control(){asm volatile("ldmxcsr %0"::"m"(old));}};
#endif
#if defined(__aarch64__) || defined(__SSE__)
    {Control control;error(restore(previous,0,reference,scale_only),ErrorCode::floating_environment);check(std::holds_alternative<Restored>(restore(previous,0x10000000,Unknown{},templates)),"Raw copies bypass FZ");}
#endif
}
} // namespace
int main(int argc,char** argv){try{if(argc>2)throw std::runtime_error("Usage: joint-pose-tests [oracle.bin]");boundaries();const auto cases=argc==2?oracle(argv[1]):0;std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_cases\":"<<cases<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
