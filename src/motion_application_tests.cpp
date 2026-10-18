#include "motion_application.hpp"
#include <bit>
#include <cfenv>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
namespace ma=mh4u::motion_application;namespace mi=mh4u::motion_initialization;
namespace mb=mh4u::motion_binding;namespace ms=mh4u::motion_sampler;
using Layer=mh4u::motion_pending::Layer;using B4=ma::Bits4;
constexpr std::uint32_t one=0x3f800000U;
std::size_t checks{};void check(bool x,const char* why){++checks;if(!x)throw std::runtime_error(why);}
std::uint32_t b(float x){return std::bit_cast<std::uint32_t>(x);}
mi::Templates templates(){return {ma::Bits3{1,2,3},ma::Bits3{4,5,6},B4{0,0,0,one},B4{0,0,0,one}};}
ma::Model model(){return {{b(10),b(20),b(30),0xdeadbeef},{0,0,0,one},{one,one,one,0x7fc12345},{one,one,one,0x7f800000},{7,8,9,10},{11,12,13,14}};}
Layer layer(){Layer out{};auto& s=out.animation;s.layer.selector=0;s.layer.weight=1;s.layer.end_frame=5;s.layer.frame=1;s.layer.prepared_frame=2;s.layer.prepared_next_frame=3;s.previous_rotation={0,0,0,one};s.tracks.translation.weight_bits=s.tracks.rotation.weight_bits=one;return out;}
void error(const ma::Result& r,ma::ErrorCode expected){const auto* e=std::get_if<ma::Error>(&r);check(e&&e->code==expected,"Typed rejection");}
const ma::Output& output(const ma::Result& r){check(std::holds_alternative<ma::Output>(r),"Expected success");return std::get<ma::Output>(r);}
struct Context{bool reject{};B4 t{b(2),b(3),b(4),b(9)},q{0,0,0,one};};
mi::Resolution resolve(const void* opaque,const mb::TrackReference& id){const auto& c=*static_cast<const Context*>(opaque);if(c.reject)return mi::ResolutionError{42,"Boundary rejection"};return ms::TrackView{id,id.ordinal?2U:1U,id.ordinal?c.q:c.t,{}, {}};}
void main_tests(){
 auto m=model();auto defaults=templates();std::vector<Layer> rows;
 auto r=ma::apply(m,rows,defaults,{});const auto& empty=output(r);check(empty.model.position==m.position&&empty.model.rotation==m.rotation,"No layers preserve pose");check(empty.model.translation_delta==B4{4,5,6,0}&&empty.model.rotation_delta==*defaults.identity_10e6560,"No contribution templates");check(empty.samples==0,"No calls");
 for(unsigned i=0;i<3;++i){auto t=defaults;if(i==0)t.base_translation_10e62c0.reset();if(i==1)t.identity_10e6560.reset();if(i==2)t.previous_translation_10e6370.reset();error(ma::apply(m,rows,t,{}),ma::ErrorCode::missing_template);}
 rows={layer()};ma::Limits limit;limit.layers=0;error(ma::apply(m,rows,defaults,{},limit),ma::ErrorCode::resource_limit);
 rows[0].animation.tracks.joints.resize(1);limit=ma::Limits{};limit.bindings_per_layer=0;error(ma::apply(m,rows,defaults,{},limit),ma::ErrorCode::resource_limit);rows[0].animation.tracks.joints.clear();
 r=ma::apply(m,rows,defaults,{});const auto& absent=output(r);check(absent.samples==4&&absent.prediction_updates==1,"Null channels still sampled");check(absent.layers[0].rotation_98==B4{0,0,0,one}&&absent.layers[0].translation_c8==B4{},"Null source defaults plus T W zero");check(absent.model.scale==m.scale&&absent.model.null_scale==m.null_scale,"Raw padding preserved");
 mb::Joint borrowed{};borrowed.channels[2].track=mb::TrackReference{999,7,83,47};borrowed.channels[2].key_data=mb::BufferReference{999,456};borrowed.channels[2].frame_bits=0x7fc12345;borrowed.flags=0xdeadbeef;rows[0].animation.tracks.joints.push_back(borrowed);rows[0].animation.layer.bindings[2]=mh4u::motion_request::Binding{888,4,92,2};r=ma::apply(m,rows,defaults,{});check(output(r).layers[0].animation.tracks.joints==rows[0].animation.tracks.joints&&output(r).layers[0].animation.layer.bindings==rows[0].animation.layer.bindings,"Unconsumed joint storage and event identities retained");
 auto& l=rows[0].animation.layer;l.flags=4;l.field08=1;
 r=ma::apply(m,rows,defaults,{});check(output(r).samples==2&&output(r).prediction_updates==0,"Prediction gate preserves state");l.flags=5;
 r=ma::apply(m,rows,defaults,{});check(output(r).samples==4&&output(r).prediction_updates==1,"Flag1 priority over flag4 previous gate");l.flags=4;l.field08=0;
 r=ma::apply(m,rows,defaults,{});check(output(r).samples==6,"Flag4 samples end and frame");
 l.attributes=1;r=ma::apply(m,rows,defaults,{});check(output(r).contributing_layers==0&&output(r).samples==6,"Attr1 excludes accumulation only");
 l.selector=0xffff;r=ma::apply(m,rows,defaults,{});check(output(r).samples==0&&output(r).sampled_layers==0,"Sentinel excludes full layer");
 rows={layer()};auto& s=rows[0].animation;s.tracks.translation.track=mb::TrackReference{77,2,3,0};s.tracks.rotation.track=mb::TrackReference{77,2,3,1};error(ma::apply(m,rows,defaults,{}),ma::ErrorCode::unresolved_track);
 Context c;c.reject=true;r=ma::apply(m,rows,defaults,{&c,resolve});error(r,ma::ErrorCode::unresolved_track);check(std::get<ma::Error>(r).resolution->code==42,"Resolution reason retained");c.reject=false;
 s.layer.attributes=0x2080;s.layer.flags=4;s.layer.field08=1;s.layer.weight=std::numeric_limits<float>::quiet_NaN();s.tracks.translation.weight_bits=0x7fc12345;
 r=ma::apply(m,rows,defaults,{&c,resolve});check(output(r).model.position==B4{b(2),b(3),b(4),0},"First raw absolute copy ignores discarded weight products");
 c.q={0x7fc54321,0x80000000,0x7f800000,1};r=ma::apply(m,rows,defaults,{&c,resolve});check(output(r).model.rotation==c.q,"Absolute raw Q can preserve nonfinite bits when unconsumed");
 c.q={0,0,0,0};s.layer.attributes=0x20;r=ma::apply(m,rows,defaults,{&c,resolve});error(r,ma::ErrorCode::arithmetic);check(std::get<ma::Error>(r).arithmetic->code==mh4u::motion_transform_math::ErrorCode::degenerate_quaternion,"Zero normalization has no invented default");
 s.layer.attributes=0;s.previous_rotation={};auto unknown=defaults;unknown.inverse_fallback_10e6570.reset();error(ma::apply(m,rows,unknown,{&c,resolve}),ma::ErrorCode::arithmetic);
 c.q={0,0,0,one};s.previous_rotation=c.q;s.layer.weight=1;s.tracks.translation.weight_bits=one;
 const auto prior=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"Set rounding control");r=ma::apply(m,rows,defaults,{&c,resolve});check(std::fesetround(prior)==0,"Restore rounding control");error(r,ma::ErrorCode::arithmetic);check(std::get<ma::Error>(r).arithmetic->code==mh4u::motion_transform_math::ErrorCode::floating_environment,"Environment error retained");
 s.previous_translation[0]=0x7f800000;error(ma::apply(m,rows,defaults,{&c,resolve}),ma::ErrorCode::arithmetic);
}
}
int main(){try{main_tests();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
