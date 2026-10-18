#include "joint_palette.hpp"
#include <bitset>
#include <limits>
#include <new>
#include <stdexcept>

namespace mh4u::joint_palette {
namespace {
namespace math=motion_transform_math;
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view message,std::size_t palette=0,std::size_t entry=0){
    throw Failure{{code,message,palette,entry,std::nullopt}};
}
template<class T,class F>Result<T> guarded(F fn){
    try{return fn();}catch(const Failure& f){return f.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Palette allocation failed",0,0,std::nullopt};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"Palette capacity exceeded",0,0,std::nullopt};}
}
const Manager& manager(const ManagerReference& ref,std::size_t palette=0){
    if(std::holds_alternative<Unknown>(ref))fail(ErrorCode::unknown_manager,"Manager identity is unknown",palette);
    if(std::holds_alternative<Null>(ref))fail(ErrorCode::null_manager,"Consumed manager is null",palette);
    return std::get<Manager>(ref);
}
void region(const Region& r,const Limits& limits,std::size_t palette=0){
    if(!r.base.storage)fail(ErrorCode::invalid_storage,"Buffer needs a nonzero registered storage token",palette);
    if(r.extent>limits.buffer_bytes)fail(ErrorCode::resource_limit,"Buffer extent exceeds caller policy",palette);
    if(std::uint64_t(r.base.offset)+r.extent>std::numeric_limits<std::uint32_t>::max())
        fail(ErrorCode::address_overflow,"Registered region exceeds the admitted nonwrapping offset domain",palette);
}
Location at(const Region& r,std::uint64_t relative,std::uint64_t bytes,const Limits& limits,std::size_t palette=0){
    region(r,limits,palette);
    if(relative>std::numeric_limits<std::uint32_t>::max()||relative+std::uint64_t(r.base.offset)>std::numeric_limits<std::uint32_t>::max())
        fail(ErrorCode::address_overflow,"Buffer address calculation exceeds u32 without wrap",palette);
    if(relative>r.extent||bytes>r.extent-relative)fail(ErrorCode::buffer_extent,"Calculated buffer range exceeds owned region",palette);
    return {r.base.storage,static_cast<std::uint32_t>(r.base.offset+relative)};
}
Location destination(const Manager& m,const Region& r,std::uint32_t handle,const Limits& limits,std::size_t palette=0){
    if(!m.stride)fail(ErrorCode::unknown_stride,"Consumed bank stride is unknown",palette);
    const auto bank=handle&0xfffU,slot=(handle>>12)&0xfffU;
    const std::uint64_t offset=std::uint64_t(*m.stride)*bank+48ULL*slot;
    return at(r,offset,0,limits,palette);
}
Generated generate_checked(std::span<const Matrix> source,JointMatrices joints,
    std::span<const std::uint8_t> indices,const Bits3& offset,InputIdentity identity,Limits limits){
    if(indices.size()>limits.indices_per_palette||indices.size()>limits.total_indices||indices.size()>65535U||
       source.size()>limits.matrices||joints.size()>limits.matrices)
        fail(ErrorCode::resource_limit,"Numerical palette inputs exceed policy/source count domain",identity.palette);
    Generated out{identity,{indices.begin(),indices.end()},{}};out.values.reserve(indices.size());
    for(std::size_t i=0;i<indices.size();++i){const auto index=indices[i];
        if(index>=source.size())fail(ErrorCode::source_extent,"Palette index exceeds source matrix table",identity.palette,i);
        if(index>=joints.size())fail(ErrorCode::joint_extent,"Palette index exceeds joint matrix table",identity.palette,i);
        auto result=math::palette_product(source[index],joints[index],offset);
        if(const auto* e=std::get_if<math::Error>(&result))throw Failure{{ErrorCode::arithmetic,"Palette arithmetic failed",identity.palette,i,*e}};
        out.values.push_back(std::get<Bits12>(result));
    }
    return out;
}
void command(Prepared& out,const ModelView& model,const Limits& limits){
    auto& c=out.context;
    if(((c.flags1bc>>16)&0x813U)!=0)fail(ErrorCode::unsupported_material,"1a5c10 material path is outside profile");
    if(!c.descriptor_word)fail(ErrorCode::unknown_descriptor,"Command descriptor word is unknown");
    if(std::holds_alternative<Unknown>(c.command))fail(ErrorCode::unknown_command,"Command buffer identity is unknown");
    if(std::holds_alternative<Null>(c.command))fail(ErrorCode::null_command,"Null command pointer is outside owned-buffer profile");
    auto& stream=std::get<CommandCursor>(c.command);
    region(stream.buffer,limits);
    const auto size=(((*c.descriptor_word&0xffffU)*4U)+15U)&~15U;
    if(stream.cursor>stream.end||stream.end>stream.buffer.extent)fail(ErrorCode::buffer_extent,"Command cursor/end exceed owned region");
    const std::uint64_t next=std::uint64_t(stream.cursor)+size;
    if(next>std::numeric_limits<std::uint32_t>::max()||next+stream.buffer.base.offset>std::numeric_limits<std::uint32_t>::max())
        fail(ErrorCode::address_overflow,"Command cursor addition exceeds nonwrapping domain");
    if(next>stream.end)fail(ErrorCode::allocation_required,"Original command growth path would be consumed");
    const auto where=at(stream.buffer,stream.cursor,4,limits);
    if(where.offset&3U)fail(ErrorCode::invalid_alignment,"Command word requires four-byte alignment");
    if(limits.patches==0)fail(ErrorCode::resource_limit,"Command patch exceeds work policy");
    out.writes.push_back({where,{model.word154},WriteKind::model_word154,std::nullopt,{}});
    c.command638=where;stream.cursor=static_cast<std::uint32_t>(next);
    c.flags188|=1U;c.flags4c&=~2U;
}
void bind_one(Binding& binding,Location desired,std::uint8_t tag,std::uint32_t& flags){
    if(std::holds_alternative<Unknown>(binding))fail(ErrorCode::unknown_binding,"Previous binding address is unknown");
    if(const auto* previous=std::get_if<TaggedLocation>(&binding)){
        if(!previous->address.storage||(previous->address.offset&15U)||previous->low_bits>15U)
            fail(ErrorCode::invalid_binding,"Previous tagged binding is not canonical");
        if(previous->address==desired)return; // Source compares address only; preserves old low bits.
    }
    binding=TaggedLocation{{desired.storage,desired.offset&~15U},static_cast<std::uint8_t>((desired.offset&15U)|tag)};
    flags|=1U;
}
}
Result<Generated> generate(std::span<const Matrix> source,JointMatrices joints,
    std::span<const std::uint8_t> indices,const Bits3& offset,InputIdentity identity,Limits limits){
    return guarded<Generated>([&]{return generate_checked(source,joints,indices,offset,identity,limits);});
}
Result<Prepared> prepare(const ModelView& model,const ResourceReference& resource,
    const Context& context,const ManagerReference& manager_ref,Limits limits){
    return guarded<Prepared>([&]{
        Prepared out{context,model.flags104,{},{},{}};
        command(out,model,limits); // Original before joint-count and resource gates.
        if(model.joint_count==0)return out;
        if(model.joint_count>limits.matrices)fail(ErrorCode::resource_limit,"Model joint count exceeds caller policy");
        if(std::holds_alternative<Unknown>(resource))fail(ErrorCode::unknown_resource,"MOD resource identity is unknown");
        if(std::holds_alternative<Null>(resource))fail(ErrorCode::null_resource,"Consumed MOD resource is null");
        const auto& r=std::get<ResourceView>(resource);
        if(!r.auxiliary214)fail(ErrorCode::unknown_auxiliary,"Resource auxiliary flag is unknown");
        if(*r.auxiliary214)fail(ErrorCode::unsupported_auxiliary,"Resource auxiliary command path is outside profile");
        if(out.model_flags104&63U)out.model_flags104=(out.model_flags104&~63U)|1U;
        if(!model.parent_sharing)fail(ErrorCode::unknown_parent_sharing,"Parent-sharing byte is unknown");
        if(*model.parent_sharing){
            if(std::holds_alternative<Unknown>(model.parent))fail(ErrorCode::unknown_parent,"Consumed parent identity is unknown");
            if(std::holds_alternative<Present>(model.parent))fail(ErrorCode::unsupported_parent,"Parent type/shared palette path is outside profile");
        }
        if(!model.history_selector)fail(ErrorCode::unknown_history,"History selector is unknown");
        if((*model.history_selector&0x80000000U)==0)fail(ErrorCode::unsupported_history,"Nonnegative history selector is outside profile");
        if(model.palette_count>limits.palettes)fail(ErrorCode::resource_limit,"Palette count exceeds work policy");
        if(model.palette_count>model.handles.size())fail(ErrorCode::handle_extent,"Handle table does not cover model palette count");
        std::array<std::uint32_t,4096> dirty_cache{};std::bitset<4096> seen;
        std::size_t total_indices=0;
        for(std::uint32_t p=0;p<model.palette_count;++p){
            const auto handle=model.handles[p];const auto bank=handle&0xfffU,slot=(handle>>12)&0xfffU;
            const auto shift=slot&0xffU,mask=shift<32?(1U<<shift):0U;
            Reservation reservation{handle,bank,slot,mask,ReservationKind::invalid_handle,std::nullopt};
            if(handle==0xffffffffU){out.reservations.push_back(reservation);continue;}
            const auto& m=manager(manager_ref,p);
            if(std::holds_alternative<Unknown>(m.normal))fail(ErrorCode::unknown_buffer,"Normal buffer presence is unknown",p);
            if(std::holds_alternative<Null>(m.normal)){reservation.kind=ReservationKind::absent_buffer;out.reservations.push_back(reservation);continue;}
            if(bank>=limits.dirty_words)fail(ErrorCode::resource_limit,"Dirty bank exceeds caller policy",p);
            if(bank>=m.dirty_words.size())fail(ErrorCode::dirty_extent,"Dirty table does not cover bank",p);
            const auto previous=seen[bank]?dirty_cache[bank]:m.dirty_words[bank];
            if(previous&mask){reservation.kind=ReservationKind::already_marked;out.reservations.push_back(reservation);continue;}
            if(!m.local_lock||!m.global_lock)fail(ErrorCode::unknown_lock,"Consumed lock bytes are unknown",p);
            if(*m.local_lock||*m.global_lock)fail(ErrorCode::unsupported_lock,"Original locking path is outside profile",p);
            const auto value=previous|mask;seen[bank]=true;dirty_cache[bank]=value;
            out.dirty_writes.push_back({bank,value});
            const auto& normal=std::get<Region>(m.normal);const auto address=destination(m,normal,handle,limits,p);
            reservation.kind=ReservationKind::writable;reservation.destination=address;out.reservations.push_back(reservation);
            if(p>=r.palettes.size())fail(ErrorCode::palette_extent,"Consumed palette record is absent",p);
            const auto indices=r.palettes[p].indices;
            if(indices.size()>limits.indices_per_palette||indices.size()>65535U||
               total_indices>limits.total_indices||indices.size()>limits.total_indices-total_indices)
                fail(ErrorCode::resource_limit,"Palette indices exceed cumulative work policy",p);
            total_indices+=indices.size();
            const auto bytes=std::uint64_t(indices.size())*48U;
            at(normal,std::uint64_t(address.offset)-normal.base.offset,bytes,limits,p);
            if(bytes&&(address.offset&3U))fail(ErrorCode::invalid_alignment,"Palette word writes require four-byte alignment",p);
            if(!context.offset&& !indices.empty())fail(ErrorCode::unknown_context_offset,"Consumed context vector is unknown",p);
            if(out.writes.size()>=limits.patches)fail(ErrorCode::resource_limit,"Ordered patches exceed work policy",p);
            // Empty palettes consume no numerical fields; no fabricated context is observed.
            const Bits3 unused{};const auto& offset=context.offset?*context.offset:unused;
            auto values=generate_checked(r.source_matrices,model.joint_matrices,indices,offset,
                {r.identity,model.pose_identity,context.identity,p},limits);
            Write write{address,{},WriteKind::palette,values.identity,std::move(values.source_indices)};
            if(values.values.size()>std::numeric_limits<std::size_t>::max()/12U)fail(ErrorCode::resource_limit,"Palette representation would overflow",p);
            write.words.reserve(values.values.size()*12U);
            for(const auto& matrix:values.values)write.words.insert(write.words.end(),matrix.begin(),matrix.end());
            out.writes.push_back(std::move(write));
        }
        return out;
    });
}
Result<Context> bind(const Context& context,const ManagerReference& manager_ref,std::uint32_t handle,Limits limits){
    return guarded<Context>([&]{
        Context out=context;const auto& m=manager(manager_ref);
        if(std::holds_alternative<Unknown>(m.normal))fail(ErrorCode::unknown_buffer,"Normal buffer presence is unknown");
        if(const auto* normal=std::get_if<Region>(&m.normal)){
            const auto address=destination(m,*normal,handle,limits);
            const auto count=(handle>>24)+1U;const auto tag=count/2U>15U?0U:count/2U;
            bind_one(out.bindings[0],address,static_cast<std::uint8_t>(tag),out.flags188);
            bind_one(out.bindings[1],address,static_cast<std::uint8_t>(tag),out.flags188);
        }
        if(std::holds_alternative<Unknown>(m.history))fail(ErrorCode::unknown_buffer,"History buffer presence is unknown");
        if(std::holds_alternative<Region>(m.history))fail(ErrorCode::unsupported_history,"History-buffer binding path is outside profile");
        return out;
    });
}
} // namespace mh4u::joint_palette
