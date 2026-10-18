#include "model_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <cstring>

namespace mr=mh4u::model_resource;
namespace {
std::size_t checks=0;
void require(bool condition,const char* why){++checks;if(!condition)throw std::runtime_error(why);}
void put(std::string& b,std::size_t p,std::uint32_t v,unsigned count=4){for(unsigned k=0;k<count;++k)b.at(p+k)=static_cast<char>((v>>(k*8))&255);}
std::string digest(std::string_view b){return picosha2::hash256_hex_string(b.begin(),b.end());}
std::string file(const char* name){
    std::ifstream f(name,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot read fixture");const auto n=f.tellg();
    if(n<0||n>16*1024*1024)throw std::runtime_error("Source oracle exceeds allocation policy");
    std::string out(static_cast<std::size_t>(n),'\0');f.seekg(0);
    if(!f.read(out.data(),static_cast<std::streamsize>(n))||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture read failed or size changed");
    return out;
}
mr::Resource good(std::string_view b,mr::Limits limits={}){auto r=mr::Resource::decode(b,digest(b),limits);require(std::holds_alternative<mr::Resource>(r),"Expected accepted MOD");return std::get<mr::Resource>(std::move(r));}
void bad(std::string_view b,mr::ErrorCode code,mr::Limits limits={}){
    auto r=mr::Resource::decode(b,digest(b),limits);require(std::holds_alternative<mr::Error>(r),"Expected rejected MOD");
    require(std::get<mr::Error>(r).code==code,"Wrong typed MOD error");
}
std::string fixture(){
    // Header + two 24-byte joints + A/B matrices + remap + two palettes + two primitives.
    std::string b(840,'\0');put(b,0,0x444f4d);put(b,4,230,2);put(b,6,2,2);put(b,8,2,2);
    put(b,0x24,2);put(b,0x28,128);put(b,0x34,744);
    put(b,688,0xCAFE0001);put(b,692,1,1);put(b,716,0); // high16 is preserved but not count.
    put(b,176,0x7fc12345);put(b,304,0x80000000);put(b,308,0x7f800000);
    put(b,744+0x26,0xbeef,2);put(b,792+0x26,0x1234,2);
    return b;
}
void boundaries(){
    auto b=fixture();auto r=good(b);const auto s=r.skeleton();require(bool(s),"Skeleton absent");
    require(r.joint_count()==2&&r.declared_palette_count()==2,"Header facts");
    require(s->local_matrices[0][0]==0x7fc12345&&s->source_skin_matrices[0][0]==0x80000000&&s->source_skin_matrices[0][1]==0x7f800000,"Raw IEEE source words normalized");
    require(s->palettes[0].descriptor==0xCAFE0001&&s->palettes[0].joints.size()==1&&s->palettes[0].joints[0]==1,"Descriptor low16 semantics");
    require(s->palettes[1].joints.empty(),"Zero palette count lost");
    require(r.primitives()[0].source_group==0xbeef&&r.primitives()[1].source_group==0x1234,"Source groups lost");
    require(r.primitives()[0].loaded_group==1&&r.primitives()[1].loaded_group==1,"File group leaked into runtime grouping");
    require(std::holds_alternative<mr::Error>(r.primitive_source_record(2)),"Primitive extent");
    auto copy=r;auto separately_decoded=good(b);require(r.identity()!=0&&copy.identity()==r.identity()&&separately_decoded.identity()!=r.identity(),"Immutable snapshot identity");
    b[692]=0;require(s->palettes[0].joints[0]==1&&copy.skeleton()->palettes[0].joints[0]==1,"Input buffer was borrowed instead of copied");
    require(r.sha256()!=digest(b),"Source digest changed after input mutation");
    auto retained=[](){auto temporary=good(fixture());return temporary;}();require(retained.skeleton()->palettes[0].joints[0]==1,"Returned shared storage invalid");
    b=fixture();put(b,792+4,0x1000);require(good(b).primitives()[1].loaded_group==2,"Material group boundary");
    b=fixture();put(b,692,255,1);require(good(b).skeleton()->palettes[0].joints[0]==255,"Unconsumed index should remain source fact");
    b=fixture();put(b,688,25);bad(b,mr::ErrorCode::unsupported_layout);
    b=fixture();put(b,688,0xffff);bad(b,mr::ErrorCode::unsupported_layout);
    b=fixture();put(b,4,231,2);bad(b,mr::ErrorCode::unsupported_format);
    b=fixture();put(b,0,0);bad(b,mr::ErrorCode::unsupported_format);
    for(std::size_t n=0;n<128;++n)bad(std::string(n,'\0'),mr::ErrorCode::malformed_resource);
    b=fixture();b.resize(839);bad(b,mr::ErrorCode::malformed_resource);
    b=fixture();put(b,0x28,0xfffffff0);bad(b,mr::ErrorCode::malformed_resource);
    b=fixture();put(b,0x34,0xfffffff0);bad(b,mr::ErrorCode::malformed_resource);
    for(auto p:{0x28,0x34}){b=fixture();put(b,p,0);bad(b,mr::ErrorCode::malformed_resource);}
    b=fixture();put(b,8,65535,2);bad(b,mr::ErrorCode::malformed_resource);
    b=fixture();put(b,6,0,2);put(b,8,0,2);put(b,0x28,0xffffffff);put(b,0x34,0xffffffff);b.resize(128);
    r=good(b);require(!r.skeleton()&&r.primitives().empty()&&r.declared_palette_count()==2,"Zero-joint loader branch fabricated palette state");
    require(r.skeleton_source_offset()==0xffffffff&&r.primitives_source_offset()==0xffffffff,"Unconsumed offsets lost");
    auto lim=mr::Limits{};lim.bytes=839;bad(fixture(),mr::ErrorCode::resource_limit,lim);
    lim={};lim.joints=1;bad(fixture(),mr::ErrorCode::resource_limit,lim);
    lim={};lim.palettes=1;bad(fixture(),mr::ErrorCode::resource_limit,lim);
    lim={};lim.primitives=1;bad(fixture(),mr::ErrorCode::resource_limit,lim);
    for(auto hash:{std::string(64,'A'),std::string(63,'a')})require(std::get<mr::Error>(mr::Resource::decode(fixture(),hash)).code==mr::ErrorCode::invalid_expected,"Invalid expected digest");
    require(std::get<mr::Error>(mr::Resource::decode(fixture(),std::string(64,'0'))).code==mr::ErrorCode::hash_mismatch,"Digest mismatch accepted");
    require(std::get<mr::Error>(mr::Resource::load("/path/that/does/not/exist/mh4u.mod",std::string(64,'0'))).code==mr::ErrorCode::io,"Missing file error");
}
struct Reader {
    std::string_view b;std::size_t p{};
    std::string_view take(std::size_t n){if(n>b.size()-p)throw std::runtime_error("Truncated source oracle");auto out=b.substr(p,n);p+=n;return out;}
    std::uint32_t u32(){auto s=take(4);std::uint32_t v=0;for(unsigned k=0;k<4;++k)v|=std::uint32_t(static_cast<unsigned char>(s[k]))<<(8*k);return v;}
};
void compare_primitives(const mr::Resource& r,std::string_view before,std::string_view after){
    require(before.size()==r.primitives().size()*48&&after.size()==before.size(),"Primitive oracle sizes");
    std::string rebuilt(before);
    for(std::size_t i=0;i<r.primitives().size();++i){
        auto raw=std::get<std::span<const std::byte>>(r.primitive_source_record(i));require(std::memcmp(raw.data(),before.data()+48*i,48)==0,"Source record changed");
        const auto& p=r.primitives()[i];put(rebuilt,i*48+38,p.loaded_group,2);
        const auto base=before.substr(i*48,48);Reader in{base};auto flags0=in.u32(),flags4=in.u32(),format=in.u32();in.p=0x10;
        require(p.flags0==flags0&&p.flags4==flags4&&p.format8==format&&p.field10==in.u32()&&p.input_layout_hash==in.u32(),"Primitive facts differ");
        require(p.visibility_index==(flags4&4095)&&p.material_index==((flags4>>12)&4095)&&p.palette_index==static_cast<unsigned char>(base[37]),"Primitive selectors differ");
    }
    require(rebuilt==after,"Native grouping differs from original ARM block");
}
void oracle(const char* path,const char* expected,const char* actor,const char* body){
    auto b=file(path);require(digest(b)==expected,"Changed source oracle identity");Reader in{b};require(in.take(8)=="M4MRF001","Oracle magic");require(in.u32()==2,"Real source count");
    for(const char* name:{actor,body}){
        const auto hash=in.take(64);auto result=mr::Resource::load(name,hash);require(std::holds_alternative<mr::Resource>(result),"Real source load/hash failed");const auto r=std::get<mr::Resource>(std::move(result));auto s=r.skeleton();
        const auto n=in.u32(),palettes=in.u32(),primitives=in.u32();require(s&&r.joint_count()==n&&s->palettes.size()==palettes&&r.primitives().size()==primitives,"Real source counts");
        for(auto matrices:{s->local_matrices,s->source_skin_matrices})for(const auto& m:matrices)for(auto v:m)require(v==in.u32(),"Source matrix word differs");
        for(const auto& p:s->palettes){require(p.descriptor==in.u32(),"Source palette descriptor differs");require(p.joints.size()==in.u32(),"Palette size differs");const auto values=in.take(p.joints.size());require(std::memcmp(values.data(),p.joints.data(),values.size())==0,"Palette indices differ");}
        auto before=in.take(primitives*48),after=in.take(primitives*48);compare_primitives(r,before,after);
    }
    const auto count=in.u32();require(count<=4096,"Oracle case bound");
    for(std::uint32_t i=0;i<count;++i){
        const auto size=in.u32();require(size%48==0&&size/48<=65535,"Oracle primitive extent");const auto before=in.take(size),after=in.take(size);
        std::string bytes(128,'\0');put(bytes,0,0x444f4d);put(bytes,4,230,2);put(bytes,8,size/48,2);put(bytes,0x34,128);bytes+=before;
        auto r=good(bytes);compare_primitives(r,before,after);require(r.primitives().empty()||r.primitives()[0].loaded_group==1,"First group must start1");
    }
    require(in.p==b.size(),"Trailing source oracle data");
    std::cout<<"{\"source_cases\":2,\"arm_group_cases\":"<<count+2<<",\"checks\":"<<checks<<"}\n";
}
}
int main(int argc,char** argv){try{
    if(argc==1){boundaries();std::cout<<"{\"checks\":"<<checks<<"}\n";}
    else if(argc==5)oracle(argv[1],argv[2],argv[3],argv[4]);
    else throw std::runtime_error("Expected no args, or oracle SHA actor.mod body.mod");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
