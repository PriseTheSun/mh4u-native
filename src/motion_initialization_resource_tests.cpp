#include "motion_initialization_test_support.hpp"
#include "motion_resource.hpp"
#include <map>
namespace {
using namespace initialization_test;
namespace mr=mh4u::motion_resource;
void remap(State& state,std::uint64_t current) {
    constexpr auto old=std::numeric_limits<std::uint64_t>::max();
    auto channel=[&](mb::Channel& c) {
        if(c.track)c.track->resource_identity=c.track->resource_identity==1?current:old;
        if(c.key_data)c.key_data->resource_identity=c.key_data->resource_identity==1?current:old;
    };
    channel(state.tracks.translation);channel(state.tracks.rotation);
    for(auto& j:state.tracks.joints)for(auto& c:j.channels)channel(c);
    for(auto& b:state.layer.bindings)if(b)b->resource_identity=b->resource_identity==1?current:old;
}
Resolution resolve(const void* context,const mb::TrackReference& id) {
    auto result=static_cast<const mr::Resource*>(context)->sampler_track(id);
    if(const auto* e=std::get_if<mr::Error>(&result))return ResolutionError{static_cast<std::uint32_t>(e->code),e->message};
    return std::get<ms::TrackView>(result);
}
std::size_t oracle(const char* path,const std::map<std::uint32_t,mr::Resource>& resources) {
    std::ifstream in(path,std::ios::binary);const auto magic=bytes(in,8);
    check(std::string_view(reinterpret_cast<const char*>(magic.data()),magic.size())=="MH4UMR01","Real oracle identity");
    const auto count=read(in);check(count<100000,"Oracle count bound");
    for(std::uint32_t i=0;i<count;++i) {
        const auto entry=read(in),index=read(in),bank=read(in),slot=read(in),present=read(in),frames=read(in),loop=read(in),flags=read(in),n=read(in),m=read(in),delta=read(in);
        check(index<=3&&n<=255&&m<=4096&&present==1,"Oracle resource bound");
        const auto globals=bytes(in,56),before=bytes(in,304),map=bytes(in,256),before_joints=bytes(in,n*52),raw_tracks=bytes(in,m*36),after=bytes(in,304),after_joints=bytes(in,n*52);
        const auto samples=read(in);check(samples<=6,"Sampler bound");for(unsigned j=0;j<samples;++j){read(in);read(in);}
        const auto& resource=resources.at(entry);const auto bound=resource.binding_action(bank,slot);
        const auto* optional=std::get_if<std::optional<mb::Action>>(&bound);check(optional&&optional->has_value(),"Real action present");
        const auto& action=**optional;
        check(action.resource_identity==resource.identity()&&action.bank==bank&&action.slot==slot,"Resource identity owner");
        check(action.header.frame_count==frames&&std::bit_cast<std::uint32_t>(action.header.loop_frame_raw)==loop&&action.flags==flags,"Original action facts");
        check(action.tracks.size()==m,"Original track count");
        for(unsigned j=0;j<m;++j) {
            check(action.tracks[j].metadata==word(raw_tracks,36*j)&&action.tracks[j].weight_bits==word(raw_tracks,36*j+4),"Original track descriptors");
            const auto key=word(raw_tracks,36*j+12);check(action.tracks[j].key_data_offset==(key?std::optional<std::uint64_t>(key-0x2900000):std::nullopt),"Key ownership offset");
        }
        auto initial=state(before,before_joints,bank,slot),expected=state(after,after_joints,bank,slot);remap(initial,resource.identity());remap(expected,resource.identity());
        const auto result=initialize(initial,map,&action,parameters(globals,delta),{&resource,resolve});const auto* out=std::get_if<Output>(&result);
        if(!out)throw std::runtime_error("Rejected real ARM case "+std::to_string(i)+" entry "+std::to_string(entry)+" slot "+std::to_string(slot)+": "+std::string(std::get<Error>(result).message));
        try{check(out->samples==samples,"Original sampler count");equal(out->state,expected);}
        catch(const std::exception& e){throw std::runtime_error("Real ARM case "+std::to_string(i)+": "+e.what());}
    }
    check(in.peek()==std::char_traits<char>::eof(),"Trailing oracle data");return count;
}
} // namespace
int main(int argc,char** argv) {
    try {
        if(argc<5||(argc-2)%3!=0)throw std::runtime_error("Usage: motion-initialization-resource-tests oracle.bin entry source.lmt sha256 [entry source.lmt sha256 ...]");
        std::map<std::uint32_t,mr::Resource> resources;
        for(int i=2;i<argc;i+=3) {
            std::size_t consumed=0;const auto id=std::stoul(argv[i],&consumed);check(consumed==std::string(argv[i]).size()&&id<=0xffffffffU,"Entry index");
            auto loaded=mr::Resource::load(argv[i+1],argv[i+2]);
            if(const auto* e=std::get_if<mr::Error>(&loaded))throw std::runtime_error(e->message);
            check(resources.emplace(static_cast<std::uint32_t>(id),std::get<mr::Resource>(std::move(loaded))).second,"Unique resource entry");
        }
        const auto count=oracle(argv[1],resources);
        std::cout<<"{\"status\":\"passed\",\"checks\":"<<binding_test::checks<<",\"real_arm_cases\":"<<count<<"}\n";return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
