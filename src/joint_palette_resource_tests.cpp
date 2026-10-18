#include "joint_palette.hpp"
#include "model_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>

namespace {
namespace jp=mh4u::joint_palette;namespace mr=mh4u::model_resource;namespace math=mh4u::motion_transform_math;
using Bytes=std::vector<std::uint8_t>;using Arenas=std::array<Bytes,11>;
constexpr std::array<std::uint32_t,11> bases{0x2000000,0x2100000,0x2200000,0x2300000,0x2400000,0x2500000,0x2600000,0x2700000,0x2800000,0x2900000,0x2a00000};
constexpr std::array<std::uint32_t,11> sizes{8192,4096,4096,4096,65536,16384,4096,4096,65536,4096,4096};
constexpr std::size_t arena_bytes=184320;
std::uint64_t checks{},arm_arena_bytes{},binding_context_bytes{},immutability_bytes{},palette_patch_bytes{},command_patch_bytes{},aos_cases{},aos_joints{},observed_patch_store_bytes{},borrowed_table_bytes{};
void check(bool value,const std::string& msg){++checks;if(!value)throw std::runtime_error(msg);}
std::uint32_t word(const Bytes& b,std::size_t p){if(p>b.size()||4>b.size()-p)throw std::runtime_error("Read extent");std::uint32_t v=0;for(unsigned i=0;i<4;++i)v|=std::uint32_t(b[p+i])<<(8*i);return v;}
void put(Bytes& b,std::size_t p,std::uint32_t v){if(p>b.size()||4>b.size()-p)throw std::runtime_error("Write extent");for(unsigned i=0;i<4;++i)b[p+i]=static_cast<std::uint8_t>(v>>(8*i));}
template<std::size_t N>std::array<std::uint32_t,N> words(const Bytes& b,std::size_t p){std::array<std::uint32_t,N> out{};for(unsigned i=0;i<N;++i)out[i]=word(b,p+4*i);return out;}
struct Reader {const Bytes& b;std::size_t p{};std::uint32_t integer(){auto v=word(b,p);p+=4;return v;}Bytes take(std::size_t n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("Truncated fixture");Bytes out(b.begin()+static_cast<std::ptrdiff_t>(p),b.begin()+static_cast<std::ptrdiff_t>(p+n));p+=n;return out;}Arenas arenas(){Arenas a;for(unsigned i=0;i<11;++i)a[i]=take(sizes[i]);return a;}};
void same(const Bytes& a,const Bytes& b,const std::string& name){check(a.size()==b.size(),name+" size");if(a!=b){const auto d=std::mismatch(a.begin(),a.end(),b.begin());std::ostringstream s;s<<name<<" byte+0x"<<std::hex<<std::distance(a.begin(),d.first)<<" got "<<unsigned(*d.first)<<" expected "<<unsigned(*d.second);throw std::runtime_error(s.str());}++checks;}
void same(const Arenas& a,const Arenas& b,const std::string& name){for(unsigned i=0;i<11;++i)same(a[i],b[i],name+" arena"+std::to_string(i));}
// Only the fixture adapter knows synthetic ARM addresses. Runtime Locations
// use unrelated stable storage tokens; opaque old bindings have separate owners.
struct Pointers {
    std::map<std::uint64_t,std::uint32_t> origin{{1,bases[8]},{2,bases[10]}};
    jp::Location decode(std::uint32_t address){
        for(const auto& [id,base]:origin)if((id==1&&address>=base&&address-base<65536)||(id==2&&address>=base&&address-base<=4096))return{id,address-base};
        const auto aligned=address&~15U;for(const auto& [id,base]:origin)if(base==aligned)return{id,address-aligned};
        const std::uint64_t id=10+origin.size();origin.emplace(id,aligned);return{id,address-aligned};
    }
    std::uint32_t encode(jp::Location value)const{check(origin.contains(value.storage),"Known logical allocation identity");const std::uint64_t address=std::uint64_t(origin.at(value.storage))+value.offset;check(address<=0xffffffffU,"Projection address width");return static_cast<std::uint32_t>(address);}
    jp::Binding binding(std::uint32_t value){if(!value)return jp::Null{};return jp::TaggedLocation{decode(value&~15U),static_cast<std::uint8_t>(value&15)};}
    std::uint32_t encode(const jp::Binding& value)const{if(std::holds_alternative<jp::Null>(value))return 0;const auto* v=std::get_if<jp::TaggedLocation>(&value);check(v&&v->low_bits<16,"Represented binding");return encode(v->address)|v->low_bits;}
    std::uint32_t encode(const jp::Pointer& value)const{if(std::holds_alternative<jp::Null>(value))return 0;const auto* v=std::get_if<jp::Location>(&value);check(v!=nullptr,"Represented pointer");return encode(*v);}
};
struct Input {
    Pointers pointers;mh4u::joint_output::State joint_owner;std::vector<jp::Matrix> joints,source;std::vector<std::vector<std::uint8_t>> palette_data;std::vector<jp::PaletteView> palettes;
    std::vector<std::uint32_t> handles,dirty;jp::ModelView model;jp::ResourceReference resource;jp::Context context;jp::Manager manager;
    Input(const Arenas& a,std::uint32_t n,std::uint32_t b,std::uint32_t p,std::uint32_t source_index,const std::vector<mr::Resource>& resources,std::uint32_t seed){
        for(unsigned i=0;i<n;++i){
            const auto offset=i*168;
            if(source_index){mh4u::joint_output::Joint joint{};joint.matrix=words<16>(a[4],offset+8);
                joint.pose={words<3>(a[4],offset+0x48),words<4>(a[4],offset+0x58),words<4>(a[4],offset+0x68),words<4>(a[4],offset+0x78)};
                joint.metadata={word(a[4],offset+0x94),word(a[4],offset+0x98)};joint.depth=word(a[4],offset+0x90);
                // Callback/owner pointer applicability is unknown and unconsumed.
                joint_owner.joints.push_back(joint);
            }else joints.push_back(words<16>(a[4],offset+8));
        }for(unsigned i=0;i<b;++i)source.push_back(words<16>(a[5],i*64));
        for(unsigned i=0;i<p;++i){const auto count=word(a[3],i*28)&65535;check(count<=24,"Fixture palette record count");palette_data.emplace_back(a[3].begin()+i*28+4,a[3].begin()+i*28+4+count);handles.push_back(word(a[6],i*4));}
        for(const auto& indices:palette_data)palettes.push_back({indices});
        std::span<const jp::Matrix> source_view=source;
        if(source_index){check(source_index<=resources.size(),"Known MOD source");const auto& resource_owner=resources[source_index-1];const auto sk=resource_owner.skeleton();check(sk&&resource_owner.joint_count()==n&&sk->source_skin_matrices.size()==b&&sk->palettes.size()==p,"Source MOD skeleton facts");check(std::equal(source.begin(),source.end(),sk->source_skin_matrices.begin()),"Source B is owner's exact raw matrix table");source_view=sk->source_skin_matrices;
            palettes.clear();for(unsigned i=0;i<p;++i){check(sk->palettes[i].descriptor==word(a[3],i*28)&&std::ranges::equal(sk->palettes[i].joints,palette_data[i]),"Source palette identity and raw descriptor");palettes.push_back({sk->palettes[i].joints});}
        }
        const std::uint64_t resource_id=source_index?resources[source_index-1].identity():0xffffffffffffffffULL-seed;
        if(!source_index)for(const auto& owner:resources)check(resource_id!=owner.identity(),"Synthetic resource identity is disjoint from MOD owners");
        model.pose_identity=2000+seed;model.joint_count=n;model.palette_count=p;model.flags104=word(a[0],0x104);model.word154=word(a[0],0x154);model.history_selector=word(a[0],0xc50);model.parent_sharing=a[0][0xc4b];model.parent=jp::Unknown{};model.handles=handles;model.joint_matrices=source_index?jp::JointMatrices(std::span<const mh4u::joint_output::Joint>(joint_owner.joints)):jp::JointMatrices(std::span<const jp::Matrix>(joints));
        if(source_index){++aos_cases;aos_joints+=n;for(unsigned i=0;i<n;++i)check(&model.joint_matrices[i]==&joint_owner.joints[i].matrix,"Joint matrix borrowed from canonical AoS owner without extraction");}
        resource=jp::ResourceView{resource_id,a[2][0x214],source_view,palettes};
        context.identity=3000+seed;context.flags4c=word(a[1],0x4c);context.flags188=word(a[1],0x188);context.flags1bc=word(a[1],0x1bc);context.command=jp::CommandCursor{{{2,0},4096},word(a[1],0x14)-bases[10],word(a[1],0x18)-bases[10]};context.command638=pointers.decode(word(a[1],0x638));context.descriptor_word=word(a[9],0x814);context.offset=words<3>(a[1],0xe0);
        for(unsigned i=0;i<3;++i)context.bindings[i]=pointers.binding(word(a[1],0x438+4*i));
        for(unsigned i=0;i<64;++i)dirty.push_back(word(a[7],0x128+i*4));manager.normal=word(a[7],0x32c)?jp::Buffer{jp::Region{{1,0},65536}}:jp::Buffer{jp::Null{}};manager.history=jp::Null{};manager.stride=word(a[7],0x334);manager.local_lock=a[7][0x10];manager.global_lock=0;manager.dirty_words=dirty;
    }
    void verify_borrowed_tables(const Arenas& before)const{
        check(model.joint_count==word(before[0],0x220)&&model.palette_count==word(before[0],0xbc4),"Borrowed active counts unchanged");
        check(model.joint_matrices.size()==model.joint_count,"Borrowed joint view extent unchanged");
        for(unsigned i=0;i<model.joint_count;++i){check(model.joint_matrices[i]==words<16>(before[4],i*168+8),"Every borrowed W matrix unchanged");borrowed_table_bytes+=64;}
        const auto& view=std::get<jp::ResourceView>(resource);check(view.source_matrices.size()==source.size()&&view.palettes.size()==model.palette_count&&model.handles.size()==model.palette_count,"Borrowed resource view extents unchanged");
        for(unsigned i=0;i<view.source_matrices.size();++i){check(view.source_matrices[i]==words<16>(before[5],i*64),"Every borrowed B matrix unchanged");borrowed_table_bytes+=64;}
        for(unsigned i=0;i<model.palette_count;++i){check(model.handles[i]==word(before[6],i*4),"Every borrowed handle unchanged");borrowed_table_bytes+=4;const auto count=word(before[3],i*28)&65535;check(view.palettes[i].indices.size()==count,"Borrowed palette extent unchanged");
            for(unsigned k=0;k<count;++k){check(view.palettes[i].indices[k]==before[3][i*28+4+k],"Every borrowed palette index unchanged");++borrowed_table_bytes;}}
        check(manager.dirty_words.size()==64,"Borrowed dirty extent unchanged");for(unsigned i=0;i<64;++i){check(manager.dirty_words[i]==word(before[7],0x128+4*i),"Every borrowed dirty word unchanged");borrowed_table_bytes+=4;}
    }
    void encode_context(Bytes& bytes,const jp::Context& c)const{
        check(c.identity==context.identity,"Context snapshot identity preserved");put(bytes,0x4c,c.flags4c);put(bytes,0x188,c.flags188);put(bytes,0x1bc,c.flags1bc);
        const auto* command=std::get_if<jp::CommandCursor>(&c.command);check(command!=nullptr,"Known command cursor");const auto& original=std::get<jp::CommandCursor>(context.command);check(command->buffer==original.buffer&&command->end==original.end,"Registered command region identity/extent/end preserved");put(bytes,0x14,pointers.encode(jp::Location{command->buffer.base.storage,command->buffer.base.offset+command->cursor}));put(bytes,0x18,pointers.encode(jp::Location{command->buffer.base.storage,command->buffer.base.offset+command->end}));put(bytes,0x638,pointers.encode(c.command638));
        for(unsigned i=0;i<3;++i)put(bytes,0x438+4*i,pointers.encode(c.bindings[i]));
        check(c.descriptor_word==context.descriptor_word&&c.offset==context.offset,"Borrowed descriptor/offset preserved");
    }
    Arenas apply(const Arenas& initial,const jp::Prepared& prepared,std::span<const std::array<std::uint32_t,3>> observations)const{
        std::size_t observed=0;auto out=initial;encode_context(out[1],prepared.context);put(out[0],0x104,prepared.model_flags104);
        for(const auto& dirty_write:prepared.dirty_writes){check(dirty_write.bank<64,"Dirty bank extent");put(out[7],0x128+dirty_write.bank*4,dirty_write.value);}
        for(const auto& write:prepared.writes){
            const auto address=pointers.encode(write.destination);unsigned arena=11;for(unsigned i=0;i<11;++i)if(address>=bases[i]&&address-bases[i]<=sizes[i]&&write.words.size()*4<=sizes[i]-(address-bases[i])){arena=i;break;}check(arena==8||arena==10,"Only admitted registered output allocation");
            if(write.kind==jp::WriteKind::model_word154){check(arena==10&&write.words==std::vector<std::uint32_t>{model.word154}&&!write.input&&write.source_indices.empty(),"Command word source and omission");command_patch_bytes+=write.words.size()*4;}
            else {check(arena==8&&write.input.has_value(),"Palette patch owner");const auto& id=*write.input;check(id.resource==std::get<jp::ResourceView>(resource).identity&&id.pose==model.pose_identity&&id.context==context.identity&&id.palette<palettes.size(),"Complete input identities");check(std::ranges::equal(write.source_indices,palettes[id.palette].indices)&&write.words.size()==write.source_indices.size()*12,"Patch source indices/count");palette_patch_bytes+=write.words.size()*4;}
            // Source stores within a palette patch use a noncontiguous instruction
            // order. Match every address/value before advancing to the next patch;
            // this retains palette order and catches values hidden by later aliases.
            std::vector<bool> covered(write.words.size());
            for(unsigned k=0;k<write.words.size();++k){check(observed<observations.size(),"Original patch store extent");const auto& store=observations[observed++];
                check(store[1]==4&&store[0]>=address&&(store[0]-address)%4==0&&(store[0]-address)/4<write.words.size(),"Original store belongs to this ordered patch");const auto index=(store[0]-address)/4;
                check(!covered[index]&&write.words[index]==store[2],"Every original patch value, including later-overwritten aliases");covered[index]=true;observed_patch_store_bytes+=4;
            }
            for(unsigned k=0;k<write.words.size();++k)put(out[arena],address-bases[arena]+k*4,write.words[k]);
        }check(observed==observations.size(),"Every original patch store consumed");return out;
    }
};
void reject(const jp::Result<jp::Prepared>& result,jp::ErrorCode code){const auto* e=std::get_if<jp::Error>(&result);check(e&&e->code==code,"Expected preparation error");}
void boundaries(const Input& in){
    auto context=in.context;context.descriptor_word.reset();reject(jp::prepare(in.model,in.resource,context,in.manager),jp::ErrorCode::unknown_descriptor);
    auto manager=in.manager;manager.local_lock=1;reject(jp::prepare(in.model,in.resource,in.context,manager),jp::ErrorCode::unsupported_lock);
    auto model=in.model;model.history_selector=0;reject(jp::prepare(model,in.resource,in.context,in.manager),jp::ErrorCode::unsupported_history);
    context=in.context;std::get<jp::CommandCursor>(context.command).end=0;reject(jp::prepare(in.model,in.resource,context,in.manager),jp::ErrorCode::allocation_required);
    manager=in.manager;manager.normal=jp::Region{{0,0},65536};reject(jp::prepare(in.model,in.resource,in.context,manager),jp::ErrorCode::invalid_storage);
    manager=in.manager;manager.normal=jp::Region{{1,0},4};reject(jp::prepare(in.model,in.resource,in.context,manager),jp::ErrorCode::buffer_extent);
    const auto context_before=in.context;const auto dirty_before=in.dirty;manager=in.manager;manager.history=jp::Region{{1,0},65536};const auto bound=jp::bind(in.context,manager,0);const auto* e=std::get_if<jp::Error>(&bound);check(e&&e->code==jp::ErrorCode::unsupported_history,"Binding history rejects explicitly");check(in.context==context_before&&in.dirty==dirty_before,"Failures preserve original context/manager facts");
}
void oracle(const Bytes& bytes,const std::array<std::string,2>& paths){
    Reader r{bytes};check(r.take(8)==Bytes({'M','H','4','U','J','P','0','2'}),"Fixture magic");const auto count=r.integer();check(count>0&&count<=256&&r.integer()==11,"Fixture counts");for(unsigned i=0;i<11;++i)check(r.integer()==bases[i]&&r.integer()==sizes[i],"Arena identity");check(r.integer()==2,"Source count");
    std::vector<mr::Resource> resources;for(unsigned i=0;i<2;++i){const auto size=r.integer(),joints=r.integer();const auto digest=r.take(32);std::string hex;picosha2::bytes_to_hex_string(digest.begin(),digest.end(),hex);auto loaded=mr::Resource::load(paths[i],hex);if(const auto* e=std::get_if<mr::Error>(&loaded))throw std::runtime_error("MOD source: "+e->message);resources.push_back(std::get<mr::Resource>(std::move(loaded)));check(resources.back().bytes().size()==size&&resources.back().joint_count()==joints,"MOD identity/count");}
    unsigned successful=0,numeric_errors=0,binding_errors=0,preparations=0,bindings=0,real_cases=0;bool negative_done=false;
    for(unsigned i=0;i<count;++i){try{
        const auto length=r.integer();check(length>=40+3*arena_bytes&&length<=40+3*arena_bytes+256*4100+128*16+36865*12,"Record size");const auto record=r.take(length);Reader f{record};const auto seed=f.integer(),error=f.integer(),n=f.integer(),b=f.integer(),p=f.integer(),binding_count=f.integer(),source=f.integer(),reservation_count=f.integer(),dirty_count=f.integer(),store_count=f.integer();check(error<=3&&n<=256&&b<=256&&p<=128&&binding_count<=256&&source<=2&&reservation_count<=p&&dirty_count<=reservation_count&&store_count<=36865,"Record counts");check(length==40+3*arena_bytes+binding_count*4100+(reservation_count+dirty_count)*8+store_count*12,"Exact record length");
        const auto before=f.arenas(),middle=f.arenas(),after=f.arenas();std::vector<std::pair<std::uint32_t,std::uint32_t>> reservations,dirty_stores;for(unsigned k=0;k<reservation_count;++k){const auto h=f.integer(),value=f.integer();reservations.emplace_back(h,value);}for(unsigned k=0;k<dirty_count;++k){const auto bank=f.integer(),value=f.integer();check(bank<64,"Trace dirty extent");dirty_stores.emplace_back(bank,value);}
        std::vector<std::array<std::uint32_t,3>> observations;for(unsigned k=0;k<store_count;++k){const auto address=f.integer(),size=f.integer(),value=f.integer();check(size==4,"Original write width");observations.push_back({address,size,value});}
        check(word(before[0],0x220)==n&&word(before[0],0xbc4)==p,"Source active counts");Input input(before,n,b,p,source,resources,seed);real_cases+=source!=0;const auto context_before=input.context;const auto dirty_before=input.dirty;const auto joints_before=input.joint_owner.joints;
        auto result=jp::prepare(input.model,input.resource,input.context,input.manager);
        check(input.joint_owner.joints==joints_before,"Complete borrowed canonical AoS joints remain unchanged");input.verify_borrowed_tables(before);
        if(error==1||error==2){const auto* e=std::get_if<jp::Error>(&result);check(e&&e->code==jp::ErrorCode::arithmetic&&e->arithmetic&&e->arithmetic->code==(error==1?math::ErrorCode::numeric_overflow:math::ErrorCode::nonfinite_operand),"Consumed arithmetic outside finite profile");check(input.context==context_before&&input.dirty==dirty_before,"No partial preparation published");for(unsigned j=0;j<binding_count;++j){f.integer();f.take(4096);}check(f.p==record.size(),"Rejected record fully parsed");++numeric_errors;continue;}
        const auto* prepared=std::get_if<jp::Prepared>(&result);if(!prepared)throw std::runtime_error("Prepare: "+std::string(std::get<jp::Error>(result).message));
        check(prepared->reservations.size()==reservations.size()&&prepared->dirty_writes.size()==dirty_stores.size(),"Original helper/store counts");
        for(unsigned k=0;k<reservations.size();++k){const auto [handle,pointer]=reservations[k];const auto& got=prepared->reservations[k];check(got.handle==handle,"Original helper input order");const auto slot=(handle>>12)&4095,shift=slot&255,mask=shift<32?std::uint32_t(1)<<shift:0;check(got.bank==(handle&4095)&&got.slot==slot&&got.mask==mask,"Handle fields and literal ARM low8 shift contract");const auto expected=handle==0xffffffffU?jp::ReservationKind::invalid_handle:!word(before[7],0x32c)?jp::ReservationKind::absent_buffer:pointer?jp::ReservationKind::writable:jp::ReservationKind::already_marked;check(got.kind==expected&&got.destination.has_value()==bool(pointer),"Original helper return presence/kind");if(pointer)check(input.pointers.encode(*got.destination)==pointer,"Original helper destination identity");}
        for(unsigned k=0;k<dirty_stores.size();++k)check(prepared->dirty_writes[k].bank==dirty_stores[k].first&&prepared->dirty_writes[k].value==dirty_stores[k].second,"Every original dirty store in order, including mask zero");
        auto actual=input.apply(before,*prepared,observations);same(actual,middle,"After959d00");arm_arena_bytes+=arena_bytes;++preparations;
        check(input.context==context_before&&input.dirty==dirty_before,"Preparation borrows unchanged state");auto untouched=before;input.encode_context(untouched[1],input.context);same(untouched,before,"Borrowed projection");immutability_bytes+=arena_bytes;
        auto context=prepared->context;auto manager=input.manager;std::vector<std::uint32_t> dirty=input.dirty;for(const auto& w:prepared->dirty_writes)dirty.at(w.bank)=w.value;manager.dirty_words=dirty;
        for(unsigned j=0;j<binding_count;++j){const auto handle=f.integer();const auto expected=f.take(4096);const auto old_context=context;const auto bound=jp::bind(context,manager,handle);
            if(error==3){const auto* e=std::get_if<jp::Error>(&bound);check(e&&e->code==jp::ErrorCode::buffer_extent,"Out-of-owned-capacity ARM pointer is explicit native profile rejection");check(context==old_context,"Binding rejection publishes no state");++binding_errors;continue;}
            const auto* out=std::get_if<jp::Context>(&bound);if(!out)throw std::runtime_error("Bind: "+std::string(std::get<jp::Error>(bound).message));check(context==old_context,"Binding borrows unchanged context");context=*out;input.encode_context(actual[1],context);same(actual[1],expected,"After6bdc44 step"+std::to_string(j));binding_context_bytes+=4096;++bindings;
        }
        check(f.p==record.size(),"No trailing record");if(error!=3){same(actual,after,"Complete chain");arm_arena_bytes+=arena_bytes;++successful;}
        if(!negative_done&&n==1&&p==2&&!source){boundaries(input);negative_done=true;}
    }catch(const std::exception& e){throw std::runtime_error("Case "+std::to_string(i)+": "+e.what());}}
    check(r.p==bytes.size()&&negative_done,"Complete fixture and boundaries");std::cout<<"{\"status\":\"passed\",\"cases\":"<<count<<",\"bit_exact_chains\":"<<successful<<",\"preparations\":"<<preparations<<",\"bindings\":"<<bindings<<",\"numeric_rejections\":"<<numeric_errors<<",\"binding_extent_rejections\":"<<binding_errors<<",\"real_mod_cases\":"<<real_cases<<",\"aos_owner_cases\":"<<aos_cases<<",\"aos_owner_joints\":"<<aos_joints<<",\"checks\":"<<checks<<",\"arm_full_arena_bytes\":"<<arm_arena_bytes<<",\"binding_context_bytes\":"<<binding_context_bytes<<",\"borrowed_projection_bytes\":"<<immutability_bytes<<",\"borrowed_table_bytes\":"<<borrowed_table_bytes<<",\"palette_patch_bytes\":"<<palette_patch_bytes<<",\"observed_patch_store_bytes\":"<<observed_patch_store_bytes<<",\"command_patch_bytes\":"<<command_patch_bytes<<"}\n";
}
}
int main(int argc,char** argv){try{if(argc!=5)throw std::runtime_error("Usage: joint-palette-resource-tests oracle.bin sha256 em062.mod body001.mod");std::ifstream stream(argv[1],std::ios::binary|std::ios::ate);check(bool(stream),"Fixture open");const auto size=stream.tellg();check(size>=0&&size<=160*1024*1024,"Fixture allocation bound");stream.seekg(0);Bytes bytes(static_cast<std::size_t>(size));stream.read(reinterpret_cast<char*>(bytes.data()),size);check(bool(stream),"Fixture read");check(picosha2::hash256_hex_string(bytes)==argv[2],"Fixture SHA256 mismatch");oracle(bytes,{argv[3],argv[4]});return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
