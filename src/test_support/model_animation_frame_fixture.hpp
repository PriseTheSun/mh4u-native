#pragma once
#include "model_animation_frame.hpp"
#include "motion_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace mh4u::test_support::model_frame {
namespace frame=mh4u::model_animation_frame;
namespace clock=mh4u::motion_clock;
namespace mp=mh4u::motion_pending;
namespace mi=mh4u::motion_initialization;
namespace mb=mh4u::motion_binding;
namespace mr=mh4u::motion_resource;
namespace jp=mh4u::joint_pose;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{};
void check(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
std::uint32_t word(const Bytes& b,std::size_t p,unsigned width=4){if(p>b.size()||width>b.size()-p)throw std::runtime_error("Fixture extent");std::uint32_t v=0;for(unsigned i=0;i<width;++i)v|=std::uint32_t(b[p+i])<<(8*i);return v;}
void put(Bytes& b,std::size_t p,std::uint32_t v,unsigned width=4){if(p>b.size()||width>b.size()-p)throw std::runtime_error("Fixture write extent");for(unsigned i=0;i<width;++i)b[p+i]=static_cast<std::uint8_t>(v>>(8*i));}
template<std::size_t N>std::array<std::uint32_t,N> words(const Bytes& b,std::size_t p){std::array<std::uint32_t,N> v{};for(unsigned i=0;i<N;++i)v[i]=word(b,p+4*i);return v;}
template<std::size_t N>void put_words(Bytes& b,std::size_t p,const std::array<std::uint32_t,N>& v){for(unsigned i=0;i<N;++i)put(b,p+4*i,v[i]);}
struct Reader {
    const Bytes& b;std::size_t p{};
    std::uint32_t integer(){const auto v=word(b,p);p+=4;return v;}
    Bytes bytes(std::size_t n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("Truncated fixture");Bytes out(b.begin()+static_cast<std::ptrdiff_t>(p),b.begin()+static_cast<std::ptrdiff_t>(p+n));p+=n;return out;}
};
struct Source {std::uint32_t entry,base;mr::Resource resource;};
struct Context {
    const std::vector<Source>& sources;
    std::array<std::uint32_t,16> banks{};
    std::map<std::uint32_t,mb::TrackReference> track_addresses;
    std::map<std::uint32_t,mh4u::motion_request::Binding> event_addresses;
    enum class Fault {none,action,identity,sampler,event};Fault fault{};
    std::vector<std::pair<std::uint32_t,std::uint32_t>> getters;
    explicit Context(const std::vector<Source>& s):sources(s){
        // Resource is the only native LMT parser. Its parsed source offsets
        // establish the address adapter; no raw source records are decoded here.
        for(const auto& source:sources)for(std::uint32_t slot=0;slot<source.resource.slot_count();++slot){
            const auto info=source.resource.action_info(slot);
            const auto* optional=std::get_if<std::optional<mr::ActionInfo>>(&info);check(optional!=nullptr,"Resource action facts");if(!*optional)continue;
            const auto action_result=source.resource.binding_action(0,slot);const auto& action=*std::get<std::optional<mb::Action>>(action_result);
            for(std::size_t i=0;i<action.tracks.size();++i)check(track_addresses.emplace(source.base+(**optional).track_table_offset+static_cast<std::uint32_t>(36*i),mb::TrackReference{source.resource.identity(),0,slot,static_cast<std::uint32_t>(i)}).second,"Unique track source address");
            const auto groups=source.resource.event_groups(slot);const auto& values=*std::get<std::optional<std::array<mr::EventGroupView,4>>>(groups);
            for(unsigned g=0;g<4;++g)check(event_addresses.emplace(source.base+values[g].source_offset,mh4u::motion_request::Binding{source.resource.identity(),0,slot,g}).second,"Unique event source address");
        }
    }
    static mp::ActionResolution action(const void* ptr,std::uint32_t bank,std::uint32_t slot){
        auto& c=*const_cast<Context*>(static_cast<const Context*>(ptr));
        if(c.fault==Fault::action)return mp::ActionResolutionError{73,"Explicit unresolved fixture bank"};
        check(bank<16,"Bank extent");const auto index=c.banks[bank];
        if(!index)return std::optional<mb::Action>{};
        check(index<=c.sources.size(),"Source bank mapping");c.getters.emplace_back(index-1,slot);
        const auto result=c.sources[index-1].resource.binding_action(bank,slot);
        if(const auto* e=std::get_if<mr::Error>(&result))return mp::ActionResolutionError{static_cast<std::uint32_t>(e->code),e->message};
        auto action=std::get<std::optional<mb::Action>>(result);if(action&&c.fault==Fault::identity)action->bank^=1;return action;
    }
    static mi::Resolution sample(const void* ptr,const mb::TrackReference& id){
        const auto& c=*static_cast<const Context*>(ptr);
        if(c.fault==Fault::sampler)return mi::ResolutionError{91,"Explicit unresolved fixture track"};
        for(const auto& source:c.sources)if(source.resource.identity()==id.resource_identity){
            const auto result=source.resource.sampler_track(id);if(const auto* e=std::get_if<mr::Error>(&result))return mi::ResolutionError{static_cast<std::uint32_t>(e->code),e->message};
            return std::get<mh4u::motion_sampler::TrackView>(result);
        }
        return mi::ResolutionError{92,"Foreign fixture identity"};
    }
    static clock::EventResolution event(const void* ptr,const mh4u::motion_request::Binding& id){
        const auto& c=*static_cast<const Context*>(ptr);
        if(c.fault==Fault::event)return clock::ResolutionError{93,"Explicit unresolved fixture event"};
        for(const auto& source:c.sources)if(source.resource.identity()==id.resource_identity){
            const auto result=source.resource.event_groups(id.slot);
            if(const auto* e=std::get_if<mr::Error>(&result))return clock::ResolutionError{static_cast<std::uint32_t>(e->code),e->message};
            const auto& groups=std::get<std::optional<std::array<mr::EventGroupView,4>>>(result);
            if(!groups||id.group>=4)return clock::ResolutionError{94,"Invalid present event group"};
            return clock::EventGroup{id,(*groups)[id.group].records,(*groups)[id.group].prefix};
        }
        return clock::ResolutionError{95,"Foreign fixture event identity"};
    }
    mb::Channel channel(const Bytes& b,std::size_t p,std::uint32_t bank)const{
        mb::Channel out;const auto track=word(b,p),key=word(b,p+8);out.frame_bits=word(b,p+4);out.weight_bits=word(b,p+12);
        if(track){out.track=track_addresses.at(track);out.track->bank=bank;}
        if(key){for(const auto& source:sources)if(key>=source.base&&key-source.base<source.resource.byte_size())out.key_data=mb::BufferReference{source.resource.identity(),key-source.base};check(out.key_data.has_value(),"Owned key cursor");}
        return out;
    }
    std::uint32_t track_pointer(const std::optional<mb::TrackReference>& id)const{
        if(!id)return 0;
        for(const auto& [address,known]:track_addresses)if(known.resource_identity==id->resource_identity&&known.slot==id->slot&&known.ordinal==id->ordinal)return address;
        throw std::runtime_error("Foreign result track identity");
    }
    std::uint32_t key_pointer(const std::optional<mb::BufferReference>& id)const{
        if(!id)return 0;
        for(const auto& source:sources)if(source.resource.identity()==id->resource_identity){check(id->byte_offset<source.resource.byte_size(),"Result key extent");return source.base+static_cast<std::uint32_t>(id->byte_offset);}
        throw std::runtime_error("Foreign result key identity");
    }
    void channel(Bytes& b,std::size_t p,const mb::Channel& value)const{put(b,p,track_pointer(value.track));put(b,p+4,value.frame_bits);put(b,p+8,key_pointer(value.key_data));put(b,p+12,value.weight_bits);}
    std::optional<mh4u::motion_request::Binding> binding(std::uint32_t address,std::uint32_t bank)const{
        if(!address)return {};auto out=event_addresses.at(address);out.bank=bank;return out;
    }
    std::uint32_t event_pointer(const std::optional<mh4u::motion_request::Binding>& id)const{
        if(!id)return 0;
        for(const auto& [address,known]:event_addresses)if(known.resource_identity==id->resource_identity&&known.slot==id->slot&&known.group==id->group)return address;
        throw std::runtime_error("Foreign result event identity");
    }
};
struct Raw {Bytes model,joints,snapshots;std::vector<Bytes> channels;};
Raw raw(Reader& r,std::uint32_t n,std::uint32_t count){Raw out{r.bytes(8192),r.bytes(n*168),r.bytes(n*56),{}};for(unsigned i=0;i<count;++i)out.channels.push_back(r.bytes(n*52));return out;}
constexpr std::array<std::size_t,11> FLOAT_OFFSETS{0x10,0x14,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60};
std::array<float*,11> float_fields(mh4u::motion_request::Layer& l){return {&l.requested_frame,&l.offset,&l.interpolation,&l.remaining_interpolation,&l.weight,&l.frame,&l.previous_frame,&l.end_frame,&l.field58,&l.speed,&l.field60};}
mp::State decode(const Raw& raw,const Context& c){
    mp::State out;out.calculation_flags=word(raw.model,0x238);out.accumulated_flags=word(raw.model,0x23c);
    for(std::size_t i=0;i<raw.channels.size();++i){
        const auto p=0x240+i*304;mp::Layer value;auto& a=value.animation;auto& l=a.layer;
        l.selector=static_cast<std::uint16_t>(word(raw.model,p+4,2));l.flags=static_cast<std::uint16_t>(word(raw.model,p+6,2));l.field08=static_cast<std::uint16_t>(word(raw.model,p+8,2));l.attributes=word(raw.model,p+12);const auto bank=(l.selector>>8)&15;
        const auto fields=float_fields(l);for(unsigned k=0;k<fields.size();++k)*fields[k]=std::bit_cast<float>(word(raw.model,p+FLOAT_OFFSETS[k]));
        for(unsigned g=0;g<4;++g){l.bindings[g]=c.binding(word(raw.model,p+0xf8+4*g),bank);l.masks[g]=word(raw.model,p+0x108+4*g);}
        l.prepared_frame=std::bit_cast<float>(word(raw.model,p+0x64));l.prepared_next_frame=std::bit_cast<float>(word(raw.model,p+0x68));l.previous_step=std::bit_cast<float>(word(raw.model,p+0x18));l.interpolation_curve_bits=words<2>(raw.model,p+0x6c);l.previous_masks=words<4>(raw.model,p+0x118);
        a.prediction_rotation=words<4>(raw.model,p+0x20);a.prediction_translation=words<4>(raw.model,p+0x30);a.previous_rotation=words<4>(raw.model,p+0x78);a.base_rotation=words<4>(raw.model,p+0x88);a.previous_translation=words<4>(raw.model,p+0xa8);a.base_translation=words<4>(raw.model,p+0xb8);value.rotation_98=words<4>(raw.model,p+0x98);value.translation_c8=words<4>(raw.model,p+0xc8);
        a.tracks.translation=c.channel(raw.model,p+0xd8,bank);a.tracks.rotation=c.channel(raw.model,p+0xe8,bank);
        const auto& channels=raw.channels[i];for(std::size_t j=0;j<channels.size()/52;++j){mb::Joint joint;for(unsigned k=0;k<3;++k)joint.channels[k]=c.channel(channels,j*52+k*16,bank);joint.flags=word(channels,j*52+48);a.tracks.joints.push_back(joint);}
        out.layers.push_back(std::move(value));
    }
    for(std::size_t i=0;i<raw.joints.size()/168;++i){const auto p=i*168;out.joints.push_back({words<3>(raw.joints,p+0x48),words<4>(raw.joints,p+0x58),words<4>(raw.joints,p+0x68),words<4>(raw.joints,p+0x78)});const auto b=i*56;out.snapshots.push_back({word(raw.snapshots,b),word(raw.snapshots,b+4),words<4>(raw.snapshots,b+8),words<4>(raw.snapshots,b+24),words<4>(raw.snapshots,b+40)});}
    return out;
}
Raw encode(const Raw& before,const mp::State& state,const Context& c){
    Raw out=before;put(out.model,0x238,state.calculation_flags);put(out.model,0x23c,state.accumulated_flags);
    check(state.layers.size()==out.channels.size()&&state.joints.size()*168==out.joints.size()&&state.snapshots.size()*56==out.snapshots.size(),"Output extents preserved");
    for(std::size_t i=0;i<state.layers.size();++i){const auto p=0x240+i*304;const auto& value=state.layers[i];const auto& a=value.animation;auto l=a.layer;
        put(out.model,p+4,l.selector,2);put(out.model,p+6,l.flags,2);put(out.model,p+8,l.field08,2);put(out.model,p+12,l.attributes);
        const auto fields=float_fields(l);for(unsigned k=0;k<fields.size();++k)put(out.model,p+FLOAT_OFFSETS[k],std::bit_cast<std::uint32_t>(*fields[k]));
        for(unsigned g=0;g<4;++g){put(out.model,p+0xf8+g*4,c.event_pointer(l.bindings[g]));put(out.model,p+0x108+g*4,l.masks[g]);}
        put(out.model,p+0x64,std::bit_cast<std::uint32_t>(l.prepared_frame));put(out.model,p+0x68,std::bit_cast<std::uint32_t>(l.prepared_next_frame));put(out.model,p+0x18,std::bit_cast<std::uint32_t>(l.previous_step));put_words(out.model,p+0x6c,l.interpolation_curve_bits);put_words(out.model,p+0x118,l.previous_masks);
        put_words(out.model,p+0x20,a.prediction_rotation);put_words(out.model,p+0x30,a.prediction_translation);put_words(out.model,p+0x78,a.previous_rotation);put_words(out.model,p+0x88,a.base_rotation);put_words(out.model,p+0x98,value.rotation_98);put_words(out.model,p+0xa8,a.previous_translation);put_words(out.model,p+0xb8,a.base_translation);put_words(out.model,p+0xc8,value.translation_c8);
        c.channel(out.model,p+0xd8,a.tracks.translation);c.channel(out.model,p+0xe8,a.tracks.rotation);
        check(a.tracks.joints.size()*52==out.channels[i].size(),"Output binding extent");for(std::size_t j=0;j<a.tracks.joints.size();++j){for(unsigned k=0;k<3;++k)c.channel(out.channels[i],j*52+k*16,a.tracks.joints[j].channels[k]);put(out.channels[i],j*52+48,a.tracks.joints[j].flags);}
    }
    for(std::size_t i=0;i<state.joints.size();++i){const auto p=i*168;const auto& j=state.joints[i];put_words(out.joints,p+0x48,j.reference_translation);put_words(out.joints,p+0x58,j.rotation);put_words(out.joints,p+0x68,j.scale);put_words(out.joints,p+0x78,j.translation);
        const auto b=i*56;const auto& s=state.snapshots[i];put(out.snapshots,b,s.layer_index);put(out.snapshots,b+4,s.flags);put_words(out.snapshots,b+8,s.rotation);put_words(out.snapshots,b+24,s.translation);put_words(out.snapshots,b+40,s.scale);}
    return out;
}
void equal(const Raw& a,const Raw& b){
    auto bytes=[](const Bytes& actual,const Bytes& expected,const std::string& name){if(actual!=expected){const auto it=std::mismatch(actual.begin(),actual.end(),expected.begin());std::ostringstream s;s<<name<<" byte +0x"<<std::hex<<std::distance(actual.begin(),it.first)<<" got "<<unsigned(*it.first)<<" expected "<<unsigned(*it.second);check(false,s.str());}check(true,name);};
    bytes(a.model,b.model,"Entire model");bytes(a.joints,b.joints,"Entire J");bytes(a.snapshots,b.snapshots,"Entire B");check(a.channels.size()==b.channels.size(),"Layer channels count");for(std::size_t i=0;i<a.channels.size();++i)bytes(a.channels[i],b.channels[i],"Entire A"+std::to_string(i));
}
frame::State frame_state(const Raw& raw,const Context& c){
    frame::State state;state.animation=decode(raw,c);auto& m=state.model;
    m.position=words<4>(raw.model,0x40);m.rotation=words<4>(raw.model,0x50);m.scale=words<4>(raw.model,0x60);m.null_scale=words<4>(raw.model,0xbd0);m.translation_delta=words<4>(raw.model,0xbe0);m.rotation_delta=words<4>(raw.model,0xbf0);return state;
}
Raw frame_raw(const Raw& before,const frame::State& state,const Context& c){
    auto out=encode(before,state.animation,c);const auto& m=state.model;
    put_words(out.model,0x40,m.position);put_words(out.model,0x50,m.rotation);put_words(out.model,0x60,m.scale);put_words(out.model,0xbd0,m.null_scale);put_words(out.model,0xbe0,m.translation_delta);put_words(out.model,0xbf0,m.rotation_delta);return out;
}
// ARM stores pointers, not our logical bank provenance. Compare this extra
// identity separately so address projection cannot erase a wrong bank result.
bool logical_identities(const Raw& expected,const frame::State& before,const frame::State& actual,
                        const Context& context,const std::vector<bool>& initialized){
    if(actual.animation.layers.size()!=before.animation.layers.size())return false;
    for(std::size_t i=0;i<actual.animation.layers.size();++i){
        const auto& prior=before.animation.layers[i].animation;const auto& got=actual.animation.layers[i].animation;
        const auto bank=(prior.layer.selector>>8)&15U;const auto p=0x240+i*304;
        auto same_track=[&](std::uint32_t pointer,const std::optional<mb::TrackReference>& old,const std::optional<mb::TrackReference>& result){
            std::optional<mb::TrackReference> wanted;
            if(pointer){wanted=context.track_addresses.at(pointer);if(initialized[i])wanted->bank=bank;else{if(!old)return false;wanted->bank=old->bank;}}
            return wanted==result;
        };
        if(!same_track(word(expected.model,p+0xd8),prior.tracks.translation.track,got.tracks.translation.track)||
           !same_track(word(expected.model,p+0xe8),prior.tracks.rotation.track,got.tracks.rotation.track))return false;
        if(got.tracks.joints.size()!=prior.tracks.joints.size())return false;
        for(std::size_t j=0;j<got.tracks.joints.size();++j)for(unsigned k=0;k<3;++k)
            if(!same_track(word(expected.channels[i],j*52+k*16),prior.tracks.joints[j].channels[k].track,got.tracks.joints[j].channels[k].track))return false;
        for(unsigned g=0;g<4;++g){
            const auto pointer=word(expected.model,p+0xf8+g*4);std::optional<mh4u::motion_request::Binding> wanted;
            if(pointer){wanted=context.event_addresses.at(pointer);if(initialized[i])wanted->bank=bank;else{if(!prior.layer.bindings[g])return false;wanted->bank=prior.layer.bindings[g]->bank;}}
            if(wanted!=got.layer.bindings[g])return false;
        }
    }
    return true;
}
} // namespace mh4u::test_support::model_frame
