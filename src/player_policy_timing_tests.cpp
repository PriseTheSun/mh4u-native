#include "player_policy_timing.hpp"
#include "picosha2.h"
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>
namespace pt=mh4u::player_policy_timing;
namespace {
std::size_t checks=0;
void check(bool b,const char* m){++checks;if(!b)throw std::runtime_error(m);}
std::uint32_t word(const std::vector<std::uint8_t>& b,std::size_t o){return b.at(o)|(std::uint32_t(b.at(o+1))<<8)|(std::uint32_t(b.at(o+2))<<16)|(std::uint32_t(b.at(o+3))<<24);}
float value(std::uint32_t x){return std::bit_cast<float>(x);}
}
int main(int argc,char** argv){try{
 mh4u::motion_request::Layer l;mh4u::motion_command::Fields f;
 pt::Context context{l,f,std::nullopt};pt::Query q{6,0,0};
 check(std::get<bool>(pt::query(context,q)),"zero timing");
 l.remaining_interpolation=1;l.previous_frame=3;q={6,0x40000000,0x7fc00000};
 f.fieldsd48[4]=0x7fc00000;f.fieldsd48[5]=0x7fc00000;
 check(std::get<bool>(pt::query(context,q)),"unconsumed fields ignored");
 q.mode=0;check(std::get<pt::Error>(pt::query(context,q)).code==pt::ErrorCode::unknown_delta,"delta unknown");
 context.delta_time_bits=0;q.mode=1;
 check(!std::get<bool>(pt::query(context,q)),"active early return");
 l.remaining_interpolation=value(0x7fc00000);q.mode=6;
 check(std::get<pt::Error>(pt::query(context,q)).code==pt::ErrorCode::nonfinite_operand,"nonfinite consumed");
 l.remaining_interpolation=0;f={};l.previous_frame=value(0x7f7fffff);f.fieldsd48[4]=0x7f7fffff;
 check(std::get<pt::Error>(pt::query(context,q)).code==pt::ErrorCode::numeric_overflow,"arithmetic overflow");
 const auto old=std::fegetround();std::fesetround(FE_DOWNWARD);
 check(std::get<pt::Error>(pt::query(context,q)).code==pt::ErrorCode::floating_environment,"rounding");std::fesetround(old);
 if(argc!=1){
  check(argc==3,"fixture and independent sha required");std::ifstream in(argv[1],std::ios::binary);check(bool(in),"fixture open");
  const std::vector<std::uint8_t>b((std::istreambuf_iterator<char>(in)),{});
  check(picosha2::hash256_hex_string(b)==argv[2],"fixture identity");
  check(b.size()>=12&&std::string_view(reinterpret_cast<const char*>(b.data()),8)=="MH4UPTQ1","fixture magic");
  auto count=word(b,8);check(b.size()==12+std::uint64_t(count)*52,"fixture extent");
  for(std::uint32_t i=0;i<count;++i){const std::size_t p=12+i*52;l={};f={};q.mode=word(b,p);
   f.fieldsd48[6]=word(b,p+4);l.remaining_interpolation=value(word(b,p+8));f.fieldsd48[4]=word(b,p+12);
   l.field58=value(word(b,p+16));l.previous_frame=value(word(b,p+20));f.fieldsd48[5]=word(b,p+24);
   context.delta_time_bits=word(b,p+28);l.frame=value(word(b,p+32));l.speed=value(word(b,p+36));
   q.lower_bits=word(b,p+40);q.upper_bits=word(b,p+44);
   auto result=pt::query(context,q);
   if(!std::holds_alternative<bool>(result)||std::get<bool>(result)!=bool(word(b,p+48))){std::cerr<<"case "<<i<<" mode "<<q.mode<<'\n';throw std::runtime_error("ARM result mismatch");}
   ++checks;
  }
 }
 std::cout<<"policy timing checks="<<checks<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
