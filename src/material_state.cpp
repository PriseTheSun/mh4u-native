#include "material_state.hpp"
#include <algorithm>
#include <bit>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace mh4u::material_state {
namespace {
[[noreturn]] void fail(Phase p,ErrorCode c,std::string_view f,std::optional<Word> i={}){throw Error{p,c,f,i,{}};}
Word known(std::optional<Word> v,Phase p,std::string_view f){if(!v)fail(p,ErrorCode::unknown,f);return *v;}
template<class T> T& at(std::vector<T>& v,Word i,Phase p,std::string_view f){if(i>=v.size())fail(p,ErrorCode::extent,f,i);return v[i];}
template<class I> std::optional<I> link(const Link<I>& l,Phase p,std::string_view f){if(std::holds_alternative<Unknown>(l))fail(p,ErrorCode::unknown,f);if(auto i=std::get_if<I>(&l))return *i;return {};}
template<class I> I required(const Link<I>& l,Phase p,std::string_view f){auto i=link(l,p,f);if(!i)fail(p,ErrorCode::absent,f);return *i;}
Byte& byte(Material& m,Word offset){if(offset>=4&&offset<0x30)return m.bytes04[offset-4];if(offset>=0x34&&offset<0x6c)return m.bytes34[offset-0x34];if(offset>=0x70&&offset<0x108)return m.packed70[offset-0x70];fail(Phase::capacity,ErrorCode::extent,"material byte",offset);}
Byte& byte(Reference& r,Word offset){if(offset<0x10)return r.bytes00[offset];if(offset>=0x14&&offset<0x28)return r.bytes14[offset-0x14];fail(Phase::capacity,ErrorCode::extent,"reference byte",offset);}
Byte& byte(Buffer& b,Word offset){if(offset>=b.bytes.size())fail(Phase::copy,ErrorCode::extent,"buffer byte",offset);return b.bytes[offset];}
template<class T> Word read(T& t,Word offset,Phase p){Word out=0;for(Word i=0;i<4;++i){auto v=byte(t,offset+i);if(!v)fail(p,ErrorCode::unknown,"consumed byte",offset+i);out|=Word(*v)<<(8*i);}return out;}
template<class T> void put(T& t,Word offset,Word v,unsigned n=4){for(unsigned i=0;i<n;++i)byte(t,offset+i)=std::uint8_t(v>>(8*i));}
void bound(const State& s,const Object& o,Limits limits){
    std::size_t used=sizeof(State)+sizeof(Object);
    auto charge=[&](std::size_t n,std::size_t size){if(used>limits.representation_bytes||n>(limits.representation_bytes-used)/size)fail(Phase::capacity,ErrorCode::capacity,"representation bytes");used+=n*size;};
    charge(s.materials.size(),sizeof(Material));charge(s.descriptors.size(),sizeof(Descriptor));charge(s.references.size(),sizeof(Reference));charge(s.properties.size(),sizeof(PropertyTable));charge(s.buffers.size(),sizeof(Buffer));charge(s.shader_tables.size(),sizeof(ShaderTable));charge(s.resources.size(),sizeof(MaterialResource));
    charge(o.banks.size(),sizeof(Link<ResourceIndex>));charge(o.destinations.size(),sizeof(Link<MaterialIndex>));
    for(const auto& t:s.properties)charge(t.records.size(),sizeof(Property));for(const auto& b:s.buffers)charge(b.bytes.size(),sizeof(Byte));for(const auto& t:s.shader_tables)charge(t.entry_flags.size(),sizeof(std::optional<Word>));for(const auto& r:s.resources)charge(r.materials.size(),sizeof(Link<MaterialIndex>));
    charge(0,1);
}
bool nan(Word x){return (x&0x7fffffff)>0x7f800000;}
// VCMPE/HS includes unordered. Numeric ordering is derived from the binary32
// encoding, so a lazy comparison does not introduce host FP environment use.
bool hs(Word x,Word y){if(nan(x)||nan(y))return true;if((x&0x7fffffff)==0&&(y&0x7fffffff)==0)return true;if((x^y)&0x80000000)return !(x>>31);return x>>31?x<=y:x>=y;}
struct Engine {
    Output out;const Context& context;Limits limits;std::size_t work{};
    void tick(){if(work==limits.work)fail(Phase::capacity,ErrorCode::capacity,"work budget");++work;}
    Material& material(MaterialIndex i){return at(out.state.materials,i.value,Phase::destination,"material");}
    void binding(Material& m,Phase p){if(!m.binding)fail(p,ErrorCode::unknown,"material binding");if(*m.binding!=Binding::material_std)fail(p,ErrorCode::unsupported_binding,"material binding");}
    Word number(binary32_arithmetic::Result r,Phase p){if(auto e=std::get_if<binary32_arithmetic::Error>(&r))throw Error{p,ErrorCode::arithmetic,"binary32",{},*e};return std::get<Word>(r);}
    Word mul(Word a,Word b,Phase p){return number(binary32_arithmetic::mul(a,b),p);}
    Word pack(Word v,Word scale,Phase p){return number(binary32_arithmetic::to_unsigned_word_rtz(mul(v,scale,p)),p);}
    Word clamp(Word v,unsigned lane,Phase p){auto t=known(context.channel_templates[lane],p,"channel template");return hs(v,t)?t:v;}
    Property* property(MaterialIndex id,Word hash,Word type){
        auto& m=material(id);const auto count=read(m,0x18,Phase::property)&511U;
        if(!count)return nullptr;
        auto ti=required(m.parameters,Phase::property,"property table");auto& t=at(out.state.properties,ti.value,Phase::property,"property table");
        for(Word j=0;j<count;++j){tick();auto& p=at(t.records,j,Phase::property,"property record");auto c=known(p.control,Phase::property,"property control");if((c&15U)==type&&(c>>20)==(hash&4095U))return &p;}return nullptr;
    }
    ReferenceLink refvalue(const PropertyValue& value){if(std::holds_alternative<Unknown>(value))fail(Phase::property,ErrorCode::unknown,"property reference");if(std::holds_alternative<Absent>(value))return Absent{};if(auto i=std::get_if<ReferenceIndex>(&value))return *i;fail(Phase::property,ErrorCode::invalid_identity,"property reference kind");}
    ReferenceLink findref(MaterialIndex id,Word hash){auto p=property(id,hash,3);return p?refvalue(p->value):ReferenceLink{Absent{}};}
    std::optional<BufferLocation> findbuffer(MaterialIndex id){auto p=property(id,0x16a5b126,1);if(!p||std::holds_alternative<Absent>(p->value))return {};if(std::holds_alternative<Unknown>(p->value))fail(Phase::property,ErrorCode::unknown,"type1 buffer");auto b=std::get_if<BufferLocation>(&p->value);if(!b)fail(Phase::property,ErrorCode::invalid_identity,"type1 kind");if(b->tag>15||(b->offset&15U))fail(Phase::property,ErrorCode::invalid_identity,"type1 alignment/tag");return *b;}
    Reference& reference(ReferenceIndex i){return at(out.state.references,i.value,Phase::release,"reference");}
    void release(ReferenceIndex i){tick();auto& r=reference(i);auto n=read(r,8,Phase::release);put(r,8,n-1);++out.effects.releases;if(n==1){auto value=read(r,4,Phase::release);auto threshold=known(context.release_threshold,Phase::release,"release threshold");fail(Phase::release,std::bit_cast<std::int32_t>(value)<std::bit_cast<std::int32_t>(threshold)?ErrorCode::unsupported_destructor:ErrorCode::unsupported_retirement,"last reference",i.value);}}
    void acquire(ReferenceIndex i){tick();auto& r=reference(i);put(r,8,read(r,8,Phase::release)+1);++out.effects.acquires;}
    void rebind(MaterialIndex id,ReferenceIndex selected){
        ++out.effects.shader_rebindings;auto& m=material(id);auto old=link(m.shader,Phase::shader,"old shader");
        if(!old||*old!=selected){if(old){release(*old);m.shader=Absent{};}m.shader=selected;acquire(selected);}
        for(Word off=0x48;off<0x68;off+=4)put(m,off,0);
        for(Word j=0;j<4;++j)put(m,0x48+8*j,0xffff,2);
        auto& ref=reference(selected);auto ti=required(ref.shader_table,Phase::shader,"shader table");auto& t=at(out.state.shader_tables,ti.value,Phase::shader,"shader table");auto n=known(t.count,Phase::shader,"shader count");Word chosen=0;
        for(Word j=0;j<n;++j){tick();auto f=known(at(t.entry_flags,j,Phase::shader,"shader entry"),Phase::shader,"shader flags");if((f&2)&&chosen<4){auto off=0x48+8*chosen++;put(m,off,j,2);put(m,off+4,0);put(m,off+2,0,2);}}
    }
    void setref(MaterialIndex id,Word hash,ReferenceLink selected){
        ++out.effects.property_setters;auto p=property(id,hash,3);if(!p)return;auto prior=refvalue(p->value);auto old=link(prior,Phase::property,"old property");auto next=link(selected,Phase::property,"new property");if(old==next)return;
        auto stamp=read(material(id),4,Phase::property),threshold=known(context.release_threshold,Phase::property,"property threshold");
        if(old){if(std::bit_cast<std::int32_t>(stamp)>=std::bit_cast<std::int32_t>(threshold))put(reference(*old),4,threshold+known(context.retirement_delay,Phase::property,"retirement delay"));release(*old);}
        if(!next){p->value=Absent{};p->word8=0;}else{acquire(*next);p->value=*next;p->word8=(hash&0xfffff000U)|read(reference(*next),12,Phase::property);}
    }
    bool compatible(MaterialIndex source,MaterialIndex dest){
        auto& sm=material(source);binding(sm,Phase::descriptor);auto si=required(sm.descriptor,Phase::descriptor,"source descriptor");
        auto& dm=material(dest);binding(dm,Phase::descriptor);auto di=required(dm.descriptor,Phase::descriptor,"destination descriptor");
        auto target=known(at(out.state.descriptors,si.value,Phase::descriptor,"source descriptor").id,Phase::descriptor,"source descriptor ID");
        for(std::size_t walked=0;;++walked){tick();auto& d=at(out.state.descriptors,di.value,Phase::descriptor,"descriptor");if(walked>=out.state.descriptors.size())fail(Phase::descriptor,ErrorCode::descriptor_cycle,"descriptor chain");if(known(d.id,Phase::descriptor,"descriptor ID")==target)return true;auto parent=link(d.parent,Phase::descriptor,"descriptor parent");if(!parent)return false;di=*parent;}
    }
    Buffer& buffer(BufferLocation l,std::size_t size,Phase p){auto& b=at(out.state.buffers,l.buffer.value,p,"buffer");if(l.offset>b.bytes.size()||size>b.bytes.size()-l.offset)fail(p,ErrorCode::extent,"buffer range",l.offset);return b;}
    void color(MaterialIndex id,const std::array<std::optional<Word>,4>& values){
        auto loc=findbuffer(id);if(!loc)return;auto& b=buffer(*loc,0x60,Phase::channels);auto& m=material(id);std::array<Word,4> v;
        for(unsigned i=0;i<4;++i){v[i]=known(values[i],Phase::channels,"channel");put(b,loc->offset+0x50+4*i,v[i]);}
        for(unsigned i=0;i<4;++i)v[i]=clamp(v[i],i,Phase::channels);
        for(unsigned i=0;i<4;++i)v[i]=pack(v[i],0x437f0000,Phase::channels);
        for(unsigned i=0;i<4;++i)put(m,0x88+i,v[i],1);
    }
    // The following three operations are the concrete bit-packers called by
    // 62a918. Their arguments are raw low-byte enums, never float conversions.
    void pack_a(Material& m,Word selector){
        auto a=read(m,0x7c,Phase::refresh),b=read(m,0x80,Phase::refresh),c=read(m,0x84,Phase::refresh);
        a=(a&~0xfffU)|0x664;b=(b&~0xfffU)|0x300;c&=~15U;
        put(m,0x7c,a);put(m,0x80,b);put(m,0x84,c);
        switch(selector){
        case 0:case 9:a=(a&~0xf0U)|0xe0;c|=1;put(m,0x7c,a);put(m,0x84,c);break;
        case 1:c|=4;put(m,0x84,c);break;
        case 2:a&=~0xf00U;c|=4;put(m,0x7c,a);put(m,0x84,c);break;
        case 3:case 4:c|=selector-2;put(m,0x84,c);break;
        case 5:case 6:case 10:a=(a&~0xff0U)|(selector==5?0x550U:selector==6?0x50U:0xe50U);c|=4;if(selector==10)b=(b&~0xf00U)|0x200;put(m,0x7c,a);if(selector==10)put(m,0x80,b);put(m,0x84,c);break;
        case 7:case 8:a=(a&~0xf0U)|0x50;c|=selector-6;put(m,0x7c,a);put(m,0x84,c);break;
        default:break;
        }
    }
    void pack_b(Material& m,Word first,Word second){
        auto a=(read(m,0x7c,Phase::refresh)&~0x0fff0000U)|0x0eee0000U;
        auto b=read(m,0x80,Phase::refresh)&~0x00777000U;
        auto c=read(m,0x84,Phase::refresh)&~0xf0000U;
        put(m,0x84,c);put(m,0x7c,a);put(m,0x80,b);
        if(first>=1&&first<=3){a=(a&~0xf0000U)|((first==1?4U:first==2?5U:6U)<<16);put(m,0x7c,a);}
        switch(second){
        case 1:a=(a&~0xf00000U)|0x600000;c|=0x20000;put(m,0x7c,a);put(m,0x84,c);break;
        case 2:case 3:case 4:c|=second==3?0x20000:0x10000;put(m,0x84,c);a=(a&~0xf00000U)|(second==2?0x600000:0x500000);put(m,0x7c,a);break;
        case 5:case 6:case 7:a&=~0xf00000U;b=(b&~0x70000U)|((second==5?2U:second==6?4U:6U)<<16);c|=0x10000;put(m,0x7c,a);put(m,0x80,b);put(m,0x84,c);break;
        default:break;
        }
    }
    void pack_c(Material& m,Word first,Word second,bool enabled){
        constexpr Word base=0xb8;
        auto a=(read(m,base,Phase::refresh)&~0xffU)|0xfe2U;
        auto b=read(m,base+4,Phase::refresh)&~0xfffU;
        auto c=(read(m,base+8,Phase::refresh)&~15U)|1;
        auto d=(read(m,base+20,Phase::refresh)&~15U)|0xff2;
        auto e=read(m,base+24,Phase::refresh)&~0xfffU;
        auto f=(read(m,base+28,Phase::refresh)&~15U)|9;
        put(m,base+8,c);put(m,base+28,f);put(m,base,a);put(m,base+4,b);put(m,base+20,d);put(m,base+24,e);
        if(first!=0){
            if(first<=4){a=(a&~0xf0U)|(first==1?0x4fU:first==2?0x5fU:first==3?0x6fU:0x0fU);b=(b&~15U)|2;c=(c&~15U)|1;put(m,base,a);put(m,base+4,b);put(m,base+8,c);}
            if(second==1||second==2){d=(d&~0xffU)|2|(second==1?0xf50U:0xf60U);f=(f&~15U)|(enabled?9U:8U);put(m,base+20,d);put(m,base+28,f);}
            else if(second==3){d=(d&~0xffU)|0xf22;f=(f&~15U)|(enabled?9U:8U);e=(e&~(enabled?0xf0U:0xf00U))|(enabled?0x20U:0x200U);put(m,base+28,f);put(m,base+20,d);put(m,base+24,e);}
            else{d=(d&~0xf0U)|0x2f;f=(f&~15U)|1;put(m,base+20,d);put(m,base+28,f);}return;
        }
        if(enabled){d=(d&~0xff0U)|0xd2f;e=(e&~0xf00U)|0x200;}else{d=(d&~0xf0fU)|0xdf2;e=(e&~0xf0U)|0x20;}
        f=(f&~15U)|9;put(m,base+20,d);put(m,base+24,e);put(m,base+28,f);
        if(second==1||second==2){a=(a&~0xffU)|(second==1?0xe5U:0xe6U);c=(c&~15U)|1;put(m,base,a);put(m,base+8,c);}
        else if(second>=3&&second<=8){a=(a&~0xffU)|(second==3?0xe2U:second==4?2U:second==5?0x12U:second==6?0x42U:second==7?0x52U:0x62U);b=(b&~15U)|2;c=(c&~15U)|(second>=7?9U:1U);if(second>=7)a=(a&~0xf00U)|0xe00;put(m,base,a);put(m,base+4,b);put(m,base+8,c);}
        else{c=(c&~15U)|1;d=(d&~0xf0U)|0x2f;e&=~0xf0U;f=(f&~15U)|1;a=(a&~0xf0U)|0xef;b=(b&~15U)|2;put(m,base+8,c);put(m,base+20,d);put(m,base+24,e);put(m,base+28,f);put(m,base,a);put(m,base+4,b);}
    }
    void refresh_material(MaterialIndex id){
        binding(material(id),Phase::refresh);++out.effects.refreshes;auto loc=findbuffer(id);if(!loc)return;auto& b=buffer(*loc,0xa8,Phase::refresh);auto& m=material(id);
        auto readb=[&](Word off){return read(b,loc->offset+off,Phase::refresh);};
        const auto p74=readb(0x74),p8c=readb(0x8c),p90=readb(0x90),p94=readb(0x94),p98=readb(0x98),p9c=readb(0x9c),p84=readb(0x84),p78=readb(0x78),p7c=readb(0x7c),p88=readb(0x88),pa0=readb(0xa0),p80=readb(0x80);
        put(m,0x18,(read(m,0x18,Phase::refresh)&0x7fffffffU)|(p84?0x80000000U:0));
        pack_a(m,p8c&255);pack_b(m,p90&255,p94&255);pack_c(m,p98&255,p9c&255,pa0!=0);
        auto ref=link(findref(id,0x226601a2),Phase::refresh,"optional type3");bool marked=ref&&(read(reference(*ref),0x24,Phase::refresh)&0xf00000U);
        put(b,loc->offset+0xa4,marked?1U:0U);
        put(m,0x70,(read(m,0x70,Phase::refresh)&~0x2000U)|(!marked&&p88==0?0x2000U:0U));
        auto bits=[&](Word off,Word mask,Word v){put(m,off,(read(m,off,Phase::refresh)&~mask)|v);};
        bits(0xc0,0xf0000,p7c?0x10000:0);bits(0xb8,0xf0000,p84?0x40000:0xe0000);bits(0xd4,0xf0000,p78?0x10000:0);bits(0x98,0xf0000,p74?0x10000:0);bits(0x90,15,p80?2:14);
        std::array<std::array<Word,4>,6> v{};
        for(unsigned row=0;row<6;++row){unsigned count=(row==0||row==4)?3:4;for(unsigned lane=0;lane<count;++lane){auto x=readb(row*16+lane*4);if(row==0)x=mul(x,0x3f000000,Phase::refresh);v[row][lane]=clamp(x,lane,Phase::refresh);}}
        auto triplet=[&](Word off,unsigned row,Word scale){auto prior=read(m,off,Phase::refresh);auto x=pack(v[row][0],scale,Phase::refresh),y=pack(v[row][1],scale,Phase::refresh),z=pack(v[row][2],scale,Phase::refresh);put(m,off,(prior&0xc0000000U)|((x&1023U)<<20)|((y&1023U)<<10)|(z&1023U));};
        triplet(0xfc,1,0x43800000);triplet(0xf8,0,0x437f0000);triplet(0x104,2,0x43800000);triplet(0xf4,4,0x43800000);
        for(unsigned i=0;i<4;++i)put(m,0xc4+i,pack(v[3][i],0x437f0000,Phase::refresh),1);
        put(m,0xb3,pack(v[2][3],0x437f0000,Phase::refresh),1);put(m,0xef,pack(v[1][3],0x437f0000,Phase::refresh),1);
        for(unsigned i=0;i<3;++i)put(m,0x9c+i,pack(v[4][i],0x437f0000,Phase::refresh),1);
        for(unsigned i=0;i<4;++i)put(m,0x88+i,pack(v[5][i],0x437f0000,Phase::refresh),1);
    }
    void apply(Word bank,Word exclusion){
        tick();auto ri=link(at(out.object.banks,bank,Phase::resource,"bank"),Phase::resource,"bank resource");if(!ri)return;
        auto& r=at(out.state.resources,ri->value,Phase::resource,"resource");if(!r.binding)fail(Phase::resource,ErrorCode::unknown,"resource binding");if(*r.binding!=ResourceBinding::rmaterial)fail(Phase::resource,ErrorCode::unsupported_binding,"resource binding");
        if(known(r.count,Phase::resource,"resource count")==0)return;
        auto source_link=at(r.materials,0,Phase::resource,"resource material0");const auto count=known(out.object.destination_count,Phase::destination,"destination count");out.effects.resource_gate=true;
        for(Word j=0;j<count;++j){tick();++out.effects.destinations;auto dest=required(at(out.object.destinations,j,Phase::destination,"destination"),Phase::destination,"destination");
            if(!(exclusion&0x80000000U)&&exclusion==((read(material(dest),0x18,Phase::destination)>>22)&255))continue;
            auto source=required(source_link,Phase::descriptor,"source material");if(!compatible(source,dest))continue;++out.effects.compatible;
            auto selected=link(material(source).shader,Phase::shader,"source shader");if(selected)rebind(dest,*selected);
            for(Word hash:{0x5dec11aeU,0x0b9981aaU}){if(link(findref(source,hash),Phase::property,"source property")){auto again=findref(source,hash);setref(dest,hash,again);}}
            auto to=findbuffer(dest),from=findbuffer(source);if(!to||!from)continue;
            const auto length=std::size_t(known(context.buffer_word_count,Phase::copy,"MFX buffer words")&65535U)*4;
            auto& dst=buffer(*to,length,Phase::copy);auto& src=buffer(*from,length,Phase::copy);
            if(to->buffer==from->buffer&&to->offset!=from->offset&&std::max(to->offset,from->offset)<std::min(std::size_t(to->offset)+length,std::size_t(from->offset)+length))fail(Phase::copy,ErrorCode::unsupported_overlap,"partial buffer overlap");
            for(std::size_t k=0;k<length;++k){tick();auto v=src.bytes[from->offset+k];if(!v)fail(Phase::copy,ErrorCode::unknown,"copied buffer byte",Word(k));dst.bytes[to->offset+k]=v;}
            ++out.effects.copies;out.effects.copied_bytes+=length;refresh_material(dest);
        }
        out.object.flags104=known(out.object.flags104,Phase::caches,"object flags104")|0x10000000;
        for(auto& cache:out.object.caches){auto i=link(cache,Phase::caches,"cache");if(i){release(*i);cache=Absent{};++out.effects.cache_clears;}}
    }
};
template<class F> Result transaction(const State& s,const Object& o,const Context& context,Limits limits,F f){try{bound(s,o,limits);Engine e{{s,o,{}},context,limits,{}};f(e);return std::move(e.out);}catch(const Error& e){return e;}catch(const std::bad_alloc&){return Error{Phase::capacity,ErrorCode::capacity,"allocation",{}, {}};}catch(const std::length_error&){return Error{Phase::capacity,ErrorCode::capacity,"allocation length",{}, {}};}}
} // namespace
Result apply_bank(const State& s,const Object& o,Word bank,Word exclusion,const Context& c,Limits l){return transaction(s,o,c,l,[&](Engine& e){e.apply(bank,exclusion);});}
Result set_channels(const State& s,MaterialIndex id,const std::array<std::optional<Word>,4>& v,const Context& c,Limits l){return transaction(s,{},c,l,[&](Engine& e){e.color(id,v);});}
Result refresh(const State& s,MaterialIndex id,const Context& c,Limits l){return transaction(s,{},c,l,[&](Engine& e){e.refresh_material(id);});}
} // namespace mh4u::material_state
