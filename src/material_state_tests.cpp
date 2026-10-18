#include "material_state.hpp"
#include "picosha2.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace {
namespace m=mh4u::material_state;using U=std::uint32_t;using Raw=std::vector<std::uint8_t>;
std::size_t checks{},cases{},compared{},copies{},refreshes{};
void check(bool b,const char* s){++checks;if(!b)throw std::runtime_error(s);}
template<class T,class V>const T& value(const V& v){if constexpr(std::is_same_v<T,m::Output>)if(const auto* e=std::get_if<m::Error>(&v)){std::cerr<<"native error "<<int(e->phase)<<'/'<<int(e->code)<<' '<<e->field<<'\n';}check(std::holds_alternative<T>(v),"Result kind");return std::get<T>(v);}
U get(const Raw& b,std::size_t o){check(o<=b.size()&&b.size()-o>=4,"Raw extent");return U(b[o])|(U(b[o+1])<<8)|(U(b[o+2])<<16)|(U(b[o+3])<<24);}
void put(Raw& b,std::size_t o,U v,unsigned n=4){check(o<=b.size()&&n<=b.size()-o,"Write extent");for(unsigned i=0;i<n;++i)b[o+i]=std::uint8_t(v>>(8*i));}
template<std::size_t N>void decode(m::Bytes<N>& a,const Raw& b,std::size_t o){check(o+N<=b.size(),"Slice extent");for(std::size_t i=0;i<N;++i)a[i]=b[o+i];}
template<std::size_t N>void encode(Raw& b,std::size_t o,const m::Bytes<N>& a){for(std::size_t i=0;i<N;++i){check(a[i].has_value(),"Output lost known byte");b.at(o+i)=*a[i];}}
U word(const m::Material& x,U o){U r=0;for(U k=0;k<4;++k){auto b=o<0x30?x.bytes04.at(o-4+k):o<0x70?x.bytes34.at(o-0x34+k):x.packed70.at(o-0x70+k);check(b.has_value(),"Expected known material word");r|=U(*b)<<(8*k);}return r;}
void mw(m::Material& x,U o,U v){for(U k=0;k<4;++k){auto& b=o<0x30?x.bytes04.at(o-4+k):o<0x70?x.bytes34.at(o-0x34+k):x.packed70.at(o-0x70+k);b=std::uint8_t(v>>(8*k));}}
void rw(m::Reference& x,U o,U v){for(U k=0;k<4;++k)x.bytes00.at(o+k)=std::uint8_t(v>>(8*k));}
constexpr std::array<U,15> addresses{0x2000000,0x2010000,0x2011000,0x2012000,0x2020000,0x2021000,0x2022000,0x2030000,0x2031000,0x2032000,0x2040000,0x2041000,0x2042000,0x2050000,0x2060000};
constexpr std::array<U,15> sizes{0x2000,0x100,0x100,8,0x108,0x108,0x108,48,48,48,176,176,176,0x1000,0x1000};
using Arenas=std::array<Raw,15>;
m::Link<m::MaterialIndex> matlink(U p){if(!p)return m::Absent{};check(p>=addresses[4]&&p<=addresses[6]&&(p-addresses[4])%0x1000==0,"Material identity/cross-pool alias");return m::MaterialIndex{(p-addresses[4])/0x1000};}
m::ReferenceLink reflink(U p){if(!p)return m::Absent{};if(p>=addresses[13]&&p<addresses[13]+0xa00&&(p-addresses[13])%0x100==0)return m::ReferenceIndex{(p-addresses[13])/0x100};if(p==addresses[14]||p==addresses[14]+0x100)return m::ReferenceIndex{10+(p-addresses[14])/0x100};throw std::runtime_error("Reference identity/cross-pool alias");}
U refptr(const m::ReferenceLink& l){if(std::holds_alternative<m::Absent>(l))return 0;auto i=std::get<m::ReferenceIndex>(l).value;check(i<12,"Reference output identity");return i<10?addresses[13]+0x100*i:addresses[14]+0x100*(i-10);}
struct Decoded {m::State state;m::Object object;m::Context context;};
Decoded project(const Arenas& a,const std::array<U,24>& meta){
 Decoded d{};auto& s=d.state;s.materials.resize(3);s.properties.resize(3);s.buffers.resize(3);s.references.resize(12);s.shader_tables.resize(2);s.resources.resize(1);s.descriptors.push_back({0x5fb0ebe4,m::Unknown{}});
 for(U i=0;i<3;++i){auto& mat=s.materials[i];auto& raw=a[4+i];check(get(raw,0)==0xe7ae6c,"Concrete MaterialStd binding");mat.binding=m::Binding::material_std;mat.descriptor=m::DescriptorIndex{0};decode(mat.bytes04,raw,4);decode(mat.bytes34,raw,0x34);decode(mat.packed70,raw,0x70);const auto p=get(raw,0x30);check(p>=addresses[7]&&p<=addresses[9]&&(p-addresses[7])%0x1000==0,"Property-table identity");mat.parameters=m::PropertyTableIndex{(p-addresses[7])/0x1000};mat.shader=reflink(get(raw,0x6c));}
 for(U i=0;i<3;++i){auto& table=s.properties[i];for(U j=0;j<4;++j){auto control=get(a[7+i],j*12),pointer=get(a[7+i],j*12+4);m::Property p{control,m::Unknown{},get(a[7+i],j*12+8)};if(!pointer)p.value=m::Absent{};else if((control&15)==1){const auto masked=pointer&~15U;check(masked>=addresses[10]&&masked<=addresses[12]&&(masked-addresses[10])%0x1000==0,"Buffer identity/cross-pool alias");p.value=m::BufferLocation{m::BufferIndex{(masked-addresses[10])/0x1000},0,std::uint8_t(pointer&15)};}else{auto l=reflink(pointer);p.value=std::get<m::ReferenceIndex>(l);}table.records.push_back(p);}}
 for(U i=0;i<3;++i)for(auto b:a[10+i])s.buffers[i].bytes.push_back(b);
 for(U i=0;i<12;++i){auto ai=i<10?13U:14U;auto off=(i<10?i:i-10)*0x100;decode(s.references[i].bytes00,a[ai],off);decode(s.references[i].bytes14,a[ai],off+0x14);if(i>=10){const auto ptr=get(a[ai],off+0x10);check(ptr==addresses[14]+0x300+0x100*(i-10),"Shader table identity");s.references[i].shader_table=m::ShaderTableIndex{i-10};}}
 for(U i=0;i<2;++i){auto base=0x300+0x100*i;auto& t=s.shader_tables[i];t.count=get(a[14],base);check(*t.count<=6,"Fixture shader extent");for(U j=0;j<*t.count;++j){auto p=get(a[14],base+4+4*j);check(p>=addresses[14]+0x600&&p+8<=addresses[14]+sizes[14],"Shader immutable entry extent");t.entry_flags.push_back(get(a[14],p-addresses[14]+4));}}
 auto& r=s.resources[0];check(get(a[1],0)==0xe6c988,"rMaterial binding");check(get(a[1],0x64)==addresses[2]&&get(a[1],0x68)==addresses[3],"Resource table identity");r.binding=m::ResourceBinding::rmaterial;r.count=get(a[2],8);r.materials.push_back(matlink(get(a[3],0)));
 d.object.banks.resize(meta[2]+1);auto rp=get(a[0],0xd80+4*meta[2]);check(rp==0||rp==addresses[1],"Bank identity");d.object.banks[meta[2]]=rp?m::Link<m::ResourceIndex>{m::ResourceIndex{0}}:m::Link<m::ResourceIndex>{m::Absent{}};
 d.object.destination_count=get(a[0],0xf8);check(get(a[0],0xf4)==addresses[0]+0x1000,"Destination array identity");for(U i=0;i<2;++i)d.object.destinations.push_back(matlink(get(a[0],0x1000+4*i)));d.object.flags104=get(a[0],0x104);for(U i=0;i<3;++i)d.object.caches[i]=reflink(get(a[0],0x18c+4*i));
 for(U i=0;i<4;++i)d.context.channel_templates[i]=meta[8+i];d.context.release_threshold=meta[12];d.context.retirement_delay=meta[13];d.context.buffer_word_count=meta[14];return d;
}
void readonly_output(const m::State& a,const m::State& b,const m::Object* o,const m::Object& prior){
 check(a.materials.size()==b.materials.size()&&a.references.size()==b.references.size()&&a.properties.size()==b.properties.size()&&a.buffers.size()==b.buffers.size(),"Output pool extents changed");
 check(a.descriptors==b.descriptors&&a.resources==b.resources&&a.shader_tables==b.shader_tables,"Readonly provider facts changed");
 for(std::size_t i=0;i<a.materials.size();++i)check(a.materials[i].binding==b.materials[i].binding&&a.materials[i].descriptor==b.materials[i].descriptor,"Material concrete identity changed");
 for(std::size_t i=0;i<a.references.size();++i)check(a.references[i].shader_table==b.references[i].shader_table,"Shader table identity changed");
 for(std::size_t i=0;i<a.properties.size();++i)check(a.properties[i].records.size()==b.properties[i].records.size(),"Property extent changed");
 for(std::size_t i=0;i<a.buffers.size();++i)check(a.buffers[i].bytes.size()==b.buffers[i].bytes.size(),"Buffer extent changed");
 if(o)check(o->banks==prior.banks&&o->destinations==prior.destinations&&o->destination_count==prior.destination_count,"Object resource/destination identity changed");
}
Arenas serialize(const Arenas& original,const m::State& s,const m::Object* object){auto a=original;
 for(U i=0;i<3;++i){const auto& mat=s.materials[i];auto& raw=a[4+i];encode(raw,4,mat.bytes04);encode(raw,0x34,mat.bytes34);encode(raw,0x70,mat.packed70);put(raw,0x6c,refptr(mat.shader));put(raw,0x30,addresses[7]+std::get<m::PropertyTableIndex>(mat.parameters).value*0x1000);}
 for(U i=0;i<3;++i){for(U j=0;j<4;++j){const auto& p=s.properties[i].records[j];check(p.control&&p.word8,"Property knownness");put(a[7+i],j*12,*p.control);U pointer=0;if(auto ref=std::get_if<m::ReferenceIndex>(&p.value))pointer=refptr(*ref);else if(auto b=std::get_if<m::BufferLocation>(&p.value))pointer=addresses[10]+0x1000*b->buffer.value+b->offset+b->tag;else check(std::holds_alternative<m::Absent>(p.value),"Property output kind");put(a[7+i],j*12+4,pointer);put(a[7+i],j*12+8,*p.word8);}for(std::size_t j=0;j<s.buffers[i].bytes.size();++j){check(s.buffers[i].bytes[j].has_value(),"Buffer knownness");a[10+i][j]=*s.buffers[i].bytes[j];}}
 for(U i=0;i<12;++i){auto ai=i<10?13U:14U;auto off=(i<10?i:i-10)*0x100;encode(a[ai],off,s.references[i].bytes00);encode(a[ai],off+0x14,s.references[i].bytes14);}
 if(object){check(object->flags104.has_value(),"Object flags knownness");put(a[0],0x104,*object->flags104);for(U i=0;i<3;++i)put(a[0],0x18c+4*i,refptr(object->caches[i]));}
 return a;
}
struct Reader {Raw data;std::size_t pos{};explicit Reader(const char* path,const char* hash){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"Fixture open");auto n=f.tellg();check(n>=0&&n<=16*1024*1024,"Fixture size");data.resize(static_cast<std::size_t>(n));f.seekg(0);f.read(reinterpret_cast<char*>(data.data()),n);check(bool(f)&&f.peek()==std::char_traits<char>::eof(),"Fixture exact read");check(picosha2::hash256_hex_string(data)==hash,"Fixture SHA256");}U u(){auto x=get(data,pos);pos+=4;return x;}Raw raw(std::size_t n){check(pos<=data.size()&&n<=data.size()-pos,"Fixture truncated");Raw b(data.begin()+pos,data.begin()+pos+n);pos+=n;return b;}};
void oracle(const char* path,const char* hash){Reader r(path,hash);check(r.raw(8)==Raw({'M','H','4','U','M','T','0','1'}),"Fixture magic");check(r.u()==15&&r.u()==100,"Fixture counts");for(U i=0;i<15;++i){check(r.u()==addresses[i]&&r.u()==sizes[i],"Arena schema");for(U j=0;j<i;++j)check(addresses[i]>=addresses[j]+sizes[j]||addresses[j]>=addresses[i]+sizes[i],"Cross-pool arena overlap");}
 for(U i=0;i<100;++i){std::array<U,24> meta{};for(auto& x:meta)x=r.u();check(meta[0]==i&&meta[1]<=1&&meta[2]<16,"Case header");Arenas before,after;for(U j=0;j<15;++j)before[j]=r.raw(sizes[j]);for(U j=0;j<15;++j)after[j]=r.raw(sizes[j]);auto d=project(before,meta);const auto old=d;auto result=meta[1]?m::set_channels(d.state,m::MaterialIndex{1},{meta[4],meta[5],meta[6],meta[7]},d.context):m::apply_bank(d.state,d.object,meta[2],meta[3],d.context);if(!std::holds_alternative<m::Output>(result))std::cerr<<"case "<<i<<'\n';const auto& out=value<m::Output>(result);check(d.state==old.state&&d.object==old.object,"Source mutated");readonly_output(out.state,old.state,meta[1]?nullptr:&out.object,old.object);auto encoded=serialize(before,out.state,meta[1]?nullptr:&out.object);for(U j=0;j<15;++j){for(U k=0;k<sizes[j];++k){if(encoded[j][k]!=after[j][k]){std::cerr<<"case "<<i<<" arena "<<j<<" offset "<<std::hex<<k<<" got "<<U(encoded[j][k])<<" expected "<<U(after[j][k])<<std::dec<<'\n';throw std::runtime_error("Original arena differs");}++checks;++compared;}}
 check(out.effects.refreshes==meta[17]&&out.effects.shader_rebindings==meta[18]&&out.effects.property_setters==meta[19]&&out.effects.releases==meta[20]&&out.effects.acquires==meta[21]&&out.effects.copies==meta[22],"Original helper counts differ");copies+=out.effects.copies;refreshes+=out.effects.refreshes;++cases;
 }
 check(r.pos==r.data.size(),"Fixture trailing bytes");}
void boundaries();
} // namespace
int main(int argc,char** argv){try{if(argc!=1&&argc!=3)throw std::runtime_error("Expected fixture and SHA256 or no arguments");boundaries();if(argc==3)oracle(argv[1],argv[2]);std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"compared_bytes\":"<<compared<<",\"copies\":"<<copies<<",\"refreshes\":"<<refreshes<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
namespace {
Decoded fixture(){Decoded d{};auto& s=d.state;s.materials.resize(2);s.descriptors.push_back({7,m::Unknown{}});s.properties.resize(2);s.buffers.resize(2);s.references.resize(7);s.shader_tables.push_back({6,{2,0,2,2,2,2}});s.resources.push_back({m::ResourceBinding::rmaterial,1,{m::MaterialIndex{0}}});
 for(U i=0;i<2;++i){auto& mat=s.materials[i];mat.binding=m::Binding::material_std;mat.descriptor=m::DescriptorIndex{0};mat.bytes04.fill(0xa5);mat.bytes34.fill(0xa5);mat.packed70.fill(0xa5);mw(mat,4,101);mw(mat,0x18,4|(3U<<22));mat.parameters=m::PropertyTableIndex{i};mat.shader=m::ReferenceIndex{4+i};s.buffers[i].bytes.resize(176,std::uint8_t{0});auto& t=s.properties[i].records;
 for(U j=0;j<4;++j){U h=std::array<U,4>{0x5dec11ae,0x0b9981aa,0x16a5b126,0x226601a2}[j];m::Property p{((h&4095)<<20)|(j==2?1U:3U),m::Absent{},h};if(j==2)p.value=m::BufferLocation{m::BufferIndex{i},0,0};else p.value=m::ReferenceIndex{j==0?i:j==1?2+i:i};t.push_back(p);}}
 for(auto& ref:s.references){ref.bytes00.fill(0);ref.bytes14.fill(0);rw(ref,8,20);rw(ref,4,1);rw(ref,12,17);ref.shader_table=m::ShaderTableIndex{0};}
 d.object.banks={m::ResourceIndex{0}};d.object.destination_count=1;d.object.destinations={m::MaterialIndex{1}};d.object.flags104=0x1234567;d.object.caches={m::ReferenceIndex{6},m::ReferenceIndex{6},m::ReferenceIndex{6}};d.context.channel_templates.fill(0x3f800000);d.context.release_threshold=100;d.context.retirement_delay=7;d.context.buffer_word_count=44;return d;}
m::Output run(const Decoded& d,U exclusion=0xffffffff,m::Limits limits={}){auto before=d;auto r=m::apply_bank(d.state,d.object,0,exclusion,d.context,limits);check(d.state==before.state&&d.object==before.object,"Success source mutated");return value<m::Output>(r);}
void rejected(const Decoded& d,m::Phase phase,m::ErrorCode code,m::Limits limits={}){auto before=d;auto r=m::apply_bank(d.state,d.object,0,0xffffffff,d.context,limits);check(std::holds_alternative<m::Error>(r),"Expected typed failure");const auto& e=std::get<m::Error>(r);check(e.phase==phase&&e.code==code,"Failure phase/code");check(d.state==before.state&&d.object==before.object,"Failure published candidate");}
void boundaries(){
 auto d=fixture();auto o=run(d);check(o.effects.releases==6&&o.effects.acquires==3&&o.effects.refreshes==1&&o.effects.cache_clears==3,"Basic effects");check(o.object.flags104==0x11234567,"Final dirty bit");check(word(o.state.materials[1],0x48)==0&&word(o.state.materials[1],0x50)==2&&word(o.state.materials[1],0x58)==3&&word(o.state.materials[1],0x60)==4,"Shader selection/order/width");
 d=fixture();d.object.banks[0]=m::Absent{};d.object.destination_count.reset();d.object.flags104.reset();d.context={};o=run(d);check(o.state==d.state&&o.object==d.object&&!o.effects.resource_gate,"Null gate lazy fields");
 d=fixture();d.state.resources[0].count=0;d.state.resources[0].materials.clear();d.object.destination_count.reset();o=run(d);check(!o.effects.resource_gate,"Zero resource gate");
 d=fixture();d.object.destination_count=0;d.object.destinations.clear();d.state.resources[0].materials[0]=m::Unknown{};o=run(d);check(o.effects.cache_clears==3&&o.effects.destinations==0,"Empty destinations still clear caches");
 d=fixture();o=run(d,3);check(o.effects.compatible==0&&o.effects.cache_clears==3,"Exclusion full integer equality");o=run(d,0x103);check(o.effects.compatible==1,"Exclusion must not narrow");
 d=fixture();d.object.destination_count=2;d.object.destinations.push_back(m::MaterialIndex{1});o=run(d);check(o.effects.shader_rebindings==2&&o.effects.acquires==3&&o.effects.releases==6&&o.effects.copies==2,"Repeated destination identity/refcounts");
 d=fixture();d.object.destinations[0]=m::MaterialIndex{0};o=run(d);check(o.effects.shader_rebindings==1&&o.effects.acquires==0&&o.effects.releases==3&&o.effects.copies==1,"Source/destination alias");
 d=fixture();d.state.materials[1].parameters=d.state.materials[0].parameters;o=run(d);check(o.effects.property_setters==2&&o.effects.acquires==1,"Shared property-table identity");
 d=fixture();d.state.materials[1].shader=d.state.materials[0].shader;o=run(d);check(o.effects.shader_rebindings==1&&o.effects.acquires==2&&word(o.state.materials[1],0x48)==0,"Equal shader still resets slots");
 d=fixture();d.state.materials[0].shader=m::Absent{};o=run(d);check(o.effects.shader_rebindings==0&&word(o.state.materials[1],0x48)==0xa5a5a5a5,"Null source shader preserves slots");
 d=fixture();d.state.materials[0].bytes04[0x18-4].reset();rejected(d,m::Phase::property,m::ErrorCode::unknown);
 d=fixture();d.object.banks[0]=m::Unknown{};rejected(d,m::Phase::resource,m::ErrorCode::unknown);
 d=fixture();d.state.resources[0].binding.reset();rejected(d,m::Phase::resource,m::ErrorCode::unknown);
 d=fixture();d.state.resources[0].binding=static_cast<m::ResourceBinding>(42);rejected(d,m::Phase::resource,m::ErrorCode::unsupported_binding);
 d=fixture();d.object.destination_count=2;rejected(d,m::Phase::destination,m::ErrorCode::extent);
 d=fixture();d.state.materials[1].binding.reset();rejected(d,m::Phase::descriptor,m::ErrorCode::unknown);
 d=fixture();d.state.descriptors.push_back({8,m::DescriptorIndex{1}});d.state.materials[1].descriptor=m::DescriptorIndex{1};rejected(d,m::Phase::descriptor,m::ErrorCode::descriptor_cycle);
 d.state.descriptors[1].parent=m::Absent{};o=run(d);check(o.effects.compatible==0,"Known descriptor mismatch");d.state.descriptors[1].parent=m::DescriptorIndex{0};o=run(d);check(o.effects.compatible==1,"Descriptor parent compatibility");
 d=fixture();d.state.descriptors.push_back({8,m::DescriptorIndex{99}});d.state.materials[1].descriptor=m::DescriptorIndex{1};rejected(d,m::Phase::descriptor,m::ErrorCode::extent);
 d=fixture();rw(d.state.references[5],8,1);rejected(d,m::Phase::release,m::ErrorCode::unsupported_destructor);rw(d.state.references[5],4,100);rejected(d,m::Phase::release,m::ErrorCode::unsupported_retirement);
 d=fixture();rw(d.state.references[6],8,1);rejected(d,m::Phase::release,m::ErrorCode::unsupported_destructor);
 d=fixture();rw(d.state.references[5],8,0);rw(d.state.references[4],8,0xffffffff);o=run(d);check(o.state.references[5].bytes00[8]==255&&o.state.references[4].bytes00[8]==0,"Reference modulo32 arithmetic");
 d=fixture();d.context.release_threshold=0xffffffff;d.context.retirement_delay=7;o=run(d);check(o.state.references[1].bytes00[4]==6,"Timestamp signed compare and wrap");
 d=fixture();d.state.shader_tables[0].entry_flags[5].reset();rejected(d,m::Phase::shader,m::ErrorCode::unknown); // Still read after four selected slots.
 d=fixture();d.state.buffers[1].bytes.resize(160);rejected(d,m::Phase::copy,m::ErrorCode::extent);
 d=fixture();d.context.buffer_word_count=0;d.state.buffers[1].bytes.resize(160);rejected(d,m::Phase::refresh,m::ErrorCode::extent);
 d=fixture();d.context.buffer_word_count=0x1002c;o=run(d);check(o.effects.copied_bytes==176,"MFX count low16");
 d=fixture();d.context.buffer_word_count.reset();d.state.properties[0].records[2].value=m::Absent{};o=run(d);check(o.effects.copies==0,"Missing type1 skips MFX count");
 d=fixture();d.state.buffers[0].bytes.resize(192,std::uint8_t{0});d.state.properties[1].records[2].value=m::BufferLocation{m::BufferIndex{0},16,0};rejected(d,m::Phase::copy,m::ErrorCode::unsupported_overlap);
 d=fixture();d.state.properties[0].records[2].value=m::BufferLocation{m::BufferIndex{0},0,15};o=run(d);check(o.effects.copies==1,"Type1 tag is masked");
 d=fixture();d.state.materials[0].bytes34[0].reset();d.state.references[0].bytes14[0].reset();o=run(d);check(!o.state.materials[0].bytes34[0]&&!o.state.references[0].bytes14[0],"Unconsumed unknown bytes preserved");
 d=fixture();rejected(d,m::Phase::capacity,m::ErrorCode::capacity,m::Limits{0,100});rejected(d,m::Phase::capacity,m::ErrorCode::capacity,m::Limits{1U<<20,0});
 d=fixture();auto old=d.state;auto c=m::set_channels(d.state,m::MaterialIndex{1},{0x7f812345,0xbf800000,0x3f000000,0x7f800000},d.context);const auto& color=value<m::Output>(c);check(word(color.state.materials[1],0x88)==0xff7f00ff,"Color HS NaN and RTZ low bytes");check(d.state==old,"Color input immutable");
 auto& bp=color.state.buffers[1].bytes;check(bp[0x50]==0x45&&bp[0x53]==0x7f,"Raw sNaN copied before clamp");
 d.state.properties[1].records[2].value=m::Absent{};c=m::set_channels(d.state,m::MaterialIndex{1},{},{},{});check(value<m::Output>(c).state==d.state,"Absent type1 ignores channels/templates");
 d=fixture();d.context.channel_templates[2].reset();auto r=m::refresh(d.state,m::MaterialIndex{1},d.context);check(std::get<m::Error>(r).code==m::ErrorCode::unknown,"Refresh template unknown");

 // Distinct analytic controls for enum switch arms not covered by the four real
 // MRL buffers. Expected low lanes are literal source-case assignments.
 for(U e=0;e<13;++e){d=fixture();for(U k=0;k<4;++k)d.state.buffers[1].bytes[0x8c+k]=std::uint8_t(e>>(8*k));auto rr=m::refresh(d.state,m::MaterialIndex{1},d.context);const auto& mm=value<m::Output>(rr).state.materials[1];const std::array<U,13> expected{1,4,4,1,2,4,4,1,2,1,4,0,0};check((word(mm,0x84)&15)==expected[e],"First enum source switch");}
 for(U e=0;e<10;++e){d=fixture();for(U k=0;k<4;++k)d.state.buffers[1].bytes[0x94+k]=std::uint8_t(e>>(8*k));auto rr=m::refresh(d.state,m::MaterialIndex{1},d.context);const auto& mm=value<m::Output>(rr).state.materials[1];const std::array<U,10> expected{0,2,1,2,1,1,1,1,0,0};check(((word(mm,0x84)>>16)&15)==expected[e],"Second enum source switch");}
 for(U first:{0U,1U,2U,3U,4U,255U})for(U second=0;second<11;++second)for(U enable:{0U,1U}){d=fixture();for(U k=0;k<4;++k){d.state.buffers[1].bytes[0x98+k]=std::uint8_t(first>>(8*k));d.state.buffers[1].bytes[0x9c+k]=std::uint8_t(second>>(8*k));d.state.buffers[1].bytes[0xa0+k]=std::uint8_t(enable>>(8*k));}auto rr=m::refresh(d.state,m::MaterialIndex{1},d.context);const auto& mm=value<m::Output>(rr).state.materials[1];check((word(mm,0xd4)&15)==(first==0?((second==0||second>8)?1U:9U):(second>=1&&second<=3?(enable?9U:8U):1U)),"Third enum output width");}
 d=fixture();d.state.shader_tables[0].count=65537;d.state.shader_tables[0].entry_flags.assign(65537,0);d.state.shader_tables[0].entry_flags.back()=2;o=run(d);check(word(o.state.materials[1],0x48)==0&&word(o.state.materials[1],0x50)==65535,"Shader index narrows to halfword");
 d=fixture();const auto previous=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"Set rounding");auto rounded=m::set_channels(d.state,m::MaterialIndex{1},{0,0,0,0},d.context);std::fesetround(previous);check(std::holds_alternative<m::Error>(rounded)&&std::get<m::Error>(rounded).arithmetic.has_value(),"Rounding environment rejection");
#if defined(__aarch64__)
 std::uint64_t oldfpcr;asm volatile("mrs %0, fpcr":"=r"(oldfpcr));auto next=oldfpcr|(1ULL<<24);asm volatile("msr fpcr, %0"::"r"(next));auto fz=m::set_channels(d.state,m::MaterialIndex{1},{0,0,0,0},d.context);asm volatile("msr fpcr, %0"::"r"(oldfpcr));check(std::holds_alternative<m::Error>(fz),"Actual FPCR.FZ rejection");
#elif defined(__SSE__)
 auto oldcsr=_mm_getcsr();_mm_setcsr(oldcsr|(1U<<15)|(1U<<6));auto fz=m::set_channels(d.state,m::MaterialIndex{1},{0,0,0,0},d.context);_mm_setcsr(oldcsr);check(std::holds_alternative<m::Error>(fz),"Actual FTZ/DAZ rejection");
#endif
}
}
