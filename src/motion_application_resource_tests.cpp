#include "motion_application.hpp"
#include "motion_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
namespace {
namespace ma=mh4u::motion_application;namespace mb=mh4u::motion_binding;
namespace mi=mh4u::motion_initialization;namespace mr=mh4u::motion_resource;
namespace ms=mh4u::motion_sampler;namespace math=mh4u::motion_transform_math;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{};
void check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
std::uint32_t word(const Bytes& b,std::size_t p,unsigned n=4){if(p>b.size()||n>b.size()-p)throw std::runtime_error("Fixture extent");std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint32_t(b[p+i])<<(8*i);return v;}
void put(Bytes& b,std::size_t p,std::uint32_t v){if(p>b.size()||4>b.size()-p)throw std::runtime_error("Fixture write extent");for(unsigned i=0;i<4;++i)b[p+i]=static_cast<std::uint8_t>(v>>(8*i));}
template<std::size_t N>std::array<std::uint32_t,N> words(const Bytes& b,std::size_t p){std::array<std::uint32_t,N> v{};for(unsigned i=0;i<N;++i)v[i]=word(b,p+i*4);return v;}
struct Reader {const Bytes& b;std::size_t p{};std::uint32_t integer(){const auto v=word(b,p);p+=4;return v;}Bytes bytes(std::size_t n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("Truncated fixture");Bytes out(b.begin()+static_cast<std::ptrdiff_t>(p),b.begin()+static_cast<std::ptrdiff_t>(p+n));p+=n;return out;}};
struct Source {std::uint32_t entry,base;mr::Resource resource;};
struct Context {
    static constexpr std::uint64_t synthetic_identity=0xffffffffffffffffULL;
    const std::vector<Source>& sources;
    std::map<std::uint32_t,mb::TrackReference> addresses;
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
    mb::Channel channel(const Bytes& model,std::size_t p,std::uint32_t bank)const{
        mb::Channel c;c.frame_bits=word(model,p+4);c.weight_bits=word(model,p+12);const auto address=word(model,p),key=word(model,p+8);
        if(address>=0x2800000&&address<0x2801000){check((address-0x2800000)%36==0&&(address-0x2800000)/36<synthetic.size(),"Fixture descriptor pointer");c.track=mb::TrackReference{synthetic_identity,0,0,(address-0x2800000)/36};}
        else if(address){c.track=addresses.at(address);c.track->bank=bank;}
        if(key){for(const auto& s:sources)if(key>=s.base&&key-s.base<s.resource.byte_size())c.key_data=mb::BufferReference{s.resource.identity(),key-s.base};check(c.key_data.has_value(),"Owned key cursor");}return c;
    }
    void encode(Bytes& model,std::size_t p,const mb::Channel& c)const{
        std::uint32_t address=0,key=0;
        if(c.track){if(c.track->resource_identity==synthetic_identity){check(c.track->bank==0&&c.track->slot==0&&c.track->ordinal<synthetic.size(),"Result fixture identity");address=0x2800000+c.track->ordinal*36;}else for(const auto& [at,known]:addresses)if(known.resource_identity==c.track->resource_identity&&known.slot==c.track->slot&&known.ordinal==c.track->ordinal){address=at;break;}check(address!=0,"Known result track");}
        if(c.key_data){for(const auto& s:sources)if(s.resource.identity()==c.key_data->resource_identity){check(c.key_data->byte_offset<s.resource.byte_size(),"Result key extent");key=s.base+static_cast<std::uint32_t>(c.key_data->byte_offset);break;}check(key!=0,"Known result key identity");}
        put(model,p,address);put(model,p+4,c.frame_bits);put(model,p+8,key);put(model,p+12,c.weight_bits);
    }
};
using Layer=mh4u::motion_pending::Layer;
using Clock=mh4u::motion_request::Layer;
constexpr std::array<std::size_t,14> clock_offsets{0x10,0x14,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x68,0x18};
auto clock_fields(Clock& l){return std::array{&l.requested_frame,&l.offset,&l.interpolation,&l.remaining_interpolation,&l.weight,&l.frame,&l.previous_frame,&l.end_frame,&l.field58,&l.speed,&l.field60,&l.prepared_frame,&l.prepared_next_frame,&l.previous_step};}
void put_half(Bytes& out,std::size_t p,std::uint16_t v){out[p]=static_cast<std::uint8_t>(v);out[p+1]=static_cast<std::uint8_t>(v>>8);}
template<std::size_t N>void put_words(Bytes& b,std::size_t p,const std::array<std::uint32_t,N>& values){for(unsigned i=0;i<N;++i)put(b,p+4*i,values[i]);}
ma::Model model(const Bytes& b){return {words<4>(b,0x40),words<4>(b,0x50),words<4>(b,0x60),words<4>(b,0xbd0),words<4>(b,0xbe0),words<4>(b,0xbf0)};}
std::vector<Layer> decode(const Bytes& b,std::size_t count,const Context& context){
 std::vector<Layer> out;
 for(unsigned i=0;i<count;++i){const auto p=0x240+i*304;Layer row{};auto& s=row.animation;auto& l=s.layer;
  l.selector=static_cast<std::uint16_t>(word(b,p+4,2));l.flags=static_cast<std::uint16_t>(word(b,p+6,2));l.field08=static_cast<std::uint16_t>(word(b,p+8,2));l.attributes=word(b,p+12);
  auto f=clock_fields(l);for(unsigned j=0;j<f.size();++j)*f[j]=std::bit_cast<float>(word(b,p+clock_offsets[j]));
  l.interpolation_curve_bits=words<2>(b,p+0x6c);l.masks=words<4>(b,p+0x108);l.previous_masks=words<4>(b,p+0x118);
  s.prediction_rotation=words<4>(b,p+0x20);s.prediction_translation=words<4>(b,p+0x30);s.previous_rotation=words<4>(b,p+0x78);s.base_rotation=words<4>(b,p+0x88);row.rotation_98=words<4>(b,p+0x98);s.previous_translation=words<4>(b,p+0xa8);s.base_translation=words<4>(b,p+0xb8);row.translation_c8=words<4>(b,p+0xc8);
  s.tracks.translation=context.channel(b,p+0xd8,(l.selector>>8)&15);s.tracks.rotation=context.channel(b,p+0xe8,(l.selector>>8)&15);
  out.push_back(row);
 }return out;
}
Bytes encode(const Bytes& initial,const ma::Model& m,const std::vector<Layer>& layers,const Context& c){
 Bytes out=initial;put_words(out,0x40,m.position);put_words(out,0x50,m.rotation);put_words(out,0x60,m.scale);put_words(out,0xbd0,m.null_scale);put_words(out,0xbe0,m.translation_delta);put_words(out,0xbf0,m.rotation_delta);
 for(unsigned i=0;i<layers.size();++i){const auto p=0x240+i*304;const auto& row=layers[i];const auto& s=row.animation;auto l=s.layer;
  put_half(out,p+4,l.selector);put_half(out,p+6,l.flags);put_half(out,p+8,l.field08);put(out,p+12,l.attributes);auto f=clock_fields(l);for(unsigned j=0;j<f.size();++j)put(out,p+clock_offsets[j],std::bit_cast<std::uint32_t>(*f[j]));
  put_words(out,p+0x6c,l.interpolation_curve_bits);put_words(out,p+0x108,l.masks);put_words(out,p+0x118,l.previous_masks);
  put_words(out,p+0x20,s.prediction_rotation);put_words(out,p+0x30,s.prediction_translation);put_words(out,p+0x78,s.previous_rotation);put_words(out,p+0x88,s.base_rotation);put_words(out,p+0x98,row.rotation_98);put_words(out,p+0xa8,s.previous_translation);put_words(out,p+0xb8,s.base_translation);put_words(out,p+0xc8,row.translation_c8);
  c.encode(out,p+0xd8,s.tracks.translation);c.encode(out,p+0xe8,s.tracks.rotation);
 }return out;
}
void same(const Bytes& actual,const Bytes& expected,const std::string& name){if(actual!=expected){const auto it=std::mismatch(actual.begin(),actual.end(),expected.begin());std::ostringstream s;s<<name<<" byte+0x"<<std::hex<<std::distance(actual.begin(),it.first)<<" got "<<unsigned(*it.first)<<" expected "<<unsigned(*it.second);throw std::runtime_error(s.str());}check(true,name);}
void oracle(const Bytes& fixture,const std::vector<Source>& sources){
 Reader r{fixture};check(r.bytes(8)==Bytes({'M','H','4','U','M','A','0','1'}),"Fixture identity");const auto count=r.integer(),source_count=r.integer();check(count>0&&count<=10000&&source_count==sources.size(),"Fixture counts");
 for(unsigned i=0;i<source_count;++i){const auto entry=r.integer(),base=r.integer(),size=r.integer();const auto digest=r.bytes(32);std::string hex;picosha2::bytes_to_hex_string(digest.begin(),digest.end(),hex);check(entry==sources[i].entry&&base==sources[i].base&&size==sources[i].resource.byte_size()&&hex==sources[i].resource.sha256(),"Source identity");}
 Context context(sources);std::uint64_t samples=0,errors=0;bool late_failure=false;
 for(unsigned i=0;i<count;++i){try{
  const auto size=r.integer();check(size<=1024*1024,"Record extent");const auto record=r.bytes(size);Reader in{record};in.integer();const auto n=in.integer(),descriptors=in.integer(),error=in.integer(),trace_count=in.integer();check(n<=4&&descriptors<=8&&error<=1&&trace_count<=24,"Record counts");
  const auto globals=in.bytes(56),before=in.bytes(8192);context.descriptors(in.bytes(descriptors*36));const auto expected=in.bytes(8192);
  std::vector<std::pair<std::uint32_t,std::uint32_t>> trace;for(unsigned k=0;k<trace_count;++k){const auto channel=in.integer(),frame=in.integer();trace.emplace_back(channel,frame);}check(in.p==record.size(),"No trailing record");
  auto state=decode(before,n,context);const auto m=model(before);mi::Templates t{words<3>(globals,0),words<3>(globals,12),words<4>(globals,24),words<4>(globals,40)};
  const auto result=ma::apply(m,state,t,{&context,Context::resolve});same(encode(before,m,state,context),before,"Borrowed input unchanged");
  if(error){const auto* e=std::get_if<ma::Error>(&result);check(e&&e->code==ma::ErrorCode::arithmetic&&e->arithmetic&&e->arithmetic->code==math::ErrorCode::degenerate_quaternion,"Original degenerate result has explicit native domain rejection");++errors;continue;}
  const auto* out=std::get_if<ma::Output>(&result);if(!out){const auto& e=std::get<ma::Error>(result);throw std::runtime_error(std::string(e.message)+(e.arithmetic?std::string(" / ")+std::string(e.arithmetic->message):"")+" layer "+std::to_string(e.layer));}
  check(out->layers.size()==n&&out->samples==trace_count,"Native sample count");samples+=out->samples;
  std::vector<std::pair<std::uint32_t,std::uint32_t>> planned;
  std::size_t contributors=0,predictions=0,active=0;
  for(unsigned li=0;li<n;++li){const auto& s=state[li].animation;const auto& l=s.layer;const auto p=0x2000000+0x240+li*304;
   for(const auto pair:{std::pair{&s.tracks.translation,&out->layers[li].animation.tracks.translation},std::pair{&s.tracks.rotation,&out->layers[li].animation.tracks.rotation}}){const auto& b=*pair.first;const auto& a=*pair.second;check(a.track==b.track&&a.weight_bits==b.weight_bits,"Identity including bank and raw weight preserved");check(a.key_data.has_value()==b.key_data.has_value(),"Cursor presence preserved");if(b.key_data)check(a.key_data->resource_identity==b.key_data->resource_identity,"Cursor ownership preserved");}
   check(s.layer.bindings==out->layers[li].animation.layer.bindings,"Logical event bindings preserved");check(out->layers[li].animation.tracks.joints.empty(),"Absent joint vectors retained");
   if(l.selector==0xffff)continue;++active;contributors+=!(l.attributes&1U);
   auto pair=[&](float frame){planned.emplace_back(p+0xd8,std::bit_cast<std::uint32_t>(frame));planned.emplace_back(p+0xe8,std::bit_cast<std::uint32_t>(frame));};pair(l.prepared_frame);
   if(!(l.flags&1U)&&(l.flags&4U)&&(l.field08&1U))continue;
   ++predictions;pair(l.flags&(1U|4U)?l.end_frame:l.prepared_next_frame);if(!(l.flags&1U)&&(l.flags&4U))pair(l.previous_frame);
  }
  check(planned==trace,"Source sampler call/frame plan matches numeric gates");check(out->contributing_layers==contributors&&out->sampled_layers==active&&out->prediction_updates==predictions,"Counts");
  same(encode(before,out->model,out->layers,context),expected,"Full model arena");
  if(!late_failure&&n==1&&state[0].animation.tracks.translation!=out->layers[0].animation.tracks.translation){
   auto failed=state;const auto previous=failed[0].animation.tracks.translation;const mb::TrackReference bad{Context::synthetic_identity,0,0,9999};failed[0].animation.tracks.rotation.track=bad;context.resolutions.clear();const auto rejected=ma::apply(m,failed,t,{&context,Context::resolve});const auto* e=std::get_if<ma::Error>(&rejected);check(e&&e->code==ma::ErrorCode::unresolved_track&&e->component==ma::Component::rotation&&e->track==bad,"Late rotation failure identity");check(failed[0].animation.tracks.translation==previous&&m==model(before),"Late failure does not publish cursor/model");check(context.resolutions==std::vector<mb::TrackReference>{*previous.track,bad},"Late failure after real T");late_failure=true;
  }
 }catch(const std::exception& e){throw std::runtime_error("Case "+std::to_string(i)+": "+e.what());}}
 check(r.p==fixture.size()&&late_failure,"Complete fixture and late failure coverage");std::cout<<"{\"status\":\"passed\",\"cases\":"<<count<<",\"checks\":"<<checks<<",\"samples\":"<<samples<<",\"errors\":"<<errors<<",\"late_failure\":true,\"arm_model_bytes\":"<<(count-errors)*8192ULL<<",\"input_immutability_bytes\":"<<count*8192ULL<<"}\n";
}
}
int main(int argc,char** argv){try{
 if(argc<6||(argc-3)%3!=0)throw std::runtime_error("Usage: application-resource-tests oracle.bin sha256 entry source.lmt sha256 [entry source.lmt sha256 ...]");
 std::ifstream in(argv[1],std::ios::binary|std::ios::ate);check(bool(in),"Fixture open");const auto size=in.tellg();check(size>=0&&size<=128*1024*1024,"Fixture byte bound");in.seekg(0);Bytes bytes(static_cast<std::size_t>(size));in.read(reinterpret_cast<char*>(bytes.data()),size);check(bool(in),"Fixture read");check(picosha2::hash256_hex_string(bytes)==argv[2],"Fixture SHA256 mismatch");
 std::vector<Source> sources;for(int i=3;i<argc;i+=3){const auto entry=std::stoul(argv[i]);check(entry<=0xffffffffU&&sources.size()<2,"Source arguments");auto loaded=mr::Resource::load(argv[i+1],argv[i+2]);if(const auto* e=std::get_if<mr::Error>(&loaded))throw std::runtime_error(e->message);sources.push_back({static_cast<std::uint32_t>(entry),sources.empty()?0x5000000U:0x6000000U,std::get<mr::Resource>(std::move(loaded))});}oracle(bytes,sources);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
