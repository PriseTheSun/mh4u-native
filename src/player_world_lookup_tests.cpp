#include "player_world_lookup.hpp"
#include "picosha2.h"
#include <bit>
#include <cfenv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace wl=mh4u::player_world_lookup;
using U=std::uint32_t;using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},hits{},visits{},stores{},produced{},typed{},lazy{};
void need(bool b,const char*m){++checks;if(!b)throw std::runtime_error(m);}
struct Reader{std::span<const std::uint8_t>b;std::size_t at{};auto take(std::size_t n){need(n<=b.size()-at,"fixture truncated");auto v=b.subspan(at,n);at+=n;return v;}U word(){auto v=take(4);return U(v[0])|(U(v[1])<<8)|(U(v[2])<<16)|(U(v[3])<<24);}std::string text(){auto n=word();need(n<=256,"name extent");auto v=take(n);return {v.begin(),v.end()};}void end(){need(at==b.size(),"fixture trailing");}};
constexpr U record_base=0x2100000,record_stride=256;
constexpr std::array<U,16> offsets{8,0x19,0x30,0x61,0x78,0x2f,0x34,0x38,0x3c,0x50,0x4c,0x54,0x28,0x40,0x44,0x48};
constexpr std::array<U,16> widths{4,1,2,1,1,1,4,4,4,4,4,1,4,4,4,4};
std::optional<U> atom(Reader&r,U width){const U value=r.word(),mask=r.word(),full=(1U<<width)-1U;need(mask==0||mask==full,"partial scalar knowledge");if(width<4)need(value<(1U<<(8*width)),"scalar width");return mask?std::optional<U>{value}:std::nullopt;}
wl::Link link(std::optional<U> p,std::size_t count){if(!p)return wl::Unknown{};if(*p==0)return wl::Absent{};need(*p>=record_base&&*p<record_base+128*record_stride&&(*p-record_base)%record_stride==0,"record pointer identity");const auto i=(*p-record_base)/record_stride;need(i<count,"unbound record pointer");return wl::Index{i};}
template<class T>std::optional<T> narrow(std::optional<U> v){return v?std::optional<T>{T(*v)}:std::nullopt;}
void put(Bytes&b,std::size_t at,U v){need(at+4<=b.size(),"native output bounds");for(U i=0;i<4;++i)b[at+i]=std::uint8_t(v>>(8*i));}
struct Setup{wl::Manager manager;wl::Request request;std::optional<U> mask;};
void unchanged(const Setup& x,const Setup& before){need(x.manager==before.manager,"manager mutated");need(x.request.area==before.request.area&&x.request.point==before.request.point&&x.request.copy_field28==before.request.copy_field28,"request mutated");need(x.mask==before.mask,"mask mutated");}
void reject(Setup x,wl::ErrorCode code){const auto before=x;auto result=wl::query({&x.manager,x.mask},x.request);need(std::holds_alternative<wl::Error>(result),"invalid input accepted");need(std::get<wl::Error>(result).code==code,"wrong typed error");unchanged(x,before);++typed;}
void boundaries(Setup hit){
 {auto x=hit;x.manager.head=wl::Unknown{};reject(x,wl::ErrorCode::unknown_field);}
 {auto x=hit;x.manager.head=wl::Index{128};reject(x,wl::ErrorCode::invalid_link);}
 {auto x=hit;x.manager.records[0].next=wl::Index{128};reject(x,wl::ErrorCode::invalid_link);}
 {auto x=hit;x.manager.records[0].next=wl::Index{0};reject(x,wl::ErrorCode::cycle);}
 {auto x=hit;x.manager.records[0].next=wl::Unknown{};reject(x,wl::ErrorCode::unknown_field);}
 #define MISS(member) {auto x=hit;x.member.reset();reject(x,wl::ErrorCode::unknown_field);}
 MISS(mask);MISS(request.area);MISS(request.point[0]);MISS(request.point[1]);MISS(request.point[2]);
 MISS(manager.records[0].field19);MISS(manager.records[0].field30);MISS(manager.records[0].field61);MISS(manager.records[0].field78);MISS(manager.records[0].field2f);MISS(manager.records[0].field50_bits);MISS(manager.records[0].field4c_bits);MISS(manager.records[0].fields34[0]);MISS(manager.records[0].fields34[1]);MISS(manager.records[0].fields34[2]);MISS(manager.records[0].fields40[1]);MISS(manager.records[0].field28);MISS(manager.records[0].field54);
 #undef MISS
 {auto x=hit;const auto before=x;auto r=wl::query({},x.request);need(std::holds_alternative<wl::Error>(r)&&std::get<wl::Error>(r).code==wl::ErrorCode::unknown_field,"unknown manager accepted");unchanged(x,before);++typed;}
 {Setup x{};x.manager.head=wl::Absent{};const auto before=x;auto r=wl::query({&x.manager,{}},x.request);need(std::holds_alternative<wl::Output>(r),"empty lazy unknown rejected");need(!std::get<wl::Output>(r).match&&std::get<wl::Output>(r).returned==0xffff&&std::get<wl::Output>(r).visited==0,"empty lazy result");unchanged(x,before);++lazy;}
 {wl::Manager m{};m.head=wl::Absent{};mh4u::player_controller_step::PlayerFields p{};mh4u::player_fields::Fields f{};f.pw.field5a8=0xa5a5a5a5;f.pbw.field14_bits=0x7fc12345;const auto pb=p;const auto fb=f;auto r=wl::apply(p,f,{&m,{}});need(std::holds_alternative<wl::Error>(r),"canonical unknown PBW9 accepted");need(std::get<wl::Error>(r).code==wl::ErrorCode::unknown_field&&std::get<wl::Error>(r).field=="PBW9","canonical PBW9 cause");need(p==pb&&f==fb,"canonical rejection mutated input");++typed;p.selection_pbw.field9=7;f.pbw.field14_bits.reset();const auto before=f;auto ok=wl::apply(p,f,{&m,{}});need(std::holds_alternative<wl::PlayerOutput>(ok),"canonical empty consumed XYZ");auto expected=f;expected.pw.field5a8=0;const auto& out=std::get<wl::PlayerOutput>(ok);need(out.fields==expected&&!out.lookup.match&&out.lookup.returned==0xffff&&out.lookup.visited==0,"canonical empty output");need(f==before&&m.head.index()==1,"canonical empty mutated input");++lazy;}
 const int prior=std::fegetround();need(prior==FE_TONEAREST,"initial floating environment");need(std::fesetround(FE_DOWNWARD)==0,"cannot set rounding");auto wrong=wl::query({&hit.manager,hit.mask},hit.request);const int restored=std::fesetround(prior);need(restored==0,"cannot restore rounding");need(std::holds_alternative<wl::Error>(wrong)&&std::get<wl::Error>(wrong).code==wl::ErrorCode::floating_environment,"wrong rounding accepted");++typed;
}
int main(int argc,char**argv){try{
 need(argc==3,"usage: player-world-lookup-tests fixture sha256");const auto size=std::filesystem::file_size(argv[1]);need(size<=8*1024*1024,"fixture capacity");std::ifstream file(argv[1],std::ios::binary);Bytes raw{std::istreambuf_iterator<char>(file),{}};need(raw.size()==size,"fixture length");need(picosha2::hash256_hex_string(raw)==argv[2],"fixture SHA");Reader r{raw};auto magic=r.take(8);need(std::string(magic.begin(),magic.end())=="MH4UWOL1","schema");need(r.word()==326,"case count");
 for(const char* hash:{"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","6b3df9661c234d434a754efb7d7d14c18a96f103dd401f3cd5433e704e6302f3","6ac40883022f0bdf24b6218be123db00f0aee561b6fe6498e10658b30ebbd83e"}){auto b=r.take(32);need(picosha2::bytes_to_hex_string(b.begin(),b.end())==hash,"source identity");}
 need(r.word()==16,"field count");for(U i=0;i<16;++i){need(r.word()==offsets[i],"field offset");need(r.word()==widths[i],"field width");}
 Setup hit{};
 for(U index=0;index<326;++index){const auto n=r.word();need(n<=65536,"record extent");Reader q{r.take(n)};need(q.word()==index,"case order");const auto name=q.text();const auto original_return=q.word();q.word();const auto selected=q.word(),visited=q.word();need(visited<=128,"visit count");const auto global=atom(q,4),head=atom(q,4);if(global)need(*global==0||*global==0x2200000,"global pointer identity");
 Setup x{};const auto area=q.word();need(q.word()==1,"area knowledge");x.request.area=area;for(auto& v:x.request.point)v=atom(q,4);const auto mask=atom(q,4);x.mask=global&&*global?mask:std::nullopt;const auto copy=q.word();need(copy<=1,"copy flag");x.request.copy_field28=copy!=0;const auto count=q.word();need(count<=128,"record count");x.manager.head=link(head,count);
 for(U j=0;j<count;++j){std::array<std::optional<U>,16> a{};for(U k=0;k<16;++k)a[k]=atom(q,widths[k]);auto& d=x.manager.records[j];d.next=link(a[0],count);d.field19=narrow<std::uint8_t>(a[1]);d.field30=narrow<std::uint16_t>(a[2]);d.field61=narrow<std::uint8_t>(a[3]);d.field78=narrow<std::uint8_t>(a[4]);d.field2f=narrow<std::uint8_t>(a[5]);d.fields34={a[6],a[7],a[8]};d.field50_bits=a[9];d.field4c_bits=a[10];d.field54=narrow<std::uint8_t>(a[11]);d.field28=a[12];d.fields40={a[13],a[14],a[15]};}
 auto before=q.take(4096),after=q.take(4096);Bytes replay(before.begin(),before.end());const auto writes=q.word();need(writes<=9,"write count");for(U j=0;j<writes;++j){auto pc=q.word(),off=q.word(),width=q.word(),value=q.word();need(pc>=0x5b8f44&&pc<=0x5b8f74,"write PC");need(width==4&&(off==0||(off>=0x10&&off<0x20)||(off>=0x30&&off<0x3c)||off==0x50)&&off%4==0,"write bounds");put(replay,off,value);produced+=width;}stores+=writes;need(std::equal(replay.begin(),replay.end(),after.begin()),"source store replay");
 need(q.word()==visited,"visits inconsistent");for(U j=0;j<visited;++j){const auto p=q.word();need(p>=record_base&&p<record_base+count*record_stride&&(p-record_base)%record_stride==0,"visited pointer identity");}const auto accepted=q.word();need(accepted<=visited,"accept count");for(U j=0;j<accepted;++j){const auto p=q.word();need(p>=record_base&&p<record_base+count*record_stride&&(p-record_base)%record_stride==0,"accepted pointer identity");}need(q.word()==5,"arena count");q.take(5*64);q.end();
 const auto saved=x;auto result=wl::query({&x.manager,x.mask},x.request);if(auto e=std::get_if<wl::Error>(&result))throw std::runtime_error("case "+std::to_string(index)+" "+name+": "+std::string(e->field));const auto& out=std::get<wl::Output>(result);need(out.returned==original_return,"return differs from ARM");need(out.visited==visited,"visited count differs from ARM");visits+=visited;need(out.match.has_value()==(selected!=0),"match presence differs from ARM");Bytes projected(before.begin(),before.end());
 if(out.match){++hits;const auto& m=*out.match;need(selected==record_base+m.index*record_stride&&m.index<count,"selected identity differs from ARM");need(m.status==original_return,"status differs from return");put(projected,0,m.status);for(U j=0;j<4;++j)put(projected,0x10+j*4,m.vector34[j]);for(U j=0;j<3;++j)put(projected,0x30+j*4,m.words40[j]);need(m.field28.has_value()==x.request.copy_field28,"optional output presence");if(m.field28){need(std::holds_alternative<U>(*m.field28),"ARM word representation");put(projected,0x50,std::get<U>(*m.field28));}}
 need(std::equal(projected.begin(),projected.end(),after.begin()),"output differs from ARM");unchanged(x,saved);if(index==1)hit=x;
 }r.end();boundaries(hit);std::cout<<"{\"status\":\"passed\",\"cases\":326,\"hits\":"<<hits<<",\"visits\":"<<visits<<",\"source_stores\":"<<stores<<",\"produced_bytes\":"<<produced<<",\"projected_arena_bytes\":"<<326*4096<<",\"typed_errors\":"<<typed<<",\"lazy_controls\":"<<lazy<<",\"checks\":"<<checks<<"}\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
