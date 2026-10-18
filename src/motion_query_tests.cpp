#include "motion_query.hpp"
#include <bit>
#include <cfenv>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace mq=mh4u::motion_query;
namespace mi=mh4u::motion_initialization;
namespace mb=mh4u::motion_binding;
namespace ms=mh4u::motion_sampler;
namespace mm=mh4u::motion_transform_math;
namespace {
std::size_t checks=0;
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::uint32_t b(float x){return std::bit_cast<std::uint32_t>(x);}
float f(std::uint32_t x){return std::bit_cast<float>(x);}
mq::Bits4 bits(float x,float y,float z,float w){return {b(x),b(y),b(z),b(w)};}
const mq::Output& good(const mq::Result& result){check(std::holds_alternative<mq::Output>(result),"expected query output");return std::get<mq::Output>(result);}
const mq::Error& error(const mq::Result& result,mq::ErrorCode code){
    check(std::holds_alternative<mq::Error>(result),"expected query error");const auto& e=std::get<mq::Error>(result);
    check(e.code==code,"wrong query error");return e;
}
mi::Templates templates(){mi::Templates t;t.base_translation_10e62c0=mi::Bits3{b(1),b(2),b(3)};t.identity_10e6560=bits(0,0,0,1);return t;}
struct Tracks {
    std::vector<ms::TrackView> entries;
    mutable std::vector<mb::TrackReference> calls;
    bool fail=false,wrong=false;
};
mi::Resolution resolve(const void* p,const mb::TrackReference& ref){
    const auto& source=*static_cast<const Tracks*>(p);source.calls.push_back(ref);
    if(source.fail)return mi::ResolutionError{71,"synthetic missing source"};
    for(const auto& entry:source.entries)if(entry.identity==ref){auto view=entry;if(source.wrong)++view.identity.resource_identity;return view;}
    return mi::ResolutionError{72,"unrecognized identity"};
}
mb::Channel channel(Tracks& source,mq::Bits4 value){
    mb::TrackReference ref{17,1,2,static_cast<std::uint32_t>(source.entries.size())};
    source.entries.push_back({ref,1,value,{},{}});
    return {ref,0x7fc01234U,mb::BufferReference{17,43},b(1)};
}
std::vector<mq::LayerView> views(const std::vector<mi::State>& states){
    std::vector<mq::LayerView> out;for(const auto& s:states)out.push_back({s.layer,s.tracks.translation,s.tracks.rotation});return out;
}
void boundaries(){
    auto t=templates();
    auto empty=mq::query({},0x7fc01234U,t,{},{});const auto& empty_out=good(empty);
    check(empty_out.translation==bits(1,2,3,0)&&empty_out.rotation==bits(0,0,0,1),"entry templates changed");
    check(empty_out.channels.empty()&&empty_out.samples==0&&empty_out.accepted_layers==0,"empty query sampled");
    auto hidden=mq::query({},0,t,{false,false},{});check(!good(hidden).translation&&!good(hidden).rotation,"absent outputs written");
    auto missing=t;missing.identity_10e6560.reset();error(mq::query({},0,missing,{false,false},{}),mq::ErrorCode::missing_template);
    missing=t;missing.base_translation_10e62c0.reset();error(mq::query({},0,missing,{},{}),mq::ErrorCode::missing_template);
    Tracks source;std::vector<mi::State> state(3);
    for(auto& s:state){s.layer.selector=0;s.layer.end_frame=10;s.layer.weight=.5F;}
    state[0].layer.selector=0xffff;state[1].layer.flags=2;
    state[2].tracks.translation=channel(source,bits(4,6,8,9));state[2].tracks.rotation=channel(source,bits(0,0,0,2));
    const auto before=state[2].tracks;
    auto input=views(state);mi::Resolver resolver{&source,resolve};
    auto first=mq::query(input,b(2.5F),t,{},resolver);const auto& initial=good(first);
    check(initial.accepted_layers==1&&initial.samples==2,"inactive/pending layers sampled");
    check(initial.translation==bits(4,6,8,0)&&initial.rotation==bits(0,0,0,2),"first accepted layer was weighted/normalized");
    check(initial.channels.size()==3&&initial.channels[0].translation==state[0].tracks.translation&&initial.channels[2].rotation==before.rotation,"reference cursors changed");
    check(source.calls==std::vector<mb::TrackReference>{*before.translation.track,*before.rotation.track},"sampling order changed");
    auto null_outputs=mq::query(input,0,t,{false,false},resolver);check(good(null_outputs).samples==2&&!good(null_outputs).translation&&!good(null_outputs).rotation,"null outputs suppressed samples");
    error(mq::query(input,0,t,{false,false},{}),mq::ErrorCode::unresolved_track);
    source.fail=true;auto unresolved=mq::query(input,0,t,{},resolver);const auto& ue=error(unresolved,mq::ErrorCode::unresolved_track);
    check(ue.layer==2&&ue.component==mq::Component::translation&&ue.track==before.translation.track&&ue.resolution->code==71,"resolver diagnostics lost");source.fail=false;
    source.wrong=true;auto swapped=mq::query(input,0,t,{},resolver);check(error(swapped,mq::ErrorCode::sampling).sampling->code==ms::ErrorCode::resource_identity,"wrong resolved identity accepted");source.wrong=false;
    mq::Limits limits;limits.layers=2;error(mq::query(input,0,t,{},resolver,limits),mq::ErrorCode::resource_limit);
    state[2].layer.attributes=0x100;auto mirrored=mq::query(input,0,t,{},resolver);
    check(good(mirrored).translation==bits(-4,6,8,0)&&good(mirrored).rotation==bits(0,-0.0F,-0.0F,2),"mirror signs lost");state[2].layer.attributes=0;
    for(auto frame:{0U,0x80000000U,1U,0x80000001U,0x7f800000U,0xff800000U,0x7fc01234U,0x7f800001U})
        for(auto attrs:{0U,0x800U}){
            state[2].layer.attributes=attrs;state[2].layer.weight=f(0x7fc01234U);state[2].tracks.translation.weight_bits=0x7f800000U;
            auto raw=mq::query(input,frame,t,{},resolver);check(good(raw).translation==bits(4,6,8,0),"copy-only query imposed numeric domain");
        }
    state[2].tracks.translation=before.translation;state[2].layer.attributes=0;state[2].layer.weight=.5F;
    // A real null sampler returns identity Q and zero T, not the entry T template.
    state[2].tracks.translation.track.reset();state[2].tracks.rotation.track.reset();
    auto absent=mq::query(input,0,t,{},{});check(good(absent).translation==bits(0,0,0,0)&&good(absent).rotation==bits(0,0,0,1),"null channel used template fallback");
    state[2].tracks=before;
    // Raw nonfinite first copies, including VNEG sign flips, do not require
    // a finite-math environment. The domain begins at a subsequent blend.
    const auto saved_q=source.entries[1].reference_bits;
    source.entries[1].reference_bits={0x7fc01234U,0x7f800001U,0xff800000U,1};state[2].layer.attributes=0x100;
    const int rounding=std::fegetround();std::fesetround(FE_DOWNWARD);
    auto copy=mq::query(input,1,t,{},resolver);std::fesetround(rounding);
    check(good(copy).rotation==mq::Bits4{0x7fc01234U,0xff800001U,0x7f800000U,1},"raw quaternion copy/negation changed");
    source.entries[1].reference_bits=saved_q;state[2].layer.attributes=0;
    state[0]=state[2];state[0].tracks.translation=channel(source,bits(8,10,12,13));state[0].tracks.rotation=channel(source,bits(0,0,0,-4));
    // Layers are visited ascending: layer0 incoming to aggregate, layer2 blends.
    state[2].tracks.translation.weight_bits=b(1);state[2].tracks.rotation.weight_bits=b(.5F);
    auto blend=mq::query(input,0,t,{},resolver);check(good(blend).translation==bits(6,8,10,0)&&good(blend).rotation==bits(0,0,0,-1),"normal blend/hemisphere order");
    state[2].layer.attributes=0x40000;
    auto additive=mq::query(input,0,t,{},resolver);check(good(additive).translation==bits(10,13,16,0)&&good(additive).rotation==bits(0,0,0,-4),"additive delta or raw product changed");
    auto outputless=mq::query(input,0,t,{false,false},resolver);check(good(outputless).samples==4&&!good(outputless).translation&&!good(outputless).rotation,"outputless blend skipped");
    state[2].layer.attributes=0;
    std::fesetround(FE_UPWARD);auto wrong_round=mq::query(input,0,t,{},resolver);std::fesetround(rounding);
    check(error(wrong_round,mq::ErrorCode::arithmetic).arithmetic->code==mm::ErrorCode::floating_environment,"blend rounding mode accepted");
    state[2].layer.weight=f(0x7fc00000U);auto nan=mq::query(input,0,t,{},resolver);
    check(error(nan,mq::ErrorCode::arithmetic).layer==2&&error(nan,mq::ErrorCode::arithmetic).arithmetic->code==mm::ErrorCode::nonfinite_operand,"nonfinite blend accepted");state[2].layer.weight=.5F;
    source.entries[1].reference_bits={};source.entries[3].reference_bits={};
    auto degenerate=mq::query(input,0,t,{false,false},resolver);check(error(degenerate,mq::ErrorCode::arithmetic).arithmetic->code==mm::ErrorCode::degenerate_quaternion,"degenerate quaternion repaired");
    check(state[2].tracks.translation==before.translation&&state[2].tracks.rotation.frame_bits==before.rotation.frame_bits&&state[2].tracks.rotation.key_data==before.rotation.key_data,"failed query mutated caller cursors");
}
}
int main(){try{boundaries();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
