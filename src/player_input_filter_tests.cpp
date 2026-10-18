#include "player_input_filter.hpp"
#include "picosha2.h"
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
namespace pf=mh4u::player_input_filter;namespace ps=mh4u::player_input_storage;
namespace pcs=mh4u::player_controller_step;namespace shared=mh4u::player_fields;
using U=std::uint32_t;using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},wrapper_cases{},known_bytes{},stores_count{},stored_bytes{},typed_errors{},lazy_controls{};
void need(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
struct Reader{std::span<const std::uint8_t>b;std::size_t at{};auto take(std::size_t n){need(n<=b.size()-at,"fixture truncated");auto v=b.subspan(at,n);at+=n;return v;}U word(){auto v=take(4);return U(v[0])|(U(v[1])<<8)|(U(v[2])<<16)|(U(v[3])<<24);}std::string text(){const auto n=word();need(n<=256,"name extent");auto v=take(n);return {v.begin(),v.end()};}void end(){need(at==b.size(),"fixture trailing bytes");}};
constexpr std::array<U,38> addresses{0x230002b,0x230002f,0x2300d72,0x2100030,0x2100033,0x220013c,0x2400020,0x2400021,0x250ad24,0x250ad50,0x250ad64,0x250ad20,0x2100464,0x22009dc,0x22009de,0x22009dd,0x2100371,0x2100372,0x22000e8,0x270984c,0x270982f,0x260031e,0x260031c,0x2600320,0x260032c,0x260032e,0x2600338,0x260033a,0x2000e30,0x200a29c,0xfb6b7c,0xfb5edc,0xfbeffc,0x10572d8,0xedbe90,0x108b9b8,0x1083850,0x109bd29};
constexpr std::array<U,38> widths{1,1,1,1,1,4,1,1,1,4,4,1,4,1,1,1,1,2,4,1,1,2,2,2,2,2,2,2,4,4,4,4,4,4,4,1,1,1};
struct Atom{U value,mask;};using Atoms=std::array<Atom,38>;
template<class T>std::optional<T> fact(const Atoms& a,std::size_t i){const U mask=(1U<<widths[i])-1;need(a[i].mask==0||a[i].mask==mask,"partial scalar fact");if(a[i].mask!=mask)return {};if constexpr(std::is_same_v<T,std::int8_t>)return std::bit_cast<std::int8_t>(std::uint8_t(a[i].value));else return T(a[i].value);}
struct Inputs{pf::Block block;pf::Facts facts;pf::Context context;};
Inputs decode(const Atoms& a,pf::Block block){Inputs x{block,{}, {}};auto& f=x.facts;auto& c=x.context;
 #define F(name,i,T) f.name=fact<T>(a,i)
 F(global2b,0,std::uint8_t);F(global2f,1,std::uint8_t);F(globald72,2,std::uint8_t);F(pbw30,3,std::uint8_t);F(pbw33,4,std::uint8_t);F(pw13c_bits,5,U);F(pbw464_bits,12,U);F(pw9dc,13,std::uint8_t);F(pw9de,14,std::uint8_t);F(pw9dd,15,std::uint8_t);F(pbw371,16,std::uint8_t);F(pbw372,17,std::uint16_t);F(pwe8,18,U);
 #undef F
 c.object20=fact<std::int8_t>(a,6);c.object21=fact<std::int8_t>(a,7);c.manager={fact<std::uint8_t>(a,8),fact<U>(a,9),fact<U>(a,10),fact<std::int8_t>(a,11)};
 c.pad={fact<std::uint16_t>(a,21),fact<std::uint16_t>(a,22),fact<std::uint16_t>(a,23),fact<std::uint16_t>(a,24),fact<std::uint16_t>(a,25),fact<std::uint16_t>(a,26),fact<std::uint16_t>(a,27)};
 c.byte108b9b8=fact<std::uint8_t>(a,35);c.byte1083850=fact<std::uint8_t>(a,36);c.byte109bd29=fact<std::uint8_t>(a,37);
 const auto ptr=fact<U>(a,34);if(ptr){need(*ptr==0||*ptr==0x2700000,"mode2 pointer identity");if(*ptr)c.mode2=pf::Mode2Object{fact<std::uint8_t>(a,19),fact<std::uint8_t>(a,20)};else c.mode2=pf::Absent{};}
 constexpr std::array<U,6> pointers{0x2100000,0x2200000,0x2300000,0x2400000,0x2500000,0x2600000};for(std::size_t i=0;i<6;++i){auto p=fact<U>(a,28+i);if(p)need(*p==pointers[i],"owner pointer identity");}
 return x;
}
auto facts_tuple(const pf::Facts& f){return std::tuple{f.global2b,f.global2f,f.globald72,f.pbw30,f.pbw33,f.pbw371,f.pbw372,f.pw13c_bits,f.pbw464_bits,f.pwe8,f.pw9dc,f.pw9de,f.pw9dd};}
auto context_tuple(const pf::Context& c){std::optional<std::uint8_t>x,y;if(auto p=std::get_if<pf::Mode2Object>(&c.mode2)){x=p->field984c;y=p->field982f;}return std::tuple{c.object20,c.object21,c.mode2.index(),x,y,c.manager.ad24,c.manager.ad50,c.manager.ad64,c.manager.ad20,c.byte108b9b8,c.byte1083850,c.byte109bd29,c.pad.field31e,c.pad.field31c,c.pad.field320,c.pad.field32c,c.pad.field32e,c.pad.field338,c.pad.field33a};}
void unchanged(const Inputs& x,const Inputs& before){need(x.block==before.block,"block input mutated");need(facts_tuple(x.facts)==facts_tuple(before.facts),"facts mutated");need(context_tuple(x.context)==context_tuple(before.context),"context mutated");}
std::optional<U> blockword(const pf::Block& b,U offset,U n){U value=0;for(U i=0;i<n;++i){if(!b[offset-0x3a0+i])return {};value|=U(*b[offset-0x3a0+i])<<(8*i);}return value;}
bool representable(const pf::Block& b){for(const auto [o,n]:std::array<std::pair<U,U>,4>{{{0x3c0,4},{0x3c4,4},{0x3d4,4},{0x451,1}}})if(!blockword(b,o,n))return false;for(U o:{0x3a8,0x3ac,0x3b8,0x3bc})if(b[o-0x3a0].has_value()!=b[o-0x3a0+1].has_value())return false;return true;}
// Independent source offset map, checked against the canonical owner. No
// production projection/publish is used to calculate expected typed output.
constexpr bool alias(U o){return(o>=0x3a8&&o<0x3aa)||(o>=0x3ac&&o<0x3ae)||(o>=0x3b8&&o<0x3ba)||(o>=0x3bc&&o<0x3be)||(o>=0x3c0&&o<0x3c8)||(o>=0x3d4&&o<0x424)||o==0x451;}
void typed_block(const pf::Block& b,pcs::PlayerFields& p,shared::Fields& f){need(representable(b),"unrepresentable canonical input");for(U i=0;i<2;++i){p.pbw.angles[i]=std::uint16_t(*blockword(b,0x3c0+2*i,2));p.pbw.magnitudes[i]=std::uint16_t(*blockword(b,0x3c4+2*i,2));}p.pbw.field3d4=*blockword(b,0x3d4,4);p.pbw.field451=std::uint8_t(*blockword(b,0x451,1));
 auto half=[&](U o)->std::optional<std::uint16_t>{auto v=blockword(b,o,2);return v?std::optional<std::uint16_t>{std::uint16_t(*v)}:std::nullopt;};f.pbw.field3a8=half(0x3a8);f.pbw.field3ac=half(0x3ac);f.pbw.field3b8=half(0x3b8);f.pbw.field3bc=half(0x3bc);
 for(U i=0;i<19;++i)for(U j=0;j<4;++j)f.pbw.words3d8[i][j]=b[0x38+4*i+j];std::size_t n=0;for(U o=0x3a0;o<0x464;++o)if(!alias(o)){need(n<99&&ps::remaining_offsets[n]==o,"canonical complement mapping");f.pbw.input_other[n++]=b[o-0x3a0];}need(n==99,"canonical complement size");}
void canonical(const Inputs& x,const pf::Block& expected,const pf::Effects& effects){const auto& q=x.facts;if(!representable(x.block)||!q.global2b||!q.global2f||!q.pbw30||!q.pbw33||!q.pbw371||!q.pbw372||!q.pwe8)return;++wrapper_cases;
 pcs::PlayerFields p{};shared::Fields f{};p.global={0xa53b,*q.global2b,*q.global2f,0xef,q.globald72};p.pbw.field30=*q.pbw30;p.pbw.field33=*q.pbw33;p.pbw.field371=*q.pbw371;p.pbw.field372=*q.pbw372;p.pw.fielde8=*q.pwe8;
 p.fieldsd10={0x7fc12345,0x80000000,0xff800000};p.pbw.field20=0x15324678;p.pbw.field24=0xa5b6c7d8;p.pw.field6c0=0xe7;p.selection_pbw.field6fa=0xd2;f.pw.field6a4_bits=0x7f812345;f.pbw.field10_bits=0x7f800000;f.pbw.field54_bits=0xffc1abcd;
 f.pw.field13c_bits=q.pw13c_bits;f.pbw.field464_bits=q.pbw464_bits;f.pw.field9dc=q.pw9dc;f.pw.field9de=q.pw9de;f.pw.field9dd=q.pw9dd;typed_block(x.block,p,f);
 const auto p_before=p;const auto f_before=f;auto r=pf::apply(p,f,x.context);need(std::holds_alternative<pf::Output>(r),"canonical wrapper rejected");const auto& out=std::get<pf::Output>(r);auto wantp=p;auto wantf=f;typed_block(expected,wantp,wantf);need(out.player==wantp,"canonical player mismatch");need(out.fields==wantf,"canonical extras mismatch");need(out.effects==effects,"canonical effects mismatch");need(p==p_before&&f==f_before,"canonical input mutated");}
void reject(Inputs x,const char* field){const auto before=x;auto r=pf::apply_block(x.block,x.facts,x.context);need(std::holds_alternative<pf::Error>(r),"unknown accepted");const auto& e=std::get<pf::Error>(r);need(e.code==pf::ErrorCode::unknown_field&&e.field==field,"wrong unknown cause");unchanged(x,before);++typed_errors;}
void boundaries(Inputs normal,Inputs restored,Inputs suppressed){
 #define MISS(owner,name,label) {auto x=normal;x.owner.name.reset();reject(x,label);}
 MISS(facts,global2b,"global2b");MISS(facts,global2f,"global2f");MISS(facts,pbw33,"PBW33");MISS(facts,pw13c_bits,"PW13c");MISS(facts,globald72,"globald72");MISS(context,object20,"fb5edc+20");MISS(context,object21,"fb5edc+21");MISS(context.manager,ad24,"contextAD24");MISS(facts,pbw464_bits,"PBW464");MISS(context,byte108b9b8,"108b9b8");MISS(context,byte1083850,"1083850");MISS(context.manager,ad50,"contextAD50");MISS(context.manager,ad64,"contextAD64");MISS(facts,pw9dc,"PW9dc");MISS(facts,pw9de,"PW9de");MISS(facts,pw9dd,"PW9dd");MISS(facts,pbw371,"PBW371");MISS(context,byte109bd29,"109bd29");MISS(facts,pwe8,"PWe8");
 #undef MISS
 {auto x=normal;x.context.object20=1;x.context.object21.reset();reject(x,"fb5edc+21");}
 {auto x=normal;x.context.byte108b9b8=1;x.context.manager.ad50.reset();reject(x,"contextAD50");}
 {auto x=suppressed;x.block[0x3cc-0x3a0+1].reset();reject(x,"PBW3cc");}
 {auto x=restored;x.context.pad.field320.reset();reject(x,"pad320");}
 {auto x=normal;x.facts.global2b=2;x.facts.pbw30=1;x.facts.pbw33=0;x.context.mode2=pf::Unknown{};reject(x,"edbe90 presence");}
 {Inputs x{};x.facts.global2b=2;x.facts.pbw30=3;const auto before=x;auto r=pf::apply_block(x.block,x.facts,x.context);need(std::holds_alternative<pf::BlockOutput>(r),"nonlocal unknown rejected");need(std::get<pf::BlockOutput>(r).block==x.block,"nonlocal block changed");unchanged(x,before);++lazy_controls;}
}
int main(int argc,char**argv){try{
 need(argc==3,"usage: player-input-filter-tests fixture sha256");auto n=std::filesystem::file_size(argv[1]);need(n<=4*1024*1024,"fixture capacity");std::ifstream file(argv[1],std::ios::binary);Bytes raw{std::istreambuf_iterator<char>(file),{}};need(raw.size()==n,"fixture length");need(picosha2::hash256_hex_string(raw)==argv[2],"fixture SHA");Reader r{raw};auto magic=r.take(8);need(std::string(magic.begin(),magic.end())=="MH4UPIF1","schema");const auto count=r.word();need(count==309,"case count");
 for(const char* hash:{"d4191ea404df1e9ade8fb401983e3497699dae5aa994937f7859c43f84403435","35f2bb84882d245c2067a3a7d3b8fcd282c9a94da187a241f9f850f5621d2978","dde40bdb151b52a108b79074426491c793e13bc553ac262766cc53985b3e9ae2"}){auto b=r.take(32);need(picosha2::bytes_to_hex_string(b.begin(),b.end())==hash,"source identity");}
 need(r.word()==38,"atom count");for(U i=0;i<38;++i){need(r.word()==addresses[i],"atom address");need(r.word()==widths[i],"atom width");}
 std::array<Inputs,3> basics{};
 for(U index=0;index<count;++index){const auto bytes=r.word();need(bytes<=65536,"record extent");Reader q{r.take(bytes)};need(q.word()==index,"case order");const auto name=q.text();(void)name;q.word();q.word();Atoms atoms{};for(U i=0;i<38;++i){atoms[i]={q.word(),q.word()};need((atoms[i].mask&~((1U<<widths[i])-1U))==0,"known scalar mask");if(widths[i]<4)need(atoms[i].value<(1U<<(8*widths[i])),"scalar width");}
 auto before=q.take(196),bm=q.take(196),after=q.take(196),am=q.take(196);pf::Block input{},expected{};for(U i=0;i<196;++i){need(bm[i]<=1&&am[i]<=1,"known byte mask");if(bm[i])input[i]=before[i];if(am[i]){expected[i]=after[i];++known_bytes;}}
 std::array<U,5> observed{};for(auto& v:observed){v=q.word();need(v<=1,"effect flag");}const auto nc=q.word();need(nc<=32,"call count");bool any_clear=false;for(U i=0;i<nc;++i){const auto pc=q.word();need(pc>=0x100000&&pc<0xd10000,"call PC");q.word();q.word();for(U j=0;j<4;++j)q.word();if(pc==0x2fef50)any_clear=true;}
 pf::Effects effects{observed[0]!=0,any_clear,observed[2]!=0,observed[3]!=0,observed[4]!=0};need(any_clear==(observed[1]!=0||observed[2]!=0),"clear trace inconsistent");
 const auto nw=q.word();need(nw<=256,"write count");Bytes replay(before.begin(),before.end()),mask(bm.begin(),bm.end());for(U i=0;i<nw;++i){const auto pc=q.word(),o=q.word(),width=q.word(),value=q.word();need(pc>=0x100000&&pc<0xd10000,"write PC");need((width==1||width==2||width==4)&&o>=0x3a0&&o+width<=0x464,"write extent");if(width<4)need(value<(1U<<(8*width)),"write value width");for(U j=0;j<width;++j){replay[o-0x3a0+j]=std::uint8_t(value>>(8*j));mask[o-0x3a0+j]=1;}stored_bytes+=width;}stores_count+=nw;
 need(std::equal(replay.begin(),replay.end(),after.begin()),"observed stores inconsistent with output");need(std::equal(mask.begin(),mask.end(),am.begin()),"observed knowledge inconsistent");need(q.word()==8,"arena hash count");q.take(8*64);q.end();
 auto x=decode(atoms,input);const auto saved=x;auto result=pf::apply_block(x.block,x.facts,x.context);if(auto e=std::get_if<pf::Error>(&result))throw std::runtime_error("case "+std::to_string(index)+" native error "+std::string(e->field));const auto& out=std::get<pf::BlockOutput>(result);need(out.block==expected,"block differs from ARM");need(out.effects==effects,"effects differ from ARM");unchanged(x,saved);canonical(x,expected,effects);if(index<3)basics[index]=x;
 }r.end();boundaries(basics[0],basics[1],basics[2]);std::cout<<"{\"status\":\"passed\",\"cases\":309,\"canonical_cases\":"<<wrapper_cases<<",\"known_output_bytes\":"<<known_bytes<<",\"source_stores\":"<<stores_count<<",\"source_store_bytes\":"<<stored_bytes<<",\"typed_errors\":"<<typed_errors<<",\"lazy_controls\":"<<lazy_controls<<",\"checks\":"<<checks<<"}\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
