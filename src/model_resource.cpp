#include "model_resource.hpp"
#include "picosha2.h"
#include <algorithm>
#include <atomic>
#include <bit>
#include <fstream>
#include <limits>
#include <new>
#include <stdexcept>

namespace mh4u::model_resource {
namespace {
struct Failure {Error error;};
[[noreturn]]void fail(ErrorCode code,std::string message,std::size_t p=0){throw Failure{{code,std::move(message),p}};}
void expected(std::string_view hash){
    if(hash.size()!=64||!std::all_of(hash.begin(),hash.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))
        fail(ErrorCode::invalid_expected,"Expected SHA256 must have 64 lowercase hexadecimal digits");
}
void budget(std::uintmax_t size,const Limits& limits){
    if(size>limits.bytes)fail(ErrorCode::resource_limit,"MOD exceeds selected byte budget");
    if(size>std::numeric_limits<std::uint32_t>::max())fail(ErrorCode::unsupported_layout,"MOD exceeds observed 32-bit file-offset domain");
}
struct Reader {
    std::string_view bytes;
    void region(std::uint64_t p,std::uint64_t n)const{
        if(p>bytes.size()||n>bytes.size()-p)fail(ErrorCode::malformed_resource,"MOD table exceeds verified bytes",static_cast<std::size_t>(p));
    }
    std::uint8_t u8(std::size_t p)const{region(p,1);return static_cast<unsigned char>(bytes[p]);}
    std::uint16_t u16(std::size_t p)const{region(p,2);return std::uint16_t(u8(p)|(std::uint16_t(u8(p+1))<<8));}
    std::uint32_t u32(std::size_t p)const{region(p,4);std::uint32_t v=0;for(unsigned k=0;k<4;++k)v|=std::uint32_t(u8(p+k))<<(8*k);return v;}
    std::span<const std::byte> span(std::size_t p,std::size_t n)const{region(p,n);return{reinterpret_cast<const std::byte*>(bytes.data()+p),n};}
};
// Exact adjacent-record key tested by loader680ae0..680bd4 before input-layout hash
// relocation. The group bytes themselves and unrelated flag bits are excluded.
bool same_group(const Primitive& a,const Primitive& b){
    return a.input_layout_hash==b.input_layout_hash&&a.field10==b.field10&&
        a.material_index==b.material_index&&((a.format8>>24)&63)==((b.format8>>24)&63)&&
        a.palette_index==b.palette_index&&((a.format8>>16)&255)==((b.format8>>16)&255)&&
        (a.flags0&0x104U)==(b.flags0&0x104U);
}
struct GeometryData {
    Geometry facts;
    std::vector<Vertex> vertices;
    std::vector<std::uint16_t> indices;
    std::vector<Triangle> triangles;
};
struct GeometryBudget {std::size_t vertices{},indices{},triangles{};};
void geometry_budget(std::size_t used,std::uint64_t count,std::size_t limit,std::size_t offset){
    if(used>limit||count>limit-used)fail(ErrorCode::resource_limit,"Cumulative geometry budget exceeded",offset);
}
template<class Emit>void strip(const Reader& in,std::size_t begin,std::size_t count,std::uint32_t start,
        std::uint16_t vertices,Emit emit,std::size_t& restarts,std::size_t& degenerates){
    std::array<std::uint32_t,2> window{};unsigned filled=0;bool odd=false;
    restarts=degenerates=0;
    for(std::size_t i=0;i<count;++i){
        const auto raw=in.u16(begin+2*i);
        if(raw==65535){filled=0;odd=false;++restarts;continue;}
        if(raw<start||std::uint64_t(raw)-start>=vertices)
            fail(ErrorCode::malformed_resource,"Strip index outside selected vertex span",begin+2*i);
        const auto index=static_cast<std::uint32_t>(raw-start);
        if(filled<2){window[filled++]=index;continue;}
        Triangle face=odd?Triangle{window[0],index,window[1]}:Triangle{window[0],window[1],index};
        odd=!odd;window={window[1],index};
        if(face[0]==face[1]||face[0]==face[2]||face[1]==face[2])++degenerates;
        else emit(face);
    }
}
Result<GeometryData> read_geometry(const Reader& in,const Primitive& primitive,std::uint32_t ordinal,
        GeometryBudget& budget,const Limits& limits){
    try{
        const auto p=primitive.source_offset;const auto stride=in.u8(p+10);const auto type=in.u8(p+11)&63;
        VertexLayout layout;std::uint8_t influences;
        if(primitive.input_layout_hash==0xd6784014U&&stride==28){layout=VertexLayout::obj2wt;influences=2;}
        else if(primitive.input_layout_hash==0x01b36016U&&stride==36){layout=VertexLayout::obj4wt;influences=4;}
        else if(primitive.input_layout_hash==0xf606f017U&&stride==36){layout=VertexLayout::obj4wt_t;influences=4;}
        else fail(ErrorCode::unsupported_layout,"Geometry input layout/stride not supported",p+20);
        if(type!=4)fail(ErrorCode::unsupported_layout,"Geometry source type must be4",p+11);
        const auto n=in.u16(p+2);const auto start=in.u32(p+12);
        const std::uint64_t vb=in.u32(0x38),vb_size=in.u32(0x18);
        const std::uint64_t relative=std::uint64_t(start)*stride+in.u32(p+16),vertex_bytes=std::uint64_t(n)*stride;
        in.region(vb,vb_size);
        if(relative>vb_size||vertex_bytes>vb_size-relative)
            fail(ErrorCode::malformed_resource,"Primitive vertices escape declared vertex buffer",p+12);
        const std::uint64_t vertices=vb+relative;
        const std::uint64_t indices=std::uint64_t(in.u32(0x3c))+2ULL*in.u32(p+24),count=std::uint64_t(in.u32(p+28))+1;
        geometry_budget(budget.vertices,n,limits.geometry_vertices,p+2);
        geometry_budget(budget.indices,count,limits.geometry_indices,p+28);
        in.region(indices,count*2);
        // Charge bounded admitted work even if a later index/triangle rejects.
        // Invalid/unsupported extents consume no geometry allocation or scan.
        budget.vertices+=n;budget.indices+=static_cast<std::size_t>(count);
        std::size_t restarts=0,degenerates=0,triangles=0;
        strip(in,static_cast<std::size_t>(indices),static_cast<std::size_t>(count),start,n,
            [&](const Triangle&){++triangles;},restarts,degenerates);
        geometry_budget(budget.triangles,triangles,limits.geometry_triangles,p+28);budget.triangles+=triangles;
        GeometryData out;
        out.facts.primitive_index=ordinal;out.facts.vertex_source_offset=static_cast<std::uint32_t>(vertices);
        out.facts.index_source_offset=static_cast<std::uint32_t>(indices);out.facts.vertex_start=start;
        out.facts.stride=stride;out.facts.layout=layout;out.facts.restart_count=restarts;out.facts.degenerate_triangles=degenerates;
        out.vertices.resize(n);out.indices.reserve(static_cast<std::size_t>(count));out.triangles.reserve(triangles);
        for(std::size_t j=0;j<n;++j){
            const auto offset=static_cast<std::size_t>(vertices)+j*stride;auto& v=out.vertices[j];
            for(unsigned k=0;k<3;++k)v.position_bits[k]=in.u32(offset+4*k);
            for(unsigned k=0;k<2;++k)v.uv_bits[k]=in.u32(offset+16+4*k);
            for(unsigned k=0;k<4;++k)v.normal[k]=std::bit_cast<std::int8_t>(in.u8(offset+12+k));
            v.influence_count=influences;v.joints[0]=in.u8(offset+24);v.joints[1]=in.u8(offset+25);
            v.weights[0]=in.u8(offset+26);v.weights[1]=in.u8(offset+27);
            if(influences==4){
                std::array<std::int8_t,4> aux;for(unsigned k=0;k<4;++k)aux[k]=std::bit_cast<std::int8_t>(in.u8(offset+28+k));v.auxiliary=aux;
                v.joints[2]=in.u8(offset+32);v.joints[3]=in.u8(offset+33);v.weights[2]=in.u8(offset+34);v.weights[3]=in.u8(offset+35);
            }
        }
        for(std::size_t j=0;j<count;++j)out.indices.push_back(in.u16(static_cast<std::size_t>(indices)+2*j));
        strip(in,static_cast<std::size_t>(indices),static_cast<std::size_t>(count),start,n,
            [&](const Triangle& triangle){out.triangles.push_back(triangle);},restarts,degenerates);
        return out;
    }catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate primitive geometry",primitive.source_offset};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Primitive geometry exceeds host capacity",primitive.source_offset};}
}
std::uint64_t acquire_identity(){
    static std::atomic<std::uint64_t> next{1};auto value=next.load(std::memory_order_relaxed);
    for(;;){
        if(!value)fail(ErrorCode::identity_exhausted,"MOD snapshot identities exhausted");
        if(next.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
    }
}
}
struct Resource::Data {
    std::string bytes,digest;
    std::uint64_t identity{};
    std::uint16_t joint_count{};
    std::uint32_t palette_count{},skeleton_offset{},primitive_offset{};
    std::vector<Matrix> local,source;
    std::vector<JointRecord> joint_records_parsed;
    std::span<const std::byte> joint_records,remap;
    std::vector<Palette> palettes;
    std::vector<Primitive> primitives;
    std::vector<Result<GeometryData>> geometries;
};
Result<Resource> Resource::decode_owned(std::string bytes,std::string_view hash,Limits limits){
    try{
        expected(hash);budget(bytes.size(),limits);
        if(picosha2::hash256_hex_string(bytes)!=hash)fail(ErrorCode::hash_mismatch,"MOD bytes differ from expected SHA256");
        Reader in{bytes};in.region(0,128);
        if(in.u32(0)!=0x00444f4dU||in.u16(4)!=230)fail(ErrorCode::unsupported_format,"Only little-endian MOD230 is supported");
        const auto n=in.u16(6),primitives=in.u16(8);const auto palettes=in.u32(0x24);
        if(n>limits.joints)fail(ErrorCode::resource_limit,"Joint table exceeds policy",6);
        if(palettes>limits.palettes)fail(ErrorCode::resource_limit,"Declared palette allocation exceeds policy",0x24);
        if(primitives>limits.primitives)fail(ErrorCode::resource_limit,"Primitive table exceeds policy",8);
        auto data=std::make_shared<Data>();data->bytes=std::move(bytes);data->digest=std::string(hash);in.bytes=data->bytes;
        data->joint_count=n;data->palette_count=palettes;data->skeleton_offset=in.u32(0x28);data->primitive_offset=in.u32(0x34);
        if(n){
            const std::uint64_t s=data->skeleton_offset;
            if(s<128)fail(ErrorCode::malformed_resource,"Nonempty skeleton overlaps MOD header",static_cast<std::size_t>(s));
            in.region(s,std::uint64_t(n)*152+256+std::uint64_t(palettes)*28);
            data->joint_records=in.span(static_cast<std::size_t>(s),std::size_t(n)*24);
            data->remap=in.span(static_cast<std::size_t>(s)+std::size_t(n)*152,256);
            data->local.resize(n);data->source.resize(n);data->palettes.reserve(palettes);data->joint_records_parsed.resize(n);
            for(std::size_t j=0;j<n;++j){
                for(unsigned k=0;k<4;++k)data->joint_records_parsed[j].fields[k]=in.u8(static_cast<std::size_t>(s)+24*j+k);
                for(unsigned k=0;k<5;++k)data->joint_records_parsed[j].parameter_bits[k]=in.u32(static_cast<std::size_t>(s)+24*j+4+4*k);
            }
            for(std::size_t j=0;j<n;++j)for(unsigned k=0;k<16;++k){
                data->local[j][k]=in.u32(static_cast<std::size_t>(s)+24U*n+64*j+4*k);
                data->source[j][k]=in.u32(static_cast<std::size_t>(s)+88U*n+64*j+4*k);
            }
            for(std::size_t i=0;i<palettes;++i){
                const auto p=static_cast<std::size_t>(s)+std::size_t(n)*152+256+i*28;
                const auto word=in.u32(p);const auto count=word&65535U;
                if(count>24)fail(ErrorCode::unsupported_layout,"Palette count exceeds its 28-byte record",p);
                // Preserve invalid joint references as source facts. The numerical
                // consumer checks only indices of palettes actually being used.
                data->palettes.push_back({word,static_cast<std::uint32_t>(p),
                    {reinterpret_cast<const std::uint8_t*>(data->bytes.data()+p+4),count}});
            }
        }
        if(primitives){
            const auto p=data->primitive_offset;
            if(p<128)fail(ErrorCode::malformed_resource,"Nonempty primitives overlap MOD header",p);
            in.region(p,std::uint64_t(primitives)*48);data->primitives.reserve(primitives);
            for(std::size_t i=0;i<primitives;++i){
                const auto q=std::size_t(p)+i*48;const auto flags4=in.u32(q+4);
                Primitive primitive{static_cast<std::uint32_t>(q),in.u32(q),flags4,in.u32(q+8),in.u32(q+0x10),in.u32(q+0x14),
                    in.u16(q+0x26),0,static_cast<std::uint16_t>(flags4&4095),static_cast<std::uint16_t>((flags4>>12)&4095),in.u8(q+0x25)};
                primitive.loaded_group=data->primitives.empty()?1:static_cast<std::uint16_t>(data->primitives.back().loaded_group+
                    (same_group(data->primitives.back(),primitive)?0:1));
                data->primitives.push_back(primitive);
            }
        }
        GeometryBudget geometry_budget;
        data->geometries.reserve(data->primitives.size());
        for(std::size_t i=0;i<data->primitives.size();++i)
            data->geometries.push_back(read_geometry(in,data->primitives[i],static_cast<std::uint32_t>(i),geometry_budget,limits));
        data->identity=acquire_identity();return Resource{std::move(data)};
    }catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate bounded MOD snapshot",0};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"MOD snapshot exceeds host capacity",0};}
}
Result<Resource> Resource::decode(std::string_view bytes,std::string_view hash,Limits limits){
    try{expected(hash);budget(bytes.size(),limits);return decode_owned(std::string(bytes),hash,limits);}
    catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate MOD input snapshot",0};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"MOD input exceeds host capacity",0};}
}
Result<Resource> Resource::load(const std::filesystem::path& path,std::string_view hash,Limits limits){
    try{
        expected(hash);std::ifstream file(path,std::ios::binary|std::ios::ate);
        if(!file)fail(ErrorCode::io,"Cannot open selected MOD file");const auto n=file.tellg();
        if(n<0)fail(ErrorCode::io,"Cannot measure selected MOD file");budget(static_cast<std::uintmax_t>(n),limits);
        if(static_cast<std::uintmax_t>(n)>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))fail(ErrorCode::resource_limit,"MOD exceeds stream capacity");
        std::string bytes(static_cast<std::size_t>(n),'\0');file.seekg(0);
        if(!file.read(bytes.data(),static_cast<std::streamsize>(bytes.size()))||file.peek()!=std::char_traits<char>::eof())fail(ErrorCode::io,"MOD read failed or size changed");
        return decode_owned(std::move(bytes),hash,limits);
    }catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Cannot allocate MOD file snapshot",0};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"MOD file exceeds host capacity",0};}
}
std::string_view Resource::sha256()const noexcept{return data_->digest;}
std::uint64_t Resource::identity()const noexcept{return data_->identity;}
std::span<const std::byte> Resource::bytes()const noexcept{return{reinterpret_cast<const std::byte*>(data_->bytes.data()),data_->bytes.size()};}
std::uint16_t Resource::joint_count()const noexcept{return data_->joint_count;}
std::uint32_t Resource::declared_palette_count()const noexcept{return data_->palette_count;}
std::uint32_t Resource::skeleton_source_offset()const noexcept{return data_->skeleton_offset;}
std::uint32_t Resource::primitives_source_offset()const noexcept{return data_->primitive_offset;}
std::optional<Skeleton> Resource::skeleton()const noexcept{
    if(!data_->joint_count)return std::nullopt;
    return Skeleton{data_->local,data_->source,data_->joint_records,data_->remap,data_->palettes,data_->joint_records_parsed};
}
std::span<const Primitive> Resource::primitives()const noexcept{return data_->primitives;}
Result<std::span<const std::byte>> Resource::primitive_source_record(std::size_t ordinal)const{
    if(ordinal>=data_->primitives.size())return Error{ErrorCode::table_extent,"Primitive ordinal outside table",data_->primitive_offset};
    return bytes().subspan(data_->primitives[ordinal].source_offset,48);
}
Result<Geometry> Resource::geometry(std::size_t ordinal)const{
    if(ordinal>=data_->geometries.size())return Error{ErrorCode::table_extent,"Geometry primitive ordinal outside table",data_->primitive_offset};
    const auto& parsed=data_->geometries[ordinal];
    if(const auto* error=std::get_if<Error>(&parsed))return *error;
    const auto& value=std::get<GeometryData>(parsed);auto view=value.facts;
    view.resource_identity=data_->identity;view.vertices=value.vertices;view.raw_indices=value.indices;view.triangles=value.triangles;
    return view;
}
} // namespace mh4u::model_resource
