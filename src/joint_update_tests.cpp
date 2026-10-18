#include "joint_update.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace ju=mh4u::joint_update;
namespace jp=mh4u::joint_pose;
namespace mb=mh4u::motion_binding;
namespace mi=mh4u::motion_initialization;
namespace ms=mh4u::motion_sampler;
namespace mth=mh4u::motion_transform_math;
namespace {
std::size_t checks{},current{};
void check(bool b,const char* text){++checks;if(!b)throw std::runtime_error("case "+std::to_string(current)+": "+text);}
std::uint32_t bits(float f){return std::bit_cast<std::uint32_t>(f);}
struct Reader {
 std::vector<unsigned char> data;std::size_t p{};
 explicit Reader(const char* path){
  std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("oracle open");
  const auto size=f.tellg();if(size<0||size>256*1024*1024)throw std::runtime_error("oracle capacity");
  data.resize(static_cast<std::size_t>(size));f.seekg(0);f.read(reinterpret_cast<char*>(data.data()),size);
  if(!f)throw std::runtime_error("oracle read");
 }
 std::uint32_t u32(){if(data.size()-p<4)throw std::runtime_error("oracle truncated");std::uint32_t v{};for(unsigned i=0;i<4;++i)v|=std::uint32_t(data[p++])<<(8*i);return v;}
 template<std::size_t N>std::array<std::uint32_t,N> words(){std::array<std::uint32_t,N> a;for(auto& x:a)x=u32();return a;}
 std::vector<std::byte> bytes(std::size_t n){if(n>data.size()-p)throw std::runtime_error("oracle bytes");std::vector<std::byte> v(n);for(auto& x:v)x=std::byte(data[p++]);return v;}
};
struct OwnedLayer {mh4u::motion_request::Layer layer;mb::Tracks tracks;std::uint32_t frame;};
struct Resolution {
 std::vector<ms::TrackView> tracks;std::vector<std::vector<std::byte>> keys;
 std::vector<std::pair<std::uint32_t,std::uint32_t>> calls;
 static mi::Resolution resolve(const void* ptr,const mb::TrackReference& id){
  auto& self=*const_cast<Resolution*>(static_cast<const Resolution*>(ptr));
  for(std::size_t i=0;i<self.tracks.size();++i)if(self.tracks[i].identity==id){self.calls.emplace_back(static_cast<std::uint32_t>(i),0);return self.tracks[i];}
  return mi::ResolutionError{7,"Missing fixture identity"};
 }
 mi::Resolver resolver(){return {this,resolve};}
};
struct JointRead {jp::Joint joint;ju::Metadata meta;};
JointRead read_joint(Reader& r){
 auto w=r.words<42>();JointRead o;
 for(unsigned i=0;i<3;++i)o.joint.reference_translation[i]=w[0x48/4+i];
 for(unsigned i=0;i<4;++i){o.joint.rotation[i]=w[0x58/4+i];o.joint.scale[i]=w[0x68/4+i];o.joint.translation[i]=w[0x78/4+i];}
 o.meta={w[0x94/4],w[0x98/4]};return o;
}
std::size_t oracle(const char* path){
 Reader r(path);for(char c:std::string("JUPD0001"))check(r.data.at(r.p++)==static_cast<unsigned char>(c),"magic");
 const auto count=r.u32();check(count<100000,"case count");
 for(current=0;current<count;++current){
  const auto leaf=r.u32(),flags=r.u32(),n=r.u32(),lc=r.u32(),model=r.u32(),table=r.u32(),mc=r.u32(),mask=r.u32();
  check(n<=256&&lc<=8&&mc<=256,"counts");ju::Parameters params;params.leaf=static_cast<ju::Leaf>(leaf);params.flags238=flags;
  const auto T=r.words<3>();const auto Q=r.words<4>();const auto S=r.words<3>();
  if(mask&1)params.templates.translation=T;if(mask&2)params.templates.rotation=Q;if(mask&4)params.templates.scale=S;
  std::vector<jp::Joint> joints;std::vector<ju::Metadata> metadata;
  for(unsigned i=0;i<n;++i){auto j=read_joint(r);joints.push_back(j.joint);metadata.push_back(j.meta);}
  std::vector<jp::Matrix> matrices;for(unsigned i=0;i<mc;++i)matrices.push_back(r.words<16>());
  if(model==0)params.model=jp::Unknown{};else if(model==1)params.model=jp::Null{};else{
   jp::MatrixTable tab=table==0?jp::MatrixTable{jp::Unknown{}}:table==1?jp::MatrixTable{jp::Null{}}:jp::MatrixTable{std::span<const jp::Matrix>{matrices}};params.model=jp::PresentModel{tab};
  }
  std::vector<OwnedLayer> owned;owned.reserve(lc);Resolution resolve;resolve.tracks.reserve(lc*n*3);resolve.keys.reserve(lc*n*3);
  std::vector<mb::Channel*> channels;
  for(unsigned li=0;li<lc;++li){
   OwnedLayer l{};auto selector=r.u32();l.layer.selector=static_cast<std::uint16_t>(selector);l.layer.flags=static_cast<std::uint16_t>(selector>>16);l.layer.attributes=r.u32();l.layer.weight=std::bit_cast<float>(r.u32());l.frame=r.u32();auto bc=r.u32();check(bc<=256,"bindings");l.tracks.joints.resize(bc);
   for(unsigned ji=0;ji<bc;++ji){auto& b=l.tracks.joints[ji];b.flags=r.u32();
    for(unsigned ci=0;ci<3;++ci){auto& ch=b.channels[ci];auto codec=r.u32();ch.frame_bits=r.u32();auto cursor=r.u32();ch.weight_bits=r.u32();auto ref=r.words<4>();auto extremes=r.words<8>();auto length=r.u32();check(length<=4096,"keys size");
     const auto ordinal=resolve.tracks.size();mb::TrackReference id{1,0,li,ji*3+ci};mb::BufferReference base{1,4096+ordinal*4096};
     if(codec!=0xffffffffU)ch.track=id;if(cursor!=0xffffffffU)ch.key_data=mb::BufferReference{1,base.byte_offset+cursor};
     resolve.keys.push_back(r.bytes(length));resolve.tracks.push_back(ms::TrackView{id,codec,ref,ms::KeyBuffer{base,resolve.keys.back()},extremes});
    }
   }
   owned.push_back(std::move(l));
  }
  std::vector<ju::LayerView> views;std::vector<mb::Tracks> before_tracks;for(const auto& l:owned){views.push_back({l.layer,l.tracks,l.frame});before_tracks.push_back(l.tracks);}
  auto before_joints=joints;auto before_matrices=matrices;auto before_metadata=metadata;
  auto result=ju::apply_layers(joints,metadata,views,params,resolve.resolver());
  if(auto* e=std::get_if<ju::Error>(&result))throw std::runtime_error("case "+std::to_string(current)+" error "+std::to_string(int(e->code))+" "+std::string(e->message)+" layer "+std::to_string(e->layer)+" joint "+std::to_string(e->joint));
  auto out=std::get<ju::Output>(result);check(out.joints.size()==n&&out.tracks.size()==lc,"output sizes");
  for(unsigned i=0;i<n;++i){auto expected=read_joint(r);check(expected.meta==metadata[i],"metadata preserved ARM");check(out.joints[i].rotation==expected.joint.rotation,"rotation bits");check(out.joints[i].translation==expected.joint.translation,"translation bits");check(out.joints[i].scale==expected.joint.scale,"scale bits");check(out.joints[i].reference_translation==expected.joint.reference_translation,"reference bits");}
  std::size_t ordinal=0;
  for(unsigned li=0;li<lc;++li){check(out.tracks[li].translation==owned[li].tracks.translation&&out.tracks[li].rotation==owned[li].tracks.rotation,"model channels preserved");
   for(unsigned ji=0;ji<out.tracks[li].joints.size();++ji){check(out.tracks[li].joints[ji].flags==owned[li].tracks.joints[ji].flags,"binding flags");
    for(unsigned ci=0;ci<3;++ci,++ordinal){const auto& ch=out.tracks[li].joints[ji].channels[ci];auto raw=r.words<4>();check(raw[0]==(ch.track?0x2500000U+ordinal*128:0),"track pointer/identity");check(ch.track==owned[li].tracks.joints[ji].channels[ci].track,"identity preserved");check(ch.frame_bits==raw[1],"cursor frame");check(ch.weight_bits==raw[3],"weight raw");
     const auto expected_key=ch.key_data?0x2600000ULL+ch.key_data->byte_offset-4096:0;check(expected_key==raw[2],"key cursor");}
   }
  }
  const auto samples=r.u32();check(samples==out.samples&&samples==resolve.calls.size(),"sample count");for(unsigned i=0;i<samples;++i){auto sid=r.u32(),frame=r.u32();check(sid==resolve.calls[i].first,"sample order");check(frame==views[resolve.tracks[sid].identity.slot].prepared_frame_bits,"frame identity");}
  check(joints==before_joints&&metadata==before_metadata&&matrices==before_matrices,"input immutable");for(unsigned i=0;i<lc;++i)check(owned[i].tracks==before_tracks[i],"cursor transaction");
 }
 check(r.p==r.data.size(),"oracle trailing");return count;
}
void boundaries(){
 current=100000;std::vector<jp::Joint> joints(2);for(auto& j:joints){j.rotation={0,0,0,bits(1)};j.scale={bits(2),bits(3),bits(4),0x7fa12345};j.translation={bits(3),bits(4),bits(5),0x7fa23456};}
 std::vector<ju::Metadata> meta(2);OwnedLayer l{};l.layer.selector=0;l.layer.weight=1;l.tracks.joints.resize(2);
 for(auto& b:l.tracks.joints){b.flags=7;for(auto& ch:b.channels)ch.weight_bits=bits(1);}
 std::vector<ju::LayerView> layers{{l.layer,l.tracks,0}};ju::Parameters p;p.model=jp::PresentModel{jp::Null{}};p.flags238=0x10000000;p.templates={jp::Bits3{bits(2),0,0},jp::Bits4{0,0,0,bits(1)},jp::Bits3{bits(2),bits(3),bits(4)}};
 auto check_error=[&](ju::ErrorCode code){auto r=ju::apply_layers(joints,meta,layers,p,{});check(std::holds_alternative<ju::Error>(r),"expected error");check(std::get<ju::Error>(r).code==code,"error kind");};
 check(std::holds_alternative<ju::Output>(ju::apply_layers(joints,meta,layers,p,{})),"explicit globals without matrices");
 p.model=jp::Unknown{};check_error(ju::ErrorCode::unknown_model);p.model=jp::Null{};check_error(ju::ErrorCode::null_model);p.model=jp::PresentModel{jp::Unknown{}};check_error(ju::ErrorCode::unknown_table);p.model=jp::PresentModel{jp::Null{}};
 auto t=p.templates;p.templates.rotation.reset();check_error(ju::ErrorCode::missing_template);p.templates=t;p.flags238=0;check_error(ju::ErrorCode::null_table);p.flags238=0x10000000;
 p.leaf=ju::Leaf::rotation_translation_scale;l.layer.attributes=0x100;meta[1].mapping98=3;check_error(ju::ErrorCode::mirror_extent);meta[1].mapping98=0xff;l.layer.attributes=0;
 l.tracks.joints[1].channels[0].track=mb::TrackReference{77,3,9,12};auto r=ju::apply_layers(joints,meta,layers,p,{});check(std::get<ju::Error>(r).track==l.tracks.joints[1].channels[0].track,"unresolved identity retained");check(std::get<ju::Error>(r).joint==1,"late error index");
 Resolution resolution;auto rr=ju::apply_layers(joints,meta,layers,p,resolution.resolver());check(std::get<ju::Error>(rr).resolution->code==7&&std::get<ju::Error>(rr).track==l.tracks.joints[1].channels[0].track,"resolver reason retained");l.tracks.joints[1].channels[0].track.reset();
 ju::Limits limits;limits.joints=1;check(std::get<ju::Error>(ju::apply_layers(joints,meta,layers,p,{},limits)).code==ju::ErrorCode::resource_limit,"allocation bound");limits={};limits.bindings_per_layer=1;check(std::get<ju::Error>(ju::apply_layers(joints,meta,layers,p,{},limits)).code==ju::ErrorCode::resource_limit,"binding allocation bound");
 auto small=meta;small.pop_back();check(std::get<ju::Error>(ju::apply_layers(joints,small,layers,p,{})).code==ju::ErrorCode::metadata_extent,"metadata extent");
 p.model=jp::PresentModel{jp::Null{}};check(std::holds_alternative<ju::Output>(ju::apply_layers({}, {}, {},p,{})),"zero count explicit null table");p.model=jp::Unknown{};check(std::get<ju::Error>(ju::apply_layers({}, {}, {},p,{})).code==ju::ErrorCode::unknown_model,"zero count still model read");
 check(std::get<mth::Error>(mth::nlerp({0,0,0,0},{0,0,0,0},bits(.5))).code==mth::ErrorCode::degenerate_quaternion,"zero norm rejected");
 check(std::get<mth::Error>(mth::multiply(0x7f800000,bits(0))).code==mth::ErrorCode::nonfinite_operand,"nonfinite consumed");
 check(std::get<mth::Error>(mth::multiply(0x7f7fffff,bits(2))).code==mth::ErrorCode::numeric_overflow,"overflow rejected");
 const auto mode=std::fegetround();std::fesetround(FE_DOWNWARD);auto wrong=mth::multiply(bits(2),bits(3));std::fesetround(mode);check(std::get<mth::Error>(wrong).code==mth::ErrorCode::floating_environment,"rounding guard");
#if defined(__aarch64__)
 std::uint64_t saved;asm volatile("mrs %0, fpcr":"=r"(saved));const auto fz=saved|(1ULL<<24);asm volatile("msr fpcr, %0"::"r"(fz));auto flushed=mth::multiply(bits(2),bits(3));asm volatile("msr fpcr, %0"::"r"(saved));check(std::get<mth::Error>(flushed).code==mth::ErrorCode::floating_environment,"FZ guard");
#elif defined(__SSE__)
 const auto saved=_mm_getcsr();_mm_setcsr(saved|(1U<<15)|(1U<<6));auto flushed=mth::multiply(bits(2),bits(3));_mm_setcsr(saved);check(std::get<mth::Error>(flushed).code==mth::ErrorCode::floating_environment,"FTZ guard");
#endif
}
}
int main(int argc,char** argv){try{if(argc>2)throw std::runtime_error("usage: joint-update-tests [oracle.bin]");boundaries();auto n=argc==2?oracle(argv[1]):0;std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_cases\":"<<n<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
