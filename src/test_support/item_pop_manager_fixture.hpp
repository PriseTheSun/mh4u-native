#pragma once
// Test-only reversible projection of fixed ARM arenas. Never used by runtime.
#include "item_pop_resources.hpp"
#include "player_world_lookup.hpp"
#include "picosha2.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <vector>
namespace mh4u::test_support::item_pop_manager {
namespace ip=mh4u::item_pop_resources;namespace wl=mh4u::player_world_lookup;
using U=std::uint32_t;using Bytes=std::vector<std::uint8_t>;
inline std::size_t checks{};
inline void need(bool ok,const char* what){++checks;if(!ok)throw std::runtime_error(what);}
enum class BindingProfile {fresh_premise,constructor_observed};
struct Reader{
 std::span<const std::uint8_t> b;std::size_t at{};
 auto take(std::size_t n){need(n<=b.size()-at,"fixture truncated");auto v=b.subspan(at,n);at+=n;return v;}
 U word(){auto v=take(4);return U(v[0])|(U(v[1])<<8)|(U(v[2])<<16)|(U(v[3])<<24);}
 std::string text(){const auto n=word();need(n<=512,"text extent");auto v=take(n);return {v.begin(),v.end()};}
 std::string hash(){const auto v=take(32);return picosha2::bytes_to_hex_string(v.begin(),v.end());}
 void end(){need(at==b.size(),"fixture trailing bytes");}
};
constexpr U M=0x2000000,G=0x2600000,C=0x2500000,LB=0x2400000;
inline const std::vector<std::pair<U,U>> fields=[](){std::vector<std::pair<U,U>> v{{4,1},{8,4},{12,4},{0x19,1},{0x1a,1},{0x1c,4},{0x20,4},{0x24,4},{0x28,4},{0x2c,2},{0x2e,1},{0x2f,1},{0x30,2},{0x32,2}};for(U o=0x34;o<0x54;o+=4)v.emplace_back(o,4);for(U o=0x54;o<0x58;++o)v.emplace_back(o,1);v.emplace_back(0x58,4);v.emplace_back(0x5c,4);for(U o=0x60;o<0x65;++o)v.emplace_back(o,1);for(U o=0x68;o<0x78;o+=4)v.emplace_back(o,4);v.emplace_back(0x78,1);v.emplace_back(0x79,1);for(U o=0x7c;o<0x94;o+=4)v.emplace_back(o,4);return v;}();
inline U read(std::span<const std::uint8_t> b,std::size_t o,U n=4){need(n<=4,"word width");need(o<=b.size()&&n<=b.size()-o,"snapshot read bounds");U v=0;for(U i=0;i<n;++i)v|=U(b[o+i])<<(8*i);return v;}
inline void put(Bytes& b,std::size_t o,std::uint64_t v,U n=4){need(n<=4,"word width");need(o<=b.size()&&n<=b.size()-o,"projection bounds");for(U i=0;i<n;++i)b[o+i]=std::uint8_t(v>>(8*i));}
inline wl::Link decode_link(U pointer){if(pointer==0)return wl::Absent{};need(pointer>=M+0x1c&&pointer<M+0x1c+128*0x94&&(pointer-M-0x1c)%0x94==0,"record link identity");return wl::Index{(pointer-M-0x1c)/0x94};}
inline U encode_link(const wl::Link& l){if(std::holds_alternative<wl::Absent>(l))return 0;need(std::holds_alternative<wl::Index>(l),"unexpected unknown link");const auto i=std::get<wl::Index>(l).value;need(i<128,"native link bounds");return M+0x1c+U(i)*0x94;}
inline wl::Manager decode_manager(std::span<const std::uint8_t> b,const ip::Resource& list,BindingProfile profile=BindingProfile::fresh_premise){wl::Manager m{};m.head=decode_link(read(b,0x4a1c));m.field4a20=read(b,0x4a20);m.field4a24=read(b,0x4a24);
 for(U i=0;i<128;++i){auto s=b.subspan(0x1c+i*0x94,0x94);auto& r=m.records[i];
 // Old builder fixtures explicitly assume construction. New update fixtures
 // execute5ab458 and verify this actual vtable pointer against the source.
 if(profile==BindingProfile::constructor_observed)need(read(s,0)==0xe649d8,"record constructor vtable identity");
 r.update_binding=wl::RecordUpdateBinding::item_pop_5aa4cc;
 need(read(s,0x10)==0&&read(s,0x14)==0,"fresh auxiliary profile");
 r.auxiliary10=wl::AuxiliaryPresence::absent;r.auxiliary14=wl::AuxiliaryPresence::absent;
 r.next=decode_link(read(s,8));r.previous=decode_link(read(s,12));
 #define D8(x) r.field##x=std::uint8_t(read(s,0x##x,1));
 D8(4) D8(19) D8(1a) D8(2e) D8(2f) D8(54) D8(55) D8(56) D8(57) D8(60) D8(61) D8(62) D8(63) D8(64) D8(78) D8(79)
 #undef D8
 r.field2c=std::uint16_t(read(s,0x2c,2));r.field30=std::uint16_t(read(s,0x30,2));r.field32=std::uint16_t(read(s,0x32,2));
 r.field1c_bits=read(s,0x1c);r.field20_bits=read(s,0x20);r.field24=read(s,0x24);r.field4c_bits=read(s,0x4c);r.field50_bits=read(s,0x50);r.field58_bits=read(s,0x58);r.field5c_bits=read(s,0x5c);r.field74=read(s,0x74);r.field88=read(s,0x88);
 for(U k=0;k<3;++k){r.fields34[k]=read(s,0x34+4*k);r.fields40[k]=read(s,0x40+4*k);r.fields68[k]=read(s,0x68+4*k);r.fields7c[k]=read(s,0x7c+4*k);}for(U k=0;k<2;++k)r.fields8c[k]=read(s,0x8c+4*k);
 const U p=read(s,0x28);if(p==0)r.field28=wl::Field28{U(0)};else{need(p>=LB&&p-LB<list.allocation_size(),"IPL pointer provenance");r.field28=wl::Field28{wl::ResourceReference{std::string(list.sha256()),p-LB}};}}
 return m;}
inline Bytes encode_manager(const wl::Manager& m,std::span<const std::uint8_t> before,const ip::Resource& list){Bytes b(before.begin(),before.end());auto atom=[&](std::size_t o,const auto& v,U n=4){need(v.has_value(),"native output unknown");put(b,o,*v,n);};put(b,0x4a1c,encode_link(m.head));atom(0x4a20,m.field4a20);atom(0x4a24,m.field4a24);
 for(U i=0;i<128;++i){const auto o=0x1c+i*0x94;const auto& r=m.records[i];need(r.update_binding==wl::RecordUpdateBinding::item_pop_5aa4cc,"native update binding changed");need(r.auxiliary10==wl::AuxiliaryPresence::absent&&r.auxiliary14==wl::AuxiliaryPresence::absent,"native auxiliary facts changed");put(b,o+8,encode_link(r.next));put(b,o+12,encode_link(r.previous));
 #define E8(x) atom(o+0x##x,r.field##x,1);
 E8(4) E8(19) E8(1a) E8(2e) E8(2f) E8(54) E8(55) E8(56) E8(57) E8(60) E8(61) E8(62) E8(63) E8(64) E8(78) E8(79)
 #undef E8
 atom(o+0x2c,r.field2c,2);atom(o+0x30,r.field30,2);atom(o+0x32,r.field32,2);atom(o+0x1c,r.field1c_bits);atom(o+0x20,r.field20_bits);atom(o+0x24,r.field24);atom(o+0x4c,r.field4c_bits);atom(o+0x50,r.field50_bits);atom(o+0x58,r.field58_bits);atom(o+0x5c,r.field5c_bits);atom(o+0x74,r.field74);atom(o+0x88,r.field88);
 for(U k=0;k<3;++k){atom(o+0x34+4*k,r.fields34[k]);atom(o+0x40+4*k,r.fields40[k]);atom(o+0x68+4*k,r.fields68[k]);atom(o+0x7c+4*k,r.fields7c[k]);}for(U k=0;k<2;++k)atom(o+0x8c+4*k,r.fields8c[k]);need(r.field28.has_value(),"field28 missing");if(const auto* p=std::get_if<U>(&*r.field28))put(b,o+0x28,*p);else{const auto& ref=std::get<wl::ResourceReference>(*r.field28);need(ref.sha256==list.sha256()&&ref.buffer_offset<list.allocation_size(),"native IPL reference identity");put(b,o+0x28,LB+ref.buffer_offset);}}
 return b;}
template<class T>T resource_value(ip::Result<T> value){need(std::holds_alternative<T>(value),"resource fact rejected");return std::get<T>(std::move(value));}
inline std::vector<U> resource_facts(const ip::Resource& r){std::vector<U> out{U(r.kind()),r.allocation_size(),r.group_count()};auto word=[&](U v){out.push_back(v);};auto descriptor=[&](const ip::Descriptor& d){word(d.source_offset);word(d.buffer_offset);word(d.relative_offset);word(d.count);};auto raw=[&](auto b){for(auto v:b)word(std::to_integer<std::uint8_t>(v));};for(auto v:r.header())word(v);for(const auto& v:r.segments()){word(v.buffer_offset);word(v.source_offset);word(v.size);}
 if(r.kind()==ip::Kind::set){for(U i=0;i<r.group_count();++i){const auto g=resource_value(r.group(i));for(const auto& d:g.descriptors)descriptor(d);word(g.secondary_count);word(U(g.primary.size()));for(const auto& p:g.primary){word(p.source_offset);word(p.buffer_offset);raw(p.raw);word(p.member_word);for(auto v:p.position_bits)word(v);word(p.radius_bits);word(p.height_bits);for(auto v:p.words18)word(v);for(auto v:p.bytes1c)word(v);}for(const auto& nested:g.positions){descriptor(nested.descriptor);word(U(nested.records.size()));for(const auto& p:nested.records){word(p.source_offset);word(p.buffer_offset);raw(p.raw);}}for(const auto& nested:g.choices){descriptor(nested.descriptor);word(U(nested.records.size()));for(const auto& p:nested.records){word(p.source_offset);word(p.buffer_offset);raw(p.raw);}}}}
 else for(unsigned t=0;t<2;++t){const auto ds=resource_value(r.members(t));word(U(ds.size()));for(U i=0;i<ds.size();++i){descriptor(ds[i]);const auto m=resource_value(r.member(t,i));need(m.has_value(),"source member unexpectedly absent");descriptor(m->descriptor);word(m->reference.buffer_offset);need(m->reference.sha256==r.sha256(),"source member identity");word(m->source_offset.has_value());word(m->source_offset.value_or(0));}}
 return out;}
struct Resources{std::filesystem::path root;std::map<std::string,ip::Resource> cache;std::map<std::string,std::pair<Bytes,std::vector<U>>> snapshots;
 const ip::Resource& get(const std::string& path,const std::string& sha,U size){const std::filesystem::path p(path);need(!p.is_absolute(),"absolute source path");for(const auto& s:p)need(s!="..","escaping source path");const auto full=root/p;need(std::filesystem::file_size(full)==size,"source length");const auto key=path+sha;auto it=cache.find(key);if(it==cache.end()){auto result=ip::Resource::load(full,sha);need(std::holds_alternative<ip::Resource>(result),"source Resource load rejected");it=cache.emplace(key,std::get<ip::Resource>(std::move(result))).first;const auto& r=it->second;const auto* data=reinterpret_cast<const std::uint8_t*>(r.bytes().data());snapshots.emplace(key,std::make_pair(Bytes(data,data+r.bytes().size()),resource_facts(r)));}return it->second;}
 std::size_t unchanged()const{std::size_t n=0;for(const auto& [key,r]:cache){const auto& prior=snapshots.at(key);need(std::equal(prior.first.begin(),prior.first.end(),reinterpret_cast<const std::uint8_t*>(r.bytes().data())),"Resource raw storage mutated");need(resource_facts(r)==prior.second,"Resource parsed facts mutated");n+=prior.first.size();}return n;}
};
inline std::optional<U> optional(Reader& q){const U present=q.word(),v=q.word();need(present<=1,"optional tag");need(present||v==0,"absent payload");return present?std::optional<U>{v}:std::nullopt;}
} // namespace mh4u::test_support::item_pop_manager
