#include "motion_binding.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace mh4u::motion_binding;
std::size_t checks=0;
void check(bool good,const char* why){++checks;if(!good)throw std::runtime_error(why);}
using Bytes=std::vector<unsigned char>;
Bytes bytes(std::istream& in,std::size_t size){Bytes b(size);in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(size));if(!in)throw std::runtime_error("Truncated oracle");return b;}
std::uint32_t word(const Bytes& b,std::size_t offset,unsigned width=4){if(offset+width>b.size())throw std::runtime_error("Oracle bounds");std::uint32_t x=0;for(unsigned j=0;j<width;++j)x|=std::uint32_t(b[offset+j])<<(8*j);return x;}
std::uint32_t read(std::istream& in){return word(bytes(in,4),0);}
float number(const Bytes& b,std::size_t offset){return std::bit_cast<float>(word(b,offset));}
Channel channel(const Bytes& b,std::size_t offset,std::uint32_t bank,std::uint32_t slot){
    Channel c;const auto p=word(b,offset),key=word(b,offset+8);
    if(p>=0x2700000U&&p<0x2710000U){check((p-0x2700000U)%36==0,"old track alignment");c.track=TrackReference{2,11,19,(p-0x2700000U)/36};}
    else if(p>=0x2400000U&&p<0x2410000U){check((p-0x2400000U)%36==0,"new track alignment");c.track=TrackReference{1,bank,slot,(p-0x2400000U)/36};}
    else check(p==0,"Unknown oracle track pointer");
    if(key>=0x2900000U&&key<0x2a00000U)c.key_data=BufferReference{1,key-0x2900000U};
    else if(key>=0x2800000U&&key<0x2900000U)c.key_data=BufferReference{2,key-0x2800000U};
    else check(key==0,"Unknown oracle key pointer");
    c.frame_bits=word(b,offset+4);c.weight_bits=word(b,offset+12);return c;
}
Tracks tracks(const Bytes& layer,const Bytes& joints,std::uint32_t bank,std::uint32_t slot){
    Tracks t;t.translation=channel(layer,0xd8,bank,slot);t.rotation=channel(layer,0xe8,bank,slot);
    for(std::size_t p=0;p<joints.size();p+=52){Joint j;for(unsigned k=0;k<3;++k)j.channels[k]=channel(joints,p+16*k,bank,slot);j.flags=word(joints,p+48);t.joints.push_back(j);}return t;
}
mh4u::motion_request::Layer layer(const Bytes& b,std::uint32_t bank,std::uint32_t slot){
    mh4u::motion_request::Layer l;l.selector=word(b,4,2);l.flags=word(b,6,2);l.field08=word(b,8,2);l.attributes=word(b,12);
    l.requested_frame=number(b,0x10);l.offset=number(b,0x14);l.interpolation=number(b,0x40);l.remaining_interpolation=number(b,0x44);l.weight=number(b,0x48);l.frame=number(b,0x4c);l.previous_frame=number(b,0x50);l.end_frame=number(b,0x54);l.field58=number(b,0x58);l.speed=number(b,0x5c);l.field60=number(b,0x60);
    for(unsigned g=0;g<4;++g){const auto p=word(b,0xf8+4*g);if(p==0x2600000U+72*g)l.bindings[g]=mh4u::motion_request::Binding{2,11,19,g};else if(p==0x2500000U+72*g)l.bindings[g]=mh4u::motion_request::Binding{1,bank,slot,g};else check(p==0,"Unknown event pointer");l.masks[g]=word(b,0x108+4*g);}return l;
}
void equal(const mh4u::motion_request::Layer& x,const mh4u::motion_request::Layer& y){
    check(x.selector==y.selector&&x.flags==y.flags&&x.field08==y.field08,"Preserved layer words");check(x.attributes==y.attributes,"Layer attributes");
    const std::array a{x.requested_frame,x.offset,x.interpolation,x.remaining_interpolation,x.weight,x.frame,x.previous_frame,x.end_frame,x.field58,x.speed,x.field60};
    const std::array b{y.requested_frame,y.offset,y.interpolation,y.remaining_interpolation,y.weight,y.frame,y.previous_frame,y.end_frame,y.field58,y.speed,y.field60};
    for(unsigned i=0;i<a.size();++i)check(std::bit_cast<std::uint32_t>(a[i])==std::bit_cast<std::uint32_t>(b[i]),"Layer binary32 bits");
    check(x.bindings==y.bindings,"Event references");check(x.masks==y.masks,"Preserved event masks");
}
std::size_t oracle(const char* path){
    std::ifstream in(path,std::ios::binary);const auto magic=bytes(in,8);check(std::string_view(reinterpret_cast<const char*>(magic.data()),magic.size())=="MH4UMB01","Oracle identity");const auto count=read(in);check(count<100000,"Oracle case limit");
    for(std::uint32_t i=0;i<count;++i){
        const auto index=read(in),bank=read(in),slot=read(in),present=read(in),frames=read(in),loop=read(in),flags=read(in),n=read(in),m=read(in);check(index<=3&&n<=255&&m<=4096,"Oracle resource bound");
        const auto before=bytes(in,0x130),map=bytes(in,256),before_joints=bytes(in,n*52),raw_tracks=bytes(in,m*36),after=bytes(in,0x130),after_joints=bytes(in,n*52);
        const auto previous_layer=layer(before,bank,slot),expected_layer=layer(after,bank,slot);const auto previous_tracks=tracks(before,before_joints,bank,slot),expected_tracks=tracks(after,after_joints,bank,slot);
        std::vector<Track> descriptors;for(unsigned t=0;t<m;++t){const auto key=word(raw_tracks,t*36+12);Track d{word(raw_tracks,t*36),word(raw_tracks,t*36+4),std::nullopt};if(key){check(key>=0x2900000U&&key<0x2a00000U,"Track key ownership");d.key_data_offset=key-0x2900000U;}descriptors.push_back(d);}
        mh4u::motion_request::Action header{frames,std::bit_cast<std::int32_t>(loop),true,{}};const Action action{1,bank,slot,header,flags,descriptors};const auto result=prepare_tracks(previous_layer,previous_tracks,map,present?&action:nullptr);const auto* out=std::get_if<Prepared>(&result);
        if(!out)throw std::runtime_error("Unexpected rejection in ARM case "+std::to_string(i));
        try{check(out->action_present==(present!=0),"Action status");equal(out->layer,expected_layer);check(out->tracks.translation==expected_tracks.translation,"Root translation binding");check(out->tracks.rotation==expected_tracks.rotation,"Root rotation binding");check(out->tracks.joints.size()==expected_tracks.joints.size(),"Joint count");for(unsigned j=0;j<n;++j)check(out->tracks.joints[j]==expected_tracks.joints[j],"Joint descriptors/flags");}
        catch(const std::exception& e){throw std::runtime_error("ARM case "+std::to_string(i)+": "+e.what());}
    }
    check(in.peek()==std::char_traits<char>::eof(),"Trailing oracle bytes");return count;
}
void error(const Result& r,ErrorCode code){const auto* e=std::get_if<Error>(&r);check(e&&e->code==code,"Expected profile rejection");}
void boundaries(){
    mh4u::motion_request::Layer l;Tracks t;t.joints.resize(1);std::array<std::uint8_t,256> map{};std::vector<Track> data{{0,0x3f800000U,0}};
    mh4u::motion_request::Action h{120,-1,true,{}};Action a{1,3,7,h,3,data};
    auto out=std::get<Prepared>(prepare_tracks(l,t,map,&a));check(out.tracks.joints[0].flags==0x11,"Weight flags");check(out.layer.attributes==0x104000,"Action flags");check(out.tracks.joints[0].channels[0].key_data==std::optional<BufferReference>{{1,0}},"Present byte offset zero differs from null");
    check(t.joints[0].flags==0&&l.attributes==0,"Transactional input preservation");
    error(prepare_tracks(l,t,{},&a),ErrorCode::bone_map_extent);map[0]=1;error(prepare_tracks(l,t,map,&a),ErrorCode::joint_extent);map[0]=255;
    check(std::get<Prepared>(prepare_tracks(l,t,map,&a)).tracks.joints==t.joints,"Unmapped joint skipped");
    a.resource_identity=0;error(prepare_tracks(l,t,map,&a),ErrorCode::resource_identity);a.resource_identity=1;a.bank=16;error(prepare_tracks(l,t,map,&a),ErrorCode::resource_identity);a.bank=3;a.slot=256;error(prepare_tracks(l,t,map,&a),ErrorCode::resource_identity);a.slot=7;
    h.event_descriptors_present=false;error(prepare_tracks(l,t,map,&a),ErrorCode::event_descriptors);h.event_descriptors_present=true;
    error(prepare_tracks(l,t,map,&a,{0,1}),ErrorCode::resource_limit);error(prepare_tracks(l,t,map,&a,{1,0}),ErrorCode::resource_limit);
    const int mode=std::fegetround();std::fesetround(FE_DOWNWARD);auto bad=prepare_tracks(l,t,map,&a);auto no_action=prepare_tracks(l,t,{},nullptr);std::fesetround(mode);error(bad,ErrorCode::floating_environment);check(!std::get<Prepared>(no_action).action_present,"Null action bypasses unaccessed data and float conversion");
    data.clear();a.tracks=data;check(std::holds_alternative<Prepared>(prepare_tracks(l,t,{},&a)),"Zero tracks do not read map");
    // Diagnostic pointer/extent failures are native policy, not original errors.
}
} // namespace
int main(int argc,char** argv){try{if(argc>2)throw std::runtime_error("Usage: motion-binding-tests [oracle.bin]");boundaries();const auto cases=argc==2?oracle(argv[1]):0;std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"arm_prefix_cases\":"<<cases<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
