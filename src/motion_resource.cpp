#include "motion_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <fstream>
#include <limits>
#include <new>
#include <vector>

namespace mh4u::motion_resource {
namespace {
struct Failure {Error error;};
[[noreturn]]void fail(ErrorCode code,std::string message,std::size_t offset=0){throw Failure{{code,std::move(message),offset}};}
void expected(std::string_view value){
    if(value.size()!=64||!std::all_of(value.begin(),value.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))
        fail(ErrorCode::invalid_expected,"Expected SHA256 must be 64 lowercase hexadecimal digits");
}
void byte_budget(std::uintmax_t size,Limits limits){
    if(size>limits.bytes)fail(ErrorCode::resource_limit,"Selected resource exceeds byte budget");
    if(size>std::numeric_limits<std::uint32_t>::max())
        fail(ErrorCode::unsupported_layout,"Resource exceeds the observed 32-bit offset domain");
}
std::uint64_t acquire_identity(){
    static std::atomic<std::uint64_t> next{1};
    auto value=next.load(std::memory_order_relaxed);
    for(;;){
        if(!value)fail(ErrorCode::identity_exhausted,"Process-local resource identities exhausted");
        // Unsigned wrap stores the permanent exhausted sentinel; never reuse1.
        if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
    }
}
struct Reader {
    std::string_view bytes;
    void region(std::uint64_t offset,std::uint64_t size)const{
        if(offset>bytes.size()||size>bytes.size()-offset)fail(ErrorCode::malformed_resource,"Source region exceeds verified bytes",static_cast<std::size_t>(offset));
    }
    std::uint32_t u32(std::size_t p)const{region(p,4);std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(static_cast<unsigned char>(bytes[p+i]))<<(8*i);return v;}
    std::uint16_t u16(std::size_t p)const{region(p,2);return static_cast<std::uint16_t>(static_cast<unsigned char>(bytes[p])|(static_cast<unsigned>(static_cast<unsigned char>(bytes[p+1]))<<8));}
};
void charge(std::size_t& used,std::uint32_t count,std::size_t limit,std::size_t offset){
    if(used>limit||count>limit-used)fail(ErrorCode::resource_limit,"Aggregate parsed-object budget exceeded",offset);
    used+=count;
}
Error absent(){return {ErrorCode::absent_action,"Selected slot is explicitly empty",0};}
}
struct Resource::Data {
    struct SamplerExtras {
        std::array<std::uint32_t,4> reference{};
        std::uint32_t key_size{};
        std::optional<std::array<std::uint32_t,8>> extremes;
    };
    struct Group {
        std::array<std::uint16_t,32> prefix{};
        std::vector<motion_events::Record> records;
        std::uint32_t source_offset{},records_offset{};
    };
    struct ActionStorage {
        ActionInfo source;
        // Metadata/weight/key origin belong to binding_tracks. SamplerExtras
        // adds only facts not already represented there; no second decoder.
        std::vector<motion_binding::Track> binding_tracks;
        std::vector<SamplerExtras> sampler_extras;
        std::array<Group,4> groups;
    };
    std::string bytes,digest;
    std::uint64_t identity{};
    // Sole owner of slot presence, frame/loop facts and group spans.
    std::vector<std::optional<motion_request::Action>> actions;
    std::vector<ActionStorage> storage;
};
Result<Resource> Resource::decode_owned(std::string bytes,std::string_view hash,Limits limits){
    try{
        expected(hash);
        byte_budget(bytes.size(),limits);
        if(picosha2::hash256_hex_string(bytes)!=hash)fail(ErrorCode::hash_mismatch,"Selected LMT bytes differ from expected SHA256");
        Reader in{bytes};in.region(0,8);
        if(bytes.compare(0,4,std::string("LMT\0",4))!=0||in.u16(4)!=67)fail(ErrorCode::unsupported_format,"Only observed little-endian LMT67 layout is supported");
        const auto slots=in.u16(6);if(slots>limits.slots)fail(ErrorCode::resource_limit,"Action table exceeds slot budget",6);
        const auto table_end=8U+std::uint32_t(slots)*4U;in.region(8,std::uint64_t(slots)*4);
        auto data=std::make_shared<Data>();data->bytes=std::move(bytes);data->digest=std::string(hash);
        in.bytes=data->bytes;data->actions.resize(slots);data->storage.resize(slots);
        std::size_t tracks_used=0,records_used=0;
        for(std::uint32_t slot=0;slot<slots;++slot){
            const auto p=in.u32(8+4*slot);if(!p)continue;
            if(p<table_end)fail(ErrorCode::malformed_resource,"Action overlaps the file header/offset table",p);
            in.region(p,60);auto& storage=data->storage[slot];auto& info=storage.source;
            const auto tracks=in.u32(p+4),events=in.u32(p+52);
            info.source_offset=p;info.track_table_offset=in.u32(p);info.flags=in.u32(p+48);info.float_tracks_offset=in.u32(p+56);
            // This source flag bypasses original nested relocation; file offsets
            // cannot be assumed in that alternate/unresolved loading profile.
            if(info.flags&0x02000000U)fail(ErrorCode::unsupported_layout,"Action bypasses the proven nested-offset relocation profile",p+48);
            for(unsigned k=0;k<4;++k){info.scene_translation_bits[k]=in.u32(p+16+4*k);info.scene_rotation_bits[k]=in.u32(p+32+4*k);}
            if(info.float_tracks_offset)in.region(info.float_tracks_offset,1); // Payload deliberately uninterpreted.
            charge(tracks_used,tracks,limits.tracks,p+4);in.region(info.track_table_offset,std::uint64_t(tracks)*36);
            if(tracks&&!info.track_table_offset)fail(ErrorCode::malformed_resource,"Nonempty track table has null source pointer",p);
            storage.binding_tracks.reserve(tracks);storage.sampler_extras.reserve(tracks);
            for(std::uint32_t ordinal=0;ordinal<tracks;++ordinal){
                const auto t=std::size_t(info.track_table_offset)+std::size_t(ordinal)*36;
                const auto key_size=in.u32(t+8),key_offset=in.u32(t+12),extremes=in.u32(t+32);
                if((key_size==0)!=(key_offset==0))fail(ErrorCode::malformed_resource,"Track buffer size and null pointer disagree",t+8);
                if(key_offset)in.region(key_offset,key_size);
                motion_binding::Track binding{in.u32(t),in.u32(t+4),std::nullopt};
                if(key_offset)binding.key_data_offset=key_offset;
                Data::SamplerExtras extra;extra.key_size=key_size;
                for(unsigned k=0;k<4;++k)extra.reference[k]=in.u32(t+16+4*k);
                if(extremes){in.region(extremes,32);extra.extremes.emplace();for(unsigned k=0;k<8;++k)(*extra.extremes)[k]=in.u32(extremes+4*k);}
                storage.binding_tracks.push_back(binding);storage.sampler_extras.push_back(extra);
            }
            motion_request::Action action{in.u32(p+8),std::bit_cast<std::int32_t>(in.u32(p+12)),events!=0,{}};
            if(events){
                in.region(events,4*72);
                for(unsigned g=0;g<4;++g){
                    auto& group=storage.groups[g];const auto q=std::size_t(events)+72*g;
                    group.source_offset=static_cast<std::uint32_t>(q);group.records_offset=in.u32(q+68);const auto count=in.u32(q+64);
                    charge(records_used,count,limits.event_records,q+64);
                    if(count&&!group.records_offset)fail(ErrorCode::malformed_resource,"Nonempty event group has null record pointer",q+68);
                    if(group.records_offset)in.region(group.records_offset,std::uint64_t(count)*8);
                    for(unsigned k=0;k<32;++k)group.prefix[k]=in.u16(q+2*k);
                    group.records.reserve(count);
                    for(std::uint32_t k=0;k<count;++k){const auto r=std::size_t(group.records_offset)+std::size_t(k)*8;group.records.push_back({in.u32(r),in.u32(r+4)});}
                    action.groups[g]=group.records;
                }
            }
            data->actions[slot]=action;
        }
        data->identity=acquire_identity();return Resource{std::move(data)};
    }catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate bounded resource snapshot",0};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Resource allocation exceeds host capacity",0};}
}
Result<Resource> Resource::decode(std::string_view bytes,std::string_view hash,Limits limits){
    try{expected(hash);byte_budget(bytes.size(),limits);return decode_owned(std::string(bytes),hash,limits);}
    catch(const Failure& f){return f.error;}catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate input snapshot",0};}
}
Result<Resource> Resource::load(const std::filesystem::path& path,std::string_view hash,Limits limits){
    try{
        expected(hash);std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)fail(ErrorCode::io,"Cannot open selected LMT file");
        const auto n=file.tellg();if(n<0)fail(ErrorCode::io,"Cannot measure selected LMT file");
        byte_budget(static_cast<std::uintmax_t>(n),limits);
        if(static_cast<std::uintmax_t>(n)>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))fail(ErrorCode::resource_limit,"LMT file exceeds stream capacity");
        std::string bytes(static_cast<std::size_t>(n),'\0');file.seekg(0);
        if(!file.read(bytes.data(),static_cast<std::streamsize>(bytes.size()))||file.peek()!=std::char_traits<char>::eof())fail(ErrorCode::io,"LMT read failed or size changed");
        return decode_owned(std::move(bytes),hash,limits);
    }catch(const Failure& f){return f.error;}catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate file snapshot",0};}
}
std::uint64_t Resource::identity()const noexcept{return data_->identity;}
std::string_view Resource::sha256()const noexcept{return data_->digest;}
std::size_t Resource::byte_size()const noexcept{return data_->bytes.size();}
std::size_t Resource::slot_count()const noexcept{return data_->actions.size();}
motion_request::Resource Resource::request_resource()const noexcept{return {data_->identity,std::span<const std::optional<motion_request::Action>>{data_->actions}};}
Result<std::optional<motion_binding::Action>> Resource::binding_action(std::uint32_t bank,std::uint32_t slot)const{
    if(bank>15)return Error{ErrorCode::bank_extent,"Model bank must fit recovered selector",0};
    if(slot>=data_->actions.size())return Error{ErrorCode::slot_extent,"Slot is outside source offset table",0};
    if(!data_->actions[slot])return std::optional<motion_binding::Action>{};
    const auto& storage=data_->storage[slot];
    return std::optional<motion_binding::Action>{{data_->identity,bank,slot,*data_->actions[slot],storage.source.flags,storage.binding_tracks}};
}
Result<motion_sampler::TrackView> Resource::sampler_track(const motion_binding::TrackReference& ref)const{
    if(ref.resource_identity!=data_->identity)return Error{ErrorCode::foreign_identity,"Track belongs to a different immutable resource",0};
    if(ref.bank>15)return Error{ErrorCode::bank_extent,"Track bank is outside model selector domain",0};
    if(ref.slot>=data_->actions.size())return Error{ErrorCode::slot_extent,"Track action is outside source table",0};
    if(!data_->actions[ref.slot])return absent();const auto& storage=data_->storage[ref.slot];
    if(ref.ordinal>=storage.binding_tracks.size())return Error{ErrorCode::track_extent,"Track ordinal is outside selected action",storage.source.source_offset};
    const auto& binding=storage.binding_tracks[ref.ordinal];const auto& extra=storage.sampler_extras[ref.ordinal];
    motion_sampler::TrackView out{ref,binding.metadata,extra.reference,std::nullopt,extra.extremes};
    if(binding.key_data_offset)out.keys=motion_sampler::KeyBuffer{{data_->identity,*binding.key_data_offset},std::span<const std::byte>{reinterpret_cast<const std::byte*>(data_->bytes.data()+*binding.key_data_offset),extra.key_size}};
    return out;
}
Result<std::optional<ActionInfo>> Resource::action_info(std::uint32_t slot)const{
    if(slot>=data_->actions.size())return Error{ErrorCode::slot_extent,"Slot is outside source offset table",0};
    if(!data_->actions[slot])return std::optional<ActionInfo>{};return std::optional<ActionInfo>{data_->storage[slot].source};
}
Result<std::optional<std::array<EventGroupView,4>>> Resource::event_groups(std::uint32_t slot)const{
    if(slot>=data_->actions.size())return Error{ErrorCode::slot_extent,"Slot is outside source offset table",0};
    if(!data_->actions[slot])return absent();if(!data_->actions[slot]->event_descriptors_present)return std::optional<std::array<EventGroupView,4>>{};
    std::array<EventGroupView,4> out;for(unsigned g=0;g<4;++g){const auto& source=data_->storage[slot].groups[g];out[g]={source.prefix,source.records,source.source_offset,source.records_offset};}
    return std::optional<std::array<EventGroupView,4>>{out};
}
} // namespace mh4u::motion_resource
