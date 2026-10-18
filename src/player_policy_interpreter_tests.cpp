#include "player_policy_interpreter.hpp"
#include "player_controller_step.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {
namespace pi=mh4u::player_policy_interpreter;namespace pt=mh4u::player_transition;
namespace ps=mh4u::player_selection;namespace pcs=mh4u::player_controller_step;
namespace mc=mh4u::motion_command;namespace tbl=mh4u::player_policy_table;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{};
void check(bool value,const std::string& why){++checks;if(!value)throw std::runtime_error(why);}
std::uint32_t word(const Bytes& b,std::size_t p,unsigned n=4){check(p<=b.size()&&n<=b.size()-p,"Read extent");std::uint32_t v=0;for(unsigned i=0;i<n;++i)v|=std::uint32_t(b[p+i])<<(i*8);return v;}
void put(Bytes& b,std::size_t p,std::uint32_t v,unsigned n=4){check(p<=b.size()&&n<=b.size()-p,"Write extent");for(unsigned i=0;i<n;++i)b[p+i]=static_cast<std::uint8_t>(v>>(i*8));}
Bytes load(const char* path,const char* sha,std::size_t cap){const auto size=std::filesystem::file_size(path);check(size<=cap,"File capacity");std::ifstream f(path,std::ios::binary);check(bool(f),"Open fixture");Bytes b(static_cast<std::size_t>(size));f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(size));check(bool(f),"Complete read");check(picosha2::hash256_hex_string(b.begin(),b.end())==sha,"Independent digest");return b;}
struct Reader{const Bytes& b;std::size_t p{};std::uint32_t integer(){const auto v=word(b,p);p+=4;return v;}Bytes bytes(std::size_t n){check(p<=b.size()&&n<=b.size()-p,"Fixture bounds");Bytes v(b.begin()+static_cast<std::ptrdiff_t>(p),b.begin()+static_cast<std::ptrdiff_t>(p+n));p+=n;return v;}};
#include "player_arena_projection.hpp"
// One format-aware offset declaration for the extra fields in this raw oracle.
// This maps bytes only; no predicate or expected-state computation occurs here.
template<class F>void extra_fields(mh4u::player_fields::Fields& x,F visit){
    visit(x.pbw.field3a8,1,0x3a8,2);visit(x.pbw.field3ac,1,0x3ac,2);visit(x.pbw.field3b8,1,0x3b8,2);visit(x.pbw.field3bc,1,0x3bc,2);
    for(unsigned i=0;i<19;++i)visit(x.pbw.flags3d8[i],1,0x3d8+4*i,4);
    visit(x.pbw.field14_bits,1,0x14,4);visit(x.pbw.field478,1,0x478,4);
    visit(x.pbw.field6fb,1,0x6fb,1);visit(x.pbw.field6fc,1,0x6fc,1);visit(x.pbw.field93ee,1,0x93ee,1);visit(x.pbw.field93f1,1,0x93f1,1);
    visit(x.pw.field14c,2,0x14c,4);visit(x.pw.field154,2,0x154,4);visit(x.pw.field15c_bits,2,0x15c,4);visit(x.pw.field6a4_bits,2,0x6a4,4);
    visit(x.pw.field150,2,0x150,1);visit(x.pw.field158,2,0x158,1);visit(x.pw.field159,2,0x159,1);visit(x.pw.field15a,2,0x15a,1);visit(x.pw.field6a8,2,0x6a8,1);
    visit(x.pw.field188,2,0x188,2);visit(x.pw.field224,2,0x224,2);visit(x.pw.field238,2,0x238,2);
}
void same(const Bytes& a,const Bytes& b,const std::string& name){check(a.size()==b.size(),name+" extent");if(a!=b){const auto at=std::mismatch(a.begin(),a.end(),b.begin()).first-a.begin();std::ostringstream s;s<<name<<" offset0x"<<std::hex<<at<<" got"<<unsigned(a[at])<<" expected"<<unsigned(b[at]);throw std::runtime_error(s.str());}++checks;}
struct Decoded {
    ps::State state;mh4u::player_fields::Fields fields;mh4u::motion_request::Layer layer;mc::Fields motion;
    pi::Reference reference=mh4u::player_reference::Unknown{};std::uint32_t delta{},c8c{};std::uint8_t ad24{};
};
Decoded decode(const std::array<Bytes,7>& raw,const tbl::Table& table){
    Decoded d;for(unsigned i=0;i<7;++i)d.motion.fieldsd48[i]=word(raw[0],0xd48+4*i);
    Arenas arenas{raw[0],raw[1],raw[2],raw[3]};const auto s=controller(arenas,d.motion);const auto& p=s.player;
    d.state={{{word(raw[0],0x3b8),p.fieldsd10},p.pbw,p.pw,p.global},p.selection_pbw,p.selection_pw};
    const auto address=word(raw[2],0x100);
    if(address==0xf2efb8)d.state.transition.pw.policy.table=table;
    else check(address==0,"Known fixture table pointer");
    d.state.pbw.field6f9=static_cast<std::uint8_t>(word(raw[1],0x6f9,1));d.state.pbw.field9=static_cast<std::uint8_t>(word(raw[1],9,1));
    extra_fields(d.fields,[&](auto& v,unsigned arena,std::size_t offset,unsigned n){using T=typename std::remove_reference_t<decltype(v)>::value_type;v=static_cast<T>(word(raw[arena],offset,n));});
    auto& l=d.layer;l.flags=static_cast<std::uint16_t>(word(raw[0],0x246,2));
    l.remaining_interpolation=std::bit_cast<float>(word(raw[0],0x284));l.frame=std::bit_cast<float>(word(raw[0],0x28c));l.previous_frame=std::bit_cast<float>(word(raw[0],0x290));l.end_frame=std::bit_cast<float>(word(raw[0],0x294));l.field58=std::bit_cast<float>(word(raw[0],0x298));l.speed=std::bit_cast<float>(word(raw[0],0x29c));
    d.delta=word(raw[0],0x14);d.c8c=word(raw[3],0xc8c);d.ad24=static_cast<std::uint8_t>(word(raw[4],0xad24,1));
    const auto object=word(raw[5],0x1c);if(object==0)d.reference=mh4u::player_reference::Absent{};
    else {check(object==0x2600000,"Known reference object");d.reference=mh4u::player_reference::Points{std::bit_cast<float>(word(raw[6],0x40)),std::bit_cast<float>(word(raw[6],0x48)),std::bit_cast<float>(word(raw[6],0x60)),std::bit_cast<float>(word(raw[6],0x68))};}
    return d;
}
pi::Context context(const Decoded& d,const pt::Constants& k){return{d.layer,d.motion,d.delta,k,d.reference,d.ad24,d.c8c,std::array<std::uint16_t,2>{0x4000,0xc000}};}
std::array<Bytes,7> encode(const std::array<Bytes,7>& before,const pi::Output& out,const tbl::Table& table){
    auto raw=before;const auto& s=out.state;const auto& t=s.transition;
    pcs::State c{{t.player.fieldsd10,t.pbw,t.pw,t.global,s.pbw,s.pw},{}};
    // Historical projection preserves Unknown opaque pointer bytes. Resolve the
    // new Table value through its explicit fixture identity, outside runtime.
    const auto ref=c.player.pw.policy.table;c.player.pw.policy.table=tbl::Unknown{};
    Arenas a{raw[0],raw[1],raw[2],raw[3]};encode_controller(a,c);
    raw[0]=std::move(a.player);raw[1]=std::move(a.pbw);raw[2]=std::move(a.pw);raw[3]=std::move(a.global);
    put(raw[0],0x3b8,t.player.field3b8);
    if(std::holds_alternative<tbl::Absent>(ref))put(raw[2],0x100,0);
    else if(const auto* value=std::get_if<tbl::Table>(&ref)){check(value->identity()==table.identity(),"Output table identity");put(raw[2],0x100,0xf2efb8);}
    auto extra=out.fields;extra_fields(extra,[&](const auto& v,unsigned arena,std::size_t offset,unsigned n){if(v)put(raw[arena],offset,static_cast<std::uint32_t>(*v),n);});
    return raw;
}
void guards(const Decoded& original,const pt::Constants& k){
    auto d=original;auto c=context(d,k);
    const auto rejected=[&](pi::ErrorCode code){const auto r=pi::apply(d.state,d.fields,c);const auto* e=std::get_if<pi::Error>(&r);check(e&&e->code==code,"Consumed unknown must fail");};
    d.fields={};rejected(pi::ErrorCode::unknown_field);
    d.state.transition.pw.policy.table=tbl::Unknown{};rejected(pi::ErrorCode::unknown_table);
    d.state.transition.pw.policy.table=tbl::Absent{};
    check(std::holds_alternative<pi::Output>(pi::apply(d.state,d.fields,c)),"Absent table ignores unknown context");
    d.fields.pbw.field10_bits=0x7fc00001;d.fields.pbw.field18_bits=0x80000000;d.fields.pbw.field354_bits=0x7f800000;
    d.fields.pw.field1f8=0xffffffff;d.fields.pw.field220_bits=0x7fc00002;d.fields.pw.field838_bits=0xff800000;d.fields.pw.field7f0=255;
    const auto shared_before=d.fields;const auto shared_result=pi::apply(d.state,d.fields,c);const auto* shared=std::get_if<pi::Output>(&shared_result);
    check(shared&&shared->fields==shared_before&&d.fields==shared_before,"Shared additional fields remain raw and unconsumed");
    d=original;d.state.transition.global.field2b=2;d.state.transition.pbw.field30=3;d.fields={};
    check(std::holds_alternative<pi::Output>(pi::apply(d.state,d.fields,c)),"Closed local gate ignores fields");
    d=original;d.state.transition.pw.policy.timer140_bits=0x3f800000;d.fields={};
    check(std::holds_alternative<pi::Output>(pi::apply(d.state,d.fields,c)),"Positive timer ignores fields");
    d=original;d.layer.end_frame=std::numeric_limits<float>::infinity();rejected(pi::ErrorCode::nonfinite_operand);
    // Independent original b24354 observations retained in reference-mode-review:
    // mode1 chooses +154 even with f0 bit4000 clear. These synthetic one-record
    // tables exercise the interpreter adapter, not a modified shipped table.
    // ID34 additionally reads word32, beyond this interpreter's declared
    // 20-word source window; its six-case owner regression remains separate.
    for(const auto id:{35U,36U})for(const bool invert:{false,true}){
        d=original;d.state.transition.pw.fieldf0=0;d.state.transition.pw.fieldb0=invert?0x8000:0;
        d.fields.pw.field150=1;d.fields.pw.field154=invert?0:0x8000;d.fields.pw.field14c.reset();
        d.state.transition.pbw.angles={0,0};d.state.transition.pbw.magnitudes={110,0};d.state.transition.pbw.field451=0;
        d.state.transition.pbw.field3d4=id==36?1:0;for(auto& f:d.fields.pbw.flags3d8)f=0;
        Bytes record(23);record[0]=1;record[2]=9;record[6]=static_cast<std::uint8_t>(id);record[22]=255;
        if(id!=36)put(record,8,0x400,2);if(id!=35)put(record,10,0x400,2);
        const auto hash=picosha2::hash256_hex_string(record.begin(),record.end());
        const auto one=tbl::Table::decode(record,hash);check(std::holds_alternative<tbl::Table>(one),"Synthetic table decode");
        d.state.transition.pw.policy=tbl::install(std::get<tbl::Table>(one),0);const auto mode_context=context(d,k);
        const auto result=pi::apply(d.state,d.fields,mode_context);const auto* output=std::get_if<pi::Output>(&result);
        check(output&&output->returned==invert,"Reference mode independent of heading flag");
        d.fields.pw.field150.reset();const auto unknown=pi::apply(d.state,d.fields,mode_context);
        check(std::holds_alternative<pi::Error>(unknown),"Corrected sector requires unknown mode");
    }
}
}
int main(int argc,char** argv){try{
    check(argc==7,"Expected oracle/hash table/hash constants/hash");
    const auto fixture=load(argv[1],argv[2],256U*1024*1024),table_bytes=load(argv[3],argv[4],1078),constant_bytes=load(argv[5],argv[6],88);
    auto table_result=tbl::Table::decode(table_bytes,argv[4]);check(std::holds_alternative<tbl::Table>(table_result),"Table decode");const auto table=std::get<tbl::Table>(table_result);
    const auto constants_result=pt::Constants::decode(constant_bytes,{argv[6],"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435"});check(std::holds_alternative<pt::Constants>(constants_result),"Constants decode");const auto& k=std::get<pt::Constants>(constants_result);
    Reader reader{fixture};const auto magic=reader.bytes(8);check(std::string(magic.begin(),magic.end())=="MH4UPI01","Fixture magic");check(reader.integer()==7,"Arena count");const auto cases=reader.integer();check(cases>0&&cases<=512,"Case capacity");
    constexpr std::array<std::uint32_t,7> sizes{0xb000,0xa000,0x1000,0x1000,0xe000,0x1000,0x1000};
    for(unsigned i=0;i<7;++i){check(reader.integer()==0x2000000+i*0x100000,"Arena identity");check(reader.integer()==sizes[i],"Arena size");}
    for(unsigned n=0;n<cases;++n){
        const auto len=reader.integer();check(len<=1024,"Name capacity");const auto name_bytes=reader.bytes(len);const std::string name(name_bytes.begin(),name_bytes.end());
        const auto expected_return=reader.integer(),visits=reader.integer();check(visits<=48,"Visit capacity");std::vector<std::uint32_t> expected_visits;for(unsigned i=0;i<visits;++i)expected_visits.push_back(reader.integer());
        const auto transitions=reader.integer();check(transitions<=1,"Transition capacity");std::array<std::uint32_t,3> expected_request{};if(transitions)for(auto& v:expected_request)v=reader.integer();
        std::array<Bytes,7> before,after;for(unsigned i=0;i<7;++i){before[i]=reader.bytes(sizes[i]);after[i]=reader.bytes(sizes[i]);}
        const auto d=decode(before,table);const auto initial=d.state;const auto fields_before=d.fields;
        const auto result=pi::apply(d.state,d.fields,context(d,k));
        if(const auto* e=std::get_if<pi::Error>(&result)){std::ostringstream s;s<<name<<" native error"<<int(e->code)<<" "<<e->field<<" row"<<(e->record?int(*e->record):-1);throw std::runtime_error(s.str());}
        const auto& out=std::get<pi::Output>(result);check(d.state==initial&&d.fields==fields_before,name+" transactional input");
        check(out.returned==bool(expected_return),name+" return");check(out.visited_count==expected_visits.size(),name+" visit count");for(unsigned i=0;i<visits;++i)check(out.visited[i]==expected_visits[i],name+" exact visit order");
        check(out.transition.has_value()==bool(transitions),name+" transition presence");if(transitions){const auto& r=out.transition->arguments;check(std::array{r.state_arg,r.action_arg,r.flags_arg}==expected_request,name+" original transition arguments");}
        const auto actual=encode(before,out,table);for(unsigned i=0;i<7;++i)same(actual[i],after[i],name+" arena"+std::to_string(i));
        if(n==0)guards(d,k);
    }
    check(reader.p==fixture.size(),"No trailing data");std::cout<<"{\"status\":\"passed\",\"cases\":"<<cases<<",\"checks\":"<<checks<<"}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
