// Original observations only. No host geometry calculation produces expected data.
#include "collision_resource.hpp"
#include "spatial_query.hpp"
#include "picosha2.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
namespace cr=mh4u::collision_resource;
namespace sq=mh4u::spatial_query;
namespace sf=mh4u::spatial_face_query;
using Word=std::uint32_t;using Bytes=std::vector<std::uint8_t>;
std::size_t checks{};
void check(bool value,const char* why){++checks;if(!value)throw std::runtime_error(why);}
constexpr std::string_view SBC_SHA="3a408a050c5fe82eb6faceff28be897d407a79cb79e1474789c4200505f77c07";
Bytes read(const char* path,std::size_t limit){
 std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Cannot open fixture");const auto size=in.tellg();
 if(size<0||std::uintmax_t(size)>limit)throw std::runtime_error("Fixture size limit");Bytes b(static_cast<std::size_t>(size));in.seekg(0);
 if(!in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()))||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("Fixture read changed/failed");return b;
}
std::string hex(const Bytes& b){constexpr char digits[]="0123456789abcdef";std::string s;s.reserve(2*b.size());for(auto x:b){s+=digits[x>>4];s+=digits[x&15];}return s;}
struct Reader {
 const Bytes& b;std::size_t p{},end;
 explicit Reader(const Bytes& a):b(a),end(a.size()){}
 Reader(const Bytes& a,std::size_t begin,std::size_t limit):b(a),p(begin),end(limit){if(begin>limit||limit>a.size())throw std::runtime_error("Fixture record extent");}
 std::size_t left()const{return end-p;}
 Bytes bytes(std::size_t n){if(n>left())throw std::runtime_error("Truncated fixture");Bytes out(b.begin()+p,b.begin()+p+n);p+=n;return out;}
 Word word(){if(left()<4)throw std::runtime_error("Truncated fixture");Word x=0;for(unsigned i=0;i<4;++i)x|=Word(b[p++])<<(8*i);return x;}
 template<std::size_t N>std::array<Word,N> words(){std::array<Word,N> a;for(auto& x:a)x=word();return a;}
 std::string text(std::size_t max){const auto n=word();check(n>0&&n<=max,"Fixture text bound");const auto a=bytes(n);check(std::all_of(a.begin(),a.end(),[](auto x){return x>=32&&x<127;}),"Fixture ASCII text");return {a.begin(),a.end()};}
 Reader record(){const auto n=word();check(n>=64&&n<=4096&&n<=left(),"Fixture record length");Reader out(b,p,p+n);p+=n;return out;}
 void magic(std::string_view s){const auto a=bytes(8);check(std::string(a.begin(),a.end())==s,"Fixture schema");}
 void complete()const{check(p==end,"Fixture exact consumption");}
};
Word word_at(const Bytes& b,std::size_t at){if(at>b.size()||b.size()-at<4)throw std::runtime_error("Carrier word extent");Word x=0;for(unsigned i=0;i<4;++i)x|=Word(b[at+i])<<(8*i);return x;}
struct Mutation {Word kind,address,size,value;};
struct Case {
 Word ordinal{},seed{},mask{},raw_result{},height{},status{};bool wrapper{},extension{},wrapper_carrier{};
 std::string name;std::array<Word,4> position{};
 Bytes observed,known,written,tainted,grid;std::vector<Word> pairs,faces;std::vector<Mutation> mutations;
 Word classification()const{return wrapper&&!wrapper_carrier?status:raw_result;}
};
struct Fixture {std::map<std::string,Word> context;std::vector<Case> cases;};
void carrier(Reader& r,Case& c){
 c.observed=r.bytes(160);c.known=r.bytes(160);c.written=r.bytes(160);c.tainted=r.bytes(160);
 unsigned nk=0,nw=0,nt=0;
 for(std::size_t i=0;i<160;++i){check(c.known[i]<=1&&c.written[i]<=1&&c.tainted[i]<=1,"Fixture mask domain");
  check(!c.known[i]||c.written[i],"Known carrier lane was written");check(!c.tainted[i]||(!c.known[i]&&c.written[i]),"Tainted lane remains unknown");
  check(!c.tainted[i]||((i>=0x20&&i<0x32)||(i>=0x44&&i<0x48)),"Original opaque-copy range");nk+=c.known[i];nw+=c.written[i];nt+=c.tainted[i];
 }
 const bool direct_hit=!c.wrapper&&c.classification()!=0;check(nk==(direct_hit?119:51)&&nw==(direct_hit?141:51)&&nt==(direct_hit?22:0),"Original carrier initialization coverage");
}
void candidates(Reader& r,Case& c,Word np,Word nf){
 check(np<=86&&nf<=86&&4ULL*(np+nf)<=r.left(),"Fixture candidate bounds");
 for(Word i=0;i<np;++i){const auto x=r.word();check(x<86,"Source local pair bound");c.pairs.push_back(x);}for(Word i=0;i<nf;++i)c.faces.push_back(r.word());
}
Fixture parse_primary(const Bytes& b){
 Reader r(b);r.magic("MH4USQ01");check(r.word()==223,"Fixture query count");check(r.word()==7960,"Fixture source byte count");check(hex(r.bytes(32))==SBC_SHA,"Fixture source SHA");
 Fixture out;const auto n=r.word();check(n==35,"Fixture context count");
 for(Word i=0;i<n;++i){auto name=r.text(64);const auto value=r.word();check(out.context.emplace(std::move(name),value).second,"Duplicate context fact");}
 const std::map<std::string,Word> expected={
 {"manager.vtable_profile",0xe5b82c},{"manager.word38",0x02000000},{"manager.word3c",0},{"manager.word40",0},{"manager.word54",0},{"manager.word58",0},{"manager.word7c",1},{"container.group",3},{"container.object_count",1},{"resource.group",0},{"object.byte5",1},{"object.word8",0},{"object.word10",1},{"object.word14",8},{"object.word1c",0},{"object.byte33",0},{"object.resource_present",1},{"object.group_count",1},{"status.bytec",1},{"status.byte1f",0},{"status.byte20",0},{"status.pointer24",0},{"status.pointer30",0},{"direct.filter_exclusion",0},{"filter.type",1},{"filter.area_mask",8},{"wrapper.flags",2},{"wrapper.previous_present",0},{"wrapper.attribute_output_present",0},{"wrapper.group",3},{"global.word104df34",0x13579bdf},{"source.instance_address",0x2701600},{"source.resource_address",0x2700000},{"source.group_address",0x2700400},{"source.sbc_address",0x2600000}};
 check(out.context==expected,"Pinned producer context facts");
 for(Word i=0;i<223;++i){auto sub=r.record();const auto a=sub.words<15>();Case c;c.ordinal=a[0];c.wrapper=a[1];c.seed=a[2];c.mask=a[3];std::copy_n(a.begin()+4,4,c.position.begin());c.raw_result=a[8];c.height=a[9];c.status=a[10];
  check(a[0]==i&&a[1]<=1&&(a[2]==0||a[2]==165||a[2]==255),"Fixture case identity");check(c.position[3]==0xbf800000U,"Original downward direction");check(c.classification()==0||c.classification()==129,"Primary classification domain");
  carrier(sub,c);c.grid=sub.bytes(9);candidates(sub,c,a[11],a[12]);sub.complete();out.cases.push_back(std::move(c));
 }r.complete();return out;
}
std::vector<Case> parse_extension(const Bytes& b){
 Reader r(b);r.magic("MH4USQE1");check(r.word()==46,"Extension query count");check(hex(r.bytes(32))==SBC_SHA,"Extension source SHA");std::vector<Case> out;
 for(Word i=0;i<46;++i){auto sub=r.record();Case c;c.extension=true;c.ordinal=sub.word();c.name=sub.text(64);c.seed=165;c.mask=0x3fffffff;check(c.ordinal==i,"Extension ordinal");const auto nm=sub.word();check(nm<=8,"Context mutation bound");
  for(Word j=0;j<nm;++j){const auto a=sub.words<4>();check(a[0]<=2&&((a[0]==0&&a[2]==4)||(a[0]==1&&a[2]==1&&a[3]<=255)||(a[0]==2&&a[2]>0&&a[2]<=4&&a[3]==0)),"Context mutation domain");c.mutations.push_back({a[0],a[1],a[2],a[3]});}
  c.position=sub.words<4>();c.raw_result=sub.word();const auto np=sub.word(),nf=sub.word();check(c.position[3]==0xbf800000U,"Extension downward direction");check(c.raw_result==0||c.raw_result==1||c.raw_result==2||c.raw_result==4||c.raw_result==129||c.raw_result==258||c.raw_result==516,"Extension classification domain");
  carrier(sub,c);candidates(sub,c,np,nf);sub.complete();out.push_back(std::move(c));
 }r.complete();return out;
}
struct GridExpected {bool updated{};Word counter{},word74{},depth{};std::array<Word,86>marks{};};
std::vector<GridExpected> parse_grid(const Bytes& bytes){Reader r(bytes);r.magic("MH4USQG1");check(r.word()==269,"Grid sidecar count");check(hex(r.bytes(32))==SBC_SHA,"Grid sidecar source SHA");std::vector<GridExpected>out;
 for(Word i=0;i<269;++i){check(r.word()==(i<223?0U:1U)&&r.word()==(i<223?i:i-223),"Grid sidecar identity/order");GridExpected g;g.counter=r.word();g.updated=bool(g.counter);g.word74=r.word();g.depth=r.word();check(g.counter<=1&&g.word74==0&&g.depth==0&&r.word()==86,"Original grid state domain");g.marks=r.words<86>();for(auto x:g.marks)check(x<=1,"Original stamp domain");out.push_back(g);}r.complete();return out;
}
struct Lifecycle {Case c;Word family{},sequence{},step{},flags{},previous_bits{};bool previous{},duplicate{},drop_grid{};std::array<Word,4> query_ray{};GridExpected before,after;std::vector<std::array<Word,3>>tokens;};
std::vector<Lifecycle> parse_lifecycle(const Bytes& bytes){Reader r(bytes);r.magic("MH4USQL1");check(r.word()==43,"Lifecycle count");check(hex(r.bytes(32))==SBC_SHA,"Lifecycle source SHA");std::vector<Lifecycle>out;
 for(Word i=0;i<43;++i){auto sub=r.record();Lifecycle l;l.family=sub.word();l.c.ordinal=sub.word();l.sequence=sub.word();l.step=sub.word();l.c.name=sub.text(64);check(l.family==(i<9?0U:1U)&&l.c.ordinal==(i<9?i:i-9)&&l.sequence<34&&l.step<2,"Lifecycle identity/order");
  const auto a=sub.words<18>();check(a[0]<=1&&a[2]<=1&&a[4]<=1&&a[5]<=1,"Lifecycle boolean domain");l.c.wrapper=a[0];l.c.wrapper_carrier=l.c.wrapper;l.flags=a[1];l.previous=a[2];l.previous_bits=a[3];l.duplicate=a[4];l.drop_grid=a[5];l.c.mask=a[6];std::copy_n(a.begin()+7,4,l.c.position.begin());std::copy_n(a.begin()+11,4,l.query_ray.begin());l.c.raw_result=a[15];l.c.height=a[16];l.c.status=a[17];check(l.c.classification()==0||l.c.classification()==129,"Lifecycle classification");
  auto gs=[&](GridExpected& g){g.counter=sub.word();g.word74=sub.word();g.depth=sub.word();check(g.word74==0&&g.depth==0,"Lifecycle original grid noncounter fields");};gs(l.before);gs(l.after);l.after.updated=l.before.counter!=l.after.counter;l.before.marks=sub.words<86>();l.after.marks=sub.words<86>();sub.bytes(160);l.c.observed=sub.bytes(160);const auto np=sub.word(),nf=sub.word();check(np<=172&&nf<=172,"Lifecycle visits bound");for(Word j=0;j<np;++j){const auto v=sub.word();check(v<86,"Lifecycle source pair bound");l.c.pairs.push_back(v);}l.c.faces.resize(nf);for(Word j=0;j<np;++j)l.tokens.push_back(sub.words<3>());sub.complete();out.push_back(std::move(l));
 }r.complete();return out;
}
struct Inputs {sq::Scene scene;sq::Filter filter;sf::Vertical ray;};
Word context(const Fixture& f,std::string_view name,Word expected){const auto it=f.context.find(std::string(name));check(it!=f.context.end()&&it->second==expected,"Explicit source context mismatch");return it->second;}
Inputs inputs(const Fixture& f,const Case& c,const cr::Resource& source){
 Inputs a;auto& scene=a.scene;auto& filter=a.filter;const auto& group=source.groups()[0];
 // All empty collections below were produced by complete original constructors,
 // not inferred from missing pointers or populated with fabricated zero state.
 for(auto& direct:scene.direct)direct=std::vector<sq::Node>{};
 for(auto& family:scene.other_empty)for(auto& empty:family)empty=true;
 scene.extra_c8_enabled=true;for(auto& family:scene.extra_c8_empty)for(auto& empty:family)empty=true;scene.extra_c9_enabled=true;scene.extra_c9_empty=true;scene.extra_tail_count=0;scene.status_selector_a=context(f,"manager.word58",0);scene.status_selector_b=context(f,"manager.word54",0);
 scene.classification_masks={context(f,"manager.word38",0x02000000),context(f,"manager.word3c",0),context(f,"manager.word40",0)};
 sq::Instance instance;instance.enabled_5=static_cast<std::uint8_t>(context(f,"object.byte5",1));instance.object_mask=context(f,"object.word10",1);instance.area_mask=context(f,"object.word14",8);instance.exclusion_tag=context(f,"object.word1c",0);
 instance.resource_present=bool(context(f,"object.resource_present",1));instance.resource=source;instance.grid_present=true;instance.tree_present=false;instance.status_count=1;instance.grid_state=0;instance.bypass_pair_box_33=0;scene.grid_states.push_back({source,0,0,0,std::vector<std::optional<Word>>(86,0)});
 instance.status.enabled_c=static_cast<std::uint8_t>(context(f,"status.bytec",1));instance.status.disabled_1f=static_cast<std::uint8_t>(context(f,"status.byte1f",0));instance.status.field20=static_cast<std::uint8_t>(context(f,"status.byte20",0));instance.status.pointer24_present=bool(context(f,"status.pointer24",0));instance.status.pointer30_present=bool(context(f,"status.pointer30",0));
 scene.instances.push_back(instance);scene.direct[context(f,"container.group",3)]=std::vector<sq::Node>{{0,group.bbox_min[0],group.bbox_max[2]}};
 filter.object_mask=1;filter.area_mask=context(f,"filter.area_mask",8);filter.attribute_mask=c.mask;filter.excluded_tag=context(f,"direct.filter_exclusion",0);filter.flags=15;
 filter.exclusion_mode=0;filter.excluded_object={false,std::nullopt};filter.excluded_status={false,std::nullopt,std::nullopt};filter.extra_c8=true;filter.extra_c9=true;filter.face_callback_present=false;filter.post_callback_present=false;
 a.ray.origin=sf::Bits3{c.position[0],c.position[1],c.position[2]};a.ray.direction_y=c.position[3];
 auto& inst=scene.instances[0];auto& node=(*scene.direct[3])[0];
 for(const auto& m:c.mutations){
  // Exact source-address projection of explicit original setup mutations.
  // These addresses are fixture facts only; the runtime receives logical IDs.
  if(m.kind==2){if(m.address==0x2701654&&m.size==4){inst.resource_present.reset();inst.resource.reset();}
   else if(m.address==0x2400034&&m.size==4)scene.normal_thresholds[2].reset();
   else throw std::runtime_error("Unmapped unknown source context");continue;}
  const auto v=m.value;
  switch(m.address){
   case 0x2500104:filter.object_mask=v;break;case 0x2500108:filter.area_mask=v;break;case 0x250010c:filter.attribute_mask=v;break;case 0x2500110:filter.excluded_tag=v;break;case 0x2500114:filter.flags=v;break;case 0x2500140:filter.exclusion_mode=v;break;
   case 0x2500144:check(v==0x2701600||v==0x2701604,"Object exclusion source pointer");filter.excluded_object={true,v==0x2701600?0U:1U};break;
   case 0x2500148:check(v==0x2701500||v==0x2701540,"Status exclusion source pointer");filter.excluded_status={true,0,v==0x2701500?0U:1U};break;
   case 0x2701605:inst.enabled_5=static_cast<std::uint8_t>(v);break;case 0x2701614:inst.area_mask=v;break;case 0x270161c:inst.exclusion_tag=v;break;
   case 0x2701654:check(v==0,"Only observed resource null mutation");inst.resource_present=false;inst.resource.reset();break;
   case 0x270150c:inst.status.enabled_c=static_cast<std::uint8_t>(v);break;case 0x270151f:inst.status.disabled_1f=static_cast<std::uint8_t>(v);break;
   case 0x2700424:check(v==0,"Only observed grid null mutation");inst.grid_present=false;break;
   case 0x2701910:node.minimum_x=v;break;case 0x2701928:node.maximum_z=v;break;
   case 0x2701920:case 0x2701918:break; // Original consumed gate never reads these bounds.
   case 0x2400038:scene.classification_masks[0]=v;break;case 0x240003c:scene.classification_masks[1]=v;break;case 0x2400040:scene.classification_masks[2]=v;break;
   case 0x240002c:scene.normal_thresholds[0]=v;break;case 0x2400030:scene.normal_thresholds[1]=v;break;
   default:throw std::runtime_error("Unmapped source context mutation");
  }
 }
 return a;
}
// Snapshot all borrowed semantic fields independently. The Resource itself is
// immutable/shared; parsed geometry and raw bytes are also reread separately.
template<class T>void append_optional(std::vector<Word>& b,const std::optional<T>& v){b.push_back(bool(v));if(v)b.push_back(static_cast<Word>(*v));}
std::vector<Word> fingerprint(const Inputs& a){
 std::vector<Word>b;const auto& s=a.scene;const auto& f=a.filter;
 b.push_back(static_cast<Word>(s.instances.size()));
 for(const auto& x:s.instances){append_optional(b,x.enabled_5);append_optional(b,x.bypass_pair_box_33);append_optional(b,x.status_count);append_optional(b,x.grid_state);append_optional(b,x.tree_present);append_optional(b,x.object_mask);append_optional(b,x.area_mask);append_optional(b,x.exclusion_tag);append_optional(b,x.resource_present);append_optional(b,x.grid_present);b.push_back(bool(x.resource));if(x.resource)for(char c:x.resource->sha256())b.push_back(static_cast<unsigned char>(c));append_optional(b,x.status.enabled_c);append_optional(b,x.status.disabled_1f);append_optional(b,x.status.field20);append_optional(b,x.status.pointer24_present);append_optional(b,x.status.pointer30_present);}
 b.push_back(static_cast<Word>(s.grid_states.size()));for(const auto& g:s.grid_states){for(char c:g.resource.sha256())b.push_back(static_cast<unsigned char>(c));b.push_back(g.group);append_optional(b,g.counter);append_optional(b,g.depth);b.push_back(static_cast<Word>(g.pair_marks.size()));for(const auto& m:g.pair_marks)append_optional(b,m);}
 for(const auto& d:s.direct){b.push_back(bool(d));if(d){b.push_back(static_cast<Word>(d->size()));for(const auto& n:*d){append_optional(b,n.instance);append_optional(b,n.minimum_x);append_optional(b,n.maximum_z);}}}
 for(const auto& family:s.other_empty)for(const auto& x:family)append_optional(b,x);append_optional(b,s.extra_c8_enabled);append_optional(b,s.extra_tail_count);append_optional(b,s.status_selector_a);append_optional(b,s.status_selector_b);for(const auto& family:s.extra_c8_empty)for(const auto& empty:family)append_optional(b,empty);append_optional(b,s.extra_c9_enabled);append_optional(b,s.extra_c9_empty);for(const auto& x:s.classification_masks)append_optional(b,x);for(const auto& x:s.normal_thresholds)append_optional(b,x);
 append_optional(b,f.object_mask);append_optional(b,f.area_mask);append_optional(b,f.attribute_mask);append_optional(b,f.excluded_tag);append_optional(b,f.flags);append_optional(b,f.exclusion_mode);append_optional(b,f.excluded_object.present);append_optional(b,f.excluded_object.index);append_optional(b,f.excluded_status.present);append_optional(b,f.excluded_status.instance);append_optional(b,f.excluded_status.group);append_optional(b,f.extra_c8);append_optional(b,f.extra_c9);append_optional(b,f.face_callback_present);append_optional(b,f.post_callback_present);b.push_back(bool(a.ray.origin));if(a.ray.origin)b.insert(b.end(),a.ray.origin->begin(),a.ray.origin->end());append_optional(b,a.ray.direction_y);return b;
}
std::vector<Word> source_fingerprint(const cr::Resource& s){
 std::vector<Word>b;b.push_back(s.header().group_tree_type);for(const auto& g:s.groups()){b.push_back(g.source_offset);b.insert(b.end(),g.bbox_min.begin(),g.bbox_min.end());b.insert(b.end(),g.bbox_max.begin(),g.bbox_max.end());b.push_back(g.pairs.first);b.push_back(g.pairs.count);b.push_back(g.faces.first);b.push_back(g.faces.count);b.push_back(g.vertices.first);b.push_back(g.vertices.count);}
 for(const auto& f:s.faces()){b.push_back(f.source_offset);b.insert(b.end(),f.normal_bits.begin(),f.normal_bits.end());b.insert(b.end(),f.vertex_local.begin(),f.vertex_local.end());b.push_back(f.attribute_id);}
 for(const auto& v:s.vertices()){b.push_back(v.source_offset);b.insert(b.end(),v.position_bits.begin(),v.position_bits.end());}for(const auto& a:s.attributes()){b.push_back(a.source_offset);b.insert(b.end(),a.words.begin(),a.words.end());}
 for(const auto& p:s.pairs()){b.push_back(p.source_offset);b.push_back(p.face_a_local);b.push_back(p.face_b_local);b.insert(b.end(),p.order_raw.begin(),p.order_raw.end());b.push_back(p.attribute_id);}
 const auto result=s.grid(0);check(std::holds_alternative<cr::Grid>(result),"Resource-owned grid");const auto& g=std::get<cr::Grid>(result);b.insert(b.end(),g.bbox_min.begin(),g.bbox_min.end());b.insert(b.end(),g.bbox_max.begin(),g.bbox_max.end());b.push_back(g.nx);b.push_back(g.nz);b.push_back(g.index_kind);b.push_back(g.compressed);for(const auto& c:g.cells){b.push_back(c.count);b.push_back(c.unknown);b.push_back(c.start);}b.insert(b.end(),g.pair_ids.begin(),g.pair_ids.end());for(const auto& p:g.pair_bounds)b.insert(b.end(),p.words.begin(),p.words.end());return b;
}
struct Counts {std::size_t queries{},direct{},wrapper{},hits{},misses{},candidates{},faces{},math_words{},identity_fields{},wrapper_words{},carrier_known{},carrier_unknown{},grid_words{},grid_updates{},typed_rejections{},lazy{};};
void compare(const Case& c,const sq::Evaluation& e,const cr::Resource& source,const GridExpected& grid,Counts& totals){
 check(e.classification==c.classification(),"Original classification mismatch");check(bool(e.hit)==bool(c.classification()),"Original hit presence mismatch");
 check(e.effects.candidates.size()==c.pairs.size(),"Original candidate count");for(std::size_t i=0;i<c.pairs.size();++i)check(e.effects.candidates[i]==sq::Candidate{0,0,c.pairs[i]},"Original ordered candidate identity");
 check(e.effects.face_calls.size()==c.faces.size(),"Original face-call count");totals.candidates+=c.pairs.size();totals.faces+=c.faces.size();
 check(e.grid_updates.size()==(grid.updated?1U:0U),"Original grid update presence");
 if(grid.updated){const auto& update=e.grid_updates[0];check(update.slot==0&&update.state.resource.sha256()==SBC_SHA&&update.state.group==0,"Grid update source identity");check(update.state.counter==grid.counter&&update.state.depth==grid.depth,"Original grid counter/depth");check(update.state.pair_marks.size()==86,"Grid mark extent");for(unsigned i=0;i<86;++i)check(update.state.pair_marks[i]==grid.marks[i],"Original grid stamp word");totals.grid_words+=87;++totals.grid_updates;}
 
 // Face addresses in the original trace point to scratch carriers. They do not
 // establish IDs/order. Returned native IDs must still resolve to source facts.
 for(const auto& f:e.effects.face_calls){check(f.instance==0&&f.group==0&&f.pair<source.pairs().size(),"Native face-call source domain");const auto p=source.resolve_pair(f.group,f.pair);check(std::holds_alternative<cr::ResolvedPair>(p),"Native face-call source resolution");const auto& v=std::get<cr::ResolvedPair>(p);check(f.face==v.first.id&&f.attribute==v.pair->attribute_id,"Native face-call source identity consistency");}
 if(e.hit){check(e.hit->resource.sha256()==SBC_SHA,"Hit retains source Resource identity");++totals.hits;
  if(!c.wrapper||c.wrapper_carrier){const auto& hit=*e.hit;const auto& id=hit.identity;check(id.instance==word_at(c.observed,8)&&id.group==word_at(c.observed,12)&&id.pair==word_at(c.observed,16)&&id.face==word_at(c.observed,20),"Original selected source identity");
   const auto resolved=source.resolve_pair(id.group,id.pair);check(std::holds_alternative<cr::ResolvedPair>(resolved),"Selected pair owner resolution");const auto& p=std::get<cr::ResolvedPair>(resolved);
   check(id.face==p.first.id&&id.attribute==p.pair->attribute_id,"Selected face/attribute owner facts");
   check(word_at(c.observed,0x34)==0x2700400&&word_at(c.observed,0x38)==0x2600000+p.first.face->source_offset&&word_at(c.observed,0x3c)==0x2600000+source.vertices()[p.group->vertices.first].source_offset&&word_at(c.observed,0x40)==0x2600000+p.attribute->source_offset,"Original relocated pointers agree with source owner");totals.identity_fields+=5;
   const auto& r=hit.replacement;for(unsigned i=0;i<4;++i){check(r.plane[i]==word_at(c.observed,0x60+4*i),"Original plane word");check(r.position[i]==word_at(c.observed,0x70+4*i),"Original point word");check(r.opposed_direction[i]==word_at(c.observed,0x80+4*i),"Original opposed direction word");}check(r.t_bits==word_at(c.observed,0x90),"Original distance word");totals.math_words+=13;
  }
 }else ++totals.misses;
 totals.queries++;totals.wrapper+=c.wrapper;totals.direct+=!c.wrapper;
 for(auto x:c.known)totals.carrier_known+=x;totals.carrier_unknown+=c.known.size()-std::count(c.known.begin(),c.known.end(),1);
}
void run_case(const Fixture& fixture,const Case& c,const cr::Resource& source,const GridExpected& grid,Counts& totals){
 auto in=inputs(fixture,c,source);const auto saved=fingerprint(in);const auto sbc_before=source_fingerprint(source);
 try{
  if(c.wrapper){sq::HeightRequest req{sf::Bits3{c.position[0],c.position[1],c.position[2]},false,std::nullopt,3,2,c.mask,false};
   const auto result=sq::height(in.scene,req);if(const auto* e=std::get_if<sq::Error>(&result))throw std::runtime_error("Native wrapper error: "+std::string(e->field));const auto& out=std::get<sq::HeightResult>(result);check(out.height_bits==c.height&&out.status==c.status,"Original wrapper height/status");totals.wrapper_words+=2;compare(c,out.query,source,grid,totals);
  }else{const auto result=sq::query(in.scene,in.filter,in.ray);if(const auto* e=std::get_if<sq::Error>(&result))throw std::runtime_error("Native query error: "+std::string(e->field));compare(c,std::get<sq::Evaluation>(result),source,grid,totals);}
  check(fingerprint(in)==saved,"Borrowed query input unchanged");check(source_fingerprint(source)==sbc_before,"Borrowed parsed source facts unchanged");
 }catch(const std::exception& e){throw std::runtime_error(std::string(c.extension?"Extension ":"Primary ")+std::to_string(c.ordinal)+" "+c.name+": "+e.what());}
}
void boundary_controls(const Fixture& f,const cr::Resource& source,Counts& totals){
 auto baseline=inputs(f,f.cases.front(),source);const auto source_before=source_fingerprint(source);
 auto reject=[&](Inputs in,sq::ErrorCode code,std::string_view field,sq::Limits limits=sq::Limits{}){
  const auto saved=fingerprint(in);const auto result=sq::query(in.scene,in.filter,in.ray,limits);check(std::holds_alternative<sq::Error>(result),"Typed rejection exposes no partial evaluation");const auto& e=std::get<sq::Error>(result);if(e.code!=code||e.field!=field)throw std::runtime_error("Typed failure expected "+std::string(field)+" got "+std::string(e.field));check(true,"Typed failure code/consumed field");check(fingerprint(in)==saved&&source_fingerprint(source)==source_before,"Failure preserves borrowed state and source");++totals.typed_rejections;
 };
 auto a=baseline;a.scene.direct[3].reset();reject(a,sq::ErrorCode::unknown_operand,"direct container");
 a=baseline;(*a.scene.direct[3])[0].instance=9;reject(a,sq::ErrorCode::invalid_identity,"container instance");
 a=baseline;a.scene.instances[0].resource_present.reset();reject(a,sq::ErrorCode::unknown_operand,"object resource presence");
 a=baseline;a.scene.instances[0].status.enabled_c.reset();reject(a,sq::ErrorCode::unknown_operand,"status enabled");
 a=baseline;a.scene.instances[0].grid_present.reset();reject(a,sq::ErrorCode::unknown_operand,"group grid presence");
 a=baseline;a.scene.instances[0].exclusion_tag=7;a.filter.excluded_tag.reset();reject(a,sq::ErrorCode::unknown_operand,"filter excluded tag");
 a=baseline;a.filter.face_callback_present=true;reject(a,sq::ErrorCode::unsupported_route,"face callback");
 a=baseline;a.filter.post_callback_present=true;reject(a,sq::ErrorCode::unsupported_route,"post callback");
 a=baseline;a.scene.classification_masks[2].reset();reject(a,sq::ErrorCode::unknown_operand,"classification mask40");
 a=baseline;a.scene.classification_masks={0,0,0};reject(a,sq::ErrorCode::unknown_operand,"normal threshold2c");
 a=baseline;a.scene.grid_states[0].counter.reset();reject(a,sq::ErrorCode::unknown_operand,"grid counter");
 a=baseline;a.scene.grid_states[0].pair_marks[0].reset();reject(a,sq::ErrorCode::unknown_operand,"pair visit mark");
 a=baseline;a.scene.instances[0].status.field20=1;reject(a,sq::ErrorCode::unsupported_route,"status byte20 profile");
 a=baseline;a.scene.other_empty[0][3]=false;reject(a,sq::ErrorCode::unsupported_route,"family1498 container");
 a=baseline;auto limits=sq::Limits{};limits.candidates=1;reject(a,sq::ErrorCode::resource_limit,"candidate count",limits);
 a=baseline;limits=sq::Limits{};limits.faces=1;reject(a,sq::ErrorCode::resource_limit,"face count",limits);
 a=baseline;a.scene.extra_tail_count.reset();reject(a,sq::ErrorCode::unknown_operand,"manager47a0");
 a=baseline;const auto raw=source.bytes();std::string changed(reinterpret_cast<const char*>(raw.data()),raw.size());changed[0x20]^=1;const auto changed_hash=picosha2::hash256_hex_string(changed);const auto changed_result=cr::Resource::decode(changed,changed_hash);check(std::holds_alternative<cr::Resource>(changed_result),"Changed opaque source is a valid distinct owned resource");auto second=a.scene.instances[0];second.resource=std::get<cr::Resource>(changed_result);a.scene.instances.push_back(second);a.scene.direct[3]->push_back({1,source.groups()[0].bbox_min[0],source.groups()[0].bbox_max[2]});reject(a,sq::ErrorCode::invalid_identity,"grid runtime source binding");
 auto miss=[&](Inputs in){const auto saved=fingerprint(in);const auto result=sq::query(in.scene,in.filter,in.ray);check(std::holds_alternative<sq::Evaluation>(result),"Lazy unconsumed unknown is accepted");const auto& e=std::get<sq::Evaluation>(result);check(!e.hit&&e.classification==0&&e.effects.candidates.empty()&&e.grid_updates.empty(),"Lazy gate is a complete miss");check(fingerprint(in)==saved,"Lazy success preserves borrowed inputs");++totals.lazy;};
 a=baseline;a.scene.instances[0].enabled_5=0;a.scene.instances[0].resource_present.reset();a.scene.instances[0].resource.reset();a.filter.face_callback_present.reset();a.filter.post_callback_present.reset();miss(a);
 a=baseline;a.filter.area_mask=0;for(auto& d:a.scene.direct)d.reset();for(auto& fam:a.scene.other_empty)for(auto& e:fam)e.reset();for(auto& fam:a.scene.extra_c8_empty)for(auto& e:fam)e.reset();miss(a);
 a=baseline;a.scene.instances[0].resource_present=false;a.scene.instances[0].resource.reset();a.scene.instances[0].status={};miss(a);
 a=baseline;a.filter.object_mask=0;a.scene.instances[0].resource_present.reset();a.scene.instances[0].resource.reset();miss(a);
 for(unsigned kind=0;kind<2;++kind){sq::HeightRequest req{sf::Bits3{f.cases[0].position[0],f.cases[0].position[1],f.cases[0].position[2]},false,std::nullopt,3,2,0x3fffffff,false};
  if(kind==0)req.previous_present.reset();else req.attribute_output_present=true;const auto saved=fingerprint(baseline);const auto result=sq::height(baseline.scene,req);check(std::holds_alternative<sq::Error>(result),"Wrapper typed failure exposes no state");const auto& e=std::get<sq::Error>(result);check(e.code==(kind?sq::ErrorCode::unsupported_route:sq::ErrorCode::unknown_operand)&&e.field==(kind?"height attribute output":"height previous presence"),"Wrapper failure cause");check(fingerprint(baseline)==saved&&source_fingerprint(source)==source_before,"Late wrapper failure preserves scene/source");++totals.typed_rejections;
 }
}
void lifecycle(const Fixture& fixture,const std::vector<Lifecycle>& rows,const cr::Resource& source,Counts& totals){
 std::optional<sq::GridState> current;Word seq=0,expected_step=0;std::size_t sequences=0;
 for(const auto& row:rows){const auto& c=row.c;auto in=inputs(fixture,c,source);auto& g=in.scene.grid_states[0];g.counter=row.before.counter;g.depth=static_cast<std::uint8_t>(row.before.depth);for(unsigned i=0;i<86;++i)g.pair_marks[i]=row.before.marks[i];
  if(row.step==0){current.reset();seq=row.sequence;expected_step=0;++sequences;}check(seq==row.sequence&&row.step==expected_step++,"Original lifecycle sequence continuity");
  if(current){check(current->counter==g.counter&&current->depth==g.depth&&current->pair_marks==g.pair_marks,"Previous native state equals original next input");g=*current;}
  if(row.duplicate)in.scene.direct[3]->push_back(in.scene.direct[3]->front());if(row.drop_grid)in.scene.instances[0].grid_present=false;
  const auto saved=fingerprint(in);const auto source_before=source_fingerprint(source);sq::Evaluation e;
  try{if(c.wrapper){sq::HeightRequest req{sf::Bits3{c.position[0],c.position[1],c.position[2]},row.previous,row.previous?std::optional<Word>(row.previous_bits):std::nullopt,3,row.flags,c.mask,false};const auto result=sq::height(in.scene,req);if(const auto* err=std::get_if<sq::Error>(&result))throw std::runtime_error("Native lifecycle wrapper error "+std::string(err->field));const auto& out=std::get<sq::HeightResult>(result);check(out.height_bits==c.height&&out.status==c.status,"Original lifecycle wrapper scalars");totals.wrapper_words+=2;e=out.query;}
   else{const auto result=sq::query(in.scene,in.filter,in.ray);if(const auto* err=std::get_if<sq::Error>(&result))throw std::runtime_error("Native lifecycle query error "+std::string(err->field));e=std::get<sq::Evaluation>(result);}
   compare(c,e,source,row.after,totals);current=e.grid_updates.empty()?g:e.grid_updates[0].state;check(current->counter==row.after.counter&&current->depth==row.after.depth,"Committed original grid state");for(unsigned i=0;i<86;++i)check(current->pair_marks[i]==row.after.marks[i],"Committed original mark state");check(fingerprint(in)==saved&&source_fingerprint(source)==source_before,"Lifecycle borrowed state unchanged");
  }catch(const std::exception& err){throw std::runtime_error("Lifecycle "+std::to_string(row.family)+"/"+std::to_string(c.ordinal)+" "+c.name+": "+err.what());}
 }check(sequences==34,"All original lifecycle sequences");
}
void run(const Fixture& fixture,const std::vector<Case>& extra,const std::vector<GridExpected>& grids,const std::vector<Lifecycle>& lives,const cr::Resource& source){
 check(source.sha256()==SBC_SHA&&source.bytes().size()==7960&&source.groups().size()==1&&source.pairs().size()==86&&source.faces().size()==86&&source.vertices().size()==48&&source.attributes().size()==2,"Pinned source owner domain");
 Counts totals;for(const auto& c:fixture.cases)run_case(fixture,c,source,grids[c.ordinal],totals);for(const auto& c:extra)run_case(fixture,c,source,grids[223+c.ordinal],totals);
 check(totals.queries==269&&totals.direct==257&&totals.wrapper==12&&totals.hits==143&&totals.misses==126&&totals.candidates==2398&&totals.faces==363,"Complete original query coverage");
 lifecycle(fixture,lives,source,totals);
 boundary_controls(fixture,source,totals);
 check(totals.queries==312&&totals.hits==176&&totals.misses==136&&totals.candidates==2562&&totals.faces==468&&totals.math_words==2249&&totals.identity_fields==865&&totals.wrapper_words==72&&totals.grid_updates==191&&totals.grid_words==16617&&totals.typed_rejections==20&&totals.lazy==4,"Final original/negative coverage counts");
 const auto bytes=source.bytes();check(picosha2::hash256_hex_string(reinterpret_cast<const char*>(bytes.data()),reinterpret_cast<const char*>(bytes.data())+bytes.size())==SBC_SHA,"Borrowed source bytes unchanged");
 std::cout<<"{\"status\":\"passed\",\"cases\":"<<totals.queries<<",\"primary\":223,\"extension\":46,\"lifecycle\":43,\"direct\":"<<totals.direct<<",\"wrappers\":"<<totals.wrapper<<",\"hits\":"<<totals.hits<<",\"misses\":"<<totals.misses<<",\"candidates\":"<<totals.candidates<<",\"face_calls\":"<<totals.faces<<",\"math_words\":"<<totals.math_words<<",\"identity_fields\":"<<totals.identity_fields<<",\"wrapper_scalar_fields\":"<<totals.wrapper_words<<",\"retained_carrier_known_bytes\":"<<totals.carrier_known<<",\"retained_carrier_unknown_bytes\":"<<totals.carrier_unknown<<",\"typed_rejections\":"<<totals.typed_rejections<<",\"lazy_controls\":"<<totals.lazy<<",\"grid_update_count\":"<<totals.grid_updates<<",\"grid_compared_words\":"<<totals.grid_words<<",\"checks\":"<<checks<<"}\n";
}
} // namespace
int main(int argc,char** argv){try{
 if(argc!=11)throw std::runtime_error("Usage: spatial-query-tests primary sha256 extension sha256 grid sha256 lifecycle sha256 SBC sha256");const auto p=read(argv[1],1024U*1024U),e=read(argv[3],1024U*1024U);check(picosha2::hash256_hex_string(p)==argv[2],"Primary fixture SHA256");check(picosha2::hash256_hex_string(e)==argv[4],"Extension fixture SHA256");const auto g=read(argv[5],1024U*1024U);check(picosha2::hash256_hex_string(g)==argv[6],"Grid fixture SHA256");auto fixture=parse_primary(p);auto extra=parse_extension(e);auto grids=parse_grid(g);const auto l=read(argv[7],1024U*1024U);check(picosha2::hash256_hex_string(l)==argv[8],"Lifecycle fixture SHA256");auto lives=parse_lifecycle(l);auto loaded=cr::Resource::load(argv[9],argv[10]);check(std::holds_alternative<cr::Resource>(loaded),"Source SBC SHA/load");run(fixture,extra,grids,lives,std::get<cr::Resource>(loaded));return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
