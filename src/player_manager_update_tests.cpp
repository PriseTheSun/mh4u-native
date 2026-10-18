#include "player_manager_update.hpp"
#include "test_support/player_manager_effects.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace mu=mh4u::player_manager_update;
namespace fp=mh4u::binary32_arithmetic;
namespace pc=mh4u::player_controller_step;
namespace pf=mh4u::player_fields;
namespace oq=mh4u::player_object_query;
namespace pr=mh4u::player_reference;
using U=std::uint32_t;
std::size_t checks=0,cases=0,bytes_compared=0,call_checks=0;
void check(bool yes,const char* message){++checks;if(!yes)throw std::runtime_error(message);}
struct Reader {
 std::vector<std::uint8_t> data;std::size_t at{};
 Reader(const char* path,const char* sha){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("fixture open");const auto n=f.tellg();if(n<0||n>256*1024*1024)throw std::runtime_error("fixture size");data.resize(std::size_t(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(data.data()),n)||f.peek()!=EOF)throw std::runtime_error("fixture read");if(picosha2::hash256_hex_string(data)!=sha)throw std::runtime_error("fixture SHA");}
 std::vector<std::uint8_t> take(std::size_t n){if(n>data.size()-at)throw std::runtime_error("fixture truncated");auto p=data.begin()+std::ptrdiff_t(at);at+=n;return {p,p+std::ptrdiff_t(n)};}
 U word(){auto b=take(4);return U(b[0])|U(b[1])<<8|U(b[2])<<16|U(b[3])<<24;}
};
struct Arena {U base;std::vector<std::uint8_t> data,known;};
using Memory=std::vector<Arena>;
std::optional<U> get(const Memory& mem,U p,std::size_t n){for(auto& a:mem)if(p>=a.base&&p-a.base<=a.data.size()&&n<=a.data.size()-(p-a.base)){U v=0;for(std::size_t i=0;i<n;++i){if(!a.known[p-a.base+i])return {};v|=U(a.data[p-a.base+i])<<(i*8);}return v;}throw std::runtime_error("fixture address");}
U required(const Memory& m,U p,std::size_t n){auto v=get(m,p,n);if(!v)throw std::runtime_error("required projection");return *v;}
void put(Memory& mem,U p,std::optional<U> v,std::size_t n){if(!v)return;for(auto& a:mem)if(p>=a.base&&p-a.base<=a.data.size()&&n<=a.data.size()-(p-a.base)){for(std::size_t i=0;i<n;++i){a.data[p-a.base+i]=std::uint8_t(*v>>(i*8));a.known[p-a.base+i]=1;}return;}throw std::runtime_error("encode extent");}
struct Owners {pc::PlayerFields player{};pf::Fields fields{};};
Owners decode_player(const Memory& m,U b,U w){Owners out;auto& p=out.player;auto& f=out.fields;
 p.pbw.field30=std::uint8_t(required(m,b+0x30,1));p.pbw.field31=std::uint8_t(required(m,b+0x31,1));p.pbw.field33=std::uint8_t(required(m,b+0x33,1));p.pbw.field24=required(m,b+0x24,4);
 p.pbw.field371=std::uint8_t(required(m,b+0x371,1));p.pbw.field372=std::uint16_t(required(m,b+0x372,2));p.selection_pbw.field370=std::uint8_t(required(m,b+0x370,1));p.selection_pbw.field6f9=get(m,b+0x6f9,1);
 f.pbw.field10_bits=get(m,b+0x10,4);f.pbw.field14_bits=get(m,b+0x14,4);f.pbw.field18_bits=get(m,b+0x18,4);
 for(std::size_t i=0;i<19;++i)for(std::size_t j=0;j<4;++j)f.pbw.words3d8[i][j]=get(m,b+0x3d8+U(i*4+j),1);
 for(std::size_t i=0;i<28;++i)f.pbw.descriptors[0][i]=get(m,b+0x1c0+U(i),1);
 f.pw.fieldb4=get(m,w+0xb4,2);f.pw.fielde0_bits=get(m,w+0xe0,4);f.pw.fielde4=get(m,w+0xe4,2);f.pw.fielde6=get(m,w+0xe6,2);f.pw.field50c=get(m,w+0x50c,1);f.pw.field50e=get(m,w+0x50e,2);
 p.pw.fielddc=required(m,w+0xdc,4);p.pw.field50d=std::uint8_t(required(m,w+0x50d,1));return out;
}
void encode(Memory& m,U w,const pc::PlayerFields& p,const pf::Fields& f){put(m,w+0xb4,f.pw.fieldb4,2);put(m,w+0xdc,p.pw.fielddc,4);put(m,w+0xe0,f.pw.fielde0_bits,4);put(m,w+0xe4,f.pw.fielde4,2);put(m,w+0xe6,f.pw.fielde6,2);put(m,w+0x50c,f.pw.field50c,1);put(m,w+0x50d,p.pw.field50d,1);put(m,w+0x50e,f.pw.field50e,2);}
mu::ManagerState decode_manager(const Memory& m){mu::ManagerState s;constexpr U b=0x2300000;
 s.field112b=get(m,b+0x112b,1);s.field1203=get(m,b+0x1203,1);s.field15ff=get(m,b+0x15ff,1);s.field15a4=get(m,b+0x15a4,1);s.field1645=get(m,b+0x1645,1);
 s.field102c=get(m,b+0x102c,4);s.field1040=get(m,b+0x1040,4);s.field1044=get(m,b+0x1044,4);s.field1048=get(m,b+0x1048,4);s.field104c=get(m,b+0x104c,4);for(std::size_t i=0;i<6;++i)s.fields1050[i]=get(m,b+0x1050+U(i),1);return s;
}
void encode_manager(Memory& m,const mu::ManagerState& s){constexpr U b=0x2300000;
 put(m,b+0x102c,s.field102c,4);put(m,b+0x1040,s.field1040,4);put(m,b+0x1044,s.field1044,4);put(m,b+0x1048,s.field1048,4);put(m,b+0x104c,s.field104c,4);for(std::size_t i=0;i<6;++i)put(m,b+0x1050+U(i),s.fields1050[i],1);
}

void oracle(const char* path,const char* sha){Reader r(path,sha);check(r.take(8)==std::vector<std::uint8_t>({'M','H','4','U','M','G','0','1'}),"fixture magic");auto count=r.word(),arenas=r.word();check(count==408&&arenas==12,"fixture shape");std::vector<std::pair<U,U>> layout;std::size_t total=0;for(U i=0;i<arenas;++i){U b=r.word(),n=r.word();check(n<=65536,"arena bound");total+=n;layout.emplace_back(b,n);}check(total<=1024*1024,"total extent");
 for(U i=0;i<count;++i){auto n=r.word();check(n<=256,"name bound");auto namebytes=r.take(n);std::string name(namebytes.begin(),namebytes.end());U entry=r.word();check(entry==0xb11f3c||entry==0xb24c44||entry==0xb146c4,"entry identity");U nc=r.word();check(nc<=256,"call count");std::vector<mu::Call> expected_calls;for(U j=0;j<nc;++j){std::array<U,6> c;for(auto& x:c)x=r.word();if(auto x=mh4u::test_manager_effects::call(c,0x2010000,0x21101c0))expected_calls.push_back(*x);}
  Memory before,after;for(auto* target:{&before,&after})for(auto [base,size]:layout)target->push_back({base,r.take(size),r.take(size)});
  for(const auto* memory:{&before,&after})for(const auto& arena:*memory)check(std::all_of(arena.known.begin(),arena.known.end(),[](auto b){return b<=1;}),"knownness domain");
  auto caller=decode_player(before,0x2100000,0x2200000),foreign=decode_player(before,0x2110000,0x2210000);
  caller.player.global.field2b=std::uint8_t(required(before,0x240002b,1));caller.player.global.field2f=std::uint8_t(required(before,0x240002f,1));caller.player.global.field7a=get(before,0x240007a,1);
  foreign.player.global.field2f=1; // Deliberately unrelated snapshot must never own the source global.
  auto manager=decode_manager(before);oq::Collection objects{};
  for(std::size_t j=0;j<32;++j){U ptr=required(before,0x2500018+U(4*j),4);if(!ptr)objects.slots[j]=oq::Absent{};else {check(ptr>=0x2600000&&ptr<0x2604000&&(ptr-0x2600000)%0x1000==0,"root address");objects.slots[j]=oq::ObjectIndex{(ptr-0x2600000)/0x1000};}}
  for(U j=0;j<4;++j){auto ptr=get(before,0x2600e28+0x1000*j,4);if(ptr){if(!*ptr)objects.objects[j].data=oq::Absent{};else {check(*ptr>=0x2700000&&*ptr<0x2704000&&(*ptr-0x2700000)%0x1000==0,"data address");objects.objects[j].data=oq::DataIndex{(*ptr-0x2700000)/0x1000};}}auto& d=objects.data[j];d.field140=get(before,0x2700140+0x1000*j,1);for(U k=0;k<3;++k)d.position_bits[k]=get(before,0x2700198+0x1000*j+4*k,4);}
  std::array<mh4u::motion_pending::Layer,1> layers{};layers[0].animation.layer.selector=std::uint16_t(required(before,0x2000244,2));
  mu::PlayerLink link;U peer=required(before,0x2300e18,4);if(peer==0x2000000)link=mu::Caller{};else if(peer==0x2010000)link=mu::ForeignPlayer{&foreign.player,&foreign.fields};else throw std::runtime_error("player identity");
  pr::Reference reference=pr::Absent{};if(required(before,0x230001c,4)){pr::Points p{std::bit_cast<float>(required(before,0x2304040,4)),std::bit_cast<float>(required(before,0x2304048,4)),std::bit_cast<float>(required(before,0x2304060,4)),std::bit_cast<float>(required(before,0x2304068,4)),get(before,0x2304044,4),get(before,0x2304064,4)};reference=p;}
  mu::Context context{layers,&objects,mu::ManagerView{&manager,link},reference,mh4u::player_direction_refresh::Virtual84::uplayer_b085bc};
  auto saved_c=caller,saved_f=foreign;auto saved_m=manager;auto saved_o=objects;
  auto result=entry==0xb11f3c?mu::collection_phase(caller.player,caller.fields,context):entry==0xb24c44?mu::manager_phase(caller.player,caller.fields,context):mu::apply(caller.player,caller.fields,context);
  if(auto* e=std::get_if<mu::Error>(&result))throw std::runtime_error(name+" native error "+std::string(e->field));auto& out=std::get<mu::Output>(result);
  check(caller.player==saved_c.player&&caller.fields==saved_c.fields&&foreign.player==saved_f.player&&foreign.fields==saved_f.fields&&objects==saved_o&&manager==saved_m,"borrowed mutation");
  auto preserved_p=caller.player;auto preserved_f=caller.fields;
  preserved_p.pw.fielddc=out.player.pw.fielddc;preserved_p.pw.field50d=out.player.pw.field50d;
  preserved_f.pw.fieldb4=out.fields.pw.fieldb4;preserved_f.pw.fielde0_bits=out.fields.pw.fielde0_bits;preserved_f.pw.fielde4=out.fields.pw.fielde4;preserved_f.pw.fielde6=out.fields.pw.fielde6;preserved_f.pw.field50c=out.fields.pw.field50c;preserved_f.pw.field50e=out.fields.pw.field50e;
  check(out.player==preserved_p&&out.fields==preserved_f,"unconsumed owner fields");
  if(out.manager){auto preserved=manager;preserved.field102c=out.manager->field102c;preserved.field1040=out.manager->field1040;preserved.field1044=out.manager->field1044;preserved.field1048=out.manager->field1048;preserved.field104c=out.manager->field104c;preserved.fields1050=out.manager->fields1050;check(preserved==*out.manager,"manager read-only gates");}
  Memory actual=before;encode(actual,0x2200000,out.player,out.fields);if(out.manager)encode_manager(actual,*out.manager);
  if(out.foreign){check(out.foreign->identity==mu::ForeignPlayer{&foreign.player,&foreign.fields},"foreign identity");put(actual,0x22100dc,out.foreign->fielddc,4);put(actual,0x22100e0,out.foreign->fielde0_bits,4);}
  for(std::size_t j=0;j<actual.size();++j){if(actual[j].data!=after[j].data||actual[j].known!=after[j].known){for(std::size_t k=0;k<actual[j].data.size();++k)if(actual[j].data[k]!=after[j].data[k]||actual[j].known[k]!=after[j].known[k])throw std::runtime_error(name+" arena mismatch "+std::to_string(actual[j].base+U(k))+" native "+std::to_string(actual[j].data[k])+" original "+std::to_string(after[j].data[k]));}checks+=2;bytes_compared+=actual[j].data.size();}
  if(out.effects.call_count!=expected_calls.size())throw std::runtime_error(name+" helper count "+std::to_string(out.effects.call_count)+" expected "+std::to_string(expected_calls.size()));
  for(std::size_t j=0;j<expected_calls.size();++j){if(out.effects.calls[j]!=expected_calls[j])throw std::runtime_error(name+" call "+std::to_string(j)+" kind "+std::to_string(int(out.effects.calls[j].kind))+" expected "+std::to_string(int(expected_calls[j].kind)));++checks;++call_checks;}
  ++cases;
 }
 check(r.at==r.data.size(),"fixture trailing bytes");
}
mu::Output success(mu::Result result){check(std::holds_alternative<mu::Output>(result),"expected success");return std::get<mu::Output>(std::move(result));}
void rejected(const pc::PlayerFields& p,const pf::Fields& f,const mu::Context& c,bool first,mu::ErrorCode code,std::string_view field){
 const auto before_p=p;const auto before_f=f;
 auto result=first?mu::collection_phase(p,f,c):mu::manager_phase(p,f,c);
 check(std::holds_alternative<mu::Error>(result),"expected typed failure");const auto& e=std::get<mu::Error>(result);
 check(e.code==code&&e.field==field,"error identity");check(p==before_p&&f==before_f,"failure mutation");
}
void closure_boundaries(){
 pc::PlayerFields p{};pf::Fields f{};std::array<mh4u::motion_pending::Layer,1> layers{};
 mu::ManagerState m{};m.field112b=0;m.field1203=0;m.field15ff=0;m.field15a4=0;m.field1645=0;m.fields1050[4]=0;
 oq::Collection objects{};for(auto& slot:objects.slots)slot=oq::Absent{};
 p.pbw.field31=4;p.pbw.field371=1;p.global.field7a=0;p.selection_pbw.field6f9=1;
 f.pw.fieldb4=0;f.pbw.words3d8[3]=mh4u::player_input_storage::split(128);f.pbw.descriptors[0][0]=6;
 mu::Context c{layers,&objects,mu::ManagerView{&m,mu::Caller{}},pr::Absent{},mh4u::player_direction_refresh::Virtual84::uplayer_b085bc};
 auto out=success(mu::manager_phase(p,f,c));check(out.player.pw.fielddc==0x80000000&&out.fields.pw.fielde0_bits==0x80000000,"signed-zero vertical");check(out.player.pw.field50d==1&&out.fields.pw.fielde4==50,"toggle and final store");check(out.manager&&out.manager->fields1050[4]==1,"manager activated");
 for(auto raw:{2,255}){
  auto discriminant=p;auto masked=f;discriminant.selection_pbw.field6f9=std::uint8_t(raw);discriminant.pw.field50d=1;
  masked.pbw.words3d8[3]={};const auto before=discriminant;const auto before_fields=masked;const auto before_manager=m;
  const auto false_flag=success(mu::manager_phase(discriminant,masked,c));
  check(false_flag.player.pw.field50d==0&&false_flag.fields.pw.field50c==0&&false_flag.fields.pw.field50e==0,"non-one flag follows fallback and skips PBW3e4");
  check(false_flag.manager==m&&!false_flag.foreign&&!false_flag.effects.caller_vertical_written,"non-one flag preserves manager and reference");
  std::size_t flag_calls=0;for(std::size_t i=0;i<false_flag.effects.call_count;++i)flag_calls+=false_flag.effects.calls[i].kind==mu::CallKind::flag6f9;
  check(flag_calls==2,"non-one flag preserves repeated original calls");
  check(discriminant==before&&masked==before_fields&&m==before_manager,"non-one flag input owners unchanged");
 }
 auto original_m=m;c.manager=mu::ManagerView{&m,mu::Unknown{}};rejected(p,f,c,false,mu::ErrorCode::unknown_field,"manager.e18");check(m==original_m,"manager rollback");
 c.manager=mu::ManagerView{&m,mu::Absent{}};rejected(p,f,c,false,mu::ErrorCode::absent_player,"manager.e18");
 pc::PlayerFields foreign=p;pf::Fields ff=f;foreign.global.field7a.reset();foreign.global.field2f=1;
 c.manager=mu::ManagerView{&m,mu::ForeignPlayer{&foreign,&ff}};out=success(mu::manager_phase(p,f,c));check(out.foreign&&out.foreign->fielddc==0x80000000&&out.foreign->fielde0_bits==0x80000000,"foreign source-global");check(out.player.pw.fielddc==p.pw.fielddc&&!out.fields.pw.fielde0_bits,"foreign must not replace caller");check(out.foreign->identity==mu::ForeignPlayer{&foreign,&ff},"foreign patch identity");
 c.manager=mu::ManagerView{&m,mu::ForeignPlayer{&p,&f}};out=success(mu::manager_phase(p,f,c));check(!out.foreign&&out.effects.caller_vertical_written,"same owner alias");
 c.manager=mu::ManagerView{&m,mu::ForeignPlayer{&p,&ff}};rejected(p,f,c,false,mu::ErrorCode::invalid_identity,"partial Player alias");
 c.manager=mu::ManagerView{&m,mu::ForeignPlayer{&foreign,&f}};rejected(p,f,c,false,mu::ErrorCode::invalid_identity,"partial Player alias");
 c.manager=mu::ManagerView{&m,mu::ForeignPlayer{nullptr,&ff}};rejected(p,f,c,false,mu::ErrorCode::invalid_identity,"manager.e18 foreign");
 m.fields1050[4]=1;c.manager=mu::ManagerView{&m,mu::Unknown{}};p.global.field7a.reset();c.reference=pr::Unknown{};out=success(mu::manager_phase(p,f,c));check(out.manager==m&&!out.foreign,"busy skips target and global");
 m.fields1050[4]=0;c.manager=mu::ManagerView{&m,mu::Caller{}};rejected(p,f,c,false,mu::ErrorCode::unknown_field,"Global7a");
 p.global.field7a=0;rejected(p,f,c,false,mu::ErrorCode::unknown_field,"manager.1c");
 p.global.field7a=1;out=success(mu::manager_phase(p,f,c));check(!out.effects.caller_vertical_written,"global skips reference");p.global.field7a=0;
 c.reference=pr::Points{0,0,1,1};rejected(p,f,c,false,mu::ErrorCode::unknown_field,"reference44");
 c.reference=pr::Points{0,0,1,1,0,{}};rejected(p,f,c,false,mu::ErrorCode::unknown_field,"reference64");
 c.reference=pr::Points{0,0,1,1,0,0x3f800000};out=success(mu::manager_phase(p,f,c));check(out.effects.caller_vertical_written,"known reference");
 c.reference=pr::Points{0,0,1,1,0,0x7fa12345};out=success(mu::manager_phase(p,f,c));check(out.player.pw.fielddc==0x80000000,"raw NaN angular conversion");
 c.reference=pr::Unknown{};foreign.pbw.field33=1;c.manager=mu::ManagerView{&m,mu::ForeignPlayer{&foreign,&ff}};out=success(mu::manager_phase(p,f,c));check(!out.foreign,"foreign remote skips reference");
 c.manager=mu::Absent{};rejected(p,f,c,false,mu::ErrorCode::absent_manager,"manager1057294");p.pbw.field371=6;check(!success(mu::manager_phase(p,f,c)).manager,"state6 skips manager");p.pbw.field371=1;
 m.field112b=1;m.field1203.reset();c.manager=mu::ManagerView{&m,mu::Unknown{}};out=success(mu::manager_phase(p,f,c));check(out.player.pw.field50d==0&&out.fields.pw.field50c==0&&!out.fields.pw.field50e,"gate clear width/laziness");m.field112b=0;rejected(p,f,c,false,mu::ErrorCode::unknown_field,"manager1203");m.field1203=0;
 c.manager=mu::ManagerView{&m,mu::Caller{}};c.reference=pr::Absent{};
 p.selection_pbw.field6f9.reset();rejected(p,f,c,false,mu::ErrorCode::unknown_field,"PBW6f9");p.selection_pbw.field6f9=1;
 f.pbw.words3d8[3][2].reset();rejected(p,f,c,false,mu::ErrorCode::unknown_field,"PBW3e4");f.pbw.words3d8[3]=mh4u::player_input_storage::split(128);
 p.selection_pbw.field6f9=0;p.pbw.field371=0;p.pbw.field372=145;rejected(p,f,c,false,mu::ErrorCode::unknown_field,"PBW3d8");f.pbw.words3d8[0]=mh4u::player_input_storage::split(0);check(!success(mu::manager_phase(p,f,c)).foreign,"mode2 mask absent");
 p.pbw.field371=1;p.selection_pbw.field6f9=1;p.pbw.field31=4;p.pw.field50d=1;f.pbw.descriptors[0][0].reset();out=success(mu::manager_phase(p,f,c));check(out.player.pw.field50d==0,"toggle zero skips descriptor");p.pw.field50d=0;f.pbw.descriptors[0][0]=11;rejected(p,f,c,false,mu::ErrorCode::unknown_field,"PBW1c0.byte1");f.pbw.descriptors[0][1]=64;out=success(mu::manager_phase(p,f,c));check(!out.fields.pw.fielde4&&out.manager->fields1050[1]==1,"descriptor skips finalstore");
 layers[0].animation.layer.selector=38;objects.slots[0]=oq::ObjectIndex{0};objects.objects[0].data=oq::DataIndex{0};objects.data[0].field140=0;out=success(mu::collection_phase(p,f,c));check(out.effects.enabled_queries==1&&!out.effects.selected_object,"disabled position unknown");
 objects.data[0].field140=1;rejected(p,f,c,true,mu::ErrorCode::unknown_field,"D198");objects.data[0].position_bits={0,0,0};rejected(p,f,c,true,mu::ErrorCode::unknown_field,"PBW10");f.pbw.field10_bits=0;f.pbw.field14_bits=0;f.pbw.field18_bits=0;
 objects.slots[31]=oq::Unknown{};rejected(p,f,c,true,mu::ErrorCode::collection,"slot");objects.slots[31]=oq::ObjectIndex{0};out=success(mu::collection_phase(p,f,c));check(out.effects.accepted_objects==2&&out.effects.selected_object==oq::ObjectIndex{0},"repeated aliases");
 objects.data[0].position_bits[0]=0x7fc12345;out=success(mu::collection_phase(p,f,c));check(!out.effects.selected_object,"NaN distance rejected");
 const int rounding=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"set round");rejected(p,f,c,true,mu::ErrorCode::arithmetic,"distance.x");layers[0].animation.layer.selector=0;check(success(mu::collection_phase(p,f,c)).fields.pw.fieldb4==0,"unconsumed FP environment");check(std::fesetround(rounding)==0,"restore round");
 for(auto [x,z,expected]:std::array<std::array<U,3>,5>{{{0,0,0},{0x3f800000,0,16384},{0,0xbf800000,32768},{0xbf800000,0,49152},{0x7fa12345,0,0}}})check(std::get<U>(pr::angular_core(x,z))==expected,"angular core analytic cardinal");
}
void boundaries(){
 pc::PlayerFields p{};pf::Fields f{};mu::Context c{};
 auto bad=mu::collection_phase(p,f,c);check(std::get<mu::Error>(bad).field=="virtual84","binding unknown");
 c.virtual84=mh4u::player_direction_refresh::Virtual84::uplayer_b085bc;
 check(std::holds_alternative<mu::Error>(mu::collection_phase(p,f,c)),"selector extent");
 std::array<mh4u::motion_pending::Layer,1> layers{};c.layers=layers;f.pw.fieldb4=1025;
 check(std::get<mu::Output>(mu::collection_phase(p,f,c)).fields.pw.fieldb4==1,"lazy manager relaxation");
 p.global.field2f=1;check(std::holds_alternative<mu::Output>(mu::manager_phase(p,f,c)),"remote lazy manager");
 for(U x:{0U,0x80000000U,0x7f800000U,0x7fc12345U,0x7fa12345U,0xffa12345U,0xbf800000U}){auto result=fp::sqrt(x);check(std::holds_alternative<U>(result),"sqrt raw");U expected=(x&0x7fffffffU)>0x7f800000U?x|0x400000U:((x&0x80000000U)&&(x&0x7fffffffU))?0x7fc00000U:x;check(std::get<U>(result)==expected,"sqrt pattern");}
 check(std::get<U>(fp::sqrt(0x40800000))==0x40000000,"sqrt finite");
 for(U t=0;t<256;++t){p.pbw.field31=std::uint8_t(t);for(U s:{0U,1U,2U,3U,0xffffffffU})check(mh4u::player_transition::type_group(p.pbw,s)==(s==0?(t==4||t==5||t==6||t==15):s==2?t==10:(t==4||t==5||t==6||t==10||t==15)),"type predicate");}
}
int main(int argc,char** argv){try{boundaries();closure_boundaries();if(argc==3)oracle(argv[1],argv[2]);else if(argc!=1)throw std::runtime_error("arguments");std::cout<<"{\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"compared_bytes\":"<<bytes_compared<<",\"calls\":"<<call_checks<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
