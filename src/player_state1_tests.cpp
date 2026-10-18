#include "player_state1.hpp"
#include "test_support/motion_command_arena.hpp"
#include "picosha2.h"
#include <fstream>
#include <iostream>

namespace {
using namespace mh4u::test::motion_arena;
namespace st=mh4u::player_state1;
namespace tbl=mh4u::player_policy_table;
#include "player_arena_projection.hpp"
constexpr std::array<std::uint32_t,7> bases{0x2000000,0x2100000,0x2200000,0x2300000,0x2400000,0x2500000,0x2600000};
constexpr std::array<std::size_t,7> sizes{0xb000,0xa000,0x1000,0x1000,0xe000,0x1000,0x1000};
using Snapshot=std::array<Bytes,7>;
template<std::size_t N>std::array<std::uint32_t,N> integers(Reader& r){std::array<std::uint32_t,N> out{};for(auto& v:out)v=r.integer();return out;}
template<std::size_t N>auto records(Reader& r,std::size_t limit){const auto count=r.integer();check(count<=limit,"Record count exceeds profile");std::vector<std::array<std::uint32_t,N>> out;out.reserve(count);for(unsigned i=0;i<count;++i)out.push_back(integers<N>(r));return out;}
Bytes read(const std::filesystem::path& path,std::size_t limit){
    std::ifstream in(path,std::ios::binary|std::ios::ate);check(bool(in),"Input open");const auto n=in.tellg();
    check(n>=0&&std::uint64_t(n)<=limit,"Input allocation bound");Bytes out(static_cast<std::size_t>(n));in.seekg(0);
    check(bool(in.read(reinterpret_cast<char*>(out.data()),n))&&in.peek()==std::char_traits<char>::eof(),"Input size stable");return out;
}
tbl::Table table(const char* path,const char* expected){
    auto parsed=tbl::Table::decode(read(path,tbl::maximum_source_bytes),expected);
    if(auto* e=std::get_if<tbl::Error>(&parsed))throw std::runtime_error(std::string(e->message));
    return std::get<tbl::Table>(std::move(parsed));
}
Arenas core(const Snapshot& s){return {s[0],s[1],s[2],s[3]};}
std::uint32_t table_address(const tbl::TableReference& reference,const st::Tables& tables,std::uint32_t original){
    if(std::holds_alternative<tbl::Unknown>(reference))return original;
    if(std::holds_alternative<tbl::Absent>(reference))return 0;
    const auto& t=std::get<tbl::Table>(reference);
    if(t==std::get<tbl::Table>(tables.f2efb8))return 0xf2efb8;
    if(t==std::get<tbl::Table>(tables.f2f2d0))return 0xf2f2d0;
    throw std::runtime_error("Unmapped native policy identity");
}
Snapshot serialize(const Snapshot& before,const pcs::State& state,const st::Fields& fields,
    const std::vector<mp::Layer>& layers,const st::Tables& tables,const Context& context,const Bindings& events){
    auto output=before;auto raw=core(before);auto projected=state;
    const auto address=table_address(projected.player.pw.policy.table,tables,word(before[2],0x100));
    // The historical adapter deliberately refuses known tables. Resolve the
    // identity here and preserve its complete numeric fields through that adapter.
    projected.player.pw.policy.table=tbl::Unknown{};
    raw.player=encode(before[0],layers,state.motion,context,events);encode_controller(raw,projected);
    put(raw.pw,0x100,address);if(fields.field50_bits)put(raw.pw,0x50,*fields.field50_bits);
    output[0]=std::move(raw.player);output[1]=std::move(raw.pbw);output[2]=std::move(raw.pw);output[3]=std::move(raw.global);return output;
}
void compare(const Snapshot& got,const Snapshot& expected,const std::string& label){for(unsigned i=0;i<7;++i)same(got[i],expected[i],label+" arena"+std::to_string(i));}
void oracle(const Bytes& bytes,const std::vector<Source>& sources,const st::Tables& tables){
    Reader r{bytes};check(r.bytes(8)==Bytes({'M','H','4','U','S','1','0','2'}),"Fixture magic");
    const auto arena_count=r.integer(),source_count=r.integer(),count=r.integer();
    check(arena_count==7&&source_count==2&&source_count==sources.size()&&count>0&&count<=512,"Fixture counts");
    for(unsigned i=0;i<7;++i)check(r.integer()==bases[i]&&r.integer()==sizes[i],"Arena identity/size");
    for(const auto& source:sources){const auto entry=r.integer(),base=r.integer(),size=r.integer();const auto digest=r.bytes(32);std::string hex;picosha2::bytes_to_hex_string(digest.begin(),digest.end(),hex);
        check(entry==source.entry&&base==source.base&&size==source.resource.byte_size()&&hex==source.resource.sha256(),"LMT source identity");}
    Context context(sources);Bindings events(sources);std::size_t total_queries=0,total_samples=0,total_setters=0,total_accepted=0,skipped=0,compared_bytes=0;bool late_failure=false,unknown_preserved=false;
    for(unsigned ci=0;ci<count;++ci){
        const auto name_size=r.integer();check(name_size<=1024,"Name capacity");const auto name_bytes=r.bytes(name_size);const std::string name(name_bytes.begin(),name_bytes.end());
        const auto meta=integers<6>(r);check((meta[0]==1||meta[0]==2||meta[0]==4||meta[0]==9)&&meta[2]<=1&&meta[3]<=3&&meta[4]<=255,"Case profile");
        const auto globals_bytes=r.bytes(64);const auto wrappers=records<4>(r,4);const auto setters=records<8>(r,2);
        const auto installs=records<3>(r,2),traces=records<3>(r,128);
        Snapshot before,after;for(unsigned i=0;i<7;++i){before[i]=r.bytes(sizes[i]);after[i]=r.bytes(sizes[i]);}
        try{
            retain_inactive(before[0],2,8,context,events);
            auto storage=decode(before[0],8,context,events,2);auto state=controller(core(before),storage.fields);
            check(state.player.pbw.field371==1&&state.player.pbw.field372==meta[0]&&state.player.pbw.fields5[0]==0,"Case state matches original snapshot");
            check(storage.layers[0].animation.layer.selector==meta[1],"Current selector matches original snapshot");
            const auto initial_table=word(before[2],0x100);
            if(initial_table==0xf2efb8)state.player.pw.policy.table=tables.f2efb8;
            if(initial_table==0xf2f2d0)state.player.pw.policy.table=tables.f2f2d0;
            const st::Fields fields{word(before[2],0x50)};
            rq::Model model;model.skeleton_present=word(before[0],0xf0)!=0;
            for(unsigned bank=0;bank<16;++bank){const auto address=word(before[0],0xc04+4*bank);
                if(!address){model.banks[bank]=rq::Absent{};continue;}
                check(address>=0x2700000&&(address-0x2700000)%128==0&&(address-0x2700000)/128<=2,"Resource wrapper identity");
                const auto index=(address-0x2700000)/128;
                model.banks[bank]=index==2?rq::Resource{0xfffffffffffeULL,std::nullopt}:sources[index].resource.request_resource();
            }
            mc::Globals globals;for(unsigned i=0;i<3;++i)globals.fields6c_tables[i]=words<2>(globals_bytes,8*i);globals.vector10e6370=words<3>(globals_bytes,24);
            mc::Sampling sampling;sampling.templates.base_translation_10e62c0=words<3>(globals_bytes,36);sampling.templates.identity_10e6560=words<4>(globals_bytes,48);sampling.resolver={&context,Context::resolve};
            const auto active=word(before[0],0x234);check(active==2,"Original active layer count");context.resolutions.clear();
            auto result=st::apply(state,fields,storage.layers,active,tables,model,globals,sampling);
            if(auto* error=std::get_if<st::Error>(&result))throw std::runtime_error(std::string(error->message));
            const auto out=std::get<st::Output>(std::move(result));const auto& effect=out.effects;
            check(effect.command.has_value()&&!wrappers.empty(),"Original wrapper reached");const auto& command=*effect.command;
            const auto pc=command.entry==mc::Entry::if_changed?0xb0c198U:0xb0c060U;
            check(wrappers[0]==std::array{pc,command.selector,std::bit_cast<std::uint32_t>(command.interpolation),std::bit_cast<std::uint32_t>(command.start_frame)},"Wrapper arguments");
            check(command.speed==1&&command.layer==0&&command.flags==0,"Wrapper command profile");
            check(effect.request_count==setters.size(),"Real setter call count");
            for(unsigned i=0;i<effect.request_count;++i){const auto& got=effect.requests[i];
                check(std::array{got.request.layer,got.request.selector,got.request.attributes,std::bit_cast<std::uint32_t>(got.request.interpolation),std::bit_cast<std::uint32_t>(got.request.start_frame),std::bit_cast<std::uint32_t>(got.request.speed),got.command_flags,std::uint32_t(got.accepted)}==setters[i],"Setter arguments and real acceptance");total_accepted+=got.accepted;}
            check(wrappers.size()==1,"One original handler wrapper call");
            check(effect.installation.has_value()&&installs.size()==1,"Original table installation count");
            const auto installed=effect.installation->slot==st::TableSlot::f2efb8?0xf2efb8U:0xf2f2d0U;
            check(installs[0]==std::array{installed,0U,std::uint32_t(effect.installation->before_command)},"Installed table, delay and observed order");
            std::size_t queries=0,samples=0;for(const auto& trace:traces){check(trace[0]==1||trace[0]==2,"Trace kind");if(trace[0]==1)++queries;else ++samples;}
            check(effect.queries==queries&&effect.samples==samples,"Live query/sample count");
            compare(serialize(before,out.state,out.fields,out.layers,tables,context,events),after,"Complete return snapshot");
            compare(serialize(before,state,fields,storage.layers,tables,context,events),before,"Input unchanged");
            if(!unknown_preserved&&meta[0]==4){const auto unknown=st::apply(state,{},storage.layers,active,tables,model,globals,sampling);check(std::holds_alternative<st::Output>(unknown)&&!std::get<st::Output>(unknown).fields.field50_bits,"Unconsumed timer must remain unknown");unknown_preserved=true;}
            if(!late_failure&&queries>=2&&context.resolutions.size()>=4){
                struct Fail {const Context* context;mutable std::size_t calls{};std::size_t after;};Fail fail{&context,0,context.resolutions.size()/2};auto failing=sampling;
                failing.resolver={&fail,[](const void* ptr,const mb::TrackReference& id)->mi::Resolution{auto& f=*static_cast<const Fail*>(ptr);if(f.calls++==f.after)return mi::ResolutionError{711,"Deliberate late query failure"};return Context::resolve(f.context,id);}};
                const auto rejected=st::apply(state,fields,storage.layers,active,tables,model,globals,failing);check(std::holds_alternative<st::Error>(rejected),"Late query error must not return candidate");
                const auto& error=std::get<st::Error>(rejected);const auto* cause=std::get_if<mc::Error>(&error.cause);check(error.code==st::ErrorCode::motion&&cause&&cause->query_index==1&&cause->query_error&&cause->query_error->resolution&&cause->query_error->resolution->code==711,"Late query preserves cause");
                compare(serialize(before,state,fields,storage.layers,tables,context,events),before,"Late failure rollback");late_failure=true;
            }
            if(ci==0){
                for(unsigned n=0;n<2;++n){mc::Limits limits;if(n==0)limits.storage_layers=7;else{auto bounded=storage.layers;bounded[7].animation.tracks.joints.resize(1);limits.joint_bindings=0;check(std::get<st::Error>(st::apply(state,fields,bounded,active,tables,model,globals,sampling,limits)).code==st::ErrorCode::capacity,"Inactive binding capacity before copying");continue;}
                    check(std::get<st::Error>(st::apply(state,fields,storage.layers,active,tables,model,globals,sampling,limits)).code==st::ErrorCode::capacity,"Storage capacity before copying");}
                auto bad=state;bad.player.pbw.field371=2;check(std::get<st::Error>(st::apply(bad,fields,storage.layers,active,tables,model,globals,sampling)).code==st::ErrorCode::unsupported_state,"Unknown dispatch state rejected");
                bad=state;bad.player.pbw.field372=5;check(std::get<st::Error>(st::apply(bad,fields,storage.layers,active,tables,model,globals,sampling)).code==st::ErrorCode::unsupported_action,"Unknown action rejected");
            }
            total_queries+=queries;total_samples+=samples;total_setters+=effect.request_count;skipped+=effect.request_count==0;for(auto size:sizes)compared_bytes+=size;
        }catch(const std::exception& e){throw std::runtime_error("Case "+std::to_string(ci)+" "+name+": "+e.what());}
    }
    check(r.p==bytes.size(),"No fixture trailing bytes");check(late_failure&&unknown_preserved&&skipped>0&&total_accepted>0&&total_accepted<total_setters,"Discriminating runtime coverage");
    std::cout<<"{\"status\":\"passed\",\"cases\":"<<count<<",\"checks\":"<<checks<<",\"setters\":"<<total_setters<<",\"accepted\":"<<total_accepted<<",\"skipped\":"<<skipped<<",\"queries\":"<<total_queries<<",\"samples\":"<<total_samples<<",\"compared_bytes\":"<<compared_bytes<<"}\n";
}
}
int main(int argc,char** argv){try{
    if(argc!=13)throw std::runtime_error("Usage: state1-tests fixture sha table35 sha table31 sha entry path sha entry path sha");
    const auto fixture=read(argv[1],256U*1024U*1024U);check(picosha2::hash256_hex_string(fixture)==argv[2],"Fixture identity");
    const st::Tables tables{table(argv[3],argv[4]),table(argv[5],argv[6])};std::vector<Source> sources;
    for(int i=7;i<argc;i+=3){std::size_t end{};const std::string text=argv[i];const auto id=std::stoull(text,&end);check(end==text.size()&&id<=0xffffffff,"Source ID");auto loaded=mr::Resource::load(argv[i+1],argv[i+2]);if(auto* e=std::get_if<mr::Error>(&loaded))throw std::runtime_error(e->message);sources.push_back({std::uint32_t(id),sources.empty()?0x5000000U:0x6000000U,std::get<mr::Resource>(std::move(loaded))});}
    oracle(fixture,sources,tables);return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
