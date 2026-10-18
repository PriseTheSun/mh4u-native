#include "motion_query.hpp"
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
namespace mq=mh4u::motion_query;namespace mb=mh4u::motion_binding;
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
struct State {std::vector<mh4u::motion_request::Layer> layers;std::vector<mq::Channels> channels;};
State decode(const Bytes& model,std::uint32_t count,const Context& c){
    State out;
    for(unsigned i=0;i<count;++i){const auto p=0x240+i*304;mh4u::motion_request::Layer l;l.selector=static_cast<std::uint16_t>(word(model,p+4,2));l.flags=static_cast<std::uint16_t>(word(model,p+6,2));l.field08=static_cast<std::uint16_t>(word(model,p+8,2));l.attributes=word(model,p+12);
        const std::array fields{&l.requested_frame,&l.offset,&l.interpolation,&l.remaining_interpolation,&l.weight,&l.frame,&l.previous_frame,&l.end_frame,&l.field58,&l.speed,&l.field60};constexpr std::array<std::size_t,11> offsets{0x10,0x14,0x40,0x44,0x48,0x4c,0x50,0x54,0x58,0x5c,0x60};for(unsigned j=0;j<fields.size();++j)*fields[j]=std::bit_cast<float>(word(model,p+offsets[j]));
        const auto bank=(l.selector>>8)&15;out.layers.push_back(l);out.channels.push_back({c.channel(model,p+0xd8,bank),c.channel(model,p+0xe8,bank)});
    }return out;
}
std::vector<mq::LayerView> views(const State& state){std::vector<mq::LayerView> out;for(unsigned i=0;i<state.layers.size();++i)out.push_back({state.layers[i],state.channels[i].translation,state.channels[i].rotation});return out;}
Bytes encode(const Bytes& initial,const std::vector<mq::Channels>& channels,const Context& c){Bytes out=initial;for(unsigned i=0;i<channels.size();++i){c.encode(out,0x240+i*304+0xd8,channels[i].translation);c.encode(out,0x240+i*304+0xe8,channels[i].rotation);}return out;}
void same(const Bytes& actual,const Bytes& expected,const std::string& name){if(actual!=expected){const auto it=std::mismatch(actual.begin(),actual.end(),expected.begin());std::ostringstream s;s<<name<<" byte+0x"<<std::hex<<std::distance(actual.begin(),it.first)<<" got "<<unsigned(*it.first)<<" expected "<<unsigned(*it.second);throw std::runtime_error(s.str());}check(true,name);}
void rejection(const mq::Result& result,mq::ErrorCode code){const auto* error=std::get_if<mq::Error>(&result);check(error&&error->code==code,"Expected typed rejection");}
void oracle(const Bytes& fixture,const std::vector<Source>& sources){
    Reader r{fixture};check(r.bytes(8)==Bytes({'M','H','4','U','M','Q','0','1'}),"Fixture identity");const auto count=r.integer(),source_count=r.integer();check(count>0&&count<=10000&&source_count==sources.size(),"Fixture counts");
    for(unsigned i=0;i<source_count;++i){const auto entry=r.integer(),base=r.integer(),size=r.integer();const auto digest=r.bytes(32);std::string hex;picosha2::bytes_to_hex_string(digest.begin(),digest.end(),hex);check(entry==sources[i].entry&&base==sources[i].base&&size==sources[i].resource.byte_size()&&hex==sources[i].resource.sha256(),"Source identity");}
    Context context(sources);std::uint64_t queries=0,compared=0,samples=0,domain_errors=0;bool negative_done=false,late_error_done=false;
    for(unsigned i=0;i<count;++i){const auto size=r.integer();check(size<=4*1024*1024,"Record extent");const auto record=r.bytes(size);Reader in{record};in.integer();const auto n=in.integer(),query_count=in.integer(),descriptors=in.integer();check(n<=4&&query_count<=1000&&descriptors<=8,"Record counts");const auto templates=in.bytes(28);auto model=in.bytes(8192);context.descriptors(in.bytes(descriptors*36));auto state=decode(model,n,context);
        mi::Templates defaults;defaults.base_translation_10e62c0=words<3>(templates,0);defaults.identity_10e6560=words<4>(templates,12);
        for(unsigned j=0;j<query_count;++j){const auto frame=in.integer(),selection=in.integer(),error=in.integer(),trace_count=in.integer();check(selection<=3&&error<=1&&trace_count<=12,"Query extent");const auto before_output=in.bytes(32),expected_output=in.bytes(32),expected_model=in.bytes(8192);unsigned expected_samples=0;std::vector<std::uint32_t> source_channels;
            for(unsigned k=0;k<trace_count;++k){const auto kind=in.integer(),channel=in.integer(),sample_frame=in.integer();(void)sample_frame;check(kind==1||kind==2,"Trace kind");if(kind==1){++expected_samples;source_channels.push_back(channel);}}
            const auto view=views(state);context.resolutions.clear();const auto result=mq::query(view,frame,defaults,{bool(selection&1),bool(selection&2)},{&context,Context::resolve});++queries;
            try{
                if(error){rejection(result,mq::ErrorCode::arithmetic);check(std::get<mq::Error>(result).arithmetic->code==math::ErrorCode::degenerate_quaternion,"ARM zero norm has explicit domain rejection");same(encode(model,state.channels,context),model,"Input preserved on error");++domain_errors;continue;}
                const auto* out=std::get_if<mq::Output>(&result);if(!out)throw std::runtime_error("Native rejection: "+std::string(std::get<mq::Error>(result).message));
                check(out->channels.size()==n&&out->samples==expected_samples&&out->accepted_layers*2==expected_samples,"Original call counts");check(out->translation.has_value()==bool(selection&1)&&out->rotation.has_value()==bool(selection&2),"Explicit output absence");
                for(unsigned li=0;li<n;++li)for(const auto pair:{std::pair{&state.channels[li].translation,&out->channels[li].translation},std::pair{&state.channels[li].rotation,&out->channels[li].rotation}}){const auto& before=*pair.first;const auto& after=*pair.second;check(before.track==after.track,"Complete track identity remains unchanged, including bank");check(before.weight_bits==after.weight_bits,"Raw channel weight remains unchanged");check(before.key_data.has_value()==after.key_data.has_value(),"Cursor presence remains unchanged");if(before.key_data)check(before.key_data->resource_identity==after.key_data->resource_identity,"Cursor resource ownership remains unchanged");}
                std::vector<std::uint32_t> expected_channels;for(unsigned li=0;li<n;++li)if(state.layers[li].selector!=0xffff&&!(state.layers[li].flags&2)){expected_channels.push_back(0x2000000+0x240+li*304+0xd8);expected_channels.push_back(0x2000000+0x240+li*304+0xe8);}check(source_channels==expected_channels,"Source translation/rotation/layer order");
                auto output=before_output;if(out->translation)for(unsigned k=0;k<4;++k)put(output,k*4,(*out->translation)[k]);if(out->rotation)for(unsigned k=0;k<4;++k)put(output,16+k*4,(*out->rotation)[k]);same(output,expected_output,"Output bits");
                const auto actual_model=encode(model,out->channels,context);same(actual_model,expected_model,"Whole model and cursors");same(encode(model,state.channels,context),model,"Borrowed state unchanged");compared+=8192+32;samples+=out->samples;
                if(!negative_done&&!context.synthetic.empty()&&out->samples){
                    context.reject=true;rejection(mq::query(view,frame,defaults,{},{&context,Context::resolve}),mq::ErrorCode::unresolved_track);context.reject=false;
                    auto missing=defaults;missing.identity_10e6560.reset();rejection(mq::query(view,frame,missing,{},{&context,Context::resolve}),mq::ErrorCode::missing_template);
                    mq::Limits limit;limit.layers=0;rejection(mq::query(view,frame,defaults,{},{&context,Context::resolve},limit),mq::ErrorCode::resource_limit);same(encode(model,state.channels,context),model,"Typed failure preserves input");negative_done=true;
                }
                if(!late_error_done&&n==1&&state.channels[0].translation.track&&state.channels[0].translation!=out->channels[0].translation){
                    // The successful original/native comparison above proves
                    // this exact T query advances a compressed cursor. Force
                    // only the following Q resolution to fail in a copy.
                    auto failing=state;const mb::TrackReference unresolved{Context::synthetic_identity,0,0,9999};failing.channels[0].rotation.track=unresolved;const auto borrowed=failing.channels;const auto failing_views=views(failing);context.resolutions.clear();
                    const auto rejected=mq::query(failing_views,frame,defaults,{},{&context,Context::resolve});rejection(rejected,mq::ErrorCode::unresolved_track);const auto& e=std::get<mq::Error>(rejected);check(e.layer==0&&e.component==mq::Component::rotation&&e.track==unresolved&&e.resolution->code==92,"Late Q resolution failure retains full reason and identity");check(context.resolutions==std::vector<mb::TrackReference>{*borrowed[0].translation.track,unresolved},"T sampled before failing Q");check(failing.channels==borrowed,"Late failure publishes no partial compressed cursor");late_error_done=true;
                }
                state.channels=out->channels;model=actual_model;
            }catch(const std::exception& e){throw std::runtime_error("Sequence "+std::to_string(i)+" query "+std::to_string(j)+": "+e.what());}
        }check(in.p==record.size(),"No trailing record");
    }check(r.p==fixture.size()&&negative_done&&late_error_done,"Complete fixture and rejection coverage");std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"sequences\":"<<count<<",\"queries\":"<<queries<<",\"successful_queries\":"<<(queries-domain_errors)<<",\"numeric_domain_rejections\":"<<domain_errors<<",\"late_compressed_cursor_failure\":true,\"samples\":"<<samples<<",\"compared_bytes\":"<<compared<<"}\n";
}
} // namespace
int main(int argc,char** argv){try{
    if(argc<6||(argc-3)%3!=0)throw std::runtime_error("Usage: motion-query-resource-tests oracle.bin sha256 entry source.lmt sha256 [entry source.lmt sha256 ...]");
    std::ifstream in(argv[1],std::ios::binary|std::ios::ate);check(bool(in),"Fixture open");const auto size=in.tellg();check(size>=0&&size<=128*1024*1024,"Fixture byte bound");in.seekg(0);Bytes bytes(static_cast<std::size_t>(size));in.read(reinterpret_cast<char*>(bytes.data()),size);check(bool(in),"Fixture read");check(picosha2::hash256_hex_string(bytes)==argv[2],"Fixture SHA256 mismatch");
    std::vector<Source> sources;for(int i=3;i<argc;i+=3){const auto entry=std::stoul(argv[i]);check(entry<=0xffffffffU&&sources.size()<2,"Source arguments");auto loaded=mr::Resource::load(argv[i+1],argv[i+2]);if(const auto* e=std::get_if<mr::Error>(&loaded))throw std::runtime_error(e->message);sources.push_back({static_cast<std::uint32_t>(entry),sources.empty()?0x5000000U:0x6000000U,std::get<mr::Resource>(std::move(loaded))});}oracle(bytes,sources);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
