#include "spatial_face_query.hpp"
#include "collision_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
namespace sfq=mh4u::spatial_face_query;
namespace cr=mh4u::collision_resource;
using Word=std::uint32_t;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks=0;
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
Bytes read(const std::filesystem::path& path,std::size_t limit){
 std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("Cannot read fixture");
 const auto size=f.tellg();if(size<0||std::uintmax_t(size)>limit)throw std::runtime_error("Fixture size limit");
 Bytes b(static_cast<std::size_t>(size));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()))||f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture changed/read failed");return b;
}
struct Reader {
 const Bytes& bytes;std::size_t at{};
 Word word(){if(bytes.size()-at<4)throw std::runtime_error("Truncated fixture");Word out=0;for(unsigned i=0;i<4;++i)out|=Word(bytes[at++])<<(8*i);return out;}
 template<std::size_t N>std::array<Word,N> words(){std::array<Word,N> out;for(auto& x:out)x=word();return out;}
 void header(std::string_view magic,std::size_t stride){
  check(bytes.size()>=12,"Truncated fixture header");check(std::equal(magic.begin(),magic.end(),bytes.begin()),"Fixture magic");at=8;
  check(word()==1444,"Fixture case count");check(bytes.size()==12+1444*stride,"Fixture exact extent");
 }
};
struct State {
 std::array<Word,13> geometry;Word identity{},flag{};
 bool operator==(const State&)const=default;
};
std::array<Word,13> project(const sfq::Replacement& r){
 std::array<Word,13> out;std::copy(r.position.begin(),r.position.end(),out.begin());std::copy(r.opposed_direction.begin(),r.opposed_direction.end(),out.begin()+4);std::copy(r.plane.begin(),r.plane.end(),out.begin()+8);out[12]=r.t_bits;return out;
}
void run(const Bytes& source,const Bytes& sidecar,const cr::Resource& sbc){
 Reader r{source},s{sidecar};r.header("MH4USF01",204);s.header("MH4USFS1",20);
 check(sbc.groups().size()==1&&sbc.faces().size()==86&&sbc.pairs().size()==86,"Village source domain");
 std::array<std::optional<State>,4> sequences{};std::array<Word,4> next_step{};
 std::size_t real=0,synthetic=0,accepted=0,missed=0,unknown_origin=0,unknown_limit=0,sequence_calls=0,source_words=0;
 for(Word ordinal=0;ordinal<1444;++ordinal){
  const auto meta=r.words<5>();check(meta[0]==ordinal,"Original ordinal");check(meta[1]<=1&&(meta[3]&~7U)==0,"Fixture kind/flags domain");
  const auto normal=r.words<3>();std::array<sfq::Bits3,3>vertices;for(auto& v:vertices)v=r.words<3>();const auto ray=r.words<4>();const auto maximum=r.word();
  const auto before_geometry=r.words<13>(),after_geometry=r.words<13>();const auto expected=r.words<3>();const auto extra=s.words<5>();
  check(extra[0]==ordinal,"Sidecar ordinal");check(extra[2]<=255&&expected[0]<=1&&expected[2]<=255,"Observed result/flag domain");
  sfq::Face face{normal,vertices};
  if(meta[1]==0){
   check(meta[2]<86,"Original source face ID");const auto resolved=sbc.resolve_pair(0,meta[2]);check(std::holds_alternative<cr::ResolvedPair>(resolved),"Source owner resolution");const auto& p=std::get<cr::ResolvedPair>(resolved);
   // This fixed SBC has one pair per face. Prove that mapping before use; do
   // not introduce a generic interpretation of pair IDs as face IDs.
   check(p.first.id==meta[2],"Declared village pair-to-face mapping");face.source_normal=p.first.face->normal_bits;
   for(unsigned i=0;i<3;++i)std::copy_n(p.first.vertices[i]->position_bits.begin(),3,face.vertices[i].begin());
   for(unsigned k=0;k<3;++k){check(face.source_normal[k]==normal[k],"Source normal raw bits");++source_words;}
   for(unsigned v=0;v<3;++v)for(unsigned k=0;k<3;++k){check(face.vertices[v][k]==vertices[v][k],"Source vertex raw bits");++source_words;}++real;
  }else{check(meta[2]==0xffffffffU,"Synthetic geometry never assigned a source face");++synthetic;}
  State current{before_geometry,extra[1],extra[2]};
  if(meta[3]&4){
   check(extra[3]<sequences.size()&&extra[4]==next_step[extra[3]],"Sequence identity/step");
   if(extra[4]){check(sequences[extra[3]].has_value()&&*sequences[extra[3]]==current,"Previous native output equals original next input");current=*sequences[extra[3]];}
   else check(!sequences[extra[3]],"Sequence begins exactly once");
   check(current.geometry[12]==maximum,"Original continuous maximum t");++next_step[extra[3]];++sequence_calls;
  }else check(extra[3]==0xffffffffU&&extra[4]==0xffffffffU,"Independent record has no sequence");
  sfq::Vertical vertical{};vertical.direction_y=ray[3];if(!(meta[3]&1))vertical.origin=sfq::Bits3{ray[0],ray[1],ray[2]};else ++unknown_origin;
  std::optional<Word> limit;if(!(meta[3]&2))limit=(meta[3]&4)?current.geometry[12]:maximum;else ++unknown_limit;
  const auto saved_face=face;const auto saved_vertical=vertical;const auto saved_limit=limit;
  const auto result=sfq::intersect(face,vertical,limit);
  if(const auto* e=std::get_if<sfq::Error>(&result))throw std::runtime_error("Unexpected native error at case "+std::to_string(ordinal)+": "+std::string(e->field));
  const auto& replacement=std::get<std::optional<sfq::Replacement>>(result);check(bool(replacement)==bool(expected[0]),"Original hit/miss");
  if(replacement){current.geometry=project(*replacement);current.identity=meta[4];current.flag=replacement->flag;++accepted;}
  else{check(before_geometry==after_geometry,"Original miss geometry is unchanged");++missed;}
  for(unsigned k=0;k<13;++k)check(current.geometry[k]==after_geometry[k],"Original output geometry word");
  check(current.identity==expected[1]&&current.flag==expected[2],"Original selection/flag projection");
  check(face.source_normal==saved_face.source_normal&&face.vertices==saved_face.vertices&&vertical.origin==saved_vertical.origin&&vertical.direction_y==saved_vertical.direction_y&&limit==saved_limit,"Borrowed inputs unchanged");
  if(meta[3]&4)sequences[extra[3]]=current;
 }
 check(r.at==source.size()&&s.at==sidecar.size(),"Complete fixture consumption");
 check(real==1290&&synthetic==154&&accepted==910&&missed==534&&sequence_calls==9,"Full original corpus coverage");
 check(std::all_of(sequences.begin(),sequences.end(),[](const auto& p){return p.has_value();}),"Four original continuous sequences");
 const auto bytes=sbc.bytes();check(picosha2::hash256_hex_string(reinterpret_cast<const char*>(bytes.data()),reinterpret_cast<const char*>(bytes.data())+bytes.size())==sbc.sha256(),"Immutable SBC source remains unchanged");
 std::cout<<"{\"status\":\"passed\",\"cases\":1444,\"source_cases\":"<<real<<",\"synthetic_cases\":"<<synthetic<<",\"accepted\":"<<accepted<<",\"missed\":"<<missed<<",\"source_words\":"<<source_words<<",\"output_words\":18772,\"sequences\":4,\"sequence_calls\":"<<sequence_calls<<",\"unknown_origin\":"<<unknown_origin<<",\"unknown_limit\":"<<unknown_limit<<",\"checks\":"<<checks<<"}\n";
}
}
int main(int argc,char** argv){
 try{
  if(argc!=7)throw std::runtime_error("Usage: spatial-face-query-tests fixture sha256 sidecar sha256 SBC sha256");
  const auto f=read(argv[1],512U*1024U),s=read(argv[3],64U*1024U);
  check(picosha2::hash256_hex_string(f)==argv[2],"Fixture SHA256");check(picosha2::hash256_hex_string(s)==argv[4],"Sidecar SHA256");
  auto loaded=cr::Resource::load(argv[5],argv[6]);check(std::holds_alternative<cr::Resource>(loaded),"Source SBC owner load");run(f,s,std::get<cr::Resource>(loaded));return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
