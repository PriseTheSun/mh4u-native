#include "motion_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace mr=mh4u::motion_resource;
namespace mb=mh4u::motion_binding;
namespace mq=mh4u::motion_request;
namespace ms=mh4u::motion_sampler;
namespace {
std::size_t checks=0,resources=0,actions=0,tracks=0,records=0;
void check(bool v,const char* why){++checks;if(!v)throw std::runtime_error(why);}
std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}
void put(std::string& b,std::size_t p,std::uint32_t v,unsigned n=4){for(unsigned k=0;k<n;++k)b.at(p+k)=static_cast<char>(v>>(8*k));}
std::string synthetic(){
    std::string b(480,'\0');b.replace(0,4,std::string("LMT\0",4));put(b,4,67,2);put(b,6,3,2);put(b,8,20);
    put(b,20,80);put(b,24,1);put(b,28,6);put(b,32,0xffffffff);put(b,64,bits(1));put(b,68,3);put(b,72,128);
    put(b,80,0x404);put(b,84,bits(1));put(b,88,16);put(b,92,464);put(b,112,432);
    for(unsigned k=0;k<32;++k)put(b,128+k*2,500+k,2);
    put(b,192,2);put(b,196,416);put(b,416,1);put(b,420,2);put(b,424,2);put(b,428,4);
    for(unsigned k=0;k<3;++k)put(b,432+k*4,bits(1));
    for(unsigned k=0;k<3;++k)put(b,464+k*2,8,2);put(b,470,5,2);
    put(b,472,16,2);put(b,474,32,2);put(b,476,64,2);
    return b;
}
mr::Resource decoded(const std::string& b,mr::Limits limits={}){
    auto result=mr::Resource::decode(b,picosha2::hash256_hex_string(b),limits);
    if(auto* e=std::get_if<mr::Error>(&result))throw std::runtime_error(e->message);
    return std::get<mr::Resource>(std::move(result));
}
void rejected(const std::string& b,mr::ErrorCode code,mr::Limits limits={}){
    auto result=mr::Resource::decode(b,picosha2::hash256_hex_string(b),limits);
    check(std::holds_alternative<mr::Error>(result),"malformed source was accepted");
    check(std::get<mr::Error>(result).code==code,"wrong structural rejection category");
}
template<class T>void error(const mr::Result<T>& r,mr::ErrorCode code){check(std::holds_alternative<mr::Error>(r),"missing view error");check(std::get<mr::Error>(r).code==code,"wrong view error");}
void boundaries(){
    const auto base=synthetic();auto resource=decoded(base);check(resource.slot_count()==3,"slot table size");
    auto copy=resource;check(copy.identity()==resource.identity(),"copy changed resource identity");
    auto second=decoded(base);check(second.identity()!=resource.identity(),"separate loads reused identity");
    auto detached=[&](){auto owner=decoded(base);return owner;}();
    auto model=mq::Model{};model.skeleton_present=true;model.banks.fill(mq::Absent{});model.banks[2]=detached.request_resource();
    auto requested=mq::apply({},model,{0,0x200,0,0,3,1});check(std::holds_alternative<mq::Output>(requested),"verified resource rejected by request");
    const auto& q=std::get<mq::Output>(requested);check(q.accepted&&q.layer.masks[0]==2,"request did not use owned events");
    check(q.layer.bindings[0]->resource_identity==detached.identity(),"request lost owner identity");
    auto action=std::get<std::optional<mb::Action>>(detached.binding_action(2,0));
    check(&action->header==&(*detached.request_resource().slots)[0].value(),"request and binder disagree on header owner");
    check(action->header.groups[0].data()==std::get<std::optional<std::array<mr::EventGroupView,4>>>(detached.event_groups(0))->at(0).records.data(),"event spans duplicate storage");
    std::array<std::uint8_t,256> map;map.fill(255);
    auto prepared=mb::prepare_tracks(q.layer,{},map,&*action);check(std::holds_alternative<mb::Prepared>(prepared),"verified action rejected by binding");
    const auto& channel=std::get<mb::Prepared>(prepared).tracks.translation;check(channel.track.has_value(),"root track missing");
    auto view=std::get<ms::TrackView>(detached.sampler_track(*channel.track));
    auto sample=ms::sample(channel,&view,bits(5));check(std::holds_alternative<ms::Sample>(sample),"owned key buffer rejected by sampler");
    check(std::get<ms::Sample>(sample).channel.key_data->byte_offset==472,"sampler cursor lost resource-relative offset");
    error(second.sampler_track(*channel.track),mr::ErrorCode::foreign_identity);
    model.banks[2]=second.request_resource(); // Existing channels must not follow this replacement.
    auto ref=*channel.track;ref.resource_identity=second.identity();check(std::holds_alternative<ms::TrackView>(second.sampler_track(ref)),"explicit replacement failed");
    auto moved=std::move(copy);check(moved.identity()==resource.identity(),"move changed immutable snapshot");
    auto observed=std::get<std::optional<mb::Action>>(moved.binding_action(0,0));check(observed->tracks[0].key_data_offset==464,"move invalidated owned offsets");
    check(!std::get<std::optional<mb::Action>>(resource.binding_action(0,1)),"empty action became present");
    error(resource.event_groups(1),mr::ErrorCode::absent_action);error(resource.binding_action(16,0),mr::ErrorCode::bank_extent);
    error(resource.action_info(3),mr::ErrorCode::slot_extent);error(resource.event_groups(3),mr::ErrorCode::slot_extent);
    ref={resource.identity(),0,0,1};error(resource.sampler_track(ref),mr::ErrorCode::track_extent);
    ref.ordinal=0;ref.bank=16;error(resource.sampler_track(ref),mr::ErrorCode::bank_extent);
    ref.bank=0;ref.slot=1;error(resource.sampler_track(ref),mr::ErrorCode::absent_action);
    ref.slot=3;error(resource.sampler_track(ref),mr::ErrorCode::slot_extent);
    auto b=base;put(b,72,0);auto noevents=decoded(b);check(!std::get<std::optional<std::array<mr::EventGroupView,4>>>(noevents.event_groups(0)),"null events became empty descriptors");
    model.banks[0]=noevents.request_resource();check(std::get<mq::Error>(mq::apply({},model,{0,0,0,0,0,1})).code==mq::ErrorCode::event_descriptors,"consumer guessed absent event descriptors");
    b=base;put(b,192,0);put(b,196,480);auto empty=decoded(b);auto groups=std::get<std::optional<std::array<mr::EventGroupView,4>>>(empty.event_groups(0));check(groups&&groups->at(0).records.empty()&&groups->at(0).records_offset==480,"empty one-past source pointer lost");
    b=base;put(b,28,0);put(b,32,0x80000001);put(b,36,0x7fc01234);put(b,80,0xfffeffff);put(b,84,0x7fa01234);put(b,96,0x80000000);put(b,432,0x7fc01234);put(b,428,0xffffffff);auto raw=decoded(b);
    auto raw_action=std::get<std::optional<mb::Action>>(raw.binding_action(0,0));check(raw_action->header.frame_count==0&&raw_action->header.loop_frame_raw==std::bit_cast<std::int32_t>(0x80000001U),"resource owner imposed frame semantics");
    check(raw_action->tracks[0].metadata==0xfffeffff&&raw_action->tracks[0].weight_bits==0x7fa01234,"raw metadata/NaN payload changed");
    auto raw_view=std::get<ms::TrackView>(raw.sampler_track({raw.identity(),0,0,0}));check(raw_view.reference_bits[0]==0x80000000&&raw_view.extremes_bits->at(0)==0x7fc01234,"float bits were normalized");
    check(std::get<std::optional<mr::ActionInfo>>(raw.action_info(0))->scene_translation_bits[0]==0x7fc01234,"scene bits changed");
    b=base;put(b,428,0xffffffff);auto overflow=decoded(b);model.banks[0]=overflow.request_resource();check(std::get<mq::Error>(mq::apply({},model,{0,0,0,0,0,1})).code==mq::ErrorCode::event_accumulator,"consumer failed to own signed duration rejection");
    b=base;put(b,88,0);put(b,92,0);auto nokeys=decoded(b);check(!std::get<ms::TrackView>(nokeys.sampler_track({nokeys.identity(),0,0,0})).keys,"null key buffer became present");
    b.assign(8,'\0');b.replace(0,4,std::string("LMT\0",4));put(b,4,67,2);auto zero=decoded(b);check(zero.request_resource().slots&&zero.request_resource().slots->empty(),"zero-slot table became absent");
    const std::array<std::size_t,8> cuts{0,4,7,19,79,115,415,479};for(auto n:cuts)rejected(base.substr(0,n),mr::ErrorCode::malformed_resource);
    b=base;b[0]='X';rejected(b,mr::ErrorCode::unsupported_format);b=base;put(b,4,68,2);rejected(b,mr::ErrorCode::unsupported_format);
    b=base;put(b,8,8);rejected(b,mr::ErrorCode::malformed_resource);b=base;put(b,8,0xfffffffc);rejected(b,mr::ErrorCode::malformed_resource);
    b=base;put(b,68,0x02000000);rejected(b,mr::ErrorCode::unsupported_layout);
    b=base;put(b,20,0);rejected(b,mr::ErrorCode::malformed_resource);b=base;put(b,20,0xfffffffc);rejected(b,mr::ErrorCode::malformed_resource);
    for(const auto offset:{88U,92U}){b=base;put(b,offset,0);rejected(b,mr::ErrorCode::malformed_resource);}
    for(const auto offset:{92U,112U,72U,76U,196U}){b=base;put(b,offset,0xffffffff);rejected(b,mr::ErrorCode::malformed_resource);}
    b=base;put(b,196,0);rejected(b,mr::ErrorCode::malformed_resource);
    mr::Limits limits;limits.bytes=base.size()-1;rejected(base,mr::ErrorCode::resource_limit,limits);
    limits={};limits.slots=2;rejected(base,mr::ErrorCode::resource_limit,limits);limits={};limits.tracks=0;rejected(base,mr::ErrorCode::resource_limit,limits);limits={};limits.event_records=1;rejected(base,mr::ErrorCode::resource_limit,limits);
    b=base;put(b,12,20);limits={};limits.tracks=1;rejected(b,mr::ErrorCode::resource_limit,limits);limits={};limits.event_records=3;rejected(b,mr::ErrorCode::resource_limit,limits);
    error(mr::Resource::decode(base,"bad"),mr::ErrorCode::invalid_expected);error(mr::Resource::decode(base,std::string(64,'A')),mr::ErrorCode::invalid_expected);error(mr::Resource::decode(base,std::string(64,'0')),mr::ErrorCode::hash_mismatch);
    error(mr::Resource::load("__mh4u_nonexistent_resource__",picosha2::hash256_hex_string(base)),mr::ErrorCode::io);
}
struct Fixture {
    std::string bytes;std::size_t offset{};
    std::string take(std::size_t n){check(offset<=bytes.size()&&n<=bytes.size()-offset,"truncated oracle");auto value=bytes.substr(offset,n);offset+=n;return value;}
    std::uint32_t u32(){auto value=take(4);std::uint32_t out=0;for(unsigned k=0;k<4;++k)out|=std::uint32_t(static_cast<unsigned char>(value[k]))<<(8*k);return out;}
    std::uint16_t u16(){auto value=take(2);return static_cast<std::uint16_t>(static_cast<unsigned char>(value[0])|(static_cast<unsigned>(static_cast<unsigned char>(value[1]))<<8));}
    std::string string(){const auto n=u32();check(n<16384,"oversized fixture string");return take(n);}
};
std::string read_fixture(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);check(bool(f),"cannot open oracle");const auto n=f.tellg();check(n>=0&&n<=8*1024*1024,"oracle size policy");std::string b(static_cast<std::size_t>(n),'\0');f.seekg(0);check(bool(f.read(b.data(),n)),"oracle read failure");return b;}
void corpus(const std::filesystem::path& path,std::string_view hash){
    Fixture fixture{read_fixture(path)};check(picosha2::hash256_hex_string(fixture.bytes)==hash,"oracle hash mismatch");check(fixture.take(8)=="MH4ULR01","oracle schema");const auto count=fixture.u32();check(count<=8,"oracle resource policy");
    for(std::uint32_t i=0;i<count;++i){
        const auto source=fixture.string(),digest=fixture.take(64);auto loaded=mr::Resource::load(source,digest);if(auto* e=std::get_if<mr::Error>(&loaded))throw std::runtime_error(source+": "+e->message);
        auto owner=std::get<mr::Resource>(std::move(loaded));++resources;check(owner.sha256()==digest,"source hash");check(owner.byte_size()==fixture.u32(),"source length");const auto slots=fixture.u32();check(owner.slot_count()==slots,"source slot count");
        for(std::uint32_t slot=0;slot<slots;++slot){
            const bool present=fixture.u32()!=0;auto action=std::get<std::optional<mb::Action>>(owner.binding_action(0,slot));auto info=std::get<std::optional<mr::ActionInfo>>(owner.action_info(slot));check(action.has_value()==present&&info.has_value()==present,"action presence");if(!present)continue;++actions;
            check(action->header.frame_count==fixture.u32(),"frame count");check(std::bit_cast<std::uint32_t>(action->header.loop_frame_raw)==fixture.u32(),"loop bits");check(info->source_offset==fixture.u32(),"action source offset");check(info->track_table_offset==fixture.u32(),"track table offset");check(info->flags==fixture.u32(),"action flags");check(info->float_tracks_offset==fixture.u32(),"uninterpreted float pointer");
            for(auto word:info->scene_translation_bits)check(word==fixture.u32(),"scene translation bits");for(auto word:info->scene_rotation_bits)check(word==fixture.u32(),"scene rotation bits");
            const auto track_count=fixture.u32();check(action->tracks.size()==track_count,"track count");
            for(std::uint32_t ordinal=0;ordinal<track_count;++ordinal){
                ++tracks;const auto& binding=action->tracks[ordinal];auto view=std::get<ms::TrackView>(owner.sampler_track({owner.identity(),0,slot,ordinal}));const auto metadata=fixture.u32();check(binding.metadata==metadata&&view.metadata==metadata,"shared metadata");check(binding.weight_bits==fixture.u32(),"weight bits");const auto size=fixture.u32(),offset=fixture.u32();check(binding.key_data_offset== (offset?std::optional<std::uint64_t>{offset}:std::nullopt),"buffer pointer presence");
                for(auto word:view.reference_bits)check(word==fixture.u32(),"reference bits");const bool extremes=fixture.u32()!=0;check(view.extremes_bits.has_value()==extremes,"extremes presence");if(extremes)for(auto word:*view.extremes_bits)check(word==fixture.u32(),"extremes bits");
                const auto key_hash=fixture.take(64);check(view.keys.has_value()==(offset!=0),"key view presence");std::string key_bytes;
                if(view.keys){check(view.keys->base.resource_identity==owner.identity()&&view.keys->base.byte_offset==offset,"key source identity");check(view.keys->bytes.size()==size,"key byte extent");key_bytes.assign(reinterpret_cast<const char*>(view.keys->bytes.data()),view.keys->bytes.size());}
                check(picosha2::hash256_hex_string(key_bytes)==key_hash,"key bytes differ from independent owner");
            }
            const bool event_present=fixture.u32()!=0;auto groups=std::get<std::optional<std::array<mr::EventGroupView,4>>>(owner.event_groups(slot));check(groups.has_value()==event_present&&action->header.event_descriptors_present==event_present,"event descriptor presence");if(!groups)continue;
            for(unsigned g=0;g<4;++g){const auto& group=groups->at(g);check(group.source_offset==fixture.u32(),"event source offset");check(group.records_offset==fixture.u32(),"event record offset");for(auto word:group.prefix)check(word==fixture.u16(),"event prefix");const auto n=fixture.u32();check(group.records.size()==n&&action->header.groups[g].data()==group.records.data(),"event extent/owner");for(const auto& record:group.records){++records;check(record.mask==fixture.u32(),"event mask");check(record.duration==fixture.u32(),"event duration");}}
        }
    }
    check(fixture.offset==fixture.bytes.size(),"oracle trailing bytes");
}
}
int main(int argc,char** argv){try{boundaries();if(argc==4&&std::string_view(argv[1])=="--oracle")corpus(argv[2],argv[3]);else check(argc==1,"usage: --oracle PATH SHA256");std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"resources\":"<<resources<<",\"actions\":"<<actions<<",\"tracks\":"<<tracks<<",\"records\":"<<records<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
