#include "player_status_update.hpp"
#include "test_support/player_status_effects.hpp"
#include "test_support/player_update_checkpoint.hpp"
#include "test_support/player_object_fixture.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
namespace st=mh4u::player_status_update;
namespace cp=mh4u::test_player_checkpoint;
namespace oq=mh4u::player_object_query;
namespace of=mh4u::test_object_fixture;
using U=std::uint32_t;using Bytes=std::vector<std::uint8_t>;using Raw=std::array<Bytes,8>;
std::size_t checks{},returns{},prefixes{},scans{},timers{},clears{},stores{},store_bytes{},arena_bytes{},projected_bytes{},immutable_bytes{};
void check(bool v,const std::string&m){++checks;if(!v)throw std::runtime_error(m);}
constexpr std::array<U,8>addresses{0x2000000,0x2100000,0x2200000,0x2300000,0x2b00000,0x2c00000,0x2d00000,0x3700000};
constexpr std::array<U,8>sizes{0xb000,0xa000,0x1000,0x1000,0x1000,0x4000,0x10000,0x1000};
constexpr std::array<const char*,8>names{"player","pbw","pw","global","manager","objects","data","event_manager"};
U word(const Bytes&b,std::size_t o,U n=4){check(n<=4&&o<=b.size()&&n<=b.size()-o,"word extent");U v{};for(U i=0;i<n;++i)v|=U(b[o+i])<<(8*i);return v;}
void put(Bytes&b,std::size_t o,std::uint64_t v,U n=4){check(n<=8&&o<=b.size()&&n<=b.size()-o,"store extent");for(U i=0;i<n;++i)b[o+i]=std::uint8_t(v>>(8*i));}
Bytes load(const char*path,const std::string&sha){const auto n=std::filesystem::file_size(path);check(n<=64U*1024*1024,"fixture capacity");std::ifstream f(path,std::ios::binary);Bytes b(n);f.read(reinterpret_cast<char*>(b.data()),std::streamsize(n));check(bool(f)&&f.peek()==EOF,"exact fixture length");check(picosha2::hash256_hex_string(b)==sha,"fixture SHA");return b;}
struct Reader{std::span<const std::uint8_t>b;std::size_t at{};auto take(std::size_t n){check(at<=b.size()&&n<=b.size()-at,"fixture truncated");auto v=b.subspan(at,n);at+=n;return v;}U u(){const auto v=take(4);return U(v[0])|(U(v[1])<<8)|(U(v[2])<<16)|(U(v[3])<<24);}std::uint64_t u64(){auto lo=u();return std::uint64_t(lo)|(std::uint64_t(u())<<32);}Bytes bytes(std::size_t n){auto v=take(n);return{v.begin(),v.end()};}std::string text(){auto n=u();check(n<=256,"text extent");auto v=take(n);return{v.begin(),v.end()};}std::string hash(){auto v=take(32);return picosha2::bytes_to_hex_string(v.begin(),v.end());}void end(){check(at==b.size(),"fixture trailing");}};
struct Call{U pc,ret,value,returned;std::array<U,4>args;U start,end;};
struct Store{U pc,address,width;std::uint64_t value;};
struct Case{std::string name;bool prefix;std::vector<std::pair<U,U>>known;std::map<U,U>pointers;Raw before,after;std::vector<Store>writes;std::vector<Call>calls;std::vector<U>trace;U fpscr{};Bytes event;std::array<U,4>event_args{};};
bool known(const Case&c,U p,U n){for(U i=0;i<n;++i)if(!std::any_of(c.known.begin(),c.known.end(),[&](auto r){return r.first<=p+i&&p+i<r.second;}))return false;return true;}
template<class T>std::optional<T>field(const Case&c,U arena,U off,U n){if(!known(c,addresses[arena]+off,n))return{};auto v=word(c.before[arena],off,n);if constexpr(std::is_same_v<T,std::int8_t>)return std::bit_cast<std::int8_t>(std::uint8_t(v));else if constexpr(std::is_same_v<T,std::int16_t>)return std::bit_cast<std::int16_t>(std::uint16_t(v));else return static_cast<T>(v);}
cp::Input input(const Raw&r){auto x=cp::decode({r[0],r[1],r[2]});x.p.global.field22=std::uint16_t(word(r[3],0x22,2));x.p.global.field2b=std::uint8_t(word(r[3],0x2b,1));x.p.global.field2f=std::uint8_t(word(r[3],0x2f,1));x.p.global.field5d=std::uint8_t(word(r[3],0x5d,1));return x;}
const Call*call(const Case&c,U pc){auto i=std::find_if(c.calls.begin(),c.calls.end(),[&](const auto&v){return v.pc==pc;});return i==c.calls.end()?nullptr:&*i;}
U count(const Case&c,U pc){return U(std::count_if(c.calls.begin(),c.calls.end(),[&](const auto&v){return v.pc==pc;}));}
void same(const Bytes&a,const Bytes&b,const std::string&label){check(a.size()==b.size(),label+" size");if(a!=b){const auto off=std::mismatch(a.begin(),a.end(),b.begin()).first-a.begin();std::ostringstream s;s<<label<<" offset0x"<<std::hex<<off<<" actual="<<unsigned(a[off])<<" original="<<unsigned(b[off]);throw std::runtime_error(s.str());}++checks;}
Case read_case(Reader&r,U id,const std::vector<Bytes>&blobs){const auto n=r.u();check(n<=1024*1024,"record capacity");Reader q{r.take(n)};Case c;check(q.u()==id,"record ordinal");c.name=q.text();auto prefix=q.u();check(prefix<=1,"prefix tag");c.prefix=prefix!=0;
 auto count0=q.u();check(count0<=128,"known range count");U last{};for(U i=0;i<count0;++i){auto lo=q.u(),hi=q.u();check(lo<hi&&(i==0||last<lo),"known range order");c.known.emplace_back(lo,hi);last=hi;}
 check(q.u()==3,"singleton count");for(U i=0;i<3;++i){auto p=q.u(),v=q.u();check(c.pointers.emplace(p,v).second,"unique singleton");}check(c.pointers.at(0xfb6b7c)==addresses[3]&&c.pointers.at(0xf32004)==addresses[4]&&c.pointers.at(0x105729c)==addresses[7],"singleton source identities");
 for(auto*side:{&c.before,&c.after})for(U i=0;i<8;++i){auto ix=q.u();check(ix<blobs.size()&&blobs[ix].size()==sizes[i],"arena blob binding");(*side)[i]=blobs[ix];}
 for(U i=0;i<3;++i)check(known(c,addresses[i],sizes[i]),"explicit primary diagnostic knowledge");
 count0=q.u();check(count0<=4096,"write count");for(U i=0;i<count0;++i){Store w{q.u(),q.u(),q.u(),q.u64()};check(w.width==1||w.width==2||w.width==4||w.width==8,"write width");if(w.width<8)check((w.value>>(8*w.width))==0,"write value width");c.writes.push_back(w);}
 count0=q.u();check(count0<=256,"call count");for(U i=0;i<count0;++i){Call v{q.u(),q.u(),q.u(),q.u(),{q.u(),q.u(),q.u(),q.u()},q.u(),q.u()};check(v.returned<=1,"call result tag");c.calls.push_back(v);}
 count0=q.u();check(count0<=10000,"trace count");for(U i=0;i<count0;++i)c.trace.push_back(q.u());c.fpscr=q.u();
 check(!c.trace.empty()&&c.trace.front()==0xb22164,"original entry");for(const auto&v:c.calls){check(v.start>0&&v.start<=c.trace.size()&&c.trace[v.start-1]==v.pc,"call entry observation");if(v.returned)check(v.end>=v.start&&v.end<=c.trace.size(),"call return observation");else check(c.prefix&&v.pc==0xb22164&&v.end==0,"open event-prefix call");}
 if(c.prefix){check(q.u()==0xb22508&&q.u()==0x3003e70,"event boundary and frame");for(auto&v:c.event_args)v=q.u();c.event=q.bytes(104);check(c.event_args[0]==addresses[7]&&c.event_args[1]==0x3003e70,"event source arguments");check(c.trace.back()==0xb22508&&call(c,0xc0c3f0)&&call(c,0xc0c3f0)->returned,"real constructor prefix");}
 else check(call(c,0xb22164)&&call(c,0xb22164)->returned,"complete source return");q.end();return c;}
void run_case(const Case&c){Raw replay=c.before;Bytes stack(1024,0xa5);for(const auto&w:c.writes){++stores;bool found=false;for(U i=0;i<8;++i)if(w.address>=addresses[i]&&std::uint64_t(w.address)+w.width<=std::uint64_t(addresses[i])+sizes[i]){check(i==2&&(w.address==addresses[2]+0x1f4||w.address==addresses[2]+0x1f8||w.address==addresses[2]+0x299||w.address==addresses[2]+0x2fc),"source write footprint");put(replay[i],w.address-addresses[i],w.value,w.width);store_bytes+=w.width;found=true;}
 if(!found){check(w.address>=0x3003b00&&std::uint64_t(w.address)+w.width<=0x3003f00,"source scratch write footprint");put(stack,w.address-0x3003b00,w.value,w.width);}}
 for(U i=0;i<8;++i)same(replay[i],c.after[i],"original store replay "+std::string(names[i]));if(c.prefix)same(Bytes(stack.begin()+0x370,stack.begin()+0x3d8),c.event,"original event constructor replay");
 const auto before_snapshot=c.before;auto in=input(c.before);const auto player_snapshot=in.p;const auto fields_snapshot=in.f;
 auto objects=of::project([&]<class T>(U region,U off,U width){return field<T>(c,4+region,off,width);},addresses[5],addresses[6],0);
 const auto objects_snapshot=objects;const auto result=st::apply(in.p,in.f,&objects,field<U>(c,0,0x14,4));
 check(in.p==player_snapshot&&in.f==fields_snapshot&&objects==objects_snapshot,"all canonical borrowed owners immutable");for(U i=0;i<8;++i){same(c.before[i],before_snapshot[i],"source input immutable");immutable_bytes+=sizes[i];}
 if(c.prefix){const auto*e=std::get_if<st::Error>(&result);check(e&&e->code==st::ErrorCode::unsupported_event&&e->phase==st::Phase::event&&std::holds_alternative<std::monostate>(e->cause),"event prefix typed atomic failure");++prefixes;return;}
 if(auto*e=std::get_if<st::Error>(&result))throw std::runtime_error(c.name+": native error "+std::string(e->field));const auto&o=std::get<st::Output>(result);auto expected=input(c.after);check(o.field1f4==expected.p.pw.field1f4&&o.fields==expected.f,"complete canonical output");
 Raw projected=c.before;put(projected[2],0x1f4,o.field1f4);cp::shared_fields(o.fields,[&](const auto&v,U ar,U off,U n){check(v.has_value(),"projected source fact known");using T=typename std::remove_cvref_t<decltype(v)>::value_type;if constexpr(std::is_same_v<T,bool>)check((word(projected[ar],off,n)!=0)==*v,"pointer presence unchanged");else put(projected[ar],off,U(*v),n);});
 for(U i=0;i<8;++i){same(projected[i],c.after[i],c.name+" native arena "+names[i]);arena_bytes+=sizes[i];}projected_bytes+=13;++returns;
 std::vector<mh4u::test_status_effects::Call>observed;for(const auto&v:c.calls)observed.push_back({v.pc,v.value,v.args});std::vector<U>write_pcs;for(const auto&w:c.writes)write_pcs.push_back(w.pc);const auto counts=mh4u::test_status_effects::compare(o.effects,observed,c.trace,write_pcs,check);scans+=counts.scans;timers+=counts.timers;clears+=counts.clears;
}
int main(int argc,char**argv){try{check(argc==3,"usage: status-tests fixture sha256");auto raw=load(argv[1],argv[2]);Reader r{raw};check(r.bytes(8)==Bytes({'M','H','4','U','S','T','A','1'}),"schema");check(r.u()==566,"original case count");check(r.hash()=="d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","source ELF");check(r.hash()=="02de4be95f44b5995e5cfdbf26bb58bba38f838728550df40a337123e9a6c2b2","source execution");check(r.u()==8,"arena count");for(U i=0;i<8;++i)check(r.text()==names[i]&&r.u()==addresses[i]&&r.u()==sizes[i],"arena source schema");auto n=r.u();check(n==850,"snapshot blob count");std::vector<Bytes>blobs;for(U i=0;i<n;++i){auto size=r.u();check(size<=65536,"blob extent");auto hash=r.hash();auto b=r.bytes(size);check(picosha2::hash256_hex_string(b)==hash,"snapshot blob identity");blobs.push_back(std::move(b));}for(U i=0;i<566;++i){auto c=read_case(r,i,blobs);try{run_case(c);}catch(const std::exception&e){throw std::runtime_error(c.name+": "+e.what());}}r.end();check(returns==562&&prefixes==4,"completion categories");std::cout<<"{\"checks\":"<<checks<<",\"complete_returns\":"<<returns<<",\"event_prefixes\":"<<prefixes<<",\"scans\":"<<scans<<",\"timer_increments\":"<<timers<<",\"timer_clears\":"<<clears<<",\"original_stores\":"<<stores<<",\"original_store_bytes\":"<<store_bytes<<",\"compared_arena_bytes\":"<<arena_bytes<<",\"output_lane_bytes\":"<<projected_bytes<<",\"immutability_bytes\":"<<immutable_bytes<<"}\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
