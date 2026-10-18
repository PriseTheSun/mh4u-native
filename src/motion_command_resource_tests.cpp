#include "motion_command.hpp"
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
namespace mc=mh4u::motion_command;namespace mp=mh4u::motion_pending;namespace rq=mh4u::motion_request;namespace mq=mh4u::motion_query;namespace mb=mh4u::motion_binding;
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
struct State {std::vector<mp::Layer> layers;mc::Fields fields;};
struct Bindings {
 const std::vector<Source>& sources;std::map<std::uint32_t,rq::Binding> addresses;
 explicit Bindings(const std::vector<Source>& src):sources(src){for(const auto& s:src)for(std::uint32_t slot=0;slot<s.resource.slot_count();++slot){auto result=s.resource.event_groups(slot);auto* groups=std::get_if<std::optional<std::array<mr::EventGroupView,4>>>(&result);if(!groups||!*groups)continue;for(unsigned gi=0;gi<4;++gi)addresses.emplace(s.base+(**groups)[gi].source_offset,rq::Binding{s.resource.identity(),0,slot,gi});}}
 std::optional<rq::Binding> decode(std::uint32_t address,std::uint32_t bank)const{if(!address)return{};auto value=addresses.at(address);value.bank=bank;return value;}
 std::uint32_t encode(const std::optional<rq::Binding>& id)const{if(!id)return 0;for(auto&[address,known]:addresses)if(known.resource_identity==id->resource_identity&&known.slot==id->slot&&known.group==id->group)return address;throw std::runtime_error("Unknown output event identity");}
};
void put_half(Bytes& b,std::size_t p,std::uint16_t v){b.at(p)=std::uint8_t(v);b.at(p+1)=std::uint8_t(v>>8);}
State decode(const Bytes& model,std::uint32_t count,const Context& c,const Bindings& events){
 State out;for(unsigned i=0;i<count;++i){const auto p=0x240+i*304;mp::Layer layer;auto& a=layer.animation;auto& l=a.layer;l.selector=std::uint16_t(word(model,p+4,2));l.flags=std::uint16_t(word(model,p+6,2));l.field08=std::uint16_t(word(model,p+8,2));l.attributes=word(model,p+12);
 const std::array fields{&l.requested_frame,&l.offset,&l.previous_step,&l.interpolation,&l.remaining_interpolation,&l.weight,&l.frame,&l.previous_frame,&l.end_frame,&l.field58,&l.speed,&l.field60,&l.prepared_frame,&l.prepared_next_frame};constexpr std::array<std::size_t,14> offsets{0x10,0x14,0x18,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x68};for(unsigned j=0;j<fields.size();++j)*fields[j]=std::bit_cast<float>(word(model,p+offsets[j]));
 l.interpolation_curve_bits=words<2>(model,p+0x6c);l.masks=words<4>(model,p+0x108);l.previous_masks=words<4>(model,p+0x118);
 for(auto[off,target]:std::initializer_list<std::pair<std::size_t,mi::Bits4*>>{{0x20,&a.prediction_rotation},{0x30,&a.prediction_translation},{0x78,&a.previous_rotation},{0x88,&a.base_rotation},{0x98,&layer.rotation_98},{0xa8,&a.previous_translation},{0xb8,&a.base_translation},{0xc8,&layer.translation_c8}})*target=words<4>(model,p+off);
 const auto bank=(l.selector>>8)&15;a.tracks.translation=c.channel(model,p+0xd8,bank);a.tracks.rotation=c.channel(model,p+0xe8,bank);for(unsigned gi=0;gi<4;++gi)l.bindings[gi]=events.decode(word(model,p+0xf8+4*gi),bank);out.layers.push_back(layer);
 }out.fields.fieldsd48=words<7>(model,0xd48);out.fields.fieldd68=word(model,0xd68);out.fields.fieldsd70=words<4>(model,0xd70);return out;
}
Bytes encode(const Bytes& initial,const std::vector<mp::Layer>& layers,const mc::Fields& fields,const Context& c,const Bindings& events){
 Bytes out=initial;for(unsigned i=0;i<layers.size();++i){const auto p=0x240+i*304;const auto& layer=layers[i];const auto& a=layer.animation;const auto& l=a.layer;put_half(out,p+4,l.selector);put_half(out,p+6,l.flags);put_half(out,p+8,l.field08);put(out,p+12,l.attributes);
 const std::array values{l.requested_frame,l.offset,l.previous_step,l.interpolation,l.remaining_interpolation,l.weight,l.frame,l.previous_frame,l.end_frame,l.field58,l.speed,l.field60,l.prepared_frame,l.prepared_next_frame};constexpr std::array<std::size_t,14> offsets{0x10,0x14,0x18,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60,0x64,0x68};for(unsigned j=0;j<values.size();++j)put(out,p+offsets[j],std::bit_cast<std::uint32_t>(values[j]));
 for(unsigned j=0;j<2;++j)put(out,p+0x6c+4*j,l.interpolation_curve_bits[j]);
 for(auto[off,value]:std::initializer_list<std::pair<std::size_t,const mi::Bits4*>>{{0x20,&a.prediction_rotation},{0x30,&a.prediction_translation},{0x78,&a.previous_rotation},{0x88,&a.base_rotation},{0x98,&layer.rotation_98},{0xa8,&a.previous_translation},{0xb8,&a.base_translation},{0xc8,&layer.translation_c8},{0x108,&l.masks},{0x118,&l.previous_masks}})for(unsigned j=0;j<4;++j)put(out,p+off+4*j,(*value)[j]);
 c.encode(out,p+0xd8,a.tracks.translation);c.encode(out,p+0xe8,a.tracks.rotation);for(unsigned j=0;j<4;++j)put(out,p+0xf8+4*j,events.encode(l.bindings[j]));
 }for(unsigned j=0;j<7;++j)put(out,0xd48+4*j,fields.fieldsd48[j]);put(out,0xd68,fields.fieldd68);for(unsigned j=0;j<4;++j)put(out,0xd70+4*j,fields.fieldsd70[j]);return out;
}
void same(const Bytes& a,const Bytes& b,const std::string& message){if(a!=b){auto it=std::mismatch(a.begin(),a.end(),b.begin());std::ostringstream s;s<<message<<" byte0x"<<std::hex<<std::distance(a.begin(),it.first)<<" got"<<unsigned(*it.first)<<" expected"<<unsigned(*it.second);throw std::runtime_error(s.str());}check(true,message);}
void oracle(const Bytes& fixture,const std::vector<Source>& sources){
 Reader r{fixture};check(r.bytes(8)==Bytes({'M','4','M','C','L','0','0','1'}),"Fixture magic");const auto count=r.integer(),source_count=r.integer();check(count<=10000&&source_count==sources.size(),"Fixture counts");
 for(unsigned i=0;i<source_count;++i){auto entry=r.integer(),base=r.integer(),size=r.integer();auto digest=r.bytes(32);std::string hex;picosha2::bytes_to_hex_string(digest.begin(),digest.end(),hex);check(entry==sources[i].entry&&base==sources[i].base&&size==sources[i].resource.byte_size()&&hex==sources[i].resource.sha256(),"Source pin");}
 Context context(sources);Bindings events(sources);std::size_t commands=0,queries=0,samples=0,setters=0,accepted=0,compared=0;bool late_error=false;
 for(unsigned ci=0;ci<count;++ci){auto size=r.integer();check(size<=1024*1024,"Record bound");auto body=r.bytes(size);Reader in{body};in.integer();auto storage=in.integer(),ncommands=in.integer(),trackbytes=in.integer();check(storage<=8&&ncommands<=100&&trackbytes<=4096&&trackbytes%36==0,"Record metadata");auto templates=in.bytes(28),tables=in.bytes(24),vector=in.bytes(12);auto model=in.bytes(8192);context.descriptors(in.bytes(trackbytes));State state=decode(model,storage,context,events);const auto active=word(model,0x234);
 rq::Model resource_model;resource_model.skeleton_present=word(model,0xf0)!=0;for(unsigned bank=0;bank<16;++bank){const auto address=word(model,0xc04+4*bank);if(!address){resource_model.banks[bank]=rq::Absent{};continue;}check(address>=0x2700000&&(address-0x2700000)%0x80==0&&(address-0x2700000)/0x80<sources.size(),"Bank wrapper source");resource_model.banks[bank]=sources[(address-0x2700000)/0x80].resource.request_resource();}
 mc::Globals globals;for(unsigned t=0;t<3;++t)globals.fields6c_tables[t]=words<2>(tables,t*8);globals.vector10e6370=words<3>(vector,0);mc::Sampling sampling;sampling.templates.base_translation_10e62c0=words<3>(templates,0);sampling.templates.identity_10e6560=words<4>(templates,12);sampling.resolver={&context,Context::resolve};
 for(unsigned command_i=0;command_i<ncommands;++command_i){mc::Command command;command.entry=static_cast<mc::Entry>(in.integer());command.mode=in.integer();command.layer=in.integer();command.selector=in.integer();command.flags=in.integer();command.interpolation=std::bit_cast<float>(in.integer());command.start_frame=std::bit_cast<float>(in.integer());command.speed=std::bit_cast<float>(in.integer());auto expected=in.bytes(8192);const auto ncalls=in.integer(),ntrace=in.integer();check(ncalls<=2&&ntrace<=100,"Observation bound");std::vector<std::array<std::uint32_t,8>> calls;for(unsigned j=0;j<ncalls;++j){std::array<std::uint32_t,8> call;for(auto& v:call)v=in.integer();calls.push_back(call);}unsigned expected_queries=0,expected_samples=0;for(unsigned j=0;j<ntrace;++j){auto type=in.integer();in.integer();auto last=in.integer();check(type==1||type==2,"Trace kind");if(type==1){++expected_queries;check(last==0,"Original query Q output null");}else ++expected_samples;}
 try{context.resolutions.clear();auto result=mc::apply(state.layers,active,state.fields,resource_model,command,globals,sampling);if(auto* e=std::get_if<mc::Error>(&result))throw std::runtime_error("Native error "+std::to_string(int(e->code))+": "+std::string(e->message));auto out=std::get<mc::Output>(std::move(result));check(out.request_count==ncalls&&out.queries==expected_queries&&out.samples==expected_samples,"Original query/sample/setter counts");
 for(unsigned j=0;j<ncalls;++j){auto& call=out.requests[j];const std::array<std::uint32_t,8> got{call.request.layer,call.request.selector,call.request.attributes,std::bit_cast<std::uint32_t>(call.request.interpolation),std::bit_cast<std::uint32_t>(call.request.start_frame),std::bit_cast<std::uint32_t>(call.request.speed),call.command_flags,std::uint32_t(call.accepted)};check(got==calls[j],"Original setter arguments and result");accepted+=call.accepted;}
 auto output=encode(model,out.layers,out.fields,context,events);same(output,expected,"Complete model");same(encode(model,state.layers,state.fields,context,events),model,"Input remains unchanged");for(unsigned li=0;li<storage;++li){auto& a=state.layers[li].animation.tracks;auto& b=out.layers[li].animation.tracks;check(a.translation.track==b.translation.track&&a.rotation.track==b.rotation.track,"Live query retains bound track identities across selector change");}
 // Fail only a second query after the first successfully advanced compressed cursors.
 if(!late_error&&out.queries>=2&&context.resolutions.size()>=4&&state.layers[0].animation.tracks.translation!=out.layers[0].animation.tracks.translation){
  struct FailureContext{const Context* source;mutable std::size_t calls{};std::size_t fail_at;};FailureContext failure{&context,0,context.resolutions.size()/out.queries};
  mc::Sampling failing=sampling;failing.resolver={&failure,[](const void* ptr,const mb::TrackReference& id)->mi::Resolution{auto& f=*static_cast<const FailureContext*>(ptr);if(f.calls++==f.fail_at)return mi::ResolutionError{97,"Second query deliberately unresolved"};return Context::resolve(f.source,id);}};
  auto failed=mc::apply(state.layers,active,state.fields,resource_model,command,globals,failing);check(std::holds_alternative<mc::Error>(failed),"Late query failure");const auto& e=std::get<mc::Error>(failed);check(e.code==mc::ErrorCode::query_failed&&e.query_index==1&&e.query_error->resolution->code==97,"Exact late failure reason and ordinal");same(encode(model,state.layers,state.fields,context,events),model,"Late failure preserves original cursor/state");late_error=true;
 }
 ++commands;queries+=out.queries;samples+=out.samples;setters+=ncalls;compared+=8192;state.layers=std::move(out.layers);state.fields=out.fields;model=std::move(output);
 }catch(const std::exception& e){throw std::runtime_error("Sequence "+std::to_string(ci)+" command "+std::to_string(command_i)+": "+e.what());}
 }check(in.p==body.size(),"No record tail");
 }check(r.p==fixture.size(),"No fixture tail");check(late_error,"Compressed-cursor late failure covered");std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"sequences\":"<<count<<",\"commands\":"<<commands<<",\"queries\":"<<queries<<",\"samples\":"<<samples<<",\"setters\":"<<setters<<",\"accepted\":"<<accepted<<",\"compared_bytes\":"<<compared<<",\"late_query_failure\":true}\n";
}
}
int main(int argc,char** argv){try{if(argc!=9)throw std::runtime_error("Usage: motion-command-resource-tests oracle sha entry path sha entry path sha");std::ifstream in(argv[1],std::ios::binary|std::ios::ate);check(bool(in),"Fixture open");auto n=in.tellg();check(n>=0&&n<=128*1024*1024,"Fixture bound");Bytes data(static_cast<std::size_t>(n));in.seekg(0);check(bool(in.read(reinterpret_cast<char*>(data.data()),n))&&in.peek()==std::char_traits<char>::eof(),"Fixture read size stable");check(picosha2::hash256_hex_string(data)==argv[2],"Fixture hash");std::vector<Source> sources;for(int i=3;i<argc;i+=3){std::size_t end=0;std::string id=argv[i];auto entry=std::stoull(id,&end);check(end==id.size()&&entry<=0xffffffff,"Source entry");auto resource=mr::Resource::load(argv[i+1],argv[i+2]);if(auto* e=std::get_if<mr::Error>(&resource))throw std::runtime_error(e->message);sources.push_back({std::uint32_t(entry),sources.empty()?0x5000000U:0x6000000U,std::get<mr::Resource>(std::move(resource))});}oracle(data,sources);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
