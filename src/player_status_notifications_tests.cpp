#include "player_status_notifications.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <vector>
namespace {
namespace nq=mh4u::notification_queue;namespace pn=mh4u::player_status_notifications;
using U=std::uint32_t;using Player=mh4u::player_controller_step::PlayerFields;using Fields=mh4u::player_fields::Fields;
std::size_t checks{},cases{},attempts{},cleanups{};
void check(bool yes,const char* why){++checks;if(!yes)throw std::runtime_error(why);}
template<class T,class...V>T value(std::variant<V...> x){if(auto* p=std::get_if<T>(&x))return std::move(*p);throw std::runtime_error("Unexpected tagged result");}
Player local(){Player p{};p.global.field2b=2;p.pbw.field30=1;p.pbw.field33=0;return p;}
Fields fields(){Fields f{};f.pbw.field34c=1;f.pw.field1f8=0;f.pw.field200=0;f.pw.field1fc=0;f.pw.field586=0x4d5;return f;}
nq::State empty(){nq::State s{};for(auto& r:s.pool)r[0]=0;s.slots.fill(nq::Absent{});s.counter=0;return s;}
void boundaries(){
 static_assert(std::is_trivially_copyable_v<nq::State>);
 const nq::Request request{0x123,0x12345,0x23456,0xaabbccdd,{0x7fa12345,0x80000000,0xffffffff}};
 nq::State unknown{};const auto unchanged=unknown;
 auto e=value<nq::Error>(nq::enqueue(unknown,request));check(e.phase==nq::Phase::pool&&e.code==nq::ErrorCode::unknown_byte&&e.index==0,"Unknown occupancy invented");check(unknown==unchanged,"Failure changed input");
 auto occupied=unknown;for(auto& r:occupied.pool)r[0]=1;
 auto full=value<nq::Output>(nq::enqueue(occupied,request));check(!full.inserted&&full.queue==occupied&&full.effects.pool_examined==16&&full.effects.slots_examined==0,"Full pool consumed slots/counter");
 auto sparse=unknown;sparse.pool[0][0]=0;sparse.slots[0]=nq::Absent{};sparse.counter=0;
 const auto sparse_before=sparse;auto accepted=value<nq::Output>(nq::enqueue(sparse,request));check(sparse==sparse_before,"Success changed borrowed state");check(accepted.inserted==nq::PoolIndex{0}&&accepted.queue.counter==1,"Insertion facts");
 std::size_t known=0;for(const auto& b:accepted.queue.pool[0])known+=bool(b);check(known==97,"Mutation footprint");
 for(std::size_t i=0;i<148;++i){const bool opaque=i==2||(i>=4&&i<0x34)||(i>=0x52&&i<0x54);check(bool(accepted.queue.pool[0][i])!=opaque,"Opaque byte was manufactured or written byte omitted");}
 check(accepted.queue.pool[0][1]==0x23&&accepted.queue.pool[0][0x34]==0x45&&accepted.queue.pool[0][0x35]==0x23&&accepted.queue.pool[0][0x36]==0x56,"Argument narrowing");
 for(std::size_t word=0;word<3;++word)for(std::size_t b=0;b<4;++b)check(accepted.queue.pool[0][0x3c+word*4+b]==std::uint8_t(request.payload[word]>>(8*b)),"Raw payload normalized");
 auto bad=sparse;bad.counter.reset();e=value<nq::Error>(nq::enqueue(bad,request));check(e.code==nq::ErrorCode::unknown_counter&&e.phase==nq::Phase::counter,"Unknown counter ignored");
 bad=sparse;bad.slots[0]=nq::Unknown{};e=value<nq::Error>(nq::enqueue(bad,request));check(e.code==nq::ErrorCode::unknown_slot,"Unknown slot treated as empty");
 bad.slots[0]=nq::PoolIndex{255};e=value<nq::Error>(nq::enqueue(bad,request));check(e.code==nq::ErrorCode::invalid_index,"Bad pool index accepted");
 bad=sparse;bad.slots[15]=nq::PoolIndex{255};check(std::holds_alternative<nq::Output>(nq::enqueue(bad,request)),"Unconsumed slot rejected");
 auto opaque=sparse;opaque.slots.fill(nq::Opaque{0});opaque.counter.reset();auto no_slot=value<nq::Output>(nq::enqueue(opaque,request));check(!no_slot.inserted&&no_slot.queue==opaque&&no_slot.effects.cleanup_attempted&&!no_slot.effects.cleanup_slot,"Opaque zero identity conflated with null or pool");
 auto aliases=sparse;aliases.slots.fill(nq::PoolIndex{0});aliases.counter=0;
 auto removed=value<nq::Output>(nq::enqueue(aliases,request));check(!removed.inserted&&removed.effects.cleanup_slot==0&&removed.queue.counter==0xffffffff,"Cleanup return/underflow");
 for(std::size_t i=0;i<15;++i)check(removed.queue.slots[i]==nq::Slot{nq::PoolIndex{0}},"Duplicate alias repaired");
 for(const auto& b:removed.queue.pool[0])check(b==0,"Cleanup did not zero whole record");
 check(std::holds_alternative<nq::Absent>(removed.queue.slots[15]),"Cleanup final slot");
 auto inserted=value<nq::Output>(nq::enqueue(removed.queue,request));check(inserted.inserted==nq::PoolIndex{0}&&inserted.queue.counter==0,"Counter increment wrap or repeated alias repair");
 auto p=local();auto f=fields();auto q=empty();const auto p0=p;const auto f0=f;const auto q0=q;
 f.pw.field1f8=0x100000;
 auto failure=value<pn::Error>(pn::apply(p,f,q,{}));check(failure.code==pn::ErrorCode::unknown_field&&failure.field=="prior second byte","Inherited byte fabricated");check(q==q0&&p==p0,"Late missing byte published first enqueue");
 f.pbw.field34c=0;f.pw.field586.reset();auto suppressed=value<pn::Output>(pn::apply(p,f,unknown,{}));check(suppressed.effects.count==0&&suppressed.effects.second_byte==0&&suppressed.queue==unknown,"Suppressed rise consumed unknown queue/kind/byte");
 f=f0;f.pw.field200=0x300000;f.pw.field1fc.reset();failure=value<pn::Error>(pn::apply(p,f,q,{}));check(failure.field=="PW1fc"&&failure.phase==pn::Phase::tail&&q==q0,"Late tail failure published earlier requests");
 f=f0;f.pw.field200=0x300000;p.pw.field1f4=0x20000;auto repeats=value<pn::Output>(pn::apply(p,f,occupied,{}));check(repeats.effects.count==3&&repeats.effects.attempts[0].id==54&&repeats.effects.attempts[1].id==54&&repeats.effects.attempts[2].id==55,"Full queue stopped repeated attempts");
 for(std::size_t i=0;i<3;++i)check(repeats.effects.attempts[i].permitted&&repeats.effects.attempts[i].queue&&!repeats.effects.attempts[i].inserted,"Permission/insertion conflated");
 p=local();p.pbw.field30=3;auto denied=value<pn::Mode1Output>(pn::mode1(p,unknown,0xffff0036));check(!denied.attempt.permitted&&!denied.attempt.queue&&denied.attempt.id==54&&denied.queue==unknown,"Denied mode1 consumed queue or lost low16");
 f=f0;f.pbw.field34c.reset();f.pw.field1f8=0x100000;auto remote=value<pn::Output>(pn::apply(p,f,unknown,2));check(remote.effects.count==1&&!remote.effects.attempts[0].permitted&&remote.effects.second_byte==2,"Remote path consumed PBW34c or non1 stackbyte");
 f.pw.field586.reset();failure=value<pn::Error>(pn::apply(p,f,unknown,0));check(failure.field=="PW586","Denied mode manufactured consumed kind");
 p=local();f=f0;f.pbw.field34c.reset();failure=value<pn::Error>(pn::apply(p,f,q,{}));check(failure.field=="PBW34c","Local no-edge omitted real gate");
 f=f0;f.pw.field200=0x300000;auto first_fits=empty();for(std::size_t i=1;i<16;++i)first_fits.pool[i][0]=1;auto seq=value<pn::Output>(pn::apply(p,f,first_fits,{}));check(seq.effects.count==2&&seq.effects.attempts[0].inserted==nq::PoolIndex{0}&&!seq.effects.attempts[1].inserted&&seq.queue.counter==1,"Second attempt used old queue candidate");
 check(f0==fields(),"Borrowed fields changed");
}
struct Reader {
 std::vector<std::uint8_t> bytes;std::size_t at{};
 Reader(const char* path,const char* sha){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"Fixture open");auto n=f.tellg();check(n>=0&&n<=16*1024*1024,"Fixture limit");bytes.resize(std::size_t(n));f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(bytes.data()),n))&&f.peek()==std::char_traits<char>::eof(),"Fixture read");check(picosha2::hash256_hex_string(bytes)==sha,"Fixture hash");}
 U u(){check(at<=bytes.size()&&bytes.size()-at>=4,"Fixture truncated");U x=0;for(unsigned i=0;i<4;++i)x|=U(bytes[at++])<<(8*i);return x;}
 std::vector<std::uint8_t> raw(std::size_t n){check(at<=bytes.size()&&n<=bytes.size()-at,"Fixture truncated");auto a=bytes.begin()+std::ptrdiff_t(at);at+=n;return {a,a+std::ptrdiff_t(n)};}
};
constexpr U base=0x1082da8;
U word(const std::vector<std::uint8_t>& b,std::size_t o){return U(b[o])|(U(b[o+1])<<8)|(U(b[o+2])<<16)|(U(b[o+3])<<24);}
struct Codec {
 std::map<U,std::uint64_t> ids;
 nq::State decode(const std::vector<std::uint8_t>& b){
  nq::State s;s.counter=word(b,8);
  for(std::size_t i=0;i<16;++i){for(std::size_t j=0;j<148;++j)s.pool[i][j]=b[12+i*148+j];
   const auto raw=word(b,2380+4*i);if(raw==0)s.slots[i]=nq::Absent{};
   else if(raw>=base+12&&raw<base+12+16*148&&(raw-base-12)%148==0)s.slots[i]=nq::PoolIndex{std::uint8_t((raw-base-12)/148)};
   else{auto [it,added]=ids.try_emplace(raw,ids.size());(void)added;s.slots[i]=nq::Opaque{it->second};}
  }return s;
 }
 void compare(const nq::State& state,const std::vector<std::uint8_t>& expected){
  const auto decoded=decode(expected);check(state.counter==decoded.counter,"Counter differs ARM");
  for(std::size_t i=0;i<16;++i){check(state.slots[i]==decoded.slots[i],"Slot identity differs ARM");for(std::size_t j=0;j<148;++j)check(state.pool[i][j]==decoded.pool[i][j],"Pool byte differs ARM");}
 }
};
std::optional<nq::PoolIndex> index(U v){if(v==0xffffffff)return {};check(v<16,"Fixture pool index");return nq::PoolIndex{std::uint8_t(v)};}
void oracle(const char* path,const char* sha){
 Reader r(path,sha);check(r.raw(8)==std::vector<std::uint8_t>({'M','H','4','U','N','Q','0','1'}),"Fixture magic");const auto count=r.u();check(count==216,"Fixture count");
 for(U case_index=0;case_index<count;++case_index){
  const auto source_index=r.u(),kind=r.u(),length=r.u();check(source_index<252&&kind<=2&&length<=256,"Fixture case header");const auto namebytes=r.raw(length);const std::string name(namebytes.begin(),namebytes.end());
  try{
   std::array<U,10> values{};for(auto& v:values)v=r.u();const auto prior=r.u(),returned=r.u(),second=r.u();check(prior<=255&&second<=255,"Fixture stackbyte");
   std::array<U,7> args{};for(auto& v:args)v=r.u();const auto before=r.raw(2444),after=r.raw(2444);Codec codec;const auto queue=codec.decode(before);const auto input=queue;
   auto p=local();auto f=fields();p.global.field2b=std::uint8_t(values[0]);p.global.field2f=std::uint8_t(values[1]);p.pbw.field30=std::uint8_t(values[2]);p.pbw.field33=std::uint8_t(values[3]);f.pbw.field34c=std::bit_cast<std::int16_t>(std::uint16_t(values[4]));p.pw.field1f4=values[5];f.pw.field1f8=values[6];f.pw.field1fc=values[7];f.pw.field200=values[8];f.pw.field586=std::uint16_t(values[9]);
   const auto player_before=p;const auto fields_before=f;const auto steps=r.u();check(steps<=(kind==0?3U:1U),"Fixture attempts");
   std::optional<pn::Output> output;std::optional<pn::Mode1Output> mode;std::optional<nq::Output> direct;
   if(kind==0){output=value<pn::Output>(pn::apply(p,f,queue,std::uint8_t(prior)));check(output->effects.count==steps&&output->effects.second_byte==second,"Original requests/local byte differ");codec.compare(output->queue,after);}
   else if(kind==1){mode=value<pn::Mode1Output>(pn::mode1(p,queue,args[2]));codec.compare(mode->queue,after);if(mode->attempt.permitted)check(returned==U(bool(mode->attempt.inserted)),"Original wrapper return differs");}
   else{direct=value<nq::Output>(nq::enqueue(queue,{args[0],args[1],args[2],args[3],{args[4],args[5],args[6]}}));codec.compare(direct->queue,after);check(returned==(direct->inserted?base+12+148*direct->inserted->value:0),"Original queue return differs");}
   auto replay=queue;
   for(U j=0;j<steps;++j){
    const auto id=r.u(),permitted=r.u(),inserted=r.u(),cleanup=r.u(),cleanupslot=r.u(),selected=r.u();const auto intermediate=r.raw(2444);++attempts;cleanups+=cleanup;
    pn::Attempt attempt;
    if(kind==0)attempt=output->effects.attempts[j];else if(kind==1)attempt=mode->attempt;else attempt={std::uint16_t(args[2]),true,direct->effects,direct->inserted};
    check(id<=0xffff&&permitted<=1&&cleanup<=1,"Fixture attempt flags");check(attempt.id==id&&attempt.permitted==bool(permitted)&&attempt.inserted==index(inserted),"Attempt differs original calls/stores");
    if(permitted){check(attempt.queue.has_value(),"Missing reached queue effects");check(attempt.queue->selected==index(selected)&&attempt.queue->cleanup_attempted==bool(cleanup),"Queue selection/cleanup differs original");check(attempt.queue->cleanup_slot==(cleanupslot==0xffffffff?std::optional<std::uint8_t>{}:std::optional<std::uint8_t>{std::uint8_t(cleanupslot)}),"Cleanup slot differs original");}else check(!attempt.queue,"Denied attempt manufactured queue effect");
    if(kind==0){auto next=value<pn::Mode1Output>(pn::mode1(p,replay,id));replay=next.queue;codec.compare(replay,intermediate);}else codec.compare(kind==1?mode->queue:direct->queue,intermediate);
   }
   check(queue==input&&p==player_before&&f==fields_before,"Native call mutated borrowed input");++cases;
  }catch(const std::exception& e){throw std::runtime_error(name+": "+e.what());}
 }
 check(r.at==r.bytes.size(),"Fixture trailing bytes");
}
}
int main(int argc,char** argv){try{check(argc==1||argc==3,"Expected optional fixture and SHA256");boundaries();if(argc==3)oracle(argv[1],argv[2]);std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"cases\":"<<cases<<",\"attempts\":"<<attempts<<",\"cleanups\":"<<cleanups<<"}\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
