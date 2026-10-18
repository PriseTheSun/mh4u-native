#pragma once
// Single test-only projection of retained Object->D identities and raw fields.
#include "player_object_query.hpp"
#include <stdexcept>
namespace mh4u::test_object_fixture {
namespace oq=player_object_query;
using U=std::uint32_t;
inline U pointer_index(U value,U base,U stride,U count){
 if(value<base||(value-base)%stride||(value-base)/stride>=count)throw std::runtime_error("object source pointer identity");
 return(value-base)/stride;
}
// read<T>(region,offset,width), with regions manager/roots/data =0/1/2.
// Omitted bytes remain Unknown. The override address is a logical fixture binding.
template<class Read>oq::Collection project(Read read,U roots,U data,U override_address){
 oq::Collection out;
 for(U i=0;i<32;++i)if(auto v=read.template operator()<U>(0,0x18+4*i,4))out.slots[i]=*v?oq::Slot{oq::ObjectIndex{pointer_index(*v,roots,0x1000,4)}}:oq::Slot{oq::Absent{}};
 for(U i=0;i<4;++i)if(auto v=read.template operator()<U>(1,0x1000*i+0xe28,4))out.objects[i].data=*v?oq::DataLink{oq::DataIndex{pointer_index(*v,data,0x8000,2)}}:oq::DataLink{oq::Absent{}};
 for(U i=0;i<2;++i){auto&d=out.data[i];const U p=i*0x8000;
 #define D(name,off,T,n) d.name=read.template operator()<T>(2,p+off,n)
 D(field140,0x140,std::uint8_t,1);D(field34c,0x34c,std::uint8_t,1);D(field290,0x290,std::uint8_t,1);D(field1c0,0x1c0,std::uint8_t,1);D(field14c,0x14c,std::uint8_t,1);D(field1b4,0x1b4,U,4);D(field349,0x349,std::uint8_t,1);D(field1c4,0x1c4,std::uint16_t,2);D(field78b8_bits,0x78b8,U,4);D(field1bc_bits,0x1bc,U,4);D(field78e5,0x78e5,std::uint8_t,1);D(field278,0x278,std::uint8_t,1);D(field27c,0x27c,std::uint8_t,1);D(field1ca,0x1ca,std::uint8_t,1);D(field1cb,0x1cb,std::uint8_t,1);D(field797e,0x797e,std::uint8_t,1);D(field530,0x530,std::uint8_t,1);
 #undef D
 for(U j=0;j<3;++j)d.position_bits[j]=read.template operator()<U>(2,p+0x198+4*j,4);
 if(auto v=read.template operator()<U>(2,p+0x580,4)){
  if(*v!=0&&*v!=override_address)throw std::runtime_error("override source pointer binding");
  d.field580=*v?oq::OverrideLink{oq::OverrideIndex{0}}:oq::OverrideLink{oq::Absent{}};
 }
 }
 return out;
}
}
