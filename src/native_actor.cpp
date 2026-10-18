#include "native_actor.hpp"
#include "native_actor_data.hpp"
#include "catalog.hpp"
#include "image.hpp"
#include "joint_output.hpp"
#include "joint_bootstrap.hpp"
#include "joint_palette.hpp"
#include "model_resource.hpp"
#include "motion_resource.hpp"
#include "motion_transition.hpp"
#include "motion_bank.hpp"
#include "vertex_skinning.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <numeric>
#include <set>

namespace mh4u::native_actor {
namespace {
namespace mod=model_resource;namespace lmt=motion_resource;
namespace jp=joint_pose;namespace jo=joint_output;namespace ju=joint_update;
namespace mi=motion_initialization;namespace pal=joint_palette;
namespace mf=model_animation_frame;namespace mp=motion_pending;namespace mt=motion_transition;
constexpr std::uint32_t one=0x3f800000;
constexpr jp::Bits3 zero3{},ones3{one,one,one};
constexpr jp::Bits4 zero4{},identity4{0,0,0,one};
struct Failure {Error error;};
[[noreturn]]void fail(ErrorCode c,std::string m,std::size_t p=0,std::size_t g=0,std::size_t v=0){throw Failure{{c,std::move(m),p,g,v}};}
template<class T,class E>T unwrap(std::variant<T,E> result,ErrorCode code,const char* stage){
    if(auto* e=std::get_if<E>(&result))fail(code,std::string(stage)+": "+std::string(e->message));
    return std::get<T>(std::move(result));
}
float value(std::uint32_t x){return std::bit_cast<float>(x);}
jp::Matrix identity(){jp::Matrix m{};m[0]=m[5]=m[10]=m[15]=one;return m;}
jo::Parameters output_parameters(const mod::Skeleton& skeleton){
    jo::Parameters p;p.model=jp::PresentModel{skeleton.local_matrices};p.parent=jo::Null{};
    p.reference_basis=motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};
    p.reference_translation=identity4;return p;
}
unsigned number(const std::string& text){unsigned v{};auto [p,e]=std::from_chars(text.data(),text.data()+text.size(),v);if(e!=std::errc{}||p!=text.data()+text.size())fail(ErrorCode::recipe,"Invalid unsigned recipe value");return v;}
std::vector<std::uint8_t> read(const std::filesystem::path& path,std::size_t limit){
    std::ifstream input(path,std::ios::binary|std::ios::ate);if(!input)fail(ErrorCode::io,"Cannot open "+path.string());
    auto n=input.tellg();if(n<0||static_cast<std::uint64_t>(n)>limit)fail(ErrorCode::capacity,"Input exceeds allocation policy");
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(n));input.seekg(0);input.read(reinterpret_cast<char*>(bytes.data()),n);
    if(!input||input.peek()!=std::char_traits<char>::eof())fail(ErrorCode::io,"Input changed or could not be read");return bytes;
}
std::vector<std::string> fields(const std::string& line){std::vector<std::string> out;std::size_t from=0;for(;;){auto at=line.find('\t',from);out.push_back(line.substr(from,at-from));if(at==std::string::npos)return out;from=at+1;}}
mi::Resolution resolve(const void* context,const motion_binding::TrackReference& id){
    auto r=static_cast<const lmt::Resource*>(context)->sampler_track(id);
    if(auto* e=std::get_if<lmt::Error>(&r))return mi::ResolutionError{static_cast<std::uint32_t>(e->code),e->message};
    return std::get<motion_sampler::TrackView>(r);
}
mp::ActionResolution resolve_action(const void* context,std::uint32_t bank,std::uint32_t slot){
    auto r=static_cast<const lmt::Resource*>(context)->binding_action(bank,slot);
    if(auto* e=std::get_if<lmt::Error>(&r))return mp::ActionResolutionError{static_cast<std::uint32_t>(e->code),e->message};
    return std::get<std::optional<motion_binding::Action>>(r);
}
motion_clock::EventResolution resolve_events(const void* context,const motion_request::Binding& binding){
    const auto& resource=*static_cast<const lmt::Resource*>(context);
    if(binding.resource_identity!=resource.identity()||binding.bank!=0||binding.group>=4)return motion_clock::ResolutionError{0,"Foreign native event binding"};
    auto r=resource.event_groups(binding.slot);
    if(auto* e=std::get_if<lmt::Error>(&r))return motion_clock::ResolutionError{static_cast<std::uint32_t>(e->code),e->message};
    const auto& groups=std::get<std::optional<std::array<lmt::EventGroupView,4>>>(r);
    if(!groups)return motion_clock::ResolutionError{0,"Bound native action has no event table"};
    return motion_clock::EventGroup{binding,(*groups)[binding.group].records};
}
jo::State rest_state(const mod::Resource& model){
    const auto skeleton=model.skeleton();if(!skeleton||!model.joint_count())fail(ErrorCode::hierarchy,"Native actor requires a skeleton");
    const auto n=model.joint_count();if(skeleton->joint_records_parsed.size()!=n)fail(ErrorCode::hierarchy,"Missing parsed joint records");
    std::set<unsigned> ids;
    for(std::size_t i=0;i<n;++i){const auto& row=skeleton->joint_records_parsed[i];if(!ids.insert(row.fields[0]).second)fail(ErrorCode::hierarchy,"Inspection bone IDs must be unique");
        if(std::to_integer<unsigned>(skeleton->remap[row.fields[0]])!=i)fail(ErrorCode::hierarchy,"Source bone ID and remap disagree");
    }
    // Managed host allocation initializes the previously unwritten upper bits
    // explicitly; the source metadata initializer preserves that caller input.
    const std::vector<std::uint32_t> prior_mapping(n,0);
    const auto bootstrap=unwrap(joint_bootstrap::initialize(skeleton->joint_records_parsed,
        skeleton->local_matrices,prior_mapping,ones3),ErrorCode::pose,"Joint metadata initialization");
    jo::State state{};state.local_rt=state.world=identity();state.scale=ones3;state.dirty=1;
    state.order=bootstrap.order;
    for(std::size_t i=0;i<n;++i){jo::Joint joint{};joint.pose=bootstrap.joints[i];joint.metadata=bootstrap.metadata[i];
        joint.matrix=bootstrap.matrices[i];joint.callback=jo::Callback::null;joint.owner=jo::Owner::self;state.joints.push_back(joint);}
    return unwrap(jo::apply(state,output_parameters(*skeleton)),ErrorCode::hierarchy,"Rest hierarchy").state;
}
void bounds(SceneFrame& frame){
    auto& mesh=frame.mesh;if(mesh.triangles.empty())fail(ErrorCode::vertex,"No native triangles");
    mesh.minimum=mesh.maximum=mesh.triangles.front().position;
    for(const auto& v:mesh.triangles){auto p=v.position;mesh.minimum={std::min(mesh.minimum.x,p.x),std::min(mesh.minimum.y,p.y),std::min(mesh.minimum.z,p.z)};mesh.maximum={std::max(mesh.maximum.x,p.x),std::max(mesh.maximum.y,p.y),std::max(mesh.maximum.z,p.z)};}
    mesh.center={mesh.minimum.x*.5f+mesh.maximum.x*.5f,mesh.minimum.y*.5f+mesh.maximum.y*.5f,mesh.minimum.z*.5f+mesh.maximum.z*.5f};
    mesh.radius=std::hypot(mesh.maximum.x-mesh.minimum.x,mesh.maximum.y-mesh.minimum.y,mesh.maximum.z-mesh.minimum.z)*.5f;
    if(!std::isfinite(mesh.radius)||mesh.radius<=1e-8f)fail(ErrorCode::vertex,"Degenerate native bounds");mesh.material_groups=frame.draws.size();mesh.has_uv=true;
}
}
Result<Frame> Actor::Data::render(Frame frame,Matrices matrices,std::uint64_t pose_revision)const {
    try{
        if(matrices.size()!=rest.joints.size()||!pose_revision)fail(ErrorCode::hierarchy,"Render matrix extent/revision differs from resource owner");
        for(std::size_t j=0;j<matrices.size();++j){Matrix matrix{};for(unsigned k=0;k<16;++k)matrix[k]=value(matrices[j][k]);frame.scene.global_transforms.push_back(matrix);}
        std::vector<std::vector<jp::Matrix>> mapped(parts.size());
        for(std::size_t p=0;p<mapped.size();++p)for(auto joint:parts[p].joint_map)mapped[p].push_back(matrices[joint]);
        std::map<std::pair<std::size_t,unsigned>,pal::Generated> palettes;
        constexpr vertex_skinning::View3 view{one,0,0,0,one,0,0,0,one};
        for(std::size_t i=0;i<draws.size();++i){const auto& draw=draws[i];const auto& part=parts[draw.part];const auto sk=part.model.skeleton().value();const auto key=std::make_pair(draw.part,unsigned(draw.palette));
            if(!palettes.contains(key)){auto generated=unwrap(pal::generate(sk.source_skin_matrices,pal::JointMatrices{std::span<const jp::Matrix>(mapped[draw.part])},sk.palettes[draw.palette].joints,zero3,{part.model.identity(),pose_revision,1,draw.palette}),ErrorCode::palette,"Palette");palettes.emplace(key,std::move(generated));}
            const auto& generated=palettes.at(key);std::vector<mh4u::Vertex> vertices;vertices.reserve(draw.geometry.vertices.size());
            for(std::size_t v=0;v<draw.geometry.vertices.size();++v){const auto& source=draw.geometry.vertices[v];auto result=vertex_skinning::skin(source,generated.values,view);
                if(auto* e=std::get_if<vertex_skinning::Error>(&result))fail(ErrorCode::vertex,std::string(e->message),draw.part,draw.geometry.primitive_index,v);
                const auto& out=std::get<vertex_skinning::Output>(result);const float u=value(source.uv_bits[0]),t=value(source.uv_bits[1]);if(!std::isfinite(u)||!std::isfinite(t))fail(ErrorCode::vertex,"Nonfinite UV",draw.part,draw.geometry.primitive_index,v);
                vertices.push_back({{value(out.position_model[0]),value(out.position_model[1]),value(out.position_model[2])},{value(out.normal_unit[0]),value(out.normal_unit[1]),value(out.normal_unit[2])},{u,1-t}});
            }
            frame.scene.draws.push_back({frame.scene.mesh.triangles.size(),draw.geometry.triangles.size()*3,draw.material,i});
            for(const auto& triangle:draw.geometry.triangles)for(auto index:triangle)frame.scene.mesh.triangles.push_back(vertices[index]);frame.scene.mesh.source_positions+=vertices.size();
        }
        bounds(frame.scene);frame.palettes=palettes.size();return frame;
    }catch(const Failure& e){return e.error;}catch(const std::bad_alloc&){return Error{ErrorCode::capacity,"Render allocation failed"};}catch(const std::exception& e){return Error{ErrorCode::pose,e.what()};}
}
Actor::Actor(std::shared_ptr<const Data> data):data_(std::move(data)){}
std::shared_ptr<const Scene> Actor::presentation()const noexcept{return data_->scene;}
std::size_t Actor::joint_count()const noexcept{return data_->rest.joints.size();}
std::size_t Actor::part_count()const noexcept{return data_->parts.size();}
float Actor::inspection_rate()const noexcept{return data_->rate;}
bool Actor::supports_transitions()const noexcept{return data_->command_globals.has_value();}
std::span<const std::filesystem::path> Actor::source_files()const noexcept{return data_->source_files;}
Result<Actor> Actor::load(const std::filesystem::path& recipe){
    try{
        const auto raw=read(recipe,65536);const std::string text(raw.begin(),raw.end());const std::string header="MH4U-NATIVE-ACTOR 1\n";
        if(!text.starts_with(header)||text.empty()||text.back()!='\n'||text.find_first_of(std::string("\0\r",2))!=std::string::npos)fail(ErrorCode::recipe,"Invalid native actor recipe header/text");
        const auto canonical_recipe=std::filesystem::canonical(recipe);const auto root=canonical_recipe.parent_path();auto data=std::make_shared<Data>();auto scene=std::make_shared<Scene>();
        data->source_files.push_back(canonical_recipe);
        const auto source_path=[&](const std::string& name){auto path=catalog_path(root,name);data->source_files.push_back(path);return path;};
        struct Selection{unsigned part,primitive,image;};struct Action{unsigned motion,slot;std::string name;};
        std::vector<Selection> selected;std::vector<Action> actions;bool rate_seen=false;std::size_t image_budget=128U*1024U*1024U;
        for(std::size_t from=header.size();from<text.size();){auto at=text.find('\n',from);const auto row=fields(text.substr(from,at-from));from=at+1;
            if(row[0]=="model"&&row.size()==3){if(data->parts.size()==16)fail(ErrorCode::capacity,"At most 16 native model parts");data->parts.push_back({unwrap(mod::Resource::load(source_path(row[1]),row[2]),ErrorCode::resource,"MOD"),{}});}
            else if(row[0]=="motion"&&row.size()==3){if(data->motions.size()==16)fail(ErrorCode::capacity,"At most 16 LMT resources");data->motions.push_back(unwrap(lmt::Resource::load(source_path(row[1]),row[2]),ErrorCode::resource,"LMT"));}
            else if(row[0]=="commands"&&row.size()==3&&!data->command_globals){
                const auto bytes=read(source_path(row[1]),24);
                if(bytes.size()!=24||picosha2::hash256_hex_string(bytes)!=row[2])fail(ErrorCode::identity,"Command curve snapshot identity/extent mismatch");
                motion_command::Globals globals;
                for(unsigned table=0;table<3;++table){std::array<std::uint32_t,2> pair{};
                    for(unsigned j=0;j<2;++j)for(unsigned k=0;k<4;++k)pair[j]|=std::uint32_t(bytes[table*8+j*4+k])<<(k*8);
                    globals.fields6c_tables[table]=pair;
                }
                data->command_globals=globals;
            }
            else if(row[0]=="texture"&&row.size()==3){if(scene->images.size()==64)fail(ErrorCode::capacity,"At most 64 images");auto png=read(source_path(row[1]),16U*1024U*1024U);
                if(picosha2::hash256_hex_string(png)!=row[2])fail(ErrorCode::identity,"Texture digest mismatch");auto pixels=decode_png(png,image_budget);const auto used=pixels.rgb.size()+pixels.alpha.size();if(used>image_budget)fail(ErrorCode::capacity,"Image budget exhausted");image_budget-=used;
                scene->images.push_back({std::move(pixels),row[2]});Material material;material.name=row[1];material.image=static_cast<int>(scene->images.size()-1);scene->materials.push_back(material);}
            else if(row[0]=="primitive"&&row.size()==4){if(selected.size()==256)fail(ErrorCode::capacity,"At most 256 selected primitives");selected.push_back({number(row[1]),number(row[2]),number(row[3])});}
            else if(row[0]=="action"&&row.size()==4){if(actions.size()==4096||row[3].empty()||row[3].size()>128)fail(ErrorCode::capacity,"Action catalog limit");actions.push_back({number(row[1]),number(row[2]),row[3]});}
            else if(row[0]=="rate"&&row.size()==2&&!rate_seen){const unsigned rate=number(row[1]);if(!rate||rate>240)fail(ErrorCode::recipe,"Inspection rate outside 1..240");data->rate=static_cast<float>(rate);rate_seen=true;}
            else fail(ErrorCode::recipe,"Unknown, duplicate or malformed recipe row");
        }
        if(data->parts.empty()||selected.empty()||!rate_seen)fail(ErrorCode::recipe,"Model, selected geometry and explicit inspection rate required");
        data->rest=rest_state(data->parts[0].model);const auto base=data->parts[0].model.skeleton().value();
        for(auto v:base.remap)data->bone_map.push_back(std::to_integer<std::uint8_t>(v));
        for(auto& part:data->parts){const auto own=rest_state(part.model);const auto source=part.model.skeleton().value();
            for(std::size_t i=0;i<own.joints.size();++i){const auto id=source.joint_records_parsed[i].fields[0];const auto target=data->bone_map[id];if(target>=data->rest.joints.size())fail(ErrorCode::hierarchy,"Part bone absent from base skeleton");
                const auto parent=source.joint_records_parsed[i].fields[1];const auto target_parent=base.joint_records_parsed[target].fields[1];
                if(parent!=255&&(parent>=own.joints.size()||target_parent==255||source.joint_records_parsed[parent].fields[0]!=base.joint_records_parsed[target_parent].fields[0]))fail(ErrorCode::hierarchy,"Part parent differs from base skeleton");
                for(unsigned k=0;k<16;++k){const float a=value(own.joints[i].matrix[k]),b=value(data->rest.joints[target].matrix[k]);if(!std::isfinite(a)||!std::isfinite(b)||std::abs(a-b)>0.001f)fail(ErrorCode::hierarchy,"Part rest matrix is not bind-compatible");}
                part.joint_map.push_back(target);
            }
        }
        std::size_t vertices=0,triangles=0;std::set<std::pair<unsigned,unsigned>> unique;
        for(auto s:selected){if(s.part>=data->parts.size()||s.image>=scene->images.size()||!unique.insert({s.part,s.primitive}).second)fail(ErrorCode::recipe,"Unknown or duplicate selected primitive/material");
            const auto& part=data->parts[s.part];const auto geometry=unwrap(part.model.geometry(s.primitive),ErrorCode::resource,"Geometry");const auto primitive=part.model.primitives()[s.primitive];
            if(primitive.palette_index>=part.model.skeleton()->palettes.size())fail(ErrorCode::palette,"Primitive palette outside model");
            if(geometry.vertices.size()>100000-vertices||geometry.triangles.size()>200000-triangles)fail(ErrorCode::capacity,"Selected geometry exceeds actor policy");vertices+=geometry.vertices.size();triangles+=geometry.triangles.size();
            data->draws.push_back({s.part,geometry,primitive.palette_index,static_cast<int>(s.image)});scene->primitives.emplace_back();scene->primitives.back().material=static_cast<int>(s.image);
        }
        std::set<std::pair<unsigned,unsigned>> clips;
        for(const auto& a:actions){if(a.motion>=data->motions.size()||a.slot>255||!clips.insert({a.motion,a.slot}).second)fail(ErrorCode::recipe,"Unknown or duplicate selected LMT slot");
            const auto& resource=data->motions[a.motion];auto action=unwrap(resource.binding_action(0,a.slot),ErrorCode::binding,"Action");if(!action)fail(ErrorCode::binding,"Selected action is absent");
            if(!action->header.frame_count||action->header.frame_count>1000000)fail(ErrorCode::capacity,"Action duration exceeds inspection policy");
            data->clips.push_back({a.motion,a.slot});Animation presentation;presentation.name=a.name;presentation.duration=static_cast<float>(action->header.frame_count-1)/data->rate;scene->animations.push_back(std::move(presentation));
        }
        scene->notes.push_back("Native MOD/LMT playback with original numeric clock, model motion, joint update and transition. Tick pacing, bootstrap, camera and materials are inspection policies; controller and gameplay remain pending.");data->scene=std::move(scene);return Actor{std::move(data)};
    }catch(const Failure& e){return e.error;}catch(const std::bad_alloc&){return Error{ErrorCode::capacity,"Native actor allocation failed"};}catch(const std::exception& e){return Error{ErrorCode::recipe,e.what()};}
}
Result<Frame> Actor::evaluate(int clip,float seconds,bool loop){return evaluate_impl(clip,seconds,loop,false,0);}
Result<Frame> Actor::seek(int clip,float seconds,bool loop){return evaluate_impl(clip,seconds,loop,false,0,true);}
Result<Frame> Actor::change_action(int clip,bool loop,float interpolation){return evaluate_impl(clip,0,loop,true,interpolation);}
Result<Frame> Actor::evaluate_impl(int clip,float seconds,bool loop,bool change,float interpolation,bool reset){
    try{
        if(clip < -1||clip>=static_cast<int>(data_->clips.size())||!std::isfinite(seconds)||seconds<0)fail(ErrorCode::time,"Invalid native clip/time");
        if(pose_revision_==std::numeric_limits<std::uint64_t>::max())fail(ErrorCode::capacity,"Pose revision exhausted");
        if(change&&(!data_->command_globals||clip<0||!std::isfinite(interpolation)||interpolation<0))fail(ErrorCode::time,"Action change requires command snapshots, an action and finite nonnegative interpolation");
        const bool transition=change&&current_clip_>=0&&next_tick_;
        Frame frame;jo::State state=data_->rest;mf::State next{};motion_command::Fields command_fields{};
        std::uint64_t next_tick=0;std::size_t samples=0;auto local_rt=identity();
        const auto base=data_->parts[0].model.skeleton().value();
        if(clip>=0){
            const double elapsed=std::floor(static_cast<double>(seconds)*data_->rate);
            if(elapsed>=static_cast<double>(std::numeric_limits<std::uint64_t>::max()-1))fail(ErrorCode::time,"Native elapsed tick exceeds representable range");
            const auto target=static_cast<std::uint64_t>(elapsed);
            const bool reuse=!reset&&current_clip_==clip&&current_loop_==loop&&next_tick_&&target>=next_tick_-1;
            next_tick=reuse&&!transition?next_tick_:0;
            if(target+1-next_tick>4096)fail(ErrorCode::capacity,"Seek requires more than 4096 source ticks; use incremental playback");
            const auto& c=data_->clips[clip];const auto& resource=data_->motions[c.motion];
            if(reuse||transition){next=current_;local_rt=current_local_rt_;samples=current_samples_;command_fields=command_fields_;}
            if(transition){
                const auto& old=data_->motions[data_->clips[current_clip_].motion];
                motion_request::Model model{};model.skeleton_present=true;model.banks[0]=old.request_resource();
                if(old.identity()!=resource.identity()){
                    auto replaced=unwrap(motion_bank::replace(model,next.animation.layers,static_cast<std::uint32_t>(next.animation.layers.size()),0,resource.request_resource()),ErrorCode::binding,"Motion bank replacement");
                    model=std::move(replaced.model);next.animation.layers=std::move(replaced.layers);
                }
                // All immutable resources stay owned by Data, including old tracks
                // whose bindings survive until the pending initializer replaces them.
                const mi::Resolver resolver{&data_->motions,[](const void* context,const motion_binding::TrackReference& id)->mi::Resolution{
                    for(const auto& source:*static_cast<const std::vector<lmt::Resource>*>(context))if(source.identity()==id.resource_identity)return resolve(&source,id);
                    return mi::ResolutionError{0,"Unknown action-change track resource"};
                }};
                const motion_command::Sampling sampling{{zero3,zero3,identity4,zero4},resolver};
                auto commanded=unwrap(motion_command::apply(next.animation.layers,static_cast<std::uint32_t>(next.animation.layers.size()),command_fields,
                    model,{motion_command::Entry::restart,0,0,c.slot,0,interpolation,0,1},*data_->command_globals,sampling),ErrorCode::binding,"Action command");
                if(!commanded.request_count||!commanded.requests[0].accepted)fail(ErrorCode::binding,"Action change request was rejected");
                next.animation.layers=std::move(commanded.layers);command_fields=commanded.fields;
                // Preserve the existing inspection loop switch. The original
                // command owns the blend; this caller requests clamp when disabled.
                if(!loop)next.animation.layers[0].animation.layer.attributes|=4U;
            }
            else if(!reuse){
                for(const auto& joint:data_->rest.joints)next.animation.joints.push_back(joint.pose);
                next.animation.snapshots.resize(data_->rest.joints.size());next.animation.layers.resize(1);
                auto& initial=next.animation.layers[0].animation;initial.tracks.joints.resize(data_->rest.joints.size());
                motion_request::Model model{};model.skeleton_present=true;model.banks[0]=resource.request_resource();
                const auto requested=unwrap(motion_request::apply(initial.layer,model,{0,c.slot,loop?0U:4U,0,0,1}),ErrorCode::binding,"Request");
                if(!requested.accepted)fail(ErrorCode::binding,"Native action request was not accepted");initial.layer=requested.layer;
                next.model.rotation=identity4;next.model.scale=next.model.null_scale={one,one,one,0};next.model.rotation_delta=identity4;
            }
            std::vector<ju::Metadata> metadata;for(const auto& joint:data_->rest.joints)metadata.push_back(joint.metadata);
            const mf::Parameters parameters{{jp::PresentModel{base.local_matrices},{one,{zero3,zero3,identity4,zero4}},ones3},metadata};
            const mf::Resolvers resolvers{{&resource,resolve_action},{&resource,resolve},{&resource,resolve_events}};
            for(;next_tick<=target;++next_tick){
                auto updated=unwrap(mf::step(next,data_->bone_map,parameters,resolvers),ErrorCode::pose,"Animation frame");
                next=std::move(updated.state);samples=updated.model_samples+updated.joint_samples;
                const std::array<mt::LayerView,1> layers{{{next.animation.layers[0].animation.layer}}};
                auto transition=unwrap(mt::apply(next.animation.joints,metadata,next.animation.snapshots,layers,
                    {next.model.rotation,{next.model.position[0],next.model.position[1],next.model.position[2]},next.animation.accumulated_flags}),ErrorCode::pose,"Transition");
                next.animation.joints=std::move(transition.joints);next.animation.accumulated_flags=transition.accumulated_flags;local_rt=transition.local_rt;
                state.local_rt=local_rt;state.scale={next.model.scale[0],next.model.scale[1],next.model.scale[2]};state.flags238=next.animation.calculation_flags;
                for(std::size_t i=0;i<state.joints.size();++i)state.joints[i].pose=next.animation.joints[i];
                state=unwrap(jo::apply(state,output_parameters(base)),ErrorCode::pose,"Joint hierarchy").state;
            }
            // A repeated display query performs no simulation tick. Reconstruct
            // derived matrices from canonical transforms; no pose is resampled.
            if(reuse&&!transition&&next_tick==next_tick_){
                state.local_rt=local_rt;state.scale={next.model.scale[0],next.model.scale[1],next.model.scale[2]};state.flags238=next.animation.calculation_flags;
                for(std::size_t i=0;i<state.joints.size();++i)state.joints[i].pose=next.animation.joints[i];
                state=unwrap(jo::apply(state,output_parameters(base)),ErrorCode::pose,"Joint hierarchy").state;
            }
            const auto& layer=next.animation.layers[0].animation.layer;frame.source_frame=layer.prepared_frame;
            frame.tick=target;frame.event_masks=layer.masks;frame.ended=(layer.attributes&4U)&&(layer.flags&4U);frame.samples=samples;
        }
        auto rendered=data_->render(std::move(frame),Data::Matrices{std::span<const jo::Joint>(state.joints)},pose_revision_+1);
        if(auto* error=std::get_if<Error>(&rendered))return *error;
        frame=std::get<Frame>(std::move(rendered));current_=std::move(next);current_clip_=clip;current_loop_=loop;next_tick_=next_tick;current_local_rt_=local_rt;current_samples_=samples;command_fields_=command_fields;++pose_revision_;return frame;
    }catch(const Failure& e){return e.error;}catch(const std::bad_alloc&){return Error{ErrorCode::capacity,"Frame allocation failed"};}catch(const std::exception& e){return Error{ErrorCode::pose,e.what()};}
}
}
