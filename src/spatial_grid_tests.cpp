#include "spatial_grid.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace {
namespace sg=mh4u::spatial_grid;
namespace cr=mh4u::collision_resource;
using Word=std::uint32_t;
using Bytes=std::vector<std::uint8_t>;
std::size_t checks{},boundary_checks{};
void check(bool ok,std::string_view why){++checks;if(!ok)throw std::runtime_error(std::string(why));}
Word bits(float v){return std::bit_cast<Word>(v);}
Word word(const Bytes& b,std::size_t p,unsigned n=4){check(p<=b.size()&&n<=b.size()-p,"Fixture extent");Word v=0;for(unsigned i=0;i<n;++i)v|=Word(b[p+i])<<(8*i);return v;}
void put(Bytes& b,std::size_t p,Word v,unsigned n=4){check(p<=b.size()&&n<=b.size()-p,"Test source extent");for(unsigned i=0;i<n;++i)b[p+i]=std::uint8_t(v>>(8*i));}
struct Reader {
 const Bytes& b;std::size_t p{};
 Word u32(){auto x=word(b,p);p+=4;return x;}
 std::uint16_t u16(){auto x=word(b,p,2);p+=2;return std::uint16_t(x);}
 Bytes take(std::size_t n){check(p<=b.size()&&n<=b.size()-p,"Truncated fixture");Bytes out(b.begin()+std::ptrdiff_t(p),b.begin()+std::ptrdiff_t(p+n));p+=n;return out;}
 template<std::size_t N>std::array<Word,N> words(){std::array<Word,N> a;for(auto& v:a)v=u32();return a;}
};
Bytes read(const char* path,std::size_t bound){std::ifstream f(path,std::ios::binary|std::ios::ate);check(bool(f),"File open");const auto n=f.tellg();check(n>=0&&std::uint64_t(n)<=bound,"File allocation bound");f.seekg(0);Bytes b(static_cast<std::size_t>(n));f.read(reinterpret_cast<char*>(b.data()),n);check(bool(f)&&f.peek()==std::ifstream::traits_type::eof(),"File size changed");return b;}
cr::Resource resource(const Bytes& b){const std::string s(b.begin(),b.end());const auto r=cr::Resource::decode(s,picosha2::hash256_hex_string(s));check(std::holds_alternative<cr::Resource>(r),"Synthetic source accepted by canonical parser");return std::get<cr::Resource>(r);}
sg::Prepared prepare(const cr::Resource& r){auto p=sg::Prepared::prepare(r,0);if(const auto* e=std::get_if<sg::Error>(&p))throw std::runtime_error("Preparation failed: "+std::string(e->field));return std::get<sg::Prepared>(std::move(p));}
Bytes empty_grid(const cr::Resource& original,const std::array<Word,8>& box,Word nx,Word nz){
 check(nx>0&&nz>0&&nx<=32767&&nz<=32767&&std::uint64_t(nx)*nz<=32767,"Synthetic grid dimensions");
 const auto g=std::get<cr::Grid>(original.grid(0));auto bytes=original.bytes();
 Bytes out(g.cells_source.offset);std::transform(bytes.begin(),bytes.begin()+std::ptrdiff_t(out.size()),out.begin(),[](auto b){return std::to_integer<std::uint8_t>(b);});
 for(unsigned i=0;i<8;++i)put(out,g.source_offset+8+i*4,box[i]);put(out,g.source_offset+40,nx,2);put(out,g.source_offset+42,nz,2);
 out.resize(out.size()+std::size_t(nx)*nz*8+4); // Explicit empty cells and index count0.
 const auto start=g.bounds_source.offset,end=start+g.bounds_source.count*g.bounds_source.stride;
 for(std::size_t i=start;i<end;++i)out.push_back(std::to_integer<std::uint8_t>(bytes[i]));
 return out; // Global tail is unconsumed by this group-grid profile.
}
sg::Setup setup(Reader& r){sg::Setup s;s.bbox_min=r.words<4>();s.bbox_max=r.words<4>();s.cell_size=r.words<4>();s.reciprocal=r.words<4>();s.origin=r.words<4>();s.nx=r.u16();s.nz=r.u16();return s;}
void boundary(const cr::Resource& original){
 const auto start=checks;const auto box=std::array<Word,8>{bits(0),0x7fc12345,bits(0),0xdeadbeef,bits(10),0xff800000,bits(12),0xcafecafe};
 auto raw=empty_grid(original,box,5,6);auto source=resource(raw);auto grid=prepare(source);
 check(grid.setup().bbox_min[1]==0x7fc12345&&grid.setup().bbox_max[1]==0xff800000&&grid.setup().origin[1]==0x7fc12345,"Unused Y bits preserved");
 check(grid.setup().bbox_min[3]==0&&grid.setup().bbox_max[3]==0&&grid.setup().origin[3]==0&&grid.setup().cell_size[1]==0&&grid.setup().reciprocal[1]==0,"Source zero lanes");
 auto expect_error=[](const auto& result,sg::ErrorCode code,std::string_view field){const auto* e=std::get_if<sg::Error>(&result);check(e&&e->code==code&&e->field==field,"Typed grid error");};
 expect_error(grid.lookup({{},bits(1)}),sg::ErrorCode::unknown_operand,"position X");
 expect_error(grid.lookup({bits(1),{}}),sg::ErrorCode::unknown_operand,"position Z");
 expect_error(grid.lookup({0x7fc12345,bits(1)}),sg::ErrorCode::nonfinite_operand,"position X");
 expect_error(grid.lookup({bits(1),0x7f800000}),sg::ErrorCode::nonfinite_operand,"position Z");
 auto miss=grid.lookup({bits(-1),{}});check(std::holds_alternative<std::optional<sg::Cell>>(miss)&&!std::get<std::optional<sg::Cell>>(miss),"X miss leaves Z unknown unconsumed");
 miss=grid.lookup({bits(11),0x7fc12345});check(!std::get<std::optional<sg::Cell>>(miss),"X upper miss ignores NaN Z");
 auto missing_group=sg::Prepared::prepare(source,99);expect_error(missing_group,sg::ErrorCode::resource,"GRCO");check(std::get<cr::Error>(std::get<sg::Error>(missing_group).cause).code==cr::ErrorCode::index_out_of_range,"Resource cause preserved");
 const auto off=std::get<cr::Grid>(source.grid(0)).source_offset;
 for(auto mutation:std::array<std::pair<unsigned,std::uint8_t>,2>{{{44,2},{45,0}}}){auto changed=raw;changed[off+mutation.first]=mutation.second;auto r=resource(changed);expect_error(sg::Prepared::prepare(r,0),sg::ErrorCode::resource,"GRCO");}
 for(auto v:std::array<Word,3>{0x7fc12345,0x7f800000,0xff800000}){auto changed=raw;put(changed,off+8,v);expect_error(sg::Prepared::prepare(resource(changed),0),sg::ErrorCode::nonfinite_operand,"setup extent X");}
 {auto changed=raw;put(changed,off+24,bits(0));expect_error(sg::Prepared::prepare(resource(changed),0),sg::ErrorCode::numeric_overflow,"setup reciprocal X");}
 {auto changed=raw;put(changed,off+8,0xff7fffff);put(changed,off+24,0x7f7fffff);expect_error(sg::Prepared::prepare(resource(changed),0),sg::ErrorCode::numeric_overflow,"setup extent X");}
 {auto changed=raw;put(changed,off+24,1);expect_error(sg::Prepared::prepare(resource(changed),0),sg::ErrorCode::numeric_overflow,"setup reciprocal X");}
 const auto preserved=grid.setup();const auto rounding=std::fegetround();check(rounding==FE_TONEAREST,"Default rounding");
 struct Restore {int mode;~Restore(){std::fesetround(mode);}} restore{rounding};
 check(std::fesetround(FE_DOWNWARD)==0,"Change rounding for rejection");
 expect_error(sg::Prepared::prepare(source,0),sg::ErrorCode::floating_environment,"setup extent Z");
 expect_error(grid.lookup({bits(1),bits(1)}),sg::ErrorCode::floating_environment,"cell delta X");
 check(!std::get<std::optional<sg::Cell>>(grid.lookup({bits(-1),{}})),"Outside bit comparison ignores unused arithmetic environment");
 check(std::fesetround(rounding)==0,"Restore rounding");check(grid.setup()==preserved,"Error preserves prepared setup");
 auto lifetime=[&]{return prepare(resource(raw));}();check(std::holds_alternative<std::optional<sg::Cell>>(lifetime.lookup({bits(1),bits(1)})),"Prepared retains source lifetime");
 const auto cell=std::get<std::optional<sg::Cell>>(grid.lookup({bits(1),bits(1)}));check(cell&&cell->source==&grid.source_grid().cells[0]&&cell->pair_ids.data()==grid.source_grid().pair_ids.data(),"Borrowed cell/candidates come from unique source owner");
 check(std::equal(source.bytes().begin(),source.bytes().end(),reinterpret_cast<const std::byte*>(raw.data())),"Source unchanged after errors and queries");boundary_checks=checks-start;
}
void oracle(const Bytes& bytes,const cr::Resource& original){
 Reader r{bytes};check(r.take(8)==Bytes({'M','H','4','U','S','G','0','1'}),"Fixture schema");const auto count=r.u32();check(count==17,"Fixture profile bound");const auto hash=r.take(32);std::string hex;picosha2::bytes_to_hex_string(hash.begin(),hash.end(),hex);check(hex==original.sha256(),"Fixture source digest");
 std::size_t queries=0,selected=0,ids=0,setup_words=0;
 for(unsigned i=0;i<count;++i){const auto n=r.u32();check(n<=1024*1024,"Fixture record bound");const auto payload=r.take(n);Reader p{payload};const auto name_size=p.u32();check(name_size>0&&name_size<=64,"Fixture name bound");p.take(name_size);const auto real=p.u32();check(real<=1,"Fixture real flag");p.u32();const auto nx=p.u32(),nz=p.u32();const auto box=p.words<8>();const auto expected=setup(p);
  check(expected.nx==nx&&expected.nz==nz,"Fixture source dimensions");const auto source=real?original:resource(empty_grid(original,box,nx,nz));const auto prepared=prepare(source);const auto& actual=prepared.setup();
  const auto check_words=[](const auto& a,const auto& b){for(std::size_t j=0;j<a.size();++j)check(a[j]==b[j],"Original setup word");};
  check_words(actual.bbox_min,expected.bbox_min);check_words(actual.bbox_max,expected.bbox_max);check_words(actual.cell_size,expected.cell_size);check_words(actual.reciprocal,expected.reciprocal);check_words(actual.origin,expected.origin);check(actual.nx==expected.nx&&actual.nz==expected.nz,"Original setup dimensions");setup_words+=20;
  const auto nc=p.u32();check(nc>0&&nc<=1024,"Fixture query bound");const auto setup_before=actual;const auto source_before=std::string(reinterpret_cast<const char*>(source.bytes().data()),source.bytes().size());
  for(unsigned q=0;q<nc;++q){p.u32();const auto position=p.words<3>();const auto index=p.u32(),coordinates=p.u32(),x=p.u32(),z=p.u32(),np=p.u32();check(coordinates<=1&&np<=65535,"Fixture candidate bound");std::vector<std::uint16_t> pairs;for(unsigned j=0;j<np;++j)pairs.push_back(p.u16());
   const auto result=prepared.lookup({position[0],position[2]});if(const auto* e=std::get_if<sg::Error>(&result))throw std::runtime_error("Profile "+std::to_string(i)+" query "+std::to_string(q)+": "+std::string(e->field));
   const auto& cell=std::get<std::optional<sg::Cell>>(result);check(cell.has_value()==(index!=0xffffffffU),"Original selected/miss cell");
   if(cell){check(coordinates==1&&Word(cell->x)==x&&Word(cell->z)==z&&cell->index==index,"Original cell coordinates/address");check(cell->source_sha256==source.sha256()&&cell->group==0,"Cell content/group identity");check(cell->source==&prepared.source_grid().cells[index],"Borrowed cell identity");check(cell->pair_ids.size()==pairs.size()&&std::equal(pairs.begin(),pairs.end(),cell->pair_ids.begin()),"Original ordered candidates");++selected;ids+=pairs.size();}
   else check(pairs.empty(),"Original miss has no candidates");check(prepared.setup()==setup_before,"Lookup preserves setup");++queries;
  }
  check(source_before==std::string(reinterpret_cast<const char*>(source.bytes().data()),source.bytes().size()),"Lookup source immutable");check(p.p==payload.size(),"Fixture record exhausted");
 }
 check(r.p==bytes.size(),"Fixture fully consumed");check(queries==565&&selected==373&&ids==2290&&setup_words==340,"Original observed coverage");boundary(original);
 std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<",\"profiles\":"<<count<<",\"queries\":"<<queries<<",\"selected_cells\":"<<selected<<",\"ordered_pair_ids\":"<<ids<<",\"setup_words\":"<<setup_words<<",\"boundary_checks\":"<<boundary_checks<<"}\n";
}
}
int main(int argc,char** argv){try{if(argc!=5)throw std::runtime_error("Usage: spatial-grid-tests source.sbc sha256 oracle sha256");const auto source=cr::Resource::load(argv[1],argv[2]);check(std::holds_alternative<cr::Resource>(source),"Source resource/hash");const auto bytes=read(argv[3],16*1024*1024);check(picosha2::hash256_hex_string(bytes)==argv[4],"Fixture SHA256 mismatch");oracle(bytes,std::get<cr::Resource>(source));return 0;}catch(const std::exception& e){std::cerr<<"spatial-grid-tests: "<<e.what()<<'\n';return 1;}}
