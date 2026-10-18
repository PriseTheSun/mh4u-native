#include "player_parameter_query.hpp"
#include "picosha2.h"
#include <algorithm>
#include <cfenv>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace q=mh4u::player_parameter_query;
namespace pf=mh4u::player_fields;
namespace {
std::size_t checks{};
void check(bool condition,const char* label){++checks;if(!condition)throw std::runtime_error(label);}
struct Blob {
    std::vector<std::byte> bytes;
    std::array<char,64> hash{};
    explicit Blob(std::size_t n):bytes(n){}
    void put(std::size_t o,std::uint32_t v,unsigned n=1){for(unsigned i=0;i<n;++i)bytes.at(o+i)=std::byte(v>>(8*i));}
    q::Bytes view(){std::string raw(reinterpret_cast<const char*>(bytes.data()),bytes.size());const auto digest=picosha2::hash256_hex_string(raw);std::copy(digest.begin(),digest.end(),hash.begin());return{bytes,std::string_view(hash.data(),hash.size()),0};}
};
void put(pf::Descriptor28& d,std::size_t o,std::uint32_t v,unsigned n=1){for(unsigned i=0;i<n;++i)d.at(o+i)=static_cast<std::uint8_t>(v>>(8*i));}
pf::Fields fields(){pf::Fields f{};for(auto& d:f.pbw.descriptors)d[0]=6;f.pbw.field380_bits=0x7fa12345;f.pbw.words3d8[2][1]=0x7f;return f;}
q::Output value(const q::Result& r){if(auto e=std::get_if<q::Error>(&r))throw std::runtime_error(std::string(e->field)+" descriptor="+std::to_string(e->descriptor.value_or(99))+" offset="+std::to_string(e->offset));return std::get<q::Output>(r);}
q::Error error(const q::Result& r,q::ErrorCode expected){check(std::holds_alternative<q::Error>(r),"Expected typed error");const auto e=std::get<q::Error>(r);check(e.code==expected,"Wrong typed error");return e;}
std::int32_t s16(std::uint32_t v){v&=65535;return v>=32768?static_cast<std::int32_t>(v)-65536:static_cast<std::int32_t>(v);}
void run(){
    q::Resources r{};auto f=fields();
    for(auto selector:{19U,0xffffffffU}){const auto o=value(q::apply({},selector,{}));check(o.value==0&&o.effects.write_count==0&&o.fields==pf::Fields{},"Out-of-switch selectors preserve unknown state");}
    for(unsigned selector=7;selector<19;++selector)error(q::apply({},selector,{}),q::ErrorCode::unsupported_selector);
    for(unsigned selector=1;selector<=5;++selector){const auto o=value(q::apply(f,selector,r));check(o.value==0&&o.fields==f&&o.effects.write_count==0,"Kind6 skips all resource/word reads");}
    auto missing=f;missing.pbw.descriptors[3][0].reset();const auto unknown=error(q::apply(missing,1,r),q::ErrorCode::unknown_field);check(unknown.descriptor==3&&unknown.offset==0,"Descriptor error identity");
    Blob caps(512),limits(12),primary(80),record24(48),record40(80),special12(48),special4(8),special10(20),growth(16),steps(8),global(0x4100);
    for(unsigned k=0;k<256;++k)caps.put(2*k,1000,2);
    for(unsigned k=0;k<6;++k)limits.put(2*k,100,2);
    r.caps_by_kind=caps.view();r.query_caps=limits.view();
    f.pbw.descriptors[0][0]=0;f.pbw.words9404[0]=0xffffffffU;
    const auto zero=value(q::apply(f,6,r));check(zero.value==0&&zero.fields.pbw.words9404[0]==0xffffffbfU,"Unsupported category6 sourcezero still writes kindword");
    const auto primary_null=value(q::apply(f,0,r));check(primary_null.value==1&&primary_null.fields.pbw.words9404[0]==0xfffffffeU,"Unknown category0 getter null fallback1");
    f.pbw.descriptors[0][0]=7;put(f.pbw.descriptors[0],2,1,2);put(f.pbw.descriptors[0],18,0x4000,2);f.pbw.words9404[7]=0xffff0000U;
    record24.put(24+8,150,2);record24.put(24+10,25);r.records24[7]=record24.view();
    const auto ordinary6=value(q::apply(f,6,r));check(ordinary6.value==170&&ordinary6.effects.writes[0].kind==7,"Normal query6 consumes+20");
    put(f.pbw.descriptors[0],18,0x8000,2);check(value(q::apply(f,0,r)).value==85,"Normal query0 consumes+60");
    r.records24[7]=q::Absent{};check(value(q::apply(f,0,r)).value==60,"Known-null query0 plus60");error(q::apply(f,6,r),q::ErrorCode::absent_resource);
    r.records24[7]=q::Unknown{};const auto er=error(q::apply(f,0,r),q::ErrorCode::unknown_resource);check(er.table==q::Table::records24&&er.kind==7,"Provider error identity");
    r.records24[7]=q::Bytes{std::span(record24.bytes).first(30),std::string_view(record24.hash.data(),record24.hash.size()),0};const auto range=error(q::apply(f,6,r),q::ErrorCode::resource_extent);check(range.offset==46&&range.size==1,"Predicate consumes flag before ordinary value");
    r.records24[7]=q::Bytes{record24.bytes,"bad",0};error(q::apply(f,6,r),q::ErrorCode::invalid_provenance);
    record24.put(24+22,0x80);r.records24[7]=record24.view();put(f.pbw.descriptors[0],12,0,2);put(f.pbw.descriptors[0],14,0x100,2);
    special12.put(0,123,2);special12.put(4,24,2);r.special12=special12.view();
    put(f.pbw.descriptors[0],18,0x4000,2);check(value(q::apply(f,6,r)).value==133,"Special query6 subtype10 but no normal20");
    put(f.pbw.descriptors[0],18,0x8000,2);check(value(q::apply(f,0,r)).value==24,"Special query0 skips normal60");
    // Exhaustive signed16 source division bonuses: no host floating arithmetic.
    // This is a native arithmetic-domain check, not65536 ARM executions.
    for(auto k:{11U,12U}){
        auto a=fields();put(a.pbw.descriptors[0],0,k);put(a.pbw.descriptors[0],1,0x20);put(a.pbw.descriptors[0],2,0,2);put(a.pbw.descriptors[0],18,0,2);a.pbw.words9404[k]=0;
        caps.put(2*k,32767,2);r.caps_by_kind=caps.view();
        for(std::int32_t n=-32768;n<=32767;++n){
            record40.put(12,static_cast<std::uint32_t>(n),2);r.records40[k]=record40.view();
            const auto result=value(q::apply(a,6,r));const auto expected=s16(static_cast<std::uint32_t>(n+n/(k==11?15:10)));
            check(result.value==expected,"Signed bonus division/wrap domain");
        }
    }
    // Repeated kind writes alias in order. Capping replaces prior accumulator.
    f=fields();for(unsigned i=1;i<=3;++i){put(f.pbw.descriptors[i],0,1);put(f.pbw.descriptors[i],2,i==2?1:0,2);}f.pbw.words9404[1]=0x400;
    primary.put(12,4);primary.put(40+12,8);r.primary40[1]=primary.view();limits.put(2,5,2);r.query_caps=limits.view();
    auto o=value(q::apply(f,1,r));check(o.value==17,"Cap resets accumulator before adding original component");check(o.effects.write_count==3&&o.effects.writes[0].after==0x400&&o.effects.writes[1].before==0x400&&o.effects.writes[1].after==0x402&&o.effects.writes[2].before==0x402&&o.effects.writes[2].after==0x400,"Aliased word stores ordered");
    check(o.fields.pbw.words3d8==f.pbw.words3d8&&o.fields.pbw.field380_bits==f.pbw.field380_bits&&o.fields.pbw.descriptors==f.pbw.descriptors,"Partial bytes and raw NaN unconsumed preserved");
    // Late unknown source has no published prefix. Query input is immutable.
    const auto before=f;f.pbw.descriptors[3][2].reset();const auto bad=f;error(q::apply(f,1,r),q::ErrorCode::unknown_field);check(f==bad,"Late failure preserves full input");f=before;
    f.pbw.words9404[1].reset();error(q::apply(f,1,r),q::ErrorCode::unknown_field);f=before;
    // Generated/source table selector ordering and invalid-index lazyzero.
    for(unsigned i=2;i<7;++i)f.pbw.descriptors[i][0]=6;
    primary.put(6,0x40);r.primary40[1]=primary.view();put(f.pbw.descriptors[1],22,3,2);
    for(unsigned i=0;i<6;++i)global.put(0x3366+24+i,10+i);r.global=global.view();
    for(unsigned k=1;k<=5;++k)limits.put(2*k,300,2);r.query_caps=limits.view();
    for(unsigned k=1;k<=5;++k){constexpr std::array<int,5> expected{11,12,13,15,14};check(value(q::apply(f,k,r)).value==expected[k-1],"Generated selector mapping");}
    put(f.pbw.descriptors[1],22,420,2);r.global=q::Unknown{};check(value(q::apply(f,1,r)).value==0,"Generated invalid index returnszero before singleton");
    primary.put(6,0x80);r.primary40[1]=primary.view();put(f.pbw.descriptors[1],12,1,2);for(unsigned i=0;i<5;++i)special10.put(10+2*i,100+i,2);r.special10=special10.view();
    for(unsigned k=1;k<=5;++k){constexpr std::array<int,5> expected{100,101,102,104,103};check(value(q::apply(f,k,r)).value==expected[k-1],"Special selector mapping");}
    // Selector0 growth: ordinal0 bypasses provider; >0 uses original count
    // low8 sum before cap, and each loop-position cap is independent of kind.
    f=fields();f.pbw.descriptors[0][0]=0;f.pbw.words9404[0]=0;f.pbw.words9404[1]=0;put(f.pbw.descriptors[1],0,1);put(f.pbw.descriptors[1],2,0,2);
    r.primary40[1]=q::Unknown{};caps.put(2,1000,2);r.caps_by_kind=caps.view();check(value(q::apply(f,0,r)).value==1,"Secondary ordinalzero returns before provider");
    put(f.pbw.descriptors[1],2,1,2);put(f.pbw.descriptors[1],1,255);primary.put(40+4,0);primary.put(40+7,10);primary.put(40+20,255);primary.put(40+21,1);r.primary40[1]=primary.view();
    check(value(q::apply(f,0,r)).value==11,"Wrapped countsum256 limitslevelzero and ignores unknown incrementtable");
    primary.put(40+21,0);r.primary40[1]=primary.view();steps.put(0,1);r.growth_steps[0]=steps.view();check(value(q::apply(f,0,r)).value==74,"Bucket255 still bounded by level63");
    caps.put(2,20,2);r.caps_by_kind=caps.view();check(value(q::apply(f,0,r)).value==21,"Loop-position cap applied before accumulation");
    f.pbw.descriptors[1][0]=3;f.pbw.words9404[3]=0;r.primary40[3]=primary.view();
    check(value(q::apply(f,0,r)).value==21,"Loop position cap is not descriptor-kind cap");
    r.growth_steps[0]=q::Absent{};error(q::apply(f,0,r),q::ErrorCode::absent_resource);
    auto wrap=fields();wrap.pbw.descriptors[1][0]=1;put(wrap.pbw.descriptors[1],2,0,2);put(wrap.pbw.descriptors[1],12,0,2);wrap.pbw.words9404[1]=0;
    special10.put(0,32767,2);r.special10=special10.view();limits.put(2,32767,2);r.query_caps=limits.view();
    const auto capped=value(q::apply(wrap,1,r));check(capped.value==-2&&capped.fields.pbw.words9404[1]==2,"Equalcap then signed16 wrap retains original value");
    auto equality=fields();equality.pbw.descriptors[0][0]=0;equality.pbw.words9404[0]=0;caps.put(0,0,2);r.caps_by_kind=caps.view();
    check(value(q::apply(equality,6,r)).fields.pbw.words9404[0]==0x40,"Query6 cap equality sets bit");
    // Primary kind255 remains a real byte index, not collapsed to kind0.
    f=fields();f.pbw.descriptors[0][0]=255;f.pbw.words9404[255]=0x40;check(value(q::apply(f,6,r)).fields.pbw.words9404[255]==0,"Full byte word-index domain");
    // Integer-equivalent VFP paths need no floating environment restriction.
    const int old=std::fegetround();if(std::fesetround(FE_DOWNWARD)==0){const auto result=q::apply(f,6,r);std::fesetround(old);check(std::holds_alternative<q::Output>(result),"Unused floating environment does not reject");}
    std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";
}
}
int main(){try{run();return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
