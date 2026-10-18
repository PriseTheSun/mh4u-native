#pragma once
#include "motion_initialization.hpp"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace binding_test {
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
}
namespace initialization_test {
using namespace mh4u::motion_initialization;
namespace mb=mh4u::motion_binding;
namespace ms=mh4u::motion_sampler;
using binding_test::check;using binding_test::bytes;using binding_test::read;
using binding_test::word;using binding_test::Bytes;
Bits4 four(const Bytes& b,std::size_t p){return {word(b,p),word(b,p+4),word(b,p+8),word(b,p+12)};}
State state(const Bytes& b,const Bytes& joints,std::uint32_t bank,std::uint32_t slot) {
    return {binding_test::layer(b,bank,slot),binding_test::tracks(b,joints,bank,slot),four(b,0x20),four(b,0x30),
        four(b,0x78),four(b,0x88),four(b,0xa8),four(b,0xb8)};
}
void equal(const State& out,const State& expected) {
    binding_test::equal(out.layer,expected.layer);
    check(out.tracks==expected.tracks,"Track/cursor/joint state");
    check(out.prediction_rotation==expected.prediction_rotation,"Preserved prediction quaternion");
    check(out.prediction_translation==expected.prediction_translation,"Preserved prediction translation");
    const std::array actual{out.base_translation,out.base_rotation,out.previous_translation,out.previous_rotation};
    const std::array wanted{expected.base_translation,expected.base_rotation,expected.previous_translation,expected.previous_rotation};
    for(unsigned i=0;i<4;++i)for(unsigned j=0;j<4;++j) {
        check(actual[i][j]==wanted[i][j],("Transform "+std::to_string(i)+" component "+std::to_string(j)+" bits "+std::to_string(actual[i][j])+" expected "+std::to_string(wanted[i][j])).c_str());
    }
}
struct Views {
    std::vector<ms::TrackView> rows;
    static Resolution resolve(const void* context,const mb::TrackReference& id) {
        const auto& views=*static_cast<const Views*>(context);
        for(const auto& row:views.rows)if(row.identity==id)return row;
        return ResolutionError{73,"Unknown fixture identity"};
    }
    Resolver resolver()const{return {this,resolve};}
};
Parameters parameters(const Bytes& globals,std::uint32_t delta) {
    return {delta,{{Bits3{word(globals,0),word(globals,4),word(globals,8)}},
        {Bits3{word(globals,12),word(globals,16),word(globals,20)}},four(globals,24),four(globals,40)}};
}
} // namespace initialization_test
