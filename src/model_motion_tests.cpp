#include "model_motion.hpp"
#include "picosha2.h"
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#ifdef MH4U_MODEL_MOTION_GENERATED
#include "model_motion_identity.hpp"
#endif

namespace mm=mh4u::model_motion;
namespace {
std::size_t assertions=0;
void check(bool condition,const char* text) { ++assertions;if(!condition)throw std::runtime_error(text); }
std::string digest(const std::string& bytes){return picosha2::hash256_hex_string(bytes);}
void u32(std::string& s,std::uint32_t value){for(unsigned i=0;i<4;++i)s.push_back(static_cast<char>((value>>(8*i))&255));}
void at32(std::string& s,std::size_t at,std::uint32_t value){std::string encoded;u32(encoded,value);s.replace(at,4,encoded);}
void f64(std::string& s,double value){const auto bits=std::bit_cast<std::uint64_t>(value);for(unsigned i=0;i<8;++i)s.push_back(static_cast<char>((bits>>(8*i))&255));}
void at64(std::string& s,std::size_t at,double value){std::string encoded;f64(encoded,value);s.replace(at,8,encoded);}
void hash(std::string& s,const std::string& value){for(unsigned i=0;i<64;i+=2)s.push_back(static_cast<char>(std::stoul(value.substr(i,2),nullptr,16)));}
void binding(std::string& s,const mm::AssetBinding& b){u32(s,static_cast<std::uint32_t>(b.relative_path.size()));s+=b.relative_path;hash(s,b.sha256);}
struct Fixture {
    std::string bytes;mm::ExpectedPackage expected;
    std::size_t first{},translation{},key0{},key1{},rotation{},second{};
};
Fixture fixture() {
    Fixture f;auto& e=f.expected;auto& s=f.bytes;
    e.gltf={"joints/animated.gltf",digest("gltf\n")};e.curves={"model-curves.json",digest("curves\n")};
    e.dependencies={{"joints/animation.bin",digest("animation\n")}};
    e.actions={{{42,7,std::string(64,'c')},5,2,3,7},{{43,0,std::string(64,'d')},1,-1,0,8}};
    s="MH4UMC02";u32(s,0x01020304);u32(s,0);u32(s,2);u32(s,0);binding(s,e.gltf);binding(s,e.curves);
    u32(s,1);binding(s,e.dependencies[0]);
    const auto action=[&](const mm::ActionInfo& a,unsigned channels){
        u32(s,a.identity.entry_id);u32(s,a.identity.slot);hash(s,a.identity.lmt_sha256);
        u32(s,a.frame_count);u32(s,std::bit_cast<std::uint32_t>(a.loop_frame_raw));u32(s,a.presence_mask);u32(s,channels);u32(s,a.gltf_clip_index);u32(s,0);
    };
    f.first=s.size();action(e.actions[0],2);
    f.translation=s.size();u32(s,1);u32(s,1);u32(s,9);u32(s,2);
    f.key0=s.size();for(double v:{0.,1.,2.,3.})f64(s,v);
    f.key1=s.size();for(double v:{4.,9.,10.,11.})f64(s,v);
    f.rotation=s.size();u32(s,2);u32(s,0);u32(s,10);u32(s,2);
    for(double v:{0.,0.,0.,0.,2.,4.,0.,0.,0.,2.})f64(s,v);
    f.second=s.size();action(e.actions[1],0);at32(s,12,static_cast<std::uint32_t>(s.size()));e.package_sha256=digest(s);return f;
}
template<class T> T value(mm::Result<T> result) {
    if(auto e=std::get_if<mm::Error>(&result))throw std::runtime_error(std::string(mm::error_name(e->code))+": "+e->message);
    return std::get<T>(std::move(result));
}
template<class T> void error(const mm::Result<T>& result,mm::ErrorCode expected) {
    const auto actual=std::get_if<mm::Error>(&result);check(actual&&actual->code==expected,"Wrong/missing error code");
}
void malformed(Fixture f,mm::ErrorCode expected) { f.expected.package_sha256=digest(f.bytes);error(mm::Package::decode(f.bytes,f.expected),expected); }
void write(const std::filesystem::path& p,const std::string& bytes){std::ofstream f(p,std::ios::binary);f.write(bytes.data(),static_cast<std::streamsize>(bytes.size()));check(bool(f),"Fixture write failed");}
struct Temp {
    std::filesystem::path path=std::filesystem::temp_directory_path()/ ("mh4u-model-curves-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp(){std::filesystem::create_directory(path);}~Temp(){std::error_code ec;std::filesystem::remove_all(path,ec);}
};
void synthetic() {
    auto f=fixture();auto p=value(mm::Package::decode(f.bytes,f.expected));
    check(p.actions()==f.expected.actions,"Action metadata was lost");
    check(p.actions()[0].gltf_clip_index==7,"Clip binding was lost");
    for(double frame:{0.,.5,1.,2.5,4.}) {
        auto s=value(p.sample(f.expected.actions[0].identity,frame));
        check(s.translation==mm::Vector{1+2*frame,2+2*frame,3+2*frame},"Fractional vector sample differs");
        check(s.rotation==mm::Quaternion{0,0,0,2},"Raw nonunit quaternion changed");
    }
    auto absent=value(p.sample(f.expected.actions[1].identity,0));check(!absent.translation&&!absent.rotation,"Absent channels received defaults");
    auto degenerate=f;at64(degenerate.bytes,f.rotation+48,0);at64(degenerate.bytes,f.rotation+88,0);
    degenerate.expected.package_sha256=digest(degenerate.bytes);
    auto raw_zero=value(value(mm::Package::decode(degenerate.bytes,degenerate.expected)).sample(degenerate.expected.actions[0].identity,2));
    check(raw_zero.rotation==mm::Quaternion{0,0,0,0},"Present zero quaternion was replaced by a fallback");
    auto wrong=f.expected.actions[0].identity;wrong.lmt_sha256=std::string(64,'b');error(p.sample(wrong,0),mm::ErrorCode::action_identity_mismatch);
    wrong.slot=999;error(p.sample(wrong,0),mm::ErrorCode::missing_action);
    for(double frame:{-1.,std::nextafter(0.,-1.),std::nextafter(4.,5.),5.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
        error(p.sample(f.expected.actions[0].identity,frame),mm::ErrorCode::invalid_sample_frame);
    auto bad=f;bad.bytes[0]='X';error(mm::Package::decode(bad.bytes,bad.expected),mm::ErrorCode::package_hash_mismatch);
    malformed(bad,mm::ErrorCode::unsupported_format);
    bad=f;at32(bad.bytes,8,0x04030201);malformed(bad,mm::ErrorCode::unsupported_format);
    bad=f;at32(bad.bytes,20,1);malformed(bad,mm::ErrorCode::unsupported_format);
    bad=f;at32(bad.bytes,f.first+60,1);malformed(bad,mm::ErrorCode::unsupported_format);
    bad=f;at32(bad.bytes,f.first+56,8);malformed(bad,mm::ErrorCode::action_identity_mismatch);
    bad=f;at32(bad.bytes,f.translation+12,0xffffffff);malformed(bad,mm::ErrorCode::resource_limit);
    bad=f;at32(bad.bytes,f.translation+12,0);malformed(bad,mm::ErrorCode::resource_limit);
    bad=f;at32(bad.bytes,f.rotation,1);malformed(bad,mm::ErrorCode::duplicate_component);
    bad=f;at32(bad.bytes,f.rotation+4,1);malformed(bad,mm::ErrorCode::unsupported_format);
    bad=f;at32(bad.bytes,f.translation,4);malformed(bad,mm::ErrorCode::unsupported_format);
    bad=f;at64(bad.bytes,f.key0,std::numeric_limits<double>::infinity());malformed(bad,mm::ErrorCode::nonfinite_value);
    bad=f;at64(bad.bytes,f.key0+8,std::numeric_limits<double>::quiet_NaN());malformed(bad,mm::ErrorCode::nonfinite_value);
    bad=f;at64(bad.bytes,f.key0,1);malformed(bad,mm::ErrorCode::invalid_timeline);
    bad=f;at64(bad.bytes,f.key1,0);malformed(bad,mm::ErrorCode::invalid_timeline);
    bad=f;at64(bad.bytes,f.key1,3);malformed(bad,mm::ErrorCode::invalid_timeline);
    bad=f;at32(bad.bytes,f.translation+4,0);malformed(bad,mm::ErrorCode::malformed_package);
    bad=f;bad.bytes[28]='/';malformed(bad,mm::ErrorCode::malformed_package);
    bad=f;at32(bad.bytes,24,0xffffffff);malformed(bad,mm::ErrorCode::resource_limit);
    bad=f;at32(bad.bytes,f.second,42);at32(bad.bytes,f.second+4,7);malformed(bad,mm::ErrorCode::duplicate_identity);
    bad=f;bad.bytes+='x';at32(bad.bytes,12,static_cast<std::uint32_t>(bad.bytes.size()));malformed(bad,mm::ErrorCode::malformed_package);
    for(std::size_t cut=0;cut<f.bytes.size();++cut) {
        bad=f;bad.bytes.resize(cut);if(cut>=16)at32(bad.bytes,12,static_cast<std::uint32_t>(cut));bad.expected.package_sha256=digest(bad.bytes);
        check(std::holds_alternative<mm::Error>(mm::Package::decode(bad.bytes,bad.expected)),"Truncated package accepted");
    }
    auto limits=mm::Limits{};limits.keys=3;error(mm::Package::decode(f.bytes,f.expected,limits),mm::ErrorCode::resource_limit);
    limits={};limits.package_bytes=10;error(mm::Package::decode(f.bytes,f.expected,limits),mm::ErrorCode::resource_limit);
    auto expected=f.expected;expected.gltf.relative_path="../escape";error(mm::Package::decode(f.bytes,expected),mm::ErrorCode::invalid_expected);
    expected=f.expected;expected.curves.sha256=std::string(64,'f');error(mm::Package::decode(f.bytes,expected),mm::ErrorCode::binding_mismatch);
    expected=f.expected;expected.dependencies.clear();error(mm::Package::decode(f.bytes,expected),mm::ErrorCode::binding_mismatch);
    expected=f.expected;expected.dependencies.push_back(expected.gltf);error(mm::Package::decode(f.bytes,expected),mm::ErrorCode::duplicate_identity);
    expected=f.expected;expected.actions[1].identity=expected.actions[0].identity;error(mm::Package::decode(f.bytes,expected),mm::ErrorCode::duplicate_identity);
    Temp temp;const auto root=temp.path/"assets";std::filesystem::create_directories(root/"joints");
    write(root/"joints/animated.gltf","gltf\n");write(root/"model-curves.json","curves\n");write(temp.path/"package.bin",f.bytes);
    write(root/"joints/animation.bin","animation\n");
    value(mm::Package::load(temp.path/"package.bin",f.expected,root));check(true,"Load failed");
    write(root/"joints/animation.bin","changed\n");error(mm::Package::load(temp.path/"package.bin",f.expected,root),mm::ErrorCode::binding_mismatch);
    write(root/"joints/animation.bin","animation\n");
    write(root/"model-curves.json","wrong\n");error(mm::Package::load(temp.path/"package.bin",f.expected,root),mm::ErrorCode::binding_mismatch);
    write(root/"model-curves.json","curves\n");limits={};limits.bound_asset_bytes=1;error(mm::Package::load(temp.path/"package.bin",f.expected,root,limits),mm::ErrorCode::resource_limit);
    limits={};limits.bound_asset_bytes=13;error(mm::Package::load(temp.path/"package.bin",f.expected,root,limits),mm::ErrorCode::resource_limit);
    std::filesystem::remove(root/"model-curves.json");write(temp.path/"outside.json","curves\n");
    std::filesystem::create_symlink(temp.path/"outside.json",root/"model-curves.json");
    error(mm::Package::load(temp.path/"package.bin",f.expected,root),mm::ErrorCode::binding_mismatch);
}
double number(const std::string& token){char* end=nullptr;const double value=std::strtod(token.c_str(),&end);check(end==token.c_str()+token.size(),"Invalid oracle number");return value;}
#ifdef MH4U_MODEL_MOTION_GENERATED
void real(const char* binary,const char* root,const char* oracle) {
    const auto expected=mm::generated::expected_package();auto package=value(mm::Package::load(binary,expected,root));
    check(package.actions()==expected.actions,"Real metadata differs");std::ifstream in(oracle);check(bool(in),"Cannot open oracle");
    std::string line;std::size_t queries=0,components=0;double maximum=0,sum_squared=0;
    while(std::getline(in,line)) {
        std::istringstream row(line);std::size_t index;std::string token;check(bool(row>>index>>token)&&index<package.actions().size(),"Invalid oracle identity");
        const double frame=number(token);auto sample=value(package.sample(package.actions()[index].identity,frame));
        for(unsigned component=0;component<2;++component) {
            unsigned present;check(bool(row>>present)&&present<=1,"Invalid oracle presence");
            check(bool(present)==(component==0?sample.translation.has_value():sample.rotation.has_value()),"Presence differs from oracle");
            if(!present)continue;
            for(unsigned j=0;j<(component==0?3U:4U);++j) {
                check(bool(row>>token),"Truncated oracle component");const double target=number(token);
                const double actual=component==0?(*sample.translation)[j]:(*sample.rotation)[j];
                const double difference=std::abs(actual-target);check(difference<=1e-11,"Native model sample differs from owner oracle");
                maximum=std::max(maximum,difference);sum_squared+=difference*difference;++components;
            }
        }
        check(!(row>>token),"Trailing oracle fields");++queries;
    }
    check(queries==2055,"Missing real source/boundary queries");
    for(const auto& action:package.actions()) {
        for(double frame:{std::nextafter(0.,-1.),std::nextafter(double(action.frame_count-1),std::numeric_limits<double>::infinity())})
            error(package.sample(action.identity,frame),mm::ErrorCode::invalid_sample_frame);
    }
    std::cout<<std::setprecision(17)<<"{\"real_queries\":"<<queries<<",\"numeric_components\":"<<components
             <<",\"max_absolute_error\":"<<maximum<<",\"rms_error\":"<<std::sqrt(sum_squared/components)<<"}\n";
}
#endif
}
int main(int argc,char** argv) {
    try {
        synthetic();
#ifdef MH4U_MODEL_MOTION_GENERATED
        if(argc==4)real(argv[1],argv[2],argv[3]);else if(argc!=1)throw std::runtime_error("Usage: model-motion-tests [binary paired-asset-root oracle.tsv]");
#else
        (void)argv;if(argc!=1)throw std::runtime_error("Recompile with MH4U_MODEL_MOTION_GENERATED and the local identity header for real resources");
#endif
        std::cout<<"{\"status\":\"passed\",\"assertions\":"<<assertions<<"}\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
