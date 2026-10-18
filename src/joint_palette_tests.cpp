#include "joint_palette.hpp"
#include <algorithm>
#include <bit>
#include <cfenv>
#include <iostream>
#include <stdexcept>
#include <string>
#if defined(__SSE__)
#include <xmmintrin.h>
#endif
namespace p=mh4u::joint_palette;
namespace math=mh4u::motion_transform_math;
namespace {
std::size_t checks;
void check(bool value,const char* text){++checks;if(!value)throw std::runtime_error(text);}
std::uint32_t b(float value){return std::bit_cast<std::uint32_t>(value);}
p::Matrix identity(){return {b(1),0,0,0,0,b(1),0,0,0,0,b(1),0,0,0,0,b(1)};}
template<class T>T ok(p::Result<T> result){if(auto* e=std::get_if<p::Error>(&result))throw std::runtime_error(std::string(e->message));++checks;return std::get<T>(std::move(result));}
template<class T>p::Error bad(p::Result<T> result,p::ErrorCode code){check(std::holds_alternative<p::Error>(result),"expected rejection");auto e=std::get<p::Error>(result);check(e.code==code,"wrong rejection");return e;}
struct Fixture {
    std::array<p::Matrix,2> source{identity(),identity()},world{identity(),identity()};
    std::array<std::uint8_t,2> indices{0,1};
    std::array<p::PaletteView,2> palettes{{{std::span<const std::uint8_t>(indices).first(1)},{indices}}};
    std::array<std::uint32_t,2> handles{0x01000000,0x02003001};
    std::array<std::uint32_t,4> dirty{};
    p::ModelView model{23,2,2,0x1234ffff,b(1),0xffffffffU,std::uint8_t(0),p::Unknown{},handles,p::JointMatrices{std::span<const p::Matrix>(world)}};
    p::ResourceView resource{17,std::uint8_t(0),source,palettes};
    p::Context context;
    p::Manager manager;
    Fixture(){
        context.identity=29;context.flags4c=0x8923;context.flags188=0x24;context.flags1bc=0;
        context.command=p::CommandCursor{{{1,0},4096},0,4096};context.command638=p::Unknown{};
        context.descriptor_word=4;context.offset=p::Bits3{b(2),b(3),b(4)};
        context.bindings={p::Null{},p::TaggedLocation{{7,16},9},p::Unknown{}};
        manager.normal=p::Region{{2,0},65536};manager.history=p::Null{};manager.stride=4096;
        manager.local_lock=0;manager.global_lock=0;manager.dirty_words=dirty;
    }
    p::Result<p::Prepared> prepare(p::Limits limits={})const{return p::prepare(model,resource,context,manager,limits);}
};
void numerical(){
    Fixture f;const p::InputIdentity origin{17,23,29,8};
    auto result=ok(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin));
    check(result.identity==origin&&result.source_indices==std::vector<std::uint8_t>({0,1}),"provenance and order retained");
    const p::Bits12 expected{b(1),0,0,b(2),0,b(1),0,b(3),0,0,b(1),b(4)};
    check(result.values.size()==2&&result.values[0]==expected&&result.values[1]==expected,"source3x4 transpose and offset");
    std::array<mh4u::joint_output::Joint,2> owned{};
    for(std::size_t i=0;i<owned.size();++i){owned[i].matrix=f.world[i];owned[i].depth=1234;}
    const auto saved=owned;
    const p::JointMatrices borrowed{std::span<const mh4u::joint_output::Joint>(owned)};
    check(borrowed.size()==owned.size()&&&borrowed[1]==&owned[1].matrix,"AoS matrices are borrowed from canonical owner");
    auto aos=ok(p::generate(f.source,borrowed,f.indices,*f.context.offset,origin));
    check(aos.values==result.values&&aos.source_indices==result.source_indices&&aos.identity==result.identity&&owned==saved,"AoS numerical output and owner immutability");
    auto before_plan=ok(f.prepare());f.model.joint_matrices=borrowed;auto aos_plan=ok(f.prepare());
    check(aos_plan.context==before_plan.context&&aos_plan.writes.size()==before_plan.writes.size(),"AoS preparation state matches matrix span");
    for(std::size_t i=0;i<aos_plan.writes.size();++i)check(aos_plan.writes[i].words==before_plan.writes[i].words&&aos_plan.writes[i].destination==before_plan.writes[i].destination,"AoS preparation patches match");
    check(owned==saved,"AoS entire joint state unchanged");
    owned[0].matrix[0]=b(2);check(borrowed[0][0]==b(2),"borrowed view observes owner storage without copy");
    f.model.joint_matrices=p::JointMatrices{std::span<const p::Matrix>(f.world)};

    f.world[0][0]=b(-2);f.world[0][5]=b(3);f.world[0][10]=b(4);
    f.world[0][12]=b(11);f.world[0][13]=b(12);f.world[0][14]=b(13);
    f.source[0][12]=b(5);f.source[0][13]=b(6);f.source[0][14]=b(7);
    result=ok(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},std::span(f.indices).first(1),*f.context.offset,origin));
    check(result.values[0][3]==b(3)&&result.values[0][7]==b(33)&&result.values[0][11]==b(45),"B thenW source order");
    for(unsigned i:{3U,7U,11U,15U})f.world[0][i]=0x7fc12345;
    auto ignored=ok(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},std::span(f.indices).first(1),*f.context.offset,origin));
    check(ignored.values==result.values,"W fourth column is not consumed");
    f.source[0][15]=0x7fc12345;
    auto e=bad(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin),p::ErrorCode::arithmetic);
    check(e.palette==8&&e.entry==0&&e.arithmetic->code==math::ErrorCode::nonfinite_operand,"consumed B error identity");
    f.indices[0]=2;bad(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin),p::ErrorCode::source_extent);
    f.indices[0]=1;bad(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world).first(1)},f.indices,*f.context.offset,origin),p::ErrorCode::joint_extent);
    auto lim=p::Limits{};lim.indices_per_palette=1;bad(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin,lim),p::ErrorCode::resource_limit);
    lim=p::Limits{};lim.total_indices=1;bad(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin,lim),p::ErrorCode::resource_limit);
    lim=p::Limits{};lim.matrices=1;bad(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin,lim),p::ErrorCode::resource_limit);
    f.indices[0]=0;f.source[0]=identity();f.world[0]=identity();f.source[0][0]=0x7f7fffff;f.world[0][0]=b(2);
    e=bad(p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin),p::ErrorCode::arithmetic);
    check(e.arithmetic->code==math::ErrorCode::numeric_overflow,"overflow is not silently accepted");
    const auto rounding=std::fegetround();check(std::fesetround(FE_UPWARD)==0,"change rounding");
    const auto empty=p::generate({}, {}, {}, {0x7fc12345,0x7f800000,0},origin);
    const auto arithmetic=p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin);
    check(std::fesetround(rounding)==0,"restore rounding");
    check(ok(empty).values.empty(),"empty palette consumes no FP environment");
    e=bad(arithmetic,p::ErrorCode::arithmetic);check(e.arithmetic->code==math::ErrorCode::floating_environment,"rounding rejected");
#if defined(__aarch64__)
    std::uint64_t original;asm volatile("mrs %0, fpcr":"=r"(original));const auto altered=original|(1ULL<<24);asm volatile("msr fpcr, %0"::"r"(altered));
    auto fz=p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin);asm volatile("msr fpcr, %0"::"r"(original));
    e=bad(fz,p::ErrorCode::arithmetic);check(e.arithmetic->code==math::ErrorCode::floating_environment,"actual FZ rejected");
#elif defined(__SSE__)
    const auto original=_mm_getcsr();_mm_setcsr(original|0x8040U);auto fz=p::generate(f.source,p::JointMatrices{std::span<const p::Matrix>(f.world)},f.indices,*f.context.offset,origin);_mm_setcsr(original);
    e=bad(fz,p::ErrorCode::arithmetic);check(e.arithmetic->code==math::ErrorCode::floating_environment,"actual FTZ/DAZ rejected");
#endif
}
void preparation(){
    Fixture f;const auto before=f.context;auto out=ok(f.prepare());
    check(f.context==before&&f.dirty==std::array<std::uint32_t,4>{},"prepare is immutable");
    check(out.model_flags104==0x1234ffc1&&out.context.flags4c==0x8921&&out.context.flags188==0x25,"source flags preserved and gated");
    check(std::get<p::CommandCursor>(out.context.command).cursor==16&&std::get<p::Location>(out.context.command638)==p::Location{1,0},"command cursor and identity");
    check(out.writes.size()==3&&out.writes[0].words==std::vector<std::uint32_t>{b(1)}&&out.writes[0].kind==p::WriteKind::model_word154,"command patch first");
    check(out.writes[1].input==p::InputIdentity{17,23,29,0}&&out.writes[2].source_indices==std::vector<std::uint8_t>({0,1}),"palette patch provenance");
    check(out.reservations.size()==2&&out.reservations[0].mask==1&&out.reservations[1].mask==8,"handle slot masks");
    check(out.writes[2].destination==p::Location{2,4096+144},"stride and slot destination");
    f.context.descriptor_word=0xffff0000;out=ok(f.prepare());
    check(std::get<p::CommandCursor>(out.context.command).cursor==0&&out.writes[0].words.size()==1,"descriptorlow16zero still writes word");
    f.context.descriptor_word=0x12340005;out=ok(f.prepare());check(std::get<p::CommandCursor>(out.context.command).cursor==32,"descriptor upper bits ignored,round16");
    f.model.joint_count=0;f.model.history_selector.reset();f.model.parent_sharing.reset();
    out=ok(p::prepare(f.model,p::Unknown{},f.context,p::Unknown{}));
    check(out.reservations.empty()&&out.writes.size()==1&&out.model_flags104==f.model.flags104,"jointzero returns after command");
    f.model.joint_count=2;f.model.history_selector=0xffffffff;f.model.parent_sharing=0;
    f.model.parent_sharing=1;f.model.parent=p::Null{};ok(f.prepare());f.model.parent=p::Unknown{};bad(f.prepare(),p::ErrorCode::unknown_parent);
    f.model.parent=p::Present{};bad(f.prepare(),p::ErrorCode::unsupported_parent);f.model.parent_sharing=0;
    f.model.history_selector=0;bad(f.prepare(),p::ErrorCode::unsupported_history);f.model.history_selector.reset();bad(f.prepare(),p::ErrorCode::unknown_history);f.model.history_selector=0xffffffff;
    f.model.parent_sharing.reset();bad(f.prepare(),p::ErrorCode::unknown_parent_sharing);f.model.parent_sharing=0;
    f.resource.auxiliary214.reset();bad(f.prepare(),p::ErrorCode::unknown_auxiliary);f.resource.auxiliary214=1;bad(f.prepare(),p::ErrorCode::unsupported_auxiliary);f.resource.auxiliary214=0;
    bad(p::prepare(f.model,p::Null{},f.context,f.manager),p::ErrorCode::null_resource);bad(p::prepare(f.model,p::Unknown{},f.context,f.manager),p::ErrorCode::unknown_resource);
    f.model.palette_count=0;out=ok(p::prepare(f.model,f.resource,f.context,p::Unknown{}));check(out.reservations.empty(),"zero palettes ignore manager");f.model.palette_count=2;
    f.handles.fill(0xffffffff);out=ok(p::prepare(f.model,f.resource,f.context,p::Unknown{}));check(out.reservations[0].kind==p::ReservationKind::invalid_handle,"sentinel ignoresmanager");
    f.handles={0x01000000,0x02003001};
    bad(p::prepare(f.model,f.resource,f.context,p::Unknown{}),p::ErrorCode::unknown_manager);bad(p::prepare(f.model,f.resource,f.context,p::Null{}),p::ErrorCode::null_manager);
    f.manager.normal=p::Null{};f.resource.palettes={};f.indices[0]=255;out=ok(f.prepare());check(out.writes.size()==1&&out.reservations[0].kind==p::ReservationKind::absent_buffer,"null destination ignores missing palettes");
    f.manager.normal=p::Unknown{};bad(f.prepare(),p::ErrorCode::unknown_buffer);f.manager.normal=p::Region{{2,0},65536};
    f.dirty={1,8,0,0};f.manager.local_lock=1;out=ok(f.prepare());check(out.dirty_writes.empty()&&out.writes.size()==1,"dirty skipslockandinvalidnumericinputs");
    f.dirty={};f.resource.palettes=f.palettes;f.indices[0]=0;
    bad(f.prepare(),p::ErrorCode::unsupported_lock);f.manager.local_lock.reset();bad(f.prepare(),p::ErrorCode::unknown_lock);f.manager.local_lock=0;
    f.manager.global_lock=0x80;bad(f.prepare(),p::ErrorCode::unsupported_lock);f.manager.global_lock.reset();bad(f.prepare(),p::ErrorCode::unknown_lock);f.manager.global_lock=0;
    f.manager.stride.reset();bad(f.prepare(),p::ErrorCode::unknown_stride);f.manager.stride=4096;
    f.handles[1]=f.handles[0];out=ok(f.prepare());check(out.writes.size()==2&&out.reservations[1].kind==p::ReservationKind::already_marked,"duplicate handle observes transaction dirty state");
    for(std::uint32_t slot:{31U,32U,255U,256U}){
        f.handles={0x01000000U|(slot<<12),0x02000000U|(slot<<12)};out=ok(f.prepare());
        const auto expected=(slot&255)<32?1U<<(slot&255):0U;
        check(out.reservations[0].mask==expected,"ARM register shift low8");
        check(out.dirty_writes.size()==(expected?1:2),"maskzero stores remainobservable");
    }
    f.handles={0x01000000,0x02000001};f.manager.stride=0;out=ok(f.prepare());
    check(out.writes.size()==3&&out.writes[1].destination==out.writes[2].destination,"same storage location alias preserved in order");
    f.manager.stride=48;out=ok(f.prepare());check(out.writes[2].destination.offset==48,"overlap placement explicit");
    f.handles[0]=0x01000004;bad(f.prepare(),p::ErrorCode::dirty_extent);f.handles[0]=0x01000000;
    f.resource.palettes={};bad(f.prepare(),p::ErrorCode::palette_extent);f.resource.palettes=f.palettes;
    f.model.handles=std::span<const std::uint32_t>(f.handles).first(1);bad(f.prepare(),p::ErrorCode::handle_extent);f.model.handles=f.handles;
    f.manager.normal=p::Region{{2,0},47};bad(f.prepare(),p::ErrorCode::buffer_extent);f.manager.normal=p::Region{{0,0},65536};bad(f.prepare(),p::ErrorCode::invalid_storage);
    f.manager.normal=p::Region{{2,0xfffffff0},65536};bad(f.prepare(),p::ErrorCode::address_overflow);
    f.manager.normal=p::Region{{2,2},65536};bad(f.prepare(),p::ErrorCode::invalid_alignment);f.manager.normal=p::Region{{2,0},65536};
    f.context.offset.reset();bad(f.prepare(),p::ErrorCode::unknown_context_offset);f.context.offset=p::Bits3{};
    auto lim=p::Limits{};lim.total_indices=2;bad(f.prepare(lim),p::ErrorCode::resource_limit);lim=p::Limits{};lim.patches=2;bad(f.prepare(lim),p::ErrorCode::resource_limit);
    lim=p::Limits{};lim.palettes=1;bad(f.prepare(lim),p::ErrorCode::resource_limit);lim=p::Limits{};lim.dirty_words=1;bad(f.prepare(lim),p::ErrorCode::resource_limit);
    lim=p::Limits{};lim.matrices=1;bad(f.prepare(lim),p::ErrorCode::resource_limit);lim=p::Limits{};lim.buffer_bytes=1024;bad(f.prepare(lim),p::ErrorCode::resource_limit);
    const auto original=f.context;f.world[1][0]=0x7fc00001;bad(f.prepare(),p::ErrorCode::arithmetic);check(f.context==original&&f.dirty==std::array<std::uint32_t,4>{},"late palette failure publishes no writes/flags");
}
void command_and_binding(){
    Fixture f;
    f.context.flags1bc=0x80000000;ok(f.prepare()); // Bit15 upperhalf is not selected by0813.
    f.context.flags1bc=0x00010000;bad(f.prepare(),p::ErrorCode::unsupported_material);f.context.flags1bc=0;
    f.context.descriptor_word.reset();bad(f.prepare(),p::ErrorCode::unknown_descriptor);f.context.descriptor_word=4;
    f.context.command=p::Unknown{};bad(f.prepare(),p::ErrorCode::unknown_command);f.context.command=p::Null{};bad(f.prepare(),p::ErrorCode::null_command);
    f.context.command=p::CommandCursor{{{1,0},4096},4096,4096};bad(f.prepare(),p::ErrorCode::allocation_required);
    f.context.command=p::CommandCursor{{{1,0},4096},0,4097};bad(f.prepare(),p::ErrorCode::buffer_extent);
    f.context.command=p::CommandCursor{{{1,0},4096},1,4096};bad(f.prepare(),p::ErrorCode::invalid_alignment);
    f.context.command=p::CommandCursor{{{1,0},4096},0,4096};auto lim=p::Limits{};lim.patches=0;bad(f.prepare(lim),p::ErrorCode::resource_limit);
    auto bound=ok(p::bind(f.context,f.manager,0x1e003001));
    auto a=std::get<p::TaggedLocation>(bound.bindings[0]);check(a.address==p::Location{2,4240}&&a.low_bits==15,"high8 count tag formula");
    bound.bindings[0]=p::TaggedLocation{a.address,7};bound.flags188=0;
    auto unchanged=ok(p::bind(bound,f.manager,0x01003001));check(std::get<p::TaggedLocation>(unchanged.bindings[0]).low_bits==7&&unchanged.flags188==0,"same address preserves existing tags andflags");
    for(auto raw:{0x00003001U,0x1f003001U,0xff003001U}){
        auto c=ok(p::bind(f.context,f.manager,raw));check(std::get<p::TaggedLocation>(c.bindings[0]).low_bits==0,"tagzero for0 orabove15");
    }
    f.manager.stride=1;bound=ok(p::bind(f.context,f.manager,0x02000001));
    a=std::get<p::TaggedLocation>(bound.bindings[0]);check(a.address.offset==0&&a.low_bits==1,"unaligned binding address lowbits ORtag");
    bound.flags188=0;bound=ok(p::bind(bound,f.manager,0x02000001));check(bound.flags188==1,"masked comparison differs from unaligned address");
    f.manager.history=p::Unknown{};bad(p::bind(f.context,f.manager,0),p::ErrorCode::unknown_buffer);
    f.manager.history=p::Region{{3,0},65536};bad(p::bind(f.context,f.manager,0),p::ErrorCode::unsupported_history);f.manager.history=p::Null{};
    f.context.bindings[0]=p::Unknown{};bad(p::bind(f.context,f.manager,0),p::ErrorCode::unknown_binding);
    f.context.bindings[0]=p::TaggedLocation{{2,1},0};bad(p::bind(f.context,f.manager,0),p::ErrorCode::invalid_binding);
    f.context.bindings[0]=p::TaggedLocation{{0,0},0};bad(p::bind(f.context,f.manager,0),p::ErrorCode::invalid_binding);
    f.context.bindings[0]=p::TaggedLocation{{2,0},16};bad(p::bind(f.context,f.manager,0),p::ErrorCode::invalid_binding);
    f.manager.normal=p::Null{};f.manager.stride.reset();auto kept=ok(p::bind(f.context,f.manager,0xffffffff));check(kept==f.context,"absentbufferspreserveunusedbindings");
    f.manager.normal=p::Region{{2,0},65536};f.manager.stride=4096;f.context.bindings[0]=p::Null{};
    bad(p::bind(f.context,f.manager,0xffffffff),p::ErrorCode::buffer_extent);
    f.manager.stride=0xffffffff;bad(p::bind(f.context,f.manager,0x01000002),p::ErrorCode::address_overflow);
    f.context.bindings[1]=p::Unknown{};bad(p::bind(f.context,f.manager,0),p::ErrorCode::unknown_binding);check(std::holds_alternative<p::Null>(f.context.bindings[0]),"bindinglateerrortransactional");
}
}
int main(){try{numerical();preparation();command_and_binding();std::cout<<"joint-palette boundaries: "<<checks<<" checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
