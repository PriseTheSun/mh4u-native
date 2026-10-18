#include "player_world_lookup.hpp"
#include <bit>
#include <cfenv>
#include <limits>

#ifdef __FAST_MATH__
#error "player_world_lookup requires ordered, nonfused binary32 arithmetic"
#endif

namespace mh4u::player_world_lookup {
namespace {
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view field,std::optional<std::size_t> index={}) {
    throw Failure{{code,field,index}};
}
template<class T>T read(std::optional<T> value,std::string_view field,std::optional<std::size_t> index={}) {
    if(!value)fail(ErrorCode::unknown_field,field,index);
    return *value;
}
float value(Word bits) {return std::bit_cast<float>(bits);}
// VMLA/VMLS on this ARM11 profile round the product before the addition.
// Intermediates can be infinite or unordered; they are not input errors.
float add(float a,float b) {volatile float result=a+b;return result;}
float sub(float a,float b) {volatile float result=a-b;return result;}
float mul(float a,float b) {volatile float result=a*b;return result;}
void environment() {
    static_assert(std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits==24);
    if(std::fegetround()!=FE_TONEAREST)fail(ErrorCode::floating_environment,"rounding");
    volatile float tiny=value(1),twice=tiny+tiny;
    if(std::bit_cast<Word>(float(twice))!=2)fail(ErrorCode::floating_environment,"underflow");
}
std::optional<std::size_t> link(const Link& item,std::string_view field,std::optional<std::size_t> owner={}) {
    if(std::holds_alternative<Absent>(item))return {};
    const auto* index=std::get_if<Index>(&item);
    if(!index)fail(ErrorCode::unknown_field,field,owner);
    if(index->value>=Manager::capacity)fail(ErrorCode::invalid_link,field,owner);
    return index->value;
}
unsigned rank(std::uint8_t type) {
    // Original ten u16 entries at e069a8; classes not listed have rank ten.
    constexpr std::array<std::uint16_t,10> order{27,28,1,29,2,0,3,4,8,10};
    unsigned result=unsigned(order.size());
    for(unsigned i=0;i<order.size();++i)if(type==order[i])result=i;
    return result;
}
Output evaluate(const Context& context,const Request& request) {
    if(!context.manager)fail(ErrorCode::unknown_field,"manager");
    const auto& manager=*context.manager;
    auto next=link(manager.head,"head");
    std::array<bool,Manager::capacity> seen{};
    std::optional<std::size_t> selected;
    Output out;
    bool environment_checked=false;
    while(next) {
        const auto i=*next;
        if(seen[i])fail(ErrorCode::cycle,"next",i);
        seen[i]=true;++out.visited;
        const auto& record=manager.records[i];
        const auto eligible=[&] {
            if(read(record.field19,"record19",i)!=1)return false;
            if(read(record.field30,"record30",i)==0)return false;
            const auto shift=read(record.field61,"record61",i);
            const auto mask=read(context.area_mask14,"area_mask14");
            if(shift>=32 || (mask&(Word{1}<<shift))==0)return false;
            if(read(record.field78,"record78",i)==1)return false;
            const auto record_area=read(record.field2f,"record2f",i);
            if(record_area!=read(request.area,"area"))return false;
            const auto center_y=value(read(record.fields34[1],"record38",i));
            const auto height=value(read(record.field50_bits,"record50",i));
            const auto point_y=value(read(request.point[1],"pointY"));
            if(!environment_checked){environment();environment_checked=true;}
            const auto half=mul(height,0.5F);
            if(!(sub(center_y,half)<=point_y))return false;
            if(!(add(center_y,half)>point_y))return false;
            const auto point_x=value(read(request.point[0],"pointX"));
            const auto center_x=value(read(record.fields34[0],"record34",i));
            const auto point_z=value(read(request.point[2],"pointZ"));
            const auto center_z=value(read(record.fields34[2],"record3c",i));
            const auto radius=value(read(record.field4c_bits,"record4c",i));
            const auto dx=sub(point_x,center_x),dz=sub(point_z,center_z);
            const auto distance2=add(mul(dx,dx),mul(dz,dz));
            return distance2<=mul(radius,radius);
        };
        if(eligible()) {
            if(!selected)selected=i;
            else {
                const auto current_rank=rank(read(record.field54,"record54",i));
                const auto old_rank=rank(read(manager.records[*selected].field54,"record54",*selected));
                if(current_rank<=old_rank)selected=i;
            }
        }
        next=link(record.next,"next",i);
    }
    if(!selected)return out;
    const auto i=*selected;const auto& record=manager.records[i];
    Match match;match.index=i;
    constexpr std::array<std::string_view,3> vector_names{"record34","record38","record3c"};
    constexpr std::array<std::string_view,3> word_names{"record40","record44","record48"};
    for(std::size_t j=0;j<3;++j)match.vector34[j]=read(record.fields34[j],vector_names[j],i);
    match.vector34[3]=0;
    for(std::size_t j=0;j<3;++j)match.words40[j]=read(record.fields40[j],word_names[j],i);
    if(request.copy_field28)match.field28=read(record.field28,"record28",i);
    match.status=read(record.field54,"record54",i);
    out.returned=match.status;out.match=match;return out;
}
} // namespace
Result query(const Context& context,const Request& request) {
    try{return evaluate(context,request);}catch(const Failure& failure){return failure.error;}
}
PlayerResult apply(const player_controller_step::PlayerFields& player,
    const player_fields::Fields& fields,const Context& context) {
    // Caller loads PBW9 before entering the wrapper, even for an empty list.
    if(!player.selection_pbw.field9)return Error{ErrorCode::unknown_field,"PBW9"};
    Request request{player.selection_pbw.field9,
        {fields.pbw.field10_bits,fields.pbw.field14_bits,fields.pbw.field18_bits},true};
    auto result=query(context,request);
    if(const auto* error=std::get_if<Error>(&result))return *error;
    const auto& output=std::get<Output>(result);
    auto updated=fields;
    updated.pw.field5a8=output.returned==0xffff?0:output.returned;
    return PlayerOutput{updated,output};
}
} // namespace mh4u::player_world_lookup
