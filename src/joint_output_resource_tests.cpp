#include "joint_output.hpp"
#include "motion_transition.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
namespace jo=mh4u::joint_output;namespace mt=mh4u::motion_transition;
namespace jp=mh4u::joint_pose;namespace ju=mh4u::joint_update;
namespace mp=mh4u::motion_pending;namespace mr=mh4u::motion_request;
namespace math=mh4u::motion_transform_math;
using Bytes=std::vector<std::uint8_t>;using Arenas=std::array<Bytes,7>;
constexpr std::array<std::uint32_t,7> addresses{0x2000000,0x2100000,0x2200000,0x2300000,0x2400000,0x2500000,0x2600000};
constexpr std::array<std::uint32_t,7> sizes{8192,8192,4096,4096,4096,4096,4096};
constexpr std::size_t arena_bytes=36864;
std::uint64_t checks{},compared_bytes{},arm_output_bytes{};
void check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
std::uint32_t word(const Bytes& bytes,std::size_t p,unsigned n=4){
    if(p>bytes.size()||n>bytes.size()-p)throw std::runtime_error("Fixture read extent");std::uint32_t value=0;
    for(unsigned i=0;i<n;++i)value|=std::uint32_t(bytes[p+i])<<(8*i);return value;
}
void put(Bytes& bytes,std::size_t p,std::uint32_t value,unsigned n=4){
    if(p>bytes.size()||n>bytes.size()-p)throw std::runtime_error("Fixture write extent");for(unsigned i=0;i<n;++i)bytes[p+i]=static_cast<std::uint8_t>(value>>(8*i));
}
template<std::size_t N>std::array<std::uint32_t,N> words(const Bytes& bytes,std::size_t p){std::array<std::uint32_t,N> out{};for(unsigned i=0;i<N;++i)out[i]=word(bytes,p+i*4);return out;}
template<std::size_t N>void write(Bytes& bytes,std::size_t p,const std::array<std::uint32_t,N>& values){for(unsigned i=0;i<N;++i)put(bytes,p+i*4,values[i]);}
struct Reader {
    const Bytes& bytes;std::size_t p{};
    std::uint32_t integer(){const auto v=word(bytes,p);p+=4;return v;}
    Bytes take(std::size_t count){if(p>bytes.size()||count>bytes.size()-p)throw std::runtime_error("Truncated fixture");Bytes out(bytes.begin()+static_cast<std::ptrdiff_t>(p),bytes.begin()+static_cast<std::ptrdiff_t>(p+count));p+=count;return out;}
    Arenas arenas(){Arenas out;for(unsigned i=0;i<out.size();++i)out[i]=take(sizes[i]);return out;}
};
void same(const Bytes& actual,const Bytes& expected,const std::string& name){
    check(actual.size()==expected.size(),name+" extent");if(actual!=expected){const auto at=std::mismatch(actual.begin(),actual.end(),expected.begin());std::ostringstream msg;msg<<name<<" byte+0x"<<std::hex<<std::distance(actual.begin(),at.first)<<" got "<<unsigned(*at.first)<<" expected "<<unsigned(*at.second);throw std::runtime_error(msg.str());}++checks;
}
void same(const Arenas& actual,const Arenas& expected,const std::string& name){for(unsigned i=0;i<actual.size();++i)same(actual[i],expected[i],name+" arena"+std::to_string(i));compared_bytes+=arena_bytes;}
jp::Joint pose(const Bytes& bytes,std::size_t p){return {words<3>(bytes,p+0x48),words<4>(bytes,p+0x58),words<4>(bytes,p+0x68),words<4>(bytes,p+0x78)};}
void encode_pose(Bytes& bytes,std::size_t p,const jp::Joint& value){write(bytes,p+0x48,value.reference_translation);write(bytes,p+0x58,value.rotation);write(bytes,p+0x68,value.scale);write(bytes,p+0x78,value.translation);}
struct Transition {
    std::vector<jp::Joint> joints;std::vector<ju::Metadata> metadata;
    std::vector<mp::Snapshot> snapshots;std::vector<mr::Layer> layers;
    mt::Parameters parameters;
    std::vector<mt::LayerView> views()const{std::vector<mt::LayerView> out;for(const auto& l:layers)out.push_back({l});return out;}
};
Transition transition(const Arenas& a,std::uint32_t n,std::uint32_t count){
    Transition out;out.parameters={words<4>(a[0],0x50),words<3>(a[0],0x40),word(a[0],0x23c)};
    for(unsigned i=0;i<n;++i){const auto p=i*168,s=i*56;out.joints.push_back(pose(a[1],p));out.metadata.push_back({word(a[1],p+0x94),word(a[1],p+0x98)});out.snapshots.push_back({word(a[2],s),word(a[2],s+4),words<4>(a[2],s+8),words<4>(a[2],s+0x18),words<4>(a[2],s+0x28)});}
    for(unsigned i=0;i<count;++i){const auto p=0x240+i*304;mr::Layer l;l.selector=static_cast<std::uint16_t>(word(a[0],p+4,2));l.flags=static_cast<std::uint16_t>(word(a[0],p+6,2));l.field08=static_cast<std::uint16_t>(word(a[0],p+8,2));l.attributes=word(a[0],p+12);l.field60=std::bit_cast<float>(word(a[0],p+0x60));out.layers.push_back(l);}return out;
}
Arenas encode(const Arenas& before,const mt::Output& out){auto result=before;write(result[0],0x70,out.local_rt);put(result[0],0x23c,out.accumulated_flags);for(unsigned i=0;i<out.joints.size();++i)encode_pose(result[1],i*168,out.joints[i]);return result;}
jo::State output_state(const Arenas& a,std::uint32_t n){
    jo::State out;out.local_rt=words<16>(a[0],0x70);out.world=words<16>(a[0],0xb0);out.scale=words<3>(a[0],0x60);
    out.flags4=word(a[0],4);out.flags104=word(a[0],0x104);out.flags230=word(a[0],0x230);out.flags238=word(a[0],0x238);out.attachment_index=word(a[0],0x34);out.attachment_mode=word(a[0],0x38);out.dirty=static_cast<std::uint8_t>(word(a[0],0xc49,1));out.fieldc4c=word(a[0],0xc4c);
    for(unsigned i=0;i<n;++i){const auto p=i*168;const auto owner=word(a[1],p+0xa0);out.joints.push_back({pose(a[1],p),{word(a[1],p+0x94),word(a[1],p+0x98)},words<16>(a[1],p+8),word(a[1],p+0x90),word(a[1],p+0x88)?jo::Callback::unsupported:jo::Callback::null,owner==addresses[0]?jo::Owner::self:owner?jo::Owner::other:jo::Owner::null});}
    out.order=std::vector<std::uint8_t>(a[3].begin(),a[3].begin()+n);return out;
}
struct Context {
    std::vector<jp::Matrix> parent_joints;std::vector<std::uint8_t> mapping;
    jo::Parameters parameters;
    Context(const Arenas& a,const Bytes& identity):mapping(a[6].begin(),a[6].begin()+256){
        for(unsigned i=0;i<24;++i)parent_joints.push_back(words<16>(a[5],i*168+8));
        parameters.model=word(a[0],0xf0)?jp::ModelReference{jp::PresentModel{jp::Unknown{}}}:jp::ModelReference{jp::Null{}};
        if(word(a[0],0x30))parameters.parent=jo::Parent{word(a[4],4),words<16>(a[4],0xb0),jo::Getter::uplayer_d02258,std::span<const std::uint8_t>(mapping),std::span<const jp::Matrix>(parent_joints)};else parameters.parent=jo::Null{};
        parameters.reference_basis=words<12>(identity,0);parameters.reference_translation=words<4>(identity,48);
    }
};
Arenas encode(const Arenas& before,const jo::State& state,bool clear=false){
    auto result=before;write(result[0],0x70,state.local_rt);write(result[0],0xb0,state.world);write(result[0],0x60,state.scale);
    for(const auto [offset,value]:std::array<std::pair<std::size_t,std::uint32_t>,8>{{{4,state.flags4},{0x104,state.flags104},{0x230,state.flags230},{0x238,state.flags238},{0x34,state.attachment_index},{0x38,state.attachment_mode},{0xc4c,state.fieldc4c},{0x220,static_cast<std::uint32_t>(state.joints.size())}}})put(result[0],offset,value);
    put(result[0],0xc49,state.dirty,1);if(clear)put(result[0],0x30,0);
    for(unsigned i=0;i<state.joints.size();++i){const auto p=i*168;const auto& j=state.joints[i];encode_pose(result[1],p,j.pose);write(result[1],p+8,j.matrix);put(result[1],p+0x90,j.depth);put(result[1],p+0x94,j.metadata.flags94);put(result[1],p+0x98,j.metadata.mapping98);}
    if(const auto* order=std::get_if<std::vector<std::uint8_t>>(&state.order)){check(order->size()<=result[3].size(),"Output order extent");std::copy(order->begin(),order->end(),result[3].begin());}return result;
}
void reject(const jo::Result& result,jo::ErrorCode code){const auto* e=std::get_if<jo::Error>(&result);check(e&&e->code==code,"Expected output typed rejection");}
void negatives(const Arenas& a,const Bytes& identity){
    auto state=output_state(a,1);Context context(a,identity);auto p=context.parameters;
    auto rejected=[&](jo::ErrorCode code){const auto before=encode(a,state);const auto joints=state.joints;reject(jo::apply(state,p),code);same(encode(a,state),before,"Transactional rejection preserves all represented fields");check(state.joints==joints,"Rejection preserves callback/owner identities");};
    p.model=jp::Unknown{};rejected(jo::ErrorCode::unknown_model);
    p=context.parameters;p.parent=jo::Unknown{};rejected(jo::ErrorCode::unknown_parent);
    p=context.parameters;p.reference_translation.reset();rejected(jo::ErrorCode::missing_template);
    p=context.parameters;state.order=std::vector<std::uint8_t>{};rejected(jo::ErrorCode::order_extent);
    state=output_state(a,1);state.dirty=1;state.joints[0].metadata.flags94=0;rejected(jo::ErrorCode::cycle);
    state=output_state(a,1);state.flags238|=0x4000000;rejected(jo::ErrorCode::unsupported_selected_callback);
    auto t=transition(a,1,1);t.layers[0].flags=0x100;t.layers[0].field60=.5f;t.snapshots[0].layer_index=999;
    auto views=t.views();auto r=mt::apply(t.joints,t.metadata,t.snapshots,views,t.parameters);check(std::get<mt::Error>(r).code==mt::ErrorCode::layer_extent,"Consumed owner extent");
    t.metadata[0].flags94|=0x41;r=mt::apply(t.joints,t.metadata,t.snapshots,views,t.parameters);check(std::holds_alternative<mt::Output>(r),"Skipped owner not consumed");
    t.snapshots[0].layer_index=0xffffffff;r=mt::apply(t.joints,{},t.snapshots,views,t.parameters);check(std::holds_alternative<mt::Output>(r),"Sentinel skips absent metadata");
    r=mt::apply(t.joints,t.metadata,{},views,t.parameters);check(std::get<mt::Error>(r).code==mt::ErrorCode::snapshot_extent,"Consumed snapshot extent");
}
void oracle(const Bytes& bytes){
    Reader reader{bytes};check(reader.take(8)==Bytes({'M','H','4','U','J','O','0','1'}),"Fixture magic");const auto count=reader.integer();check(count>0&&count<=10000&&reader.integer()==7,"Fixture counts");
    for(unsigned i=0;i<7;++i)check(reader.integer()==addresses[i]&&reader.integer()==sizes[i],"Arena contract");
    unsigned errors=0,success=0;std::uint64_t blended=0,updated=0;bool tested=false;
    for(unsigned i=0;i<count;++i){try{
        const auto length=reader.integer();check(length==16+64+3*arena_bytes,"Record byte bound");const auto record=reader.take(length);Reader r{record};r.integer();const auto expected_error=r.integer(),n=r.integer(),layers=r.integer();check(expected_error<=1&&n<=48&&layers<=8,"Record counts");
        const auto identity=r.take(64);const auto before=r.arenas(),middle=r.arenas(),after=r.arenas();check(r.p==record.size(),"No trailing record");check(word(before[0],0x220)==n&&word(before[0],0x234)==layers,"Declared active counts");
        const auto t=transition(before,n,layers);const auto views=t.views();const auto result=mt::apply(t.joints,t.metadata,t.snapshots,views,t.parameters);
        if(expected_error){const auto* e=std::get_if<mt::Error>(&result);check(e&&e->code==mt::ErrorCode::arithmetic&&e->arithmetic&&e->arithmetic->code==math::ErrorCode::degenerate_quaternion,"Zero norm retained in ARM, rejected by native");check(e->component==mt::Component::rotation&&e->joint==n-1,"Consumed zero rotation location");check(t.joints==transition(before,n,layers).joints,"No partial transition published");++errors;continue;}
        const auto* transition_out=std::get_if<mt::Output>(&result);if(!transition_out)throw std::runtime_error("Transition: "+std::string(std::get<mt::Error>(result).message));
        const auto actual_middle=encode(before,*transition_out);same(actual_middle,middle,"After95b490");blended+=transition_out->blended_joints;
        arm_output_bytes+=arena_bytes;
        auto state=output_state(actual_middle,n);Context context(actual_middle,identity);const auto output=jo::apply(state,context.parameters);const auto* out=std::get_if<jo::Output>(&output);if(!out)throw std::runtime_error("Output: "+std::string(std::get<jo::Error>(output).message));
        check(out->state.joints.size()==state.joints.size(),"Canonical joint count unchanged");
        for(unsigned j=0;j<n;++j)check(out->state.joints[j].callback==state.joints[j].callback&&out->state.joints[j].owner==state.joints[j].owner,"Callback/owner identity preserved");
        same(encode(actual_middle,out->state,out->attachment_cleared),after,"After95b940");same(encode(actual_middle,state),actual_middle,"Borrowed output state unchanged");
        arm_output_bytes+=arena_bytes;
        check(t.joints==transition(before,n,layers).joints&&t.snapshots==transition(before,n,layers).snapshots,"Borrowed transition state unchanged");updated+=out->updated_joints;++success;
        if(!tested&&n==1&&layers==1&&!word(before[0],0x30)&&word(before[0],0xf0)){negatives(actual_middle,identity);tested=true;}
    }catch(const std::exception& e){throw std::runtime_error("Case "+std::to_string(i)+": "+e.what());}}
    check(reader.p==bytes.size()&&tested,"Complete fixture and negative coverage");std::cout<<"{\"status\":\"passed\",\"cases\":"<<count<<",\"bit_exact_cases\":"<<success<<",\"numeric_domain_rejections\":"<<errors<<",\"checks\":"<<checks<<",\"compared_bytes\":"<<compared_bytes<<",\"arm_output_bytes\":"<<arm_output_bytes<<",\"immutability_bytes\":"<<(compared_bytes-arm_output_bytes)<<",\"blended_joints\":"<<blended<<",\"updated_joints\":"<<updated<<"}\n";
}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("Usage: joint-output-resource-tests oracle.bin sha256");std::ifstream in(argv[1],std::ios::binary|std::ios::ate);check(bool(in),"Fixture open");const auto size=in.tellg();check(size>=0&&size<=128*1024*1024,"Fixture allocation bound");in.seekg(0);Bytes bytes(static_cast<std::size_t>(size));in.read(reinterpret_cast<char*>(bytes.data()),size);check(bool(in),"Fixture read");check(picosha2::hash256_hex_string(bytes)==argv[2],"Fixture SHA256 mismatch");oracle(bytes);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
