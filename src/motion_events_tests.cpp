#include "motion_events.hpp"
#include "picosha2.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#ifdef MH4U_MOTION_EVENTS_GENERATED
#include "motion_events_identity.hpp"
#endif
#ifdef MH4U_MOTION_PREFIX_ORACLE
#include "prefix-fixtures.hpp"
#endif
namespace me=mh4u::motion_events;
namespace {
std::size_t assertions=0;
void check(bool ok,const char* text){++assertions;if(!ok)throw std::runtime_error(text);}
std::string digest(const std::string& s){return picosha2::hash256_hex_string(s);}
void u32(std::string& s,std::uint32_t v){for(unsigned i=0;i<4;++i)s.push_back(static_cast<char>((v>>(8*i))&255));}
void u16(std::string& s,std::uint16_t v){s.push_back(static_cast<char>(v&255));s.push_back(static_cast<char>(v>>8));}
void hash(std::string& s,const std::string& h){for(unsigned i=0;i<64;i+=2)s.push_back(static_cast<char>(std::stoul(h.substr(i,2),nullptr,16)));}
void at32(std::string& s,std::size_t at,std::uint32_t v){std::string bytes;u32(bytes,v);s.replace(at,4,bytes);}
#ifdef MH4U_MOTION_EVENTS_GENERATED
std::uint32_t get32(const std::string& s,std::size_t at){std::uint32_t value=0;for(unsigned i=0;i<4;++i)value|=std::uint32_t(static_cast<unsigned char>(s.at(at+i)))<<(8*i);return value;}
#endif
template<class T>T value(me::Result<T> r){if(auto e=std::get_if<me::Error>(&r))throw std::runtime_error(std::string(me::error_name(e->code))+": "+e->message);return std::get<T>(std::move(r));}
template<class T>void error(const me::Result<T>& r,me::ErrorCode code){auto e=std::get_if<me::Error>(&r);check(e&&e->code==code,"Wrong or missing error");}
struct Fixture {std::string bytes;me::ExpectedPackage expected;std::vector<std::size_t> actions,groups;std::size_t first_record{};};
Fixture fixture(){
    Fixture f;auto& s=f.bytes;auto& e=f.expected;e.inventory_sha256=std::string(64,'b');e.sources={{42,std::string(64,'c'),5}};
    for(unsigned i=0;i<5;++i){
        const auto state=i==1?me::ActionState::empty_slot:i==2?me::ActionState::events_absent:i==3?me::ActionState::rejected:me::ActionState::parsed;
        const auto presence=state==me::ActionState::parsed?me::GroupState::present:state==me::ActionState::events_absent?me::GroupState::absent:me::GroupState::unavailable;
        e.actions.push_back({{42,i,std::string(64,'c')},i==1?std::nullopt:std::optional<std::uint32_t>{i==4?1U:5U},state,
            {presence,presence,presence,presence},i==3?"unsupported_event_relocation":""});
    }
    s="MH4UME01";for(auto v:{0x01020304U,0U,1U,5U,1U,0U})u32(s,v);hash(s,e.inventory_sha256);
    u32(s,42);hash(s,e.sources[0].lmt_sha256);u32(s,5);u32(s,0);
    for(unsigned i=0;i<5;++i){
        const auto& info=e.actions[i];f.actions.push_back(s.size());
        for(auto v:{0U,i,info.frame_count.value_or(0),static_cast<unsigned>(info.state),static_cast<unsigned>(info.rejection_code.size()),4U,0U,0U})u32(s,v);
        s+=info.rejection_code;
        for(unsigned g=0;g<4;++g){
            f.groups.push_back(s.size());std::vector<me::Record> rows;
            if(info.state==me::ActionState::parsed){
                if(i==4)rows={{7,1}};
                else if(g==0)rows={{1,2},{0,1},{0x80000002,2}};
                else if(g==2)rows={{0,5}};
                else if(g==3)rows={{1,0xffffffff}};
            }
            for(auto v:{g,static_cast<unsigned>(info.groups[g]),static_cast<unsigned>(rows.size()),0U})u32(s,v);
            if(info.groups[g]==me::GroupState::present){
                for(unsigned j=0;j<32;++j)u16(s,static_cast<std::uint16_t>(j==0?0:j+500));
                if(i==0&&g==0)f.first_record=s.size();
                for(auto r:rows){u32(s,r.mask);u32(s,r.duration);}
            }
        }
    }
    at32(s,12,static_cast<unsigned>(s.size()));e.package_sha256=digest(s);return f;
}
void malformed(Fixture f,me::ErrorCode code){f.expected.package_sha256=digest(f.bytes);error(me::Package::decode(f.bytes,f.expected),code);}
#ifdef MH4U_MOTION_EVENTS_GENERATED
std::string read(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary);check(bool(f),"Cannot open fixture/oracle");return {std::istreambuf_iterator<char>(f),{}};}
#endif
void write(const std::filesystem::path& p,const std::string& s){std::ofstream f(p,std::ios::binary);f.write(s.data(),static_cast<std::streamsize>(s.size()));check(bool(f),"Cannot write temporary fixture");}
struct Temp {std::filesystem::path path=std::filesystem::temp_directory_path()/("mh4u-events-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp(){std::filesystem::create_directory(path);}~Temp(){std::error_code e;std::filesystem::remove_all(path,e);}};
void synthetic(){
    auto f=fixture();auto p=value(me::Package::decode(f.bytes,f.expected));check(p.actions()==f.expected.actions,"Action states changed");
    check(p.sources()==f.expected.sources,"Source bindings changed");auto id=f.expected.actions[0].identity;
    check(value(p.range(id,0,0,4))==0x80000003,"Inclusive OR differs");check(value(p.range(id,0,2,2))==0,"Zero mask was altered");
    check(value(p.range(id,1,0,4))==0,"Present empty group differs");check(value(p.group(id,0))->prefix_raw[0]==0,"Prefix zero lost");
    check(value(p.group(id,0))->prefix_raw[31]==531,"Prefix uint16 changed");check(value(p.group(id,1))->records.empty(),"Present empty records lost");
    check(!value(p.first_prefix(id,0,0)),"Zero mask acquired a prefix hit");
    check(value(p.first_prefix(id,0,1))==me::PrefixHit{0,0},"Selected prefix zero became absence");
    check(value(p.first_prefix(id,0,0x80000000))==me::PrefixHit{31,531},"High mask bit was lost");
    check(value(p.first_prefix(id,0,0x80000004))==me::PrefixHit{2,502},"Multiple bits did not select lowest index");
    check(value(p.first_prefix(id,1,2))==me::PrefixHit{1,501},"Present empty records hid their prefix");
    check(value(p.first_prefix(id,3,2))==me::PrefixHit{1,501},"Duration overflow incorrectly blocked raw prefix lookup");
    for(unsigned bit=0;bit<32;++bit)check(value(p.first_prefix(id,0,1U<<bit))==me::PrefixHit{bit,static_cast<std::uint16_t>(bit?500+bit:0)},"Single-bit prefix differs");
    check(value(p.kernel_range(id,0,-1,-1))==1,"Negative query clamped");check(value(p.kernel_range(id,0,4,1))==0,"Reversed query sorted");
    check(value(p.kernel_range(id,0,std::numeric_limits<std::int32_t>::min(),std::numeric_limits<std::int32_t>::max()))==0x80000003,"Signed limits differ");
    check(value(p.kernel_range(id,0,std::numeric_limits<std::int32_t>::max(),std::numeric_limits<std::int32_t>::min()))==0,"Signed reverse limits differ");
    check(value(p.kernel_range(f.expected.actions[4].identity,0,-1,-1))==7,"N1 negative event query differs");
    error(p.range(f.expected.actions[4].identity,0,-1,-1),me::ErrorCode::invalid_interval);
    error(p.range(id,3,0,4),me::ErrorCode::unsupported_accumulator);check(!value(p.group(id,3))->kernel_supported,"Overflow availability lost");
    for(auto pair:std::array<std::array<std::int64_t,2>,5>{{{-1,0},{0,5},{3,1},{0,std::numeric_limits<std::int64_t>::max()},{std::numeric_limits<std::int64_t>::min(),0}}})
        error(p.range(id,0,pair[0],pair[1]),me::ErrorCode::invalid_interval);
    error(p.range(f.expected.actions[1].identity,0,0,0),me::ErrorCode::empty_slot);
    error(p.kernel_range(f.expected.actions[2].identity,0,0,0),me::ErrorCode::events_absent);
    error(p.group(f.expected.actions[3].identity,0),me::ErrorCode::events_rejected);
    error(p.first_prefix(f.expected.actions[1].identity,0,0),me::ErrorCode::empty_slot);
    error(p.first_prefix(f.expected.actions[2].identity,0,0),me::ErrorCode::events_absent);
    error(p.first_prefix(f.expected.actions[3].identity,0,1),me::ErrorCode::events_rejected);
    error(p.first_prefix(id,4,0),me::ErrorCode::invalid_group);
    error(p.group(id,4),me::ErrorCode::invalid_group);
    auto wrong=id;wrong.lmt_sha256=std::string(64,'d');error(p.range(wrong,0,0,0),me::ErrorCode::action_identity_mismatch);
    error(p.first_prefix(wrong,0,0),me::ErrorCode::action_identity_mismatch);
    wrong.slot=99;error(p.range(wrong,0,0,0),me::ErrorCode::missing_action);
    error(p.first_prefix(wrong,0,1),me::ErrorCode::missing_action);
    auto z=f;at32(z.bytes,f.first_record+4,1);at32(z.bytes,f.first_record+8,2);at32(z.bytes,f.first_record+12,0);at32(z.bytes,f.first_record+16,4);at32(z.bytes,f.first_record+20,4);
    z.expected.package_sha256=digest(z.bytes);auto zp=value(me::Package::decode(z.bytes,z.expected));check(value(zp.kernel_range(id,0,0,1))==7,"Zero duration kernel changed");
    auto bad=f;bad.bytes[0]='X';error(me::Package::decode(bad.bytes,bad.expected),me::ErrorCode::package_hash_mismatch);malformed(bad,me::ErrorCode::unsupported_format);
    for(auto offset:{8U,24U,28U,104U}){bad=f;at32(bad.bytes,offset,9);malformed(bad,me::ErrorCode::unsupported_format);}
    bad=f;at32(bad.bytes,12,1);malformed(bad,me::ErrorCode::malformed_package);
    bad=f;at32(bad.bytes,16,0xffffffff);malformed(bad,me::ErrorCode::resource_limit);
    bad=f;at32(bad.bytes,20,0xffffffff);malformed(bad,me::ErrorCode::resource_limit);
    bad=f;bad.bytes[32]='\x11';malformed(bad,me::ErrorCode::binding_mismatch);
    bad=f;bad.bytes[68]='\x11';malformed(bad,me::ErrorCode::binding_mismatch);
    bad=f;at32(bad.bytes,f.actions[0],2);malformed(bad,me::ErrorCode::malformed_package);
    bad=f;at32(bad.bytes,f.actions[0]+8,6);malformed(bad,me::ErrorCode::action_identity_mismatch);
    bad=f;at32(bad.bytes,f.actions[0]+12,9);malformed(bad,me::ErrorCode::malformed_package);
    bad=f;at32(bad.bytes,f.actions[0]+16,0xffffffff);malformed(bad,me::ErrorCode::resource_limit);
    bad=f;at32(bad.bytes,f.actions[0]+20,3);malformed(bad,me::ErrorCode::malformed_package);
    bad=f;at32(bad.bytes,f.actions[0]+24,1);malformed(bad,me::ErrorCode::unsupported_format);
    bad=f;at32(bad.bytes,f.actions[1]+4,0);malformed(bad,me::ErrorCode::duplicate_identity);
    bad=f;at32(bad.bytes,f.groups[1],0);malformed(bad,me::ErrorCode::duplicate_group);
    bad=f;at32(bad.bytes,f.groups[0]+4,9);malformed(bad,me::ErrorCode::malformed_package);
    bad=f;at32(bad.bytes,f.groups[0]+8,0xffffffff);malformed(bad,me::ErrorCode::resource_limit);
    bad=f;at32(bad.bytes,f.groups[0]+8,5000);malformed(bad,me::ErrorCode::malformed_package);
    bad=f;at32(bad.bytes,f.groups[0]+12,1);malformed(bad,me::ErrorCode::unsupported_format);
    bad=f;at32(bad.bytes,f.groups[4]+8,1);malformed(bad,me::ErrorCode::malformed_package);
    bad=f;bad.bytes+='x';at32(bad.bytes,12,static_cast<unsigned>(bad.bytes.size()));malformed(bad,me::ErrorCode::malformed_package);
    for(std::size_t cut=0;cut<f.bytes.size();++cut){bad=f;bad.bytes.resize(cut);if(cut>=16)at32(bad.bytes,12,static_cast<unsigned>(cut));bad.expected.package_sha256=digest(bad.bytes);
        check(std::holds_alternative<me::Error>(me::Package::decode(bad.bytes,bad.expected)),"Truncation accepted");}
    auto e=f.expected;e.sources[0].lmt_sha256=std::string(64,'d');error(me::Package::decode(f.bytes,e),me::ErrorCode::invalid_expected);
    e=f.expected;e.actions[1].identity=e.actions[0].identity;error(me::Package::decode(f.bytes,e),me::ErrorCode::duplicate_identity);
    e=f.expected;e.actions[1].frame_count=1;error(me::Package::decode(f.bytes,e),me::ErrorCode::invalid_expected);
    e=f.expected;e.actions[3].rejection_code.clear();error(me::Package::decode(f.bytes,e),me::ErrorCode::invalid_expected);
    auto limits=me::Limits{};limits.records=2;error(me::Package::decode(f.bytes,f.expected,limits),me::ErrorCode::resource_limit);
    limits={};limits.package_bytes=1;error(me::Package::decode(f.bytes,f.expected,limits),me::ErrorCode::resource_limit);
    limits={};limits.actions=4;error(me::Package::decode(f.bytes,f.expected,limits),me::ErrorCode::resource_limit);
    Temp t;write(t.path/"event.bin",f.bytes);value(me::Package::load(t.path/"event.bin",f.expected));
    write(t.path/"event.bin",f.bytes+"x");error(me::Package::load(t.path/"event.bin",f.expected),me::ErrorCode::package_hash_mismatch);
    error(me::Package::load(t.path/"missing.bin",f.expected),me::ErrorCode::io);
}
#ifdef MH4U_MOTION_PREFIX_ORACLE
void prefix_oracle(){
    std::size_t compared=0;
    for(std::size_t index=0;index<prefix_oracle_data::tables.size();++index){
        auto f=fixture();std::string table;
        for(auto v:prefix_oracle_data::tables[index])u16(table,v);
        f.bytes.replace(f.groups[0]+16,64,table);f.expected.package_sha256=digest(f.bytes);
        const auto p=value(me::Package::decode(f.bytes,f.expected));const auto id=f.expected.actions[0].identity;
        for(const auto& row:prefix_oracle_data::cases){
            if(row.table!=index)continue;
            const auto hit=value(p.first_prefix(id,0,row.mask));
            if(row.bit<0)check(!hit,"ARM zero-mask case acquired a hit");
            else check(hit==me::PrefixHit{static_cast<std::uint32_t>(row.bit),row.output},"Native prefix differs from original ARM consumer");
            ++compared;
        }
    }
    check(compared==855,"Missing comparable present-table ARM cases");
    std::cout<<"{\"prefix_arm_present_cases\":"<<compared<<",\"arm_absent_cases_excluded\":171,\"prefix_mismatches\":0}\n";
}
#endif
#ifdef MH4U_MOTION_EVENTS_GENERATED
void real(const char* binary,const char* oracle){
    auto expected=me::generated::expected_package();auto p=value(me::Package::load(binary,expected));check(p.actions()==expected.actions,"Real states differ");
    std::ifstream input(oracle);check(bool(input),"Cannot open real oracle");std::string line;std::size_t strict=0,extended=0;
    while(std::getline(input,line)){
        std::istringstream row(line);std::size_t index;unsigned group,mode;std::int64_t begin,end;std::uint64_t mask;std::string trailing;
        check(bool(row>>index>>group>>mode>>begin>>end>>mask)&&!(row>>trailing)&&index<p.actions().size()&&group<4&&mode<=1&&mask<=0xffffffffULL&&
              begin>=std::numeric_limits<std::int32_t>::min()&&begin<=std::numeric_limits<std::int32_t>::max()&&
              end>=std::numeric_limits<std::int32_t>::min()&&end<=std::numeric_limits<std::int32_t>::max(),"Malformed oracle row");
        const auto& a=p.actions()[index];
        check(value(p.kernel_range(a.identity,group,static_cast<std::int32_t>(begin),static_cast<std::int32_t>(end)))==mask,"Native raw kernel differs from ARM/owner");
        if(mode==0){check(value(p.range(a.identity,group,begin,end))==mask,"Native strict range differs");++strict;}
        else {if(begin<0||begin>end||end>=*a.frame_count)error(p.range(a.identity,group,begin,end),me::ErrorCode::invalid_interval);++extended;}
    }
    check(strict==89697&&extended==7320,"Missing real oracle coverage");
    std::size_t empty=0,present=0,records=0;
    for(const auto& a:p.actions()){
        if(a.state==me::ActionState::empty_slot){++empty;error(p.range(a.identity,0,0,0),me::ErrorCode::empty_slot);continue;}
        ++present;for(unsigned g=0;g<4;++g){const auto* group=value(p.group(a.identity,g));records+=group->records.size();check(group->duration_sum==*a.frame_count,"Real duration sum differs");}
    }
    check(empty==82&&present==122&&records==701,"Real slot/record coverage differs");
    const auto bytes=read(binary);std::set<std::size_t> cuts;std::size_t offset=64+44*expected.sources.size();
    for(std::size_t i=0;i<expected.actions.size();++i){
        for(unsigned n=0;n<32;++n)cuts.insert(offset+n);const auto reason=get32(bytes,offset+16);offset+=32+reason;
        for(unsigned g=0;g<4;++g){
            cuts.insert(offset);cuts.insert(offset+15);const auto presence=get32(bytes,offset+4),count=get32(bytes,offset+8);
            auto mutated=bytes;at32(mutated,offset+8,0xffffffff);auto pinned=expected;pinned.package_sha256=digest(mutated);
            check(std::holds_alternative<me::Error>(me::Package::decode(mutated,pinned)),"Hostile real group count accepted");
            offset+=16;if(presence==2){cuts.insert(offset);cuts.insert(offset+63);offset+=64;
                for(unsigned n=0;n<count;++n){cuts.insert(offset);cuts.insert(offset+7);offset+=8;}}
        }
    }
    check(offset==bytes.size(),"Real binary walk disagrees with producer");
    for(auto cut:cuts){auto part=bytes.substr(0,cut);if(cut>=16)at32(part,12,static_cast<unsigned>(cut));auto pin=expected;pin.package_sha256=digest(part);
        check(std::holds_alternative<me::Error>(me::Package::decode(part,pin)),"Real boundary truncation accepted");}
    std::cout<<"{\"strict_queries\":"<<strict<<",\"signed_queries\":"<<extended<<",\"mask_mismatches\":0,\"real_truncations\":"<<cuts.size()<<"}\n";
}
#endif
}
int main(int argc,char** argv){try{synthetic();
#ifdef MH4U_MOTION_PREFIX_ORACLE
    prefix_oracle();
#endif
#ifdef MH4U_MOTION_EVENTS_GENERATED
    if(argc==3)real(argv[1],argv[2]);else if(argc!=1)throw std::runtime_error("Usage: motion-events-tests [binary oracle.tsv]");
#else
    (void)argv;if(argc!=1)throw std::runtime_error("Compile with MH4U_MOTION_EVENTS_GENERATED for local real resources");
#endif
    std::cout<<"{\"status\":\"passed\",\"assertions\":"<<assertions<<"}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
