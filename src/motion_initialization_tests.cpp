#include "motion_initialization_test_support.hpp"
namespace {
using namespace initialization_test;
std::size_t oracle(const char* path) {
    std::ifstream in(path,std::ios::binary);const auto magic=bytes(in,8);
    check(std::string_view(reinterpret_cast<const char*>(magic.data()),magic.size())=="MH4UMI01","Oracle identity");
    const auto count=read(in);check(count<100000,"Oracle count bound");
    for(std::uint32_t i=0;i<count;++i) {
        const auto index=read(in),bank=read(in),slot=read(in),present=read(in),frames=read(in),loop=read(in),flags=read(in),n=read(in),m=read(in),delta=read(in);
        check(index<=3&&n<=255&&m<=4096,"Oracle resource bound");
        const auto globals=bytes(in,56),before=bytes(in,304),map=bytes(in,256),before_joints=bytes(in,n*52),raw_tracks=bytes(in,m*36),after=bytes(in,304),after_joints=bytes(in,n*52);
        const auto sample_count=read(in);check(sample_count<=6,"At most six sampler calls");
        for(unsigned j=0;j<sample_count;++j){const auto offset=read(in);read(in);check(offset==0xd8||offset==0xe8,"Source samples model channels only");}
        std::vector<mb::Track> descriptors;Views views;
        for(unsigned j=0;j<m;++j) {
            const auto key=word(raw_tracks,j*36+12);mb::Track descriptor{word(raw_tracks,j*36),word(raw_tracks,j*36+4),{}};
            if(key){check(key>=0x2900000&&key<0x2a00000,"Key fixture ownership");descriptor.key_data_offset=key-0x2900000;}
            descriptors.push_back(descriptor);views.rows.push_back({{1,bank,slot,j},descriptor.metadata,four(raw_tracks,j*36+16),{}, {}});
        }
        const auto initial=state(before,before_joints,bank,slot);const auto expected=state(after,after_joints,bank,slot);
        mh4u::motion_request::Action header{frames,std::bit_cast<std::int32_t>(loop),true,{}};
        const mb::Action action{1,bank,slot,header,flags,descriptors};
        const auto result=initialize(initial,map,present?&action:nullptr,parameters(globals,delta),views.resolver());
        const auto* out=std::get_if<Output>(&result);
        if(!out)throw std::runtime_error("Unexpected rejection ARM case "+std::to_string(i)+": "+std::string(std::get<Error>(result).message));
        try{check(out->action_present==(present!=0),"Action presence");check(out->samples==sample_count,"Original sampler count");equal(out->state,expected);}
        catch(const std::exception& e){throw std::runtime_error("ARM case "+std::to_string(i)+": "+e.what());}
    }
    check(in.peek()==std::char_traits<char>::eof(),"Trailing oracle data");return count;
}
void error(const Result& r,ErrorCode code) {
    const auto* e=std::get_if<Error>(&r);check(e&&e->code==code,"Expected typed rejection");
}
void boundaries() {
    State state{};state.layer.frame=2;state.layer.speed=1;state.prediction_rotation={0,0,0,0x3f800000};
    mh4u::motion_request::Action header{120,-1,true,{}};std::vector<mb::Track> tracks;
    mb::Action action{1,3,7,header,0,tracks};std::array<std::uint8_t,256> map;map.fill(255);
    Parameters p{0x3f800000,{Bits3{},Bits3{},Bits4{0,0,0,0x3f800000},Bits4{}}};Views views;
    const auto good=std::get<Output>(initialize(state,map,&action,p,views.resolver()));
    check(good.samples==6&&good.state.previous_rotation==Bits4{0,0,0,0x3f800000},"Absent channels use source dispatcher default");
    state.layer.attributes=8|0x1000|2;
    auto bad=p;bad.templates.base_translation_10e62c0.reset();error(initialize(state,map,&action,bad,{}),ErrorCode::missing_template);
    bad=p;bad.templates.identity_10e6560.reset();error(initialize(state,map,&action,bad,{}),ErrorCode::missing_template);
    bad=p;bad.templates.previous_translation_10e6370.reset();error(initialize(state,map,&action,bad,{}),ErrorCode::missing_template);
    state.layer.attributes=0;bad=p;bad.templates={};check(std::holds_alternative<Output>(initialize(state,map,&action,bad,{})),"Unconsumed templates remain unknown");
    tracks={{(255U<<24)|(3U<<8)|1,0x3f800000,{}}};action.tracks=tracks;
    auto unresolved=initialize(state,map,&action,p,{});error(unresolved,ErrorCode::unresolved_track);check(std::get<Error>(unresolved).track==mb::TrackReference{1,3,7,0},"Resolution error preserves identity");
    unresolved=initialize(state,map,&action,p,views.resolver());error(unresolved,ErrorCode::unresolved_track);check(std::get<Error>(unresolved).resolution->code==73,"Resolution reason preserved");
    views.rows={{{1,3,7,0},tracks[0].metadata,{0,0,0,0},{},{}}};bad=p;bad.templates.inverse_fallback_10e6570.reset();error(initialize(state,map,&action,bad,views.resolver()),ErrorCode::missing_template);
    views.rows[0].reference_bits={0x7f800000,0,0,0};error(initialize(state,map,&action,p,views.resolver()),ErrorCode::nonfinite_operand);
    views.rows[0].reference_bits={0x7f7fffff,0,0,0};error(initialize(state,map,&action,p,views.resolver()),ErrorCode::numeric_overflow);
    views.rows[0].reference_bits={0,0,0,1};check(std::holds_alternative<Output>(initialize(state,map,&action,p,views.resolver())),"Tiny quaternion uses explicit zero-norm fallback");
    views.rows[0].reference_bits={std::bit_cast<std::uint32_t>(1e-20F),0,0,0};error(initialize(state,map,&action,p,views.resolver()),ErrorCode::numeric_overflow);
    const auto saved=state;
    state.layer.attributes=8|0x1000|2;
    state.layer.frame=std::numeric_limits<float>::quiet_NaN();state.layer.requested_frame=state.layer.frame;
    bad=p;bad.model_delta_time_bits=0x7f800000;bad.templates.identity_10e6560=Bits4{0x7fa12345,0x80000000,1,0x7f800000};
    const auto copied=std::get<Output>(initialize(state,map,&action,bad,{}));
    check(copied.samples==0&&copied.state.previous_rotation==*bad.templates.identity_10e6560,"Raw template copies and unconsumed nonfinite values preserved");
    const auto before=state;error(initialize(state,{},&action,p,{}),ErrorCode::binding);equal(state,before);
    const int rounding=std::fegetround();std::fesetround(FE_DOWNWARD);
    auto wrong_round=initialize(state,map,&action,p,{});auto absent=initialize(state,{},nullptr,{},{});
    std::fesetround(rounding);error(wrong_round,ErrorCode::binding);check(!std::get<Output>(absent).action_present,"Null action bypasses unconsumed numeric environment");equal(std::get<Output>(absent).state,state);
    state=saved;tracks.clear();action.tracks=tracks;
    state.layer.attributes=0x400;state.layer.speed=std::numeric_limits<float>::max();bad=p;bad.model_delta_time_bits=std::bit_cast<std::uint32_t>(2.F);error(initialize(state,map,&action,bad,{}),ErrorCode::numeric_overflow);
    state.layer.attributes=0;
#if defined(__aarch64__)
    struct Control {std::uint64_t before;Control(){asm volatile("mrs %0, fpcr":"=r"(before));const auto flush=before|(std::uint64_t(1)<<24);asm volatile("msr fpcr, %0"::"r"(flush));}~Control(){asm volatile("msr fpcr, %0"::"r"(before));}};
#elif defined(__SSE__)
    struct Control {unsigned before;Control(){asm volatile("stmxcsr %0":"=m"(before));const unsigned flush=before|(1U<<15)|(1U<<6);asm volatile("ldmxcsr %0"::"m"(flush));}~Control(){asm volatile("ldmxcsr %0"::"m"(before));}};
#endif
#if defined(__aarch64__) || defined(__SSE__)
    {Control control;auto rejected=initialize(state,map,&action,p,{});error(rejected,ErrorCode::floating_environment);}
#endif
}
} // namespace
int main(int argc,char** argv) {
    try{if(argc>2)throw std::runtime_error("Usage: motion-initialization-tests [oracle.bin]");boundaries();const auto count=argc==2?oracle(argv[1]):0;
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<binding_test::checks<<",\"arm_cases\":"<<count<<"}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
