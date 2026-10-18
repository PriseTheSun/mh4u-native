#include "motion_clock.hpp"
#include "motion_resource.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <stdexcept>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

namespace {
namespace mc=mh4u::motion_clock;namespace mp=mh4u::motion_pending;namespace mr=mh4u::motion_request;namespace resource=mh4u::motion_resource;namespace math=mh4u::motion_transform_math;
std::size_t checks=0;
void check(bool value,const std::string& message){++checks;if(!value)throw std::runtime_error(message);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}float value(std::uint32_t b){return std::bit_cast<float>(b);}
using Raw=std::array<std::uint8_t,304>;
std::uint32_t u32(const Raw& b,std::size_t p){return std::uint32_t(b[p])|(std::uint32_t(b[p+1])<<8)|(std::uint32_t(b[p+2])<<16)|(std::uint32_t(b[p+3])<<24);}
std::uint16_t u16(const Raw& b,std::size_t p){return std::uint16_t(b[p]|(unsigned(b[p+1])<<8));}
void put(Raw& b,std::size_t p,std::uint32_t v,unsigned n=4){for(unsigned j=0;j<n;++j)b[p+j]=std::uint8_t(v>>(j*8));}
mp::Layer decode(const Raw& b){
 mp::Layer p;auto& s=p.animation;auto& l=s.layer;l.selector=u16(b,4);l.flags=u16(b,6);l.field08=u16(b,8);l.attributes=u32(b,12);
 for(auto [off,target]:std::initializer_list<std::pair<std::size_t,float*>>{{0x10,&l.requested_frame},{0x14,&l.offset},{0x18,&l.previous_step},{0x40,&l.interpolation},{0x44,&l.remaining_interpolation},{0x48,&l.weight},{0x4c,&l.frame},{0x50,&l.previous_frame},{0x54,&l.end_frame},{0x58,&l.field58},{0x5c,&l.speed},{0x60,&l.field60},{0x64,&l.prepared_frame},{0x68,&l.prepared_next_frame}})*target=value(u32(b,off));
 for(auto [off,target]:std::initializer_list<std::pair<std::size_t,mp::Bits4*>>{{0x20,&s.prediction_rotation},{0x30,&s.prediction_translation},{0x78,&s.previous_rotation},{0x88,&s.base_rotation},{0x98,&p.rotation_98},{0xa8,&s.previous_translation},{0xb8,&s.base_translation},{0xc8,&p.translation_c8}})for(unsigned j=0;j<4;++j)(*target)[j]=u32(b,off+4*j);
 for(unsigned j=0;j<2;++j)l.interpolation_curve_bits[j]=u32(b,0x6c+4*j);
 for(unsigned j=0;j<4;++j){l.masks[j]=u32(b,0x108+4*j);l.previous_masks[j]=u32(b,0x118+4*j);}return p;
}
Raw encode(const mp::Layer& p,Raw b){
 const auto& s=p.animation;const auto& l=s.layer;put(b,4,l.selector,2);put(b,6,l.flags,2);put(b,8,l.field08,2);put(b,12,l.attributes);
 for(auto [off,x]:std::initializer_list<std::pair<std::size_t,float>>{{0x10,l.requested_frame},{0x14,l.offset},{0x18,l.previous_step},{0x40,l.interpolation},{0x44,l.remaining_interpolation},{0x48,l.weight},{0x4c,l.frame},{0x50,l.previous_frame},{0x54,l.end_frame},{0x58,l.field58},{0x5c,l.speed},{0x60,l.field60},{0x64,l.prepared_frame},{0x68,l.prepared_next_frame}})put(b,off,bits(x));
 for(auto [off,target]:std::initializer_list<std::pair<std::size_t,const mp::Bits4*>>{{0x20,&s.prediction_rotation},{0x30,&s.prediction_translation},{0x78,&s.previous_rotation},{0x88,&s.base_rotation},{0x98,&p.rotation_98},{0xa8,&s.previous_translation},{0xb8,&s.base_translation},{0xc8,&p.translation_c8}})for(unsigned j=0;j<4;++j)put(b,off+4*j,(*target)[j]);
 for(unsigned j=0;j<2;++j)put(b,0x6c+4*j,l.interpolation_curve_bits[j]);for(unsigned j=0;j<4;++j){put(b,0x108+4*j,l.masks[j]);put(b,0x118+4*j,l.previous_masks[j]);}return b;
}
struct Reader {
 std::vector<std::uint8_t>b;std::size_t pos{};
 explicit Reader(const char* path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("fixture open");auto n=f.tellg();if(n<12||n>128*1024*1024)throw std::runtime_error("fixture size");b.resize(std::size_t(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),std::streamsize(n))||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("fixture read changed");}
 void extent(std::size_t n){if(n>b.size()-pos)throw std::runtime_error("truncated fixture");}
 std::uint32_t word(){extent(4);std::uint32_t v=0;for(unsigned j=0;j<4;++j)v|=std::uint32_t(b[pos++])<<(8*j);return v;}
 Raw raw(){extent(304);Raw v;std::copy_n(b.begin()+std::ptrdiff_t(pos),304,v.begin());pos+=304;return v;}
 mp::Snapshot snap(){mp::Snapshot s;s.layer_index=word();s.flags=word();for(auto* x:{&s.rotation,&s.translation,&s.scale})for(auto& v:*x)v=word();return s;}
};
struct Resolver {
 std::vector<mc::EventGroup> views;
 static mc::EventResolution resolve(const void* raw,const mr::Binding& b){const auto& self=*static_cast<const Resolver*>(raw);for(const auto& view:self.views)if(view.identity==b)return view;return mc::ResolutionError{71,"Missing exact binding"};}
 mc::EventResolver api()const{return{this,resolve};}
};
std::size_t oracle(const char* fixture,const std::map<std::uint32_t,resource::Resource>& resources){
 Reader r(fixture);check(std::memcmp(r.b.data(),"M4CLK001",8)==0,"fixture magic");r.pos=8;const auto count=r.word();check(count<=10000,"fixture case bound");std::size_t real=0;
 for(std::uint32_t ci=0;ci<count;++ci){
  (void)r.word();const auto nl=r.word(),nj=r.word(),kind=r.word(),ns=r.word();mc::Parameters params{r.word(),{}};const auto has=r.word(),error=r.word(),reserved=r.word();check(nl<=8&&nj<=256&&ns<=256&&kind<=2&&has<=1&&!error&&!reserved,"fixture metadata");mp::Bits4 fallback;for(auto& x:fallback)x=r.word();if(has)params.inverse_fallback_10e6570=fallback;
  std::vector<Raw> before,after;std::vector<mp::Layer> layers;Resolver resolver;std::vector<std::vector<mh4u::motion_events::Record>> owned;owned.reserve(nl*4);
  for(std::uint32_t li=0;li<nl;++li){
   const auto entry=r.word(),slot=r.word();before.push_back(r.raw());after.push_back(r.raw());layers.push_back(decode(before.back()));const resource::Resource* source=nullptr;if(entry){auto it=resources.find(entry);check(it!=resources.end(),"real source provided");source=&it->second;++real;}
   std::optional<std::array<resource::EventGroupView,4>> source_groups;if(source){auto v=source->event_groups(slot);check(std::holds_alternative<decltype(source_groups)>(v),"real events parsed");source_groups=std::get<decltype(source_groups)>(v);check(source_groups.has_value(),"real events present");}
   for(std::uint32_t gi=0;gi<4;++gi){const auto n=r.word();if(n==0xffffffffU){check(!u32(before.back(),0xf8+4*gi),"null binding source");continue;}check(n<=250000&&u32(before.back(),0xf8+4*gi),"group bound/presence");owned.emplace_back();auto& records=owned.back();records.reserve(n);for(std::uint32_t k=0;k<n;++k)records.push_back({r.word(),r.word()});
    const mr::Binding id{source?source->identity():0x1000000U+li,0,slot,gi};layers.back().animation.layer.bindings[gi]=id;
    if(source){const auto actual=(*source_groups)[gi].records;check(actual.size()==records.size()&&std::equal(actual.begin(),actual.end(),records.begin()),"canonical Resource records vs original fixture");resolver.views.push_back({id,actual});}else resolver.views.push_back({id,records});
   }
  }
  std::vector<mp::Snapshot> snaps,expected;for(std::uint32_t j=0;j<ns;++j)snaps.push_back(r.snap());for(std::uint32_t j=0;j<ns;++j)expected.push_back(r.snap());
  mc::Snapshots table{nj,mc::Unknown{}};if(kind==1)table.table=mc::Null{};else if(kind==2)table.table=std::span<const mp::Snapshot>(snaps);
  const auto snapshot_copy=snaps;const auto result=mc::advance(layers,table,params,resolver.api());if(const auto* e=std::get_if<mc::Error>(&result))throw std::runtime_error("ARM case "+std::to_string(ci)+" error "+std::to_string(int(e->code))+" layer "+std::to_string(e->layer)+": "+std::string(e->message));
  const auto& out=std::get<mc::Output>(result);check(out.layers.size()==layers.size(),"layer count");
  for(std::size_t li=0;li<layers.size();++li){const auto actual=encode(out.layers[li],before[li]);for(std::size_t j=0;j<304;++j)check(actual[j]==after[li][j],"ARM case "+std::to_string(ci)+" layer "+std::to_string(li)+" byte "+std::to_string(j)+" expected "+std::to_string(after[li][j])+" actual "+std::to_string(actual[j]));check(encode(layers[li],before[li])==before[li],"immutable input layer");check(out.layers[li].animation.layer.bindings==layers[li].animation.layer.bindings,"binding preserved");}
  check(snaps==snapshot_copy,"immutable snapshots");if(kind==2)check(std::get<std::vector<mp::Snapshot>>(out.snapshots)==expected,"original snapshot owners/complete raw fields");else check(kind==0?std::holds_alternative<mc::Unknown>(out.snapshots):std::holds_alternative<mc::Null>(out.snapshots),"snapshot unknown/null preserved");
 }
 check(r.pos==r.b.size(),"fixture trailing bytes");check(real==732,"real source case count");return count;
}
mp::Layer initial(){mp::Layer p;auto& l=p.animation.layer;l.selector=3;l.speed=1;l.frame=3;l.end_frame=10;l.field58=2;p.animation.base_rotation={0,0,0,bits(1)};p.animation.previous_rotation=p.animation.base_rotation;return p;}
void boundaries(){
 auto p=initial();mc::Snapshots s{0,mc::Unknown{}};mc::Parameters params{bits(1),{}};mc::Limits limits;Resolver resolver;
 auto call=[&]{return mc::advance(std::span(&p,1),s,params,resolver.api(),limits);};
 auto good=[&]{check(std::holds_alternative<mc::Output>(call()),"boundary success");};
 auto bad=[&](mc::ErrorCode code){auto r=call();check(std::holds_alternative<mc::Error>(r),"boundary error presence");check(std::get<mc::Error>(r).code==code,"boundary error kind "+std::to_string(int(code)));};
 good();p.animation.layer.prepared_frame=value(0x7fc12345);p.animation.layer.prepared_next_frame=value(0xff800000);p.rotation_98={0x7fc12345,0xff800000,0,0};good();
 p.animation.layer.speed=value(0x7f800000);bad(mc::ErrorCode::nonfinite_operand);p=initial();params.delta_time_bits=0x7fc12345;bad(mc::ErrorCode::nonfinite_operand);params.delta_time_bits=bits(1);
 p.animation.layer.speed=std::numeric_limits<float>::max();params.delta_time_bits=bits(2);bad(mc::ErrorCode::numeric_overflow);params.delta_time_bits=bits(1);p=initial();p.animation.layer.frame=2147483648.F;bad(mc::ErrorCode::frame_domain);
 p=initial();p.animation.layer.flags=0x100;p.animation.layer.remaining_interpolation=.5F;s.joint_count=1;bad(mc::ErrorCode::unknown_snapshots);s.table=mc::Null{};bad(mc::ErrorCode::null_snapshots);std::vector<mp::Snapshot> snapshots;s.table=std::span<const mp::Snapshot>(snapshots);bad(mc::ErrorCode::snapshot_extent);s.joint_count=0;good();
 p=initial();s={0,mc::Unknown{}};limits.layers=0;bad(mc::ErrorCode::resource_limit);limits={};p.animation.layer.frame=50;limits.loop_iterations=1;bad(mc::ErrorCode::resource_limit);limits={};
 p=initial();p.animation.layer.frame=9.5F;p.animation.base_rotation={0,0,0,0};bad(mc::ErrorCode::missing_template);params.inverse_fallback_10e6570=mp::Bits4{0,0,0,bits(1)};good();params.inverse_fallback_10e6570.reset();
 p=initial();p.animation.layer.frame=-2;p.animation.layer.speed=-1;p.animation.layer.field58=10;bad(mc::ErrorCode::nonprogressing_loop);
 p=initial();p.animation.layer.bindings[0]=mr::Binding{42,0,3,0};bad(mc::ErrorCode::unresolved_events);auto unresolved=call();check(std::get<mc::Error>(unresolved).binding==p.animation.layer.bindings[0],"resolution preserves exact binding");check(std::get<mc::Error>(unresolved).resolution->code==71,"resolver reason preserved");
 std::vector<mh4u::motion_events::Record> records{{1,2},{2,3}};resolver.views.push_back({*p.animation.layer.bindings[0],records});good();limits.event_records=1;bad(mc::ErrorCode::resource_limit);limits={};limits.event_work=1;bad(mc::ErrorCode::resource_limit);limits={};records[0].duration=0x80000000;bad(mc::ErrorCode::event_accumulator);records[0].duration=2;
 p.animation.layer.attributes=0x10;resolver.views.clear();good();p.animation.layer.attributes=0;p.animation.layer.selector=0xffff;good();
 p=initial();int old=std::fegetround();check(std::fesetround(FE_UPWARD)==0,"set rounding");bad(mc::ErrorCode::floating_environment);auto empty=mc::advance({},s,{0x7fc12345,{}},{},{});check(std::holds_alternative<mc::Output>(empty),"zero layers ignores floating environment/dt");check(std::fesetround(old)==0,"restore rounding");
 // Resolver errors retain exact source identity; foreign and absent are distinct.
 p=initial();p.animation.layer.bindings[0]=mr::Binding{42,0,3,0};
 auto missing=mc::advance(std::span(&p,1),s,params,{});check(std::get<mc::Error>(missing).code==mc::ErrorCode::unresolved_events&&std::get<mc::Error>(missing).binding==p.animation.layer.bindings[0],"missing resolver identity");
 mc::EventResolver foreign{nullptr,[](const void*,const mr::Binding& b)->mc::EventResolution{auto id=b;++id.slot;return mc::EventGroup{id,{}};}};
 auto mismatch=mc::advance(std::span(&p,1),s,params,foreign);check(std::get<mc::Error>(mismatch).code==mc::ErrorCode::event_identity,"foreign binding rejected");
 p=initial();p.animation.tracks.joints.resize(2);limits.bindings_per_layer=1;bad(mc::ErrorCode::resource_limit);limits={};p=initial();snapshots.resize(2);s.table=std::span<const mp::Snapshot>(snapshots);limits.snapshots=1;bad(mc::ErrorCode::resource_limit);limits={};s={0,mc::Unknown{}};
 // Consumed environment is rejected without changing process floating policy.
#if defined(__aarch64__)
 std::uint64_t fpcr;asm volatile("mrs %0, fpcr":"=r"(fpcr));const auto fz=fpcr|(1ULL<<24);asm volatile("msr fpcr, %0"::"r"(fz));auto flushed=call();asm volatile("msr fpcr, %0"::"r"(fpcr));check(std::holds_alternative<mc::Error>(flushed)&&std::get<mc::Error>(flushed).code==mc::ErrorCode::floating_environment,"FPCR FZ rejected");
#elif defined(__SSE__)
 const auto mxcsr=_mm_getcsr();_mm_setcsr(mxcsr|0x8040U);auto flushed=call();_mm_setcsr(mxcsr);check(std::holds_alternative<mc::Error>(flushed)&&std::get<mc::Error>(flushed).code==mc::ErrorCode::floating_environment,"MXCSR FTZ/DAZ rejected");
#endif
 auto inv=math::inverse_rotation({0,0,0,bits(2)},{});check(std::get<math::Bits4>(inv)==math::Bits4{0x80000000,0x80000000,0x80000000,bits(.5F)},"inverse raw sign/order");auto norm=math::normalize_rotation({0,0,0,bits(2)});check(std::get<math::Bits4>(norm)==math::Bits4{0,0,0,bits(1)},"normalize");check(std::get<math::Error>(math::normalize_rotation({0,0,0,0})).code==math::ErrorCode::degenerate_quaternion,"zero norm rejected");
}
}
int main(int argc,char** argv){try{std::size_t count=0;boundaries();if(argc>1){if(argc!=8)throw std::runtime_error("Usage: motion-clock-tests oracle entry path sha entry path sha");std::map<std::uint32_t,resource::Resource> resources;for(int i=2;i<8;i+=3){auto parsed=resource::Resource::load(argv[i+1],argv[i+2]);if(auto* e=std::get_if<resource::Error>(&parsed))throw std::runtime_error(e->message);resources.emplace(std::uint32_t(std::stoul(argv[i])),std::get<resource::Resource>(std::move(parsed)));}count=oracle(argv[1],resources);}std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_cases\":"<<count<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
