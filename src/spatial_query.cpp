#include "spatial_query.hpp"
#include <bit>
#include <cmath>
#include <new>
#include <stdexcept>

#ifdef __FAST_MATH__
#error "spatial_query requires ordered binary32 arithmetic"
#endif

namespace mh4u::spatial_query {
namespace {
namespace math=motion_transform_math;
struct Failure {Error error;};
[[noreturn]] void fail(ErrorCode code,std::string_view field){throw Failure{{code,field,{}}};}
template<class T>const T& known(const std::optional<T>& v,std::string_view field){
    if(!v)fail(ErrorCode::unknown_operand,field);return *v;
}
template<class T,class E>T take(std::variant<T,E> v,ErrorCode code,std::string_view field){
    if(const auto* e=std::get_if<E>(&v))throw Failure{{code,field,*e}};
    return std::get<T>(std::move(v));
}
Word add(Word a,Word b,std::string_view field){return take(math::add(a,b),ErrorCode::arithmetic,field);}
float number(Word a,std::string_view field){
    const auto f=std::bit_cast<float>(a);
    if(!std::isfinite(f))throw Failure{{ErrorCode::arithmetic,field,
        math::Error{math::ErrorCode::nonfinite_operand,"finite consumed comparison"}}};
    return f;
}
float number(const std::optional<Word>& a,std::string_view field){return number(known(a,field),field);}
void require_empty(const std::optional<bool>& empty,std::string_view field){
    if(!known(empty,field))fail(ErrorCode::unsupported_route,field);
}
bool box(const collision_resource::Bits4& lo,const collision_resource::Bits4& hi,
         const spatial_face_query::Vertical& ray){
    const auto& p=known(ray.origin,"box ray origin");
    if(number(p[0],"box ray X")<number(lo[0],"box minimum X"))return false;
    if(number(p[2],"box ray Z")<number(lo[2],"box minimum Z"))return false;
    if(number(p[0],"box ray X")>number(hi[0],"box maximum X"))return false;
    if(number(p[2],"box ray Z")>number(hi[2],"box maximum Z"))return false;
    if(number(ray.direction_y,"box direction Y")<0.0F)
        return number(p[1],"box ray Y")>=number(lo[1],"box minimum Y");
    return number(p[1],"box ray Y")<=number(hi[1],"box maximum Y");
}
bool excluded(const NullableIndex& e,Word slot){
    return known(e.present,"excluded object presence")&&known(e.index,"excluded object identity")==slot;
}
bool excluded(const NullableStatus& e,Word slot,Word group){
    if(!known(e.present,"excluded status presence"))return false;
    if(known(e.instance,"excluded status instance")!=slot)return false;
    return known(e.group,"excluded status group")==group;
}
void visit(const Scene& scene,const Filter& filter,const spatial_face_query::Vertical& ray,
           const Node& node,Evaluation& out,const Limits& limits,std::size_t& runtime_words){
    const auto& origin=known(ray.origin,"container ray origin");
    if(number(node.minimum_x,"container minimum X")>number(origin[0],"container ray X"))return;
    if(number(origin[2],"container ray Z")>number(node.maximum_z,"container maximum Z"))return;
    const auto slot=known(node.instance,"container instance");
    if(slot>=scene.instances.size())fail(ErrorCode::invalid_identity,"container instance");
    const auto& instance=scene.instances[slot];
    if(known(instance.enabled_5,"object enabled")==0)return;
    const auto tag=known(instance.exclusion_tag,"object exclusion tag");
    if(tag!=0&&tag==known(filter.excluded_tag,"filter excluded tag"))return;
    const auto mode=known(filter.exclusion_mode,"filter exclusion mode");
    if(mode==1&&excluded(filter.excluded_object,slot))return;
    if((known(filter.object_mask,"filter object mask")&known(instance.object_mask,"object mask"))==0)return;
    if((known(filter.area_mask,"filter area mask")&known(instance.area_mask,"object area mask"))==0)return;
    if(!known(instance.resource_present,"object resource presence"))return;
    const auto& resource=known(instance.resource,"object resource");
    if(resource.groups().size()!=1)fail(ErrorCode::unsupported_route,"multi-group resource traversal");
    const auto& group=resource.groups()[0];
    if(!box(group.bbox_min,group.bbox_max,ray))return;
    const auto& status=instance.status;
    if(known(status.enabled_c,"status enabled")==0)return;
    if(known(status.disabled_1f,"status disabled")!=0)return;
    if(mode==1&&excluded(filter.excluded_status,slot,0))return;
    if(known(instance.status_count,"object status count")==0)
        fail(ErrorCode::invalid_identity,"object status count");
    if(known(scene.status_selector_a,"status selector58")!=0||
       known(scene.status_selector_b,"status selector54")!=0)
        fail(ErrorCode::unsupported_route,"status pointer selectors");
    // 198834 copies byte20 and these pointers into the carrier. Their broader
    // semantics are not established; retain the explicit zero/null profile.
    if(known(status.field20,"status byte20")!=0)
        fail(ErrorCode::unsupported_route,"status byte20 profile");
    if(known(status.pointer24_present,"status matrix24 presence")||
       known(status.pointer30_present,"status matrix30 presence"))
        fail(ErrorCode::unsupported_route,"status matrix pointers");
    if(known(instance.tree_present,"group tree presence"))
        fail(ErrorCode::unsupported_route,"group tree traversal");
    if(!known(instance.grid_present,"group grid presence"))return;
    if(resource.header().group_tree_type!=2)
        fail(ErrorCode::unsupported_route,"group tree type");
    auto grid=take(spatial_grid::Prepared::prepare(resource,0),ErrorCode::grid,"group grid setup");
    const auto state_slot=known(instance.grid_state,"grid runtime identity");
    if(state_slot>=scene.grid_states.size())fail(ErrorCode::invalid_identity,"grid runtime identity");
    GridState* state=nullptr;
    for(auto& update:out.grid_updates)if(update.slot==state_slot){state=&update.state;break;}
    // Validate every instance-to-runtime binding, including repeated nodes that
    // reuse an update already created earlier in this query.
    const auto& source=state?*state:scene.grid_states[state_slot];
    if(source.resource.sha256()!=resource.sha256()||source.group!=0||
       source.pair_marks.size()!=group.pairs.count)
        fail(ErrorCode::invalid_identity,"grid runtime source binding");
    if(!state){
        if(source.pair_marks.size()>limits.runtime_words-runtime_words)
            fail(ErrorCode::resource_limit,"grid runtime words");
        runtime_words+=source.pair_marks.size();
        out.grid_updates.push_back({state_slot,source});state=&out.grid_updates.back().state;
    }
    if(known(state->depth,"grid nesting depth")!=0)
        fail(ErrorCode::unsupported_route,"nested grid query");
    const auto counter=known(state->counter,"grid counter")+Word{1};
    state->counter=counter;
    // 346604 increments before lookup;3463f0 restores depth even on a miss.
    // No zero-skip or mark reset is present at counter rollover.
    const auto cell=take(grid.lookup({origin[0],origin[2]}),ErrorCode::grid,"group grid lookup");
    if(!cell)return;
    for(const auto local_pair:cell->pair_ids){
        if(out.effects.candidates.size()>=limits.candidates)fail(ErrorCode::resource_limit,"candidate count");
        out.effects.candidates.push_back({slot,0,local_pair});
        const auto pair=take(resource.resolve_pair(0,local_pair),ErrorCode::resource,"candidate pair");
        auto& mark=state->pair_marks[local_pair];
        if(known(mark,"pair visit mark")==counter)continue;
        const auto mask=known(filter.attribute_mask,"attribute mask"),attribute=pair.attribute->words[0];
        const bool overlap=(mask&attribute&0x3fffffffU)!=0;
        if(overlap!=((mask&0x40000000U)==(attribute&0x40000000U)))continue;
        mark=counter;
        if(known(instance.bypass_pair_box_33,"object pair bbox bypass")==0){
            const auto& bounds=grid.source_grid().pair_bounds[local_pair].words;
            if(!box({bounds[0],bounds[1],bounds[2],bounds[3]},
                    {bounds[4],bounds[5],bounds[6],0},ray))continue;
        }
        if(known(filter.face_callback_present,"face callback presence"))
            fail(ErrorCode::unsupported_route,"face callback");
        // The retained complete manager queries contain single-face pairs.
        // The paired-face callback dispatch must be recovered independently.
        if(pair.second)fail(ErrorCode::unsupported_route,"paired-face dispatch");
        if(out.effects.face_calls.size()>=limits.faces)fail(ErrorCode::resource_limit,"face count");
        const Identity identity{slot,0,local_pair,pair.first.id-group.faces.first,pair.pair->attribute_id};
        out.effects.face_calls.push_back(identity);
        spatial_face_query::Face face{pair.first.face->normal_bits,{}};
        for(std::size_t v=0;v<3;++v)
            for(std::size_t axis=0;axis<3;++axis)face.vertices[v][axis]=pair.first.vertices[v]->position_bits[axis];
        const auto maximum=out.hit?out.hit->replacement.t_bits:0x7f7fffffU;
        auto replacement=take(spatial_face_query::intersect(face,ray,maximum),ErrorCode::face,"source face query");
        if(replacement)out.hit=Hit{resource,identity,*replacement};
    }
}
Word classify(const Scene& scene,const Hit& hit){
    const auto attribute=hit.resource.attributes()[hit.identity.attribute].words[0];
    // All three masks are loaded and ANDed before the ordered decisions.
    const auto a=known(scene.classification_masks[0],"classification mask38")&attribute;
    const auto b=known(scene.classification_masks[1],"classification mask3c")&attribute;
    const auto c=known(scene.classification_masks[2],"classification mask40")&attribute;
    if(a!=0)return 129;if(b!=0)return 258;if(c!=0)return 516;
    const auto y=number(hit.replacement.plane[1],"classification normal Y");
    const bool first=number(scene.normal_thresholds[0],"normal threshold2c")<y;
    const auto second_threshold=number(scene.normal_thresholds[1],"normal threshold30");
    const bool second=second_threshold<y;
    const bool third=y>0.0F?second_threshold>=y:
        number(scene.normal_thresholds[2],"normal threshold34")<=y;
    if(first)return 1;if(second)return 2;return third?4:8;
}
}

Result<Evaluation> query(const Scene& scene,const Filter& filter,
                         const spatial_face_query::Vertical& ray,Limits limits){
    try{
        (void)take(math::multiply(0,0),ErrorCode::arithmetic,"query floating environment");
        Evaluation out;std::size_t node_count=0,runtime_words=0;
        const auto flags=known(filter.flags,"filter flags");
        if((flags&4U)!=0){
            const auto areas=known(filter.area_mask,"filter area mask");
            if((flags&2U)!=0){
                for(Word area=0;area<32;++area){
                    if((areas&(Word{1}<<area))==0)continue;
                    for(const auto& node:known(scene.direct[area],"direct container")){
                        if(node_count>=limits.nodes)fail(ErrorCode::resource_limit,"node count");
                        ++node_count;visit(scene,filter,ray,node,out,limits,runtime_words);
                    }
                }
            }
            for(Word area=0;area<32;++area)if((areas&(Word{1}<<area))!=0){
                require_empty(scene.other_empty[0][area],"family1498 container");
                require_empty(scene.other_empty[1][area],"familya98 container");
            }
            for(Word area=0;area<32;++area)if((areas&(Word{1}<<area))!=0){
                require_empty(scene.other_empty[2][area],"family1e98 container");
                require_empty(scene.other_empty[3][area],"family2898 container");
            }
        }
        if(known(filter.extra_c8,"filter c8")&&(flags&8U)!=0&&
           known(scene.extra_c8_enabled,"manager329c")){
            const auto areas=known(filter.area_mask,"filter area mask");
            for(Word area=0;area<32;++area)if((areas&(Word{1}<<area))!=0){
                require_empty(scene.extra_c8_empty[0][area],"family32f8 container");
                require_empty(scene.extra_c8_empty[1][area],"family3cf8 container");
            }
        }
        if(known(filter.extra_c9,"filter c9")&&known(scene.extra_c9_enabled,"manager46fc"))
            require_empty(scene.extra_c9_empty,"family4748 container");
        if(known(scene.extra_tail_count,"manager47a0")!=0)
            fail(ErrorCode::unsupported_route,"183be0 list traversal");
        if(out.hit){
            if(known(filter.post_callback_present,"post callback presence"))
                fail(ErrorCode::unsupported_route,"post callback");
            out.classification=classify(scene,*out.hit);
        }
        return out;
    }catch(const Failure& failure){return failure.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"query allocation",{}};}
    catch(const std::length_error&){return Error{ErrorCode::resource_limit,"query allocation",{}};}
}

Result<HeightResult> height(const Scene& scene,const HeightRequest& request,Limits limits){
    try{
        const auto& position=known(request.position,"height position");
        const auto flags=known(request.flags,"height flags");
        const Word base=(flags&2U)!=0?0U:((flags&8U)!=0?0x42480000U:0x42c80000U);
        Word offset=0;
        if(known(request.previous_present,"height previous presence")){
            const auto delta=add(known(request.previous_y,"height previous Y"),position[1]^0x80000000U,"height previous delta");
            offset=number(delta,"height previous delta")<number(base,"height base")?base:delta;
        }
        auto origin=position;origin[1]=add(position[1],offset,"height origin Y");
        const auto area=known(request.area,"height area")&0xffU; // ARM register LSL consumes low8.
        Filter filter;
        filter.object_mask=1;filter.area_mask=area<32?(Word{1}<<area):0U;
        filter.attribute_mask=known(request.attribute_mask,"height attribute mask");
        filter.flags=15;filter.exclusion_mode=0;filter.extra_c8=true;filter.extra_c9=(flags&4U)!=0;
        filter.face_callback_present=false;filter.post_callback_present=false;
        // 198418 does not initialize excluded_tag; preserve Unknown. It is only
        // consumed by the direct object gate when that object's tag is nonzero.
        auto result=query(scene,filter,{origin,0xbf800000U},limits);
        if(const auto* error=std::get_if<Error>(&result))return *error;
        auto evaluation=std::get<Evaluation>(std::move(result));
        if(evaluation.hit&&known(request.attribute_output_present,"height attribute output presence"))
            fail(ErrorCode::unsupported_route,"height attribute output");
        const auto height=evaluation.hit?evaluation.hit->replacement.position[1]:position[1];
        const auto status=static_cast<std::uint8_t>(evaluation.classification&0xffU);
        return HeightResult{height,status,std::move(evaluation)};
    }catch(const Failure& failure){return failure.error;}
}
} // namespace mh4u::spatial_query
