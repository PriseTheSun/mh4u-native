#pragma once
// Shared test-only projection of the motion-command ARM arenas. Numeric source
// addresses are retained evidence identities, never production runtime inputs.
#include "player_controller_step.hpp"
#include "motion_resource.hpp"
#include <algorithm>
#include <bit>
#include <map>
#include <sstream>
#include <stdexcept>
namespace mh4u::test::motion_arena {
namespace pcs=player_controller_step; namespace ps=player_selection; namespace pt=player_transition;
namespace mc=motion_command; namespace mp=motion_pending; namespace rq=motion_request;
namespace mq=motion_query; namespace mb=motion_binding; namespace mi=motion_initialization;
namespace mr=motion_resource; namespace ms=motion_sampler; namespace math=motion_transform_math;
using Bytes=std::vector<std::uint8_t>;
inline std::size_t checks{};
inline void check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
inline std::uint32_t word(const Bytes& b,std::size_t p,unsigned n=4){if(p>b.size()||n>b.size()-p)throw std::runtime_error("Fixture extent");std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint32_t(b[p+i])<<(8*i);return v;}
inline void put(Bytes& b,std::size_t p,std::uint32_t v){if(p>b.size()||4>b.size()-p)throw std::runtime_error("Fixture write extent");for(unsigned i=0;i<4;++i)b[p+i]=static_cast<std::uint8_t>(v>>(8*i));}
template<std::size_t N>std::array<std::uint32_t,N> words(const Bytes& b,std::size_t p){std::array<std::uint32_t,N> v{};for(unsigned i=0;i<N;++i)v[i]=word(b,p+i*4);return v;}
struct Reader {const Bytes& b;std::size_t p{};std::uint32_t integer(){const auto v=word(b,p);p+=4;return v;}Bytes bytes(std::size_t n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("Truncated fixture");Bytes out(b.begin()+static_cast<std::ptrdiff_t>(p),b.begin()+static_cast<std::ptrdiff_t>(p+n));p+=n;return out;}};
struct Source {std::uint32_t entry,base;mr::Resource resource;};
struct Context {
    static constexpr std::uint64_t synthetic_identity=0xffffffffffffffffULL;
    static constexpr std::uint64_t opaque_track_identity=synthetic_identity-1,opaque_key_identity=synthetic_identity-2;
    const std::vector<Source>& sources;
    std::map<std::uint32_t,mb::TrackReference> addresses;
    std::map<std::uint32_t,mb::BufferReference> opaque_keys;
    std::vector<ms::TrackView> synthetic;
    mutable std::vector<mb::TrackReference> resolutions;
    bool reject{};
    explicit Context(const std::vector<Source>& src):sources(src){
        for(const auto& s:sources)for(std::uint32_t slot=0;slot<s.resource.slot_count();++slot){
            const auto info=s.resource.action_info(slot);const auto* facts=std::get_if<std::optional<mr::ActionInfo>>(&info);check(facts!=nullptr,"Resource action facts");if(!*facts)continue;
            const auto action=s.resource.binding_action(0,slot);const auto& tracks=std::get<std::optional<mb::Action>>(action)->tracks;
            for(unsigned i=0;i<tracks.size();++i)check(addresses.emplace(s.base+(**facts).track_table_offset+36*i,mb::TrackReference{s.resource.identity(),0,slot,i}).second,"Unique source track identity");
        }
    }
    void descriptors(const Bytes& bytes){
        synthetic.clear();for(unsigned i=0;i<bytes.size()/36;++i){const auto p=i*36;check((word(bytes,p)==1||word(bytes,p)==2)&&word(bytes,p+8)==0&&word(bytes,p+12)==0&&word(bytes,p+32)==0,"Explicit fixture reference-only descriptor");synthetic.push_back({{synthetic_identity,0,0,i},word(bytes,p),words<4>(bytes,p+16),{}, {}});}
    }
    static mi::Resolution resolve(const void* ptr,const mb::TrackReference& id){
        const auto& c=*static_cast<const Context*>(ptr);c.resolutions.push_back(id);if(c.reject)return mi::ResolutionError{91,"Fixture unresolved track"};
        if(id.resource_identity==synthetic_identity){if(id.bank!=0||id.slot!=0||id.ordinal>=c.synthetic.size())return mi::ResolutionError{92,"Foreign fixture reference"};return c.synthetic[id.ordinal];}
        for(const auto& s:c.sources)if(s.resource.identity()==id.resource_identity){const auto r=s.resource.sampler_track(id);if(const auto* e=std::get_if<mr::Error>(&r))return mi::ResolutionError{static_cast<std::uint32_t>(e->code),e->message};return std::get<ms::TrackView>(r);}
        return mi::ResolutionError{93,"Foreign resource identity"};
    }
    mb::Channel channel(const Bytes& model,std::size_t p,std::uint32_t bank,bool allow_opaque=false)const{
        mb::Channel c;c.frame_bits=word(model,p+4);c.weight_bits=word(model,p+12);const auto address=word(model,p),key=word(model,p+8);
        if(auto found=addresses.find(address);address&&found!=addresses.end()){check(allow_opaque||found->second.resource_identity!=opaque_track_identity,"Opaque active track");c.track=found->second;c.track->bank=bank;}
        else if(address>=0x2800000&&address<0x2801000){check((address-0x2800000)%36==0&&(address-0x2800000)/36<synthetic.size(),"Fixture descriptor pointer");c.track=mb::TrackReference{synthetic_identity,0,0,(address-0x2800000)/36};}
        else if(address)throw std::runtime_error("Unmapped source track address "+std::to_string(address));
        if(key){for(const auto& s:sources)if(key>=s.base&&key-s.base<s.resource.byte_size())c.key_data=mb::BufferReference{s.resource.identity(),key-s.base};if(!c.key_data)if(auto it=opaque_keys.find(key);it!=opaque_keys.end()){check(allow_opaque,"Opaque active key cursor");c.key_data=it->second;}check(c.key_data.has_value(),"Owned or explicitly opaque key cursor");}return c;
    }
    void encode(Bytes& model,std::size_t p,const mb::Channel& c)const{
        std::uint32_t address=0,key=0;
        if(c.track){if(c.track->resource_identity==synthetic_identity){check(c.track->bank==0&&c.track->slot==0&&c.track->ordinal<synthetic.size(),"Result fixture identity");address=0x2800000+c.track->ordinal*36;}else for(const auto& [at,known]:addresses)if(known.resource_identity==c.track->resource_identity&&known.slot==c.track->slot&&known.ordinal==c.track->ordinal){address=at;break;}check(address!=0,"Known result track");}
        if(c.key_data){for(const auto& s:sources)if(s.resource.identity()==c.key_data->resource_identity){check(c.key_data->byte_offset<s.resource.byte_size(),"Result key extent");key=s.base+static_cast<std::uint32_t>(c.key_data->byte_offset);break;}if(!key)for(const auto& [address,known]:opaque_keys)if(known==*c.key_data){key=address;break;}check(key!=0,"Known result key identity");}
        put(model,p,address);put(model,p+4,c.frame_bits);put(model,p+8,key);put(model,p+12,c.weight_bits);
    }
};
struct CommandStorage {std::vector<mp::Layer> layers;mc::Fields fields;};
struct Bindings {
 static constexpr std::uint64_t opaque_identity=Context::synthetic_identity-3;
 const std::vector<Source>& sources;std::map<std::uint32_t,rq::Binding> addresses;
 explicit Bindings(const std::vector<Source>& src):sources(src){for(const auto& s:src)for(std::uint32_t slot=0;slot<s.resource.slot_count();++slot){auto result=s.resource.event_groups(slot);auto* groups=std::get_if<std::optional<std::array<mr::EventGroupView,4>>>(&result);if(!groups||!*groups)continue;for(unsigned gi=0;gi<4;++gi)addresses.emplace(s.base+(**groups)[gi].source_offset,rq::Binding{s.resource.identity(),0,slot,gi});}}
 std::optional<rq::Binding> decode(std::uint32_t address,std::uint32_t bank,bool allow_opaque=false)const{if(!address)return{};auto found=addresses.find(address);if(found==addresses.end())throw std::runtime_error("Unmapped source event address "+std::to_string(address));check(allow_opaque||found->second.resource_identity!=opaque_identity,"Opaque active event binding");auto value=found->second;value.bank=bank;return value;}
 std::uint32_t encode(const std::optional<rq::Binding>& id)const{if(!id)return 0;for(auto&[address,known]:addresses)if(known.resource_identity==id->resource_identity&&known.slot==id->slot&&known.group==id->group)return address;throw std::runtime_error("Unknown output event identity");}
};
// Explicitly retain uninterpreted pointer bytes only for inactive storage.
// Tokens are private adapter identities with reversible source maps. Track
// resolution still rejects them; no address or fabricated resource is supplied
// to production code and no active-layer unknown is silently accepted.
inline void retain_inactive(const Bytes& model,std::uint32_t active,std::uint32_t count,Context& context,Bindings& events){
 check(active<=count&&count<=8,"Inactive projection extent");
 for(std::uint32_t i=active;i<count;++i){const auto base=0x240+i*304;
  for(auto offset:{0xd8U,0xe8U}){const auto address=word(model,base+offset),key=word(model,base+offset+8);
   if(address&&!context.addresses.contains(address))context.addresses.emplace(address,mb::TrackReference{Context::opaque_track_identity,0,0,static_cast<std::uint32_t>(context.addresses.size())});
   if(key&&!context.opaque_keys.contains(key))context.opaque_keys.emplace(key,mb::BufferReference{Context::opaque_key_identity,context.opaque_keys.size()});
  }
  for(unsigned group=0;group<4;++group){const auto address=word(model,base+0xf8+4*group);
   if(address&&!events.addresses.contains(address))events.addresses.emplace(address,rq::Binding{Bindings::opaque_identity,0,static_cast<std::uint32_t>(events.addresses.size()),0});
  }
 }
}
inline void put_half(Bytes& b,std::size_t p,std::uint16_t v){b.at(p)=std::uint8_t(v);b.at(p+1)=std::uint8_t(v>>8);}
inline CommandStorage decode(const Bytes& model,std::uint32_t count,const Context& c,const Bindings& events,std::uint32_t active=0xffffffffU){
 CommandStorage out;for(unsigned i=0;i<count;++i){const auto p=0x240+i*304;mp::Layer layer;auto& a=layer.animation;auto& l=a.layer;l.selector=std::uint16_t(word(model,p+4,2));l.flags=std::uint16_t(word(model,p+6,2));l.field08=std::uint16_t(word(model,p+8,2));l.attributes=word(model,p+12);
 const std::array fields{&l.requested_frame,&l.offset,&l.previous_step,&l.interpolation,&l.remaining_interpolation,&l.weight,&l.frame,&l.previous_frame,&l.end_frame,&l.field58,&l.speed,&l.field60,&l.prepared_frame,&l.prepared_next_frame};constexpr std::array<std::size_t,14> offsets{0x10,0x14,0x18,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x68};for(unsigned j=0;j<fields.size();++j)*fields[j]=std::bit_cast<float>(word(model,p+offsets[j]));
 l.interpolation_curve_bits=words<2>(model,p+0x6c);l.masks=words<4>(model,p+0x108);l.previous_masks=words<4>(model,p+0x118);
 for(auto[off,target]:std::initializer_list<std::pair<std::size_t,mi::Bits4*>>{{0x20,&a.prediction_rotation},{0x30,&a.prediction_translation},{0x78,&a.previous_rotation},{0x88,&a.base_rotation},{0x98,&layer.rotation_98},{0xa8,&a.previous_translation},{0xb8,&a.base_translation},{0xc8,&layer.translation_c8}})*target=words<4>(model,p+off);
 const auto bank=(l.selector>>8)&15;a.tracks.translation=c.channel(model,p+0xd8,bank,i>=active);a.tracks.rotation=c.channel(model,p+0xe8,bank,i>=active);for(unsigned gi=0;gi<4;++gi)l.bindings[gi]=events.decode(word(model,p+0xf8+4*gi),bank,i>=active);out.layers.push_back(layer);
 }out.fields.fieldsd48=words<7>(model,0xd48);out.fields.fieldd68=word(model,0xd68);out.fields.fieldsd70=words<4>(model,0xd70);return out;
}
inline Bytes encode(const Bytes& initial,const std::vector<mp::Layer>& layers,const mc::Fields& fields,const Context& c,const Bindings& events){
 Bytes out=initial;for(unsigned i=0;i<layers.size();++i){const auto p=0x240+i*304;const auto& layer=layers[i];const auto& a=layer.animation;const auto& l=a.layer;put_half(out,p+4,l.selector);put_half(out,p+6,l.flags);put_half(out,p+8,l.field08);put(out,p+12,l.attributes);
 const std::array values{l.requested_frame,l.offset,l.previous_step,l.interpolation,l.remaining_interpolation,l.weight,l.frame,l.previous_frame,l.end_frame,l.field58,l.speed,l.field60,l.prepared_frame,l.prepared_next_frame};constexpr std::array<std::size_t,14> offsets{0x10,0x14,0x18,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x68};for(unsigned j=0;j<values.size();++j)put(out,p+offsets[j],std::bit_cast<std::uint32_t>(values[j]));
 for(unsigned j=0;j<2;++j)put(out,p+0x6c+4*j,l.interpolation_curve_bits[j]);
 for(auto[off,value]:std::initializer_list<std::pair<std::size_t,const mi::Bits4*>>{{0x20,&a.prediction_rotation},{0x30,&a.prediction_translation},{0x78,&a.previous_rotation},{0x88,&a.base_rotation},{0x98,&layer.rotation_98},{0xa8,&a.previous_translation},{0xb8,&a.base_translation},{0xc8,&layer.translation_c8},{0x108,&l.masks},{0x118,&l.previous_masks}})for(unsigned j=0;j<4;++j)put(out,p+off+4*j,(*value)[j]);
 c.encode(out,p+0xd8,a.tracks.translation);c.encode(out,p+0xe8,a.tracks.rotation);for(unsigned j=0;j<4;++j)put(out,p+0xf8+4*j,events.encode(l.bindings[j]));
 }for(unsigned j=0;j<7;++j)put(out,0xd48+4*j,fields.fieldsd48[j]);put(out,0xd68,fields.fieldd68);for(unsigned j=0;j<4;++j)put(out,0xd70+4*j,fields.fieldsd70[j]);return out;
}
inline void same(const Bytes& a,const Bytes& b,const std::string& message){if(a!=b){auto it=std::mismatch(a.begin(),a.end(),b.begin());std::ostringstream s;s<<message<<" byte0x"<<std::hex<<std::distance(a.begin(),it.first)<<" got"<<unsigned(*it.first)<<" expected"<<unsigned(*it.second);throw std::runtime_error(s.str());}check(true,message);}
}
