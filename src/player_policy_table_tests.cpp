#include "player_policy_table.hpp"
#include "picosha2.h"
#include <algorithm>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
namespace {
namespace p=mh4u::player_policy_table;
std::size_t checks{},cases{};
void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
std::string hash(std::span<const std::uint8_t> v){return picosha2::hash256_hex_string(v.begin(),v.end());}
p::Table get(p::Result<p::Table> v){if(auto* e=std::get_if<p::Error>(&v))throw std::runtime_error(std::string(e->message));return std::get<p::Table>(std::move(v));}
std::vector<std::uint8_t> read(const char* path,std::size_t limit){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"File open");const auto n=f.tellg();check(n>=0&&std::uintmax_t(n)<=limit,"File allocation limit");std::vector<std::uint8_t> b(static_cast<std::size_t>(n));f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size())))&&f.peek()==std::char_traits<char>::eof(),"File exact read");return b;}
std::uint32_t u32(std::span<const std::uint8_t> b,std::size_t off){check(off<=b.size()&&b.size()-off>=4,"Fixture range");std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(b[off+i])<<(i*8);return v;}
void put(std::span<std::uint8_t> b,std::size_t off,std::uint32_t v,unsigned size){for(unsigned i=0;i<size;++i)b[off+i]=static_cast<std::uint8_t>(v>>(8*i));}
void rejected(const std::vector<std::uint8_t>& b,p::ErrorCode code){const auto v=p::Table::decode(b,hash(b));check(std::holds_alternative<p::Error>(v)&&std::get<p::Error>(v).code==code,"Wrong parser failure");}
void boundaries(){
 rejected({},p::ErrorCode::incomplete_first_byte);
 for(std::size_t n=1;n<22;++n)rejected(std::vector<std::uint8_t>(n),p::ErrorCode::incomplete_record);
 for(std::size_t n=1;n<=48;++n)rejected(std::vector<std::uint8_t>(n*22),p::ErrorCode::incomplete_first_byte);
 for(std::size_t n=0;n<=48;++n){auto b=std::vector<std::uint8_t>(n*22+1);b.back()=255;auto t=get(p::Table::decode(b,hash(b)));check(t.coverage()==p::Coverage{p::Termination::sentinel,n,n*22+1,255}&&t.records().size()==n,"Sentinel order/count");}
 auto b=std::vector<std::uint8_t>(1057);b.back()=17;auto t=get(p::Table::decode(b,hash(b)));check(t.coverage()==p::Coverage{p::Termination::cap,48,1057,17},"Slot49 consumed before cap");
 b.resize(p::maximum_source_bytes);std::fill(b.begin()+1057,b.end(),255);auto preserved=get(p::Table::decode(b,hash(b)));check(std::equal(preserved.bytes().begin(),preserved.bytes().end(),b.begin())&&preserved.coverage()==t.coverage(),"Unexamined tail not preserved");
 b.push_back(0);rejected(b,p::ErrorCode::source_limit);
 b={255,0,0xff};auto first=get(p::Table::decode(b,hash(b)));check(first.records().empty()&&first.coverage().examined_bytes==1&&first.bytes().size()==3,"Sentinel remainder was parsed");
 check(std::holds_alternative<p::Error>(p::Table::decode(b,"short")),"Short SHA accepted");check(std::holds_alternative<p::Error>(p::Table::decode(b,std::string(64,'0'))),"Different SHA accepted");
 auto copy=first;auto independent=get(p::Table::decode(b,hash(b)));check(copy==first&&copy.bytes().data()==first.bytes().data()&&independent.identity()!=first.identity(),"Immutable snapshot identity");b[0]=0;check(first.bytes()[0]==255,"Parser retained caller-owned bytes");
 for(std::uint32_t value:{0U,1U,0x7fffU,0x8000U,0xffffU}){std::vector<std::uint8_t> row(23);for(unsigned i=0;i<11;++i)put(row,2*i,value,2);row[0]=0x7e;row.back()=0xff;const auto owner=get(p::Table::decode(row,hash(row)));const auto& r=owner.records()[0];check(r.field00()==0x7e&&r.field01()==value/256,"Byte fields changed");for(auto word:{r.field02(),r.field04(),r.field06(),r.field08(),r.field0a(),r.field0c(),r.field0e()})check(word==value,"u16 field changed");for(auto word:{r.field10(),r.field12(),r.field14()})check(std::bit_cast<std::uint16_t>(word)==value,"Signed timing narrowed/reinterpreted");}
 auto unknown=p::install(p::Unknown{},-1);check(std::holds_alternative<p::Unknown>(unknown.table)&&unknown.timer140_bits==0xbf800000&&unknown.timer144_bits==0,"Unknown table dereferenced/defaulted");
 const int old=std::fegetround();for(int mode:{FE_TONEAREST,FE_UPWARD,FE_DOWNWARD,FE_TOWARDZERO}){check(std::fesetround(mode)==0,"Cannot set float test mode");auto out=p::install(p::Absent{},16777219);check(out.timer140_bits==0x4b800002&&std::holds_alternative<p::Absent>(out.table),"Integer RNE depends on host float environment");}check(std::fesetround(old)==0,"Restore float mode");
}
void original(const p::Table& table,std::span<const std::uint8_t> fixture){
 check(fixture.size()>=12&&std::equal(fixture.begin(),fixture.begin()+8,reinterpret_cast<const std::uint8_t*>("PPTARM01")),"Fixture magic");const auto count=u32(fixture,8);check(count<=60000&&fixture.size()==12+std::uint64_t(count)*1032,"Fixture count/extent");
 for(std::size_t i=0;i<count;++i){const auto off=12+i*1032;const auto delay=std::bit_cast<std::int32_t>(u32(fixture,off));const auto address=u32(fixture,off+4);p::TableReference reference=p::Unknown{};
  if(address==0)reference=p::Absent{};else if(address==0xf2efb8)reference=table;
  const auto state=p::install(reference,delay);check(state.table==reference,"Installed table identity changed");
  std::array<std::uint8_t,512> projected{};std::copy_n(fixture.begin()+static_cast<std::ptrdiff_t>(off+8),512,projected.begin());
  // Only the oracle adapter maps its diagnostic ARM addresses to typed tokens.
  put(projected,0x100,address,4);put(projected,0x104,state.field104,2);put(projected,0x106,state.field106,2);put(projected,0x108,state.field108,2);
  std::copy(state.latches.begin(),state.latches.end(),projected.begin()+0x10a);put(projected,0x140,state.timer140_bits,4);put(projected,0x144,state.timer144_bits,4);projected[0x148]=state.field148;
  for(std::size_t j=0;j<512;++j)check(projected[j]==fixture[off+8+512+j],"Original installer byte differs");++cases;
 }
}
}
int main(int argc,char** argv){try{check(argc==5,"Expected table/tableSHA/oracle/oracleSHA");boundaries();const auto bytes=read(argv[1],p::maximum_source_bytes);const auto table=get(p::Table::decode(bytes,argv[2]));check(table.records().size()==35&&table.coverage()==p::Coverage{p::Termination::sentinel,35,771,255},"Original table extent");
 for(std::size_t i=0;i<35;++i)check(std::equal(table.records()[i].raw.begin(),table.records()[i].raw.end(),bytes.begin()+static_cast<std::ptrdiff_t>(i*22)),"Raw record bytes changed");
 const std::array<std::array<std::uint16_t,5>,3> tail{{{1,2,1,0xc017,0x4009},{1,1,1,0xc009,0x4008},{1,9,0,0xc008,0x400b}}};for(std::size_t i=0;i<3;++i){const auto& r=table.records()[32+i];check(std::array<std::uint16_t,5>{r.field00(),r.field02(),r.field04(),r.field06(),r.field0e()}==tail[i],"Real tail source fields differ");}
 const auto fixture=read(argv[3],64U*1024U*1024U);check(hash(fixture)==argv[4],"Oracle identity");original(table,fixture);std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"original_cases\":"<<cases<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
