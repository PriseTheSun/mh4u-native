#include "player_actor.hpp"
#include "native_actor_data.hpp"
#include <limits>
#include <algorithm>
#include <new>
#include <stdexcept>
#include <type_traits>

namespace mh4u::native_actor {
namespace {
namespace pa=player_animation;namespace rq=motion_request;namespace mp=motion_pending;
namespace mi=motion_initialization;namespace lmt=motion_resource;namespace jp=joint_pose;
PlayerError error(PlayerErrorCode code,std::string message){return{code,std::move(message),{}, {}};}
PlayerError capacity(){return error(PlayerErrorCode::capacity,"Player storage or operation exceeds its explicit bounds");}
}
struct Player::Resources {
    const Actor::Data& data;const rq::Model& banks;
    pa::FrameContext parameters(const PlayerStep& step)const{
        pa::FrameContext result;result.bone_to_joint=data.bone_map;
        result.pending={jp::PresentModel{data.parts[0].model.skeleton()->local_matrices},{step.delta_bits,step.templates},step.restoration_scale};
        result.resolvers={{this,action},{this,track},{this,events}};
        result.output={result.pending.model_reference,step.parent,step.reference_basis,step.reference_translation};
        return result;
    }
    static mi::Resolution track(const void* pointer,const motion_binding::TrackReference& id){
        const auto& context=*static_cast<const Resources*>(pointer);
        for(const auto& resource:context.data.motions)if(resource.identity()==id.resource_identity){
            auto result=resource.sampler_track(id);
            if(auto* e=std::get_if<lmt::Error>(&result))return mi::ResolutionError{std::uint32_t(e->code),e->message};
            return std::get<motion_sampler::TrackView>(result);
        }
        return mi::ResolutionError{0,"Player track refers to an unowned immutable resource"};
    }
    static mp::ActionResolution action(const void* pointer,std::uint32_t bank,std::uint32_t slot){
        const auto& context=*static_cast<const Resources*>(pointer);
        if(bank>=context.banks.banks.size())return mp::ActionResolutionError{0,"Player bank outside table"};
        const auto& source=context.banks.banks[bank];
        if(std::holds_alternative<rq::Absent>(source))return std::optional<motion_binding::Action>{};
        const auto* known=std::get_if<rq::Resource>(&source);
        if(!known)return mp::ActionResolutionError{0,"Player bank is explicitly unknown"};
        for(const auto& resource:context.data.motions)if(resource.identity()==known->identity){
            auto result=resource.binding_action(bank,slot);
            if(auto* e=std::get_if<lmt::Error>(&result))return mp::ActionResolutionError{std::uint32_t(e->code),e->message};
            return std::get<std::optional<motion_binding::Action>>(result);
        }
        return mp::ActionResolutionError{0,"Player bank refers to an unowned immutable resource"};
    }
    static motion_clock::EventResolution events(const void* pointer,const rq::Binding& id){
        const auto& context=*static_cast<const Resources*>(pointer);
        // Bound events follow their original resource identity, not a new bank assignment.
        for(const auto& resource:context.data.motions)if(resource.identity()==id.resource_identity){
            auto result=resource.event_groups(id.slot);
            if(auto* e=std::get_if<lmt::Error>(&result))return motion_clock::ResolutionError{std::uint32_t(e->code),e->message};
            const auto& groups=std::get<std::optional<std::array<lmt::EventGroupView,4>>>(result);
            if(!groups||id.bank>=16||id.group>=4)return motion_clock::ResolutionError{0,"Player event binding outside source descriptor"};
            return motion_clock::EventGroup{id,(*groups)[id.group].records};
        }
        return motion_clock::ResolutionError{0,"Player event refers to an unowned immutable resource"};
    }
};
Player::Player(std::shared_ptr<const Actor::Data> data,pa::State state,player_transition::Constants constants,
    rq::Model banks,pa::Limits limits,player_fields::Fields fields,player_state1::Fields state1,player_position_bridge::Fields position):data_(std::move(data)),state_(std::move(state)),constants_(std::move(constants)),shared_fields_(std::move(fields)),state1_fields_(std::move(state1)),position_fields_(std::move(position)),banks_(std::move(banks)),limits_(limits){}
PlayerResult<Player> Player::from_rest(const Actor& actor,const PlayerInitial& initial,
    const player_transition::Constants& constants,const PlayerBanks& selected,pa::Limits limits){
    try{
        if(!actor.data_)return error(PlayerErrorCode::initial_state,"Actor resource owner has been moved away");
        const auto& data=*actor.data_;
        auto state=rest_state(data,initial,limits);
        if(auto* e=std::get_if<PlayerError>(&state))return *e;
        rq::Model banks{};banks.skeleton_present=true;
        for(std::size_t i=0;i<16;++i){
            if(std::holds_alternative<rq::Unknown>(selected[i]))continue;
            if(std::holds_alternative<rq::Absent>(selected[i])){banks.banks[i]=rq::Absent{};continue;}
            const auto& digest=std::get<MotionSource>(selected[i]).sha256;
            if(digest.size()!=64)return error(PlayerErrorCode::bank,"Motion source requires a complete SHA256 identity");
            const lmt::Resource* found=nullptr;
            for(const auto& resource:data.motions)if(resource.sha256()==digest){
                if(found)return error(PlayerErrorCode::bank,"Motion source digest is ambiguous among separately owned resources");found=&resource;
            }
            if(!found)return error(PlayerErrorCode::bank,"Motion source is not owned by this actor recipe");
            banks.banks[i]=found->request_resource();
        }
        return Player{actor.data_,std::get<pa::State>(std::move(state)),constants,std::move(banks),limits,initial.shared_fields,initial.state1_fields,initial.position_fields};
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<pa::State> Player::rest_state(const Actor::Data& data,const PlayerInitial& initial,pa::Limits limits){
    try{
        const auto n=data.rest.joints.size();
        if(n>limits.joints||initial.snapshots.size()>limits.snapshots)return capacity();
        if(initial.snapshots.size()!=n)return error(PlayerErrorCode::initial_state,"Explicit snapshots must match the real MOD joint extent");
        if(const auto* order=std::get_if<std::vector<std::uint8_t>>(&data.rest.order);order&&order->size()>limits.order)return capacity();
        std::size_t bindings=0;
        for(std::size_t i=0;i<8;++i){const auto count=initial.layers[i].animation.tracks.joints.size();
            if(count>limits.bindings-bindings)return capacity();bindings+=count;
            if(i<2&&count!=n)return error(PlayerErrorCode::initial_state,"Both explicit active binding tables must match MOD joints");
        }
        pa::State state{};state.frame.model=initial.model;state.controller=initial.controller;
        state.frame.animation.layers.assign(initial.layers.begin(),initial.layers.begin()+2);
        for(std::size_t i=0;i<6;++i)state.inactive[i]=initial.layers[i+2];
        state.frame.animation.snapshots=initial.snapshots;state.frame.animation.calculation_flags=initial.calculation_flags;
        state.frame.animation.accumulated_flags=initial.accumulated_flags;
        auto& h=state.hierarchy;const auto& rest=data.rest;
        h.local_rt=rest.local_rt;h.world=rest.world;h.flags4=rest.flags4;h.flags104=rest.flags104;h.flags230=rest.flags230;
        h.attachment_index=rest.attachment_index;h.attachment_mode_high16=std::uint16_t(rest.attachment_mode>>16);h.dirty=rest.dirty;h.fieldc4c=rest.fieldc4c;h.order=rest.order;
        for(const auto& joint:rest.joints){state.frame.animation.joints.push_back(joint.pose);
            h.joints.push_back({joint.metadata,joint.matrix,joint.depth,joint.callback,joint.owner});}
        return state;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<Player> Player::from_rest(const Player& owner,const PlayerInitial& initial){
    try{
        if(!owner.data_)return error(PlayerErrorCode::initial_state,"Player resource owner has been moved away");
        auto state=rest_state(*owner.data_,initial,owner.limits_);
        if(auto* e=std::get_if<PlayerError>(&state))return *e;
        return Player{owner.data_,std::get<pa::State>(std::move(state)),owner.constants_,owner.banks_,owner.limits_,initial.shared_fields,initial.state1_fields,initial.position_fields};
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
std::shared_ptr<const Scene> Player::presentation()const noexcept{return data_->scene;}
std::span<const std::filesystem::path> Player::source_files()const noexcept{return data_->source_files;}
PlayerResult<Frame> Player::render(const pa::State& state,std::uint64_t completed,std::size_t samples,std::uint64_t revision)const{
    Frame frame;frame.tick=completed;frame.samples=samples;
    const auto& layer=state.frame.animation.layers[0].animation.layer;
    frame.source_frame=layer.prepared_frame;frame.event_masks=layer.masks;frame.ended=(layer.attributes&4U)&&(layer.flags&4U);
    auto result=data_->render(std::move(frame),Actor::Data::Matrices{std::span<const model_animation_output::JointOutput>(state.hierarchy.joints)},revision);
    if(auto* e=std::get_if<Error>(&result))return PlayerError{PlayerErrorCode::render,"Player palette/skinning: "+e->message,{},*e};
    return std::get<Frame>(std::move(result));
}
PlayerResult<Frame> Player::capture()const{return render(state_,completed_steps_,samples_,revision_);}
PlayerResult<Frame> Player::begin_update(){
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        auto result=player_update_begin::apply(state_.controller.player,shared_fields_);
        if(auto* e=std::get_if<player_update_begin::Error>(&result))return PlayerError{
            PlayerErrorCode::update_begin,"Player update begin requires "+std::string(e->field),{},{},{},{},*e};
        auto updated=std::get<player_update_begin::Output>(std::move(result));
        auto candidate=state_;candidate.controller.player=std::move(updated.player);
        auto rendered=render(candidate,completed_steps_,0,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        state_=std::move(candidate);shared_fields_=std::move(updated.fields);samples_=0;++revision_;
        return std::get<Frame>(std::move(rendered));
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerInputFilterOutput> Player::filter_input(const player_input_filter::Context& context){
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        auto result=player_input_filter::apply(state_.controller.player,shared_fields_,context);
        if(auto* e=std::get_if<player_input_filter::Error>(&result))return PlayerError{
            PlayerErrorCode::input_filter,"Player input filter failed at "+std::string(e->field),{},{},{},{},{},*e};
        auto updated=std::get<player_input_filter::Output>(std::move(result));
        auto candidate=state_;candidate.controller.player=std::move(updated.player);
        auto rendered=render(candidate,completed_steps_,0,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerInputFilterOutput output{std::get<Frame>(std::move(rendered)),updated.effects};
        state_=std::move(candidate);shared_fields_=std::move(updated.fields);samples_=0;++revision_;
        return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<Frame> Player::update_accumulator(std::optional<std::uint32_t> delta_bits){
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        auto result=player_update_accumulator::apply(shared_fields_,delta_bits);
        if(auto* e=std::get_if<player_update_accumulator::Error>(&result))return PlayerError{
            PlayerErrorCode::update_accumulator,"Player accumulator failed at "+std::string(e->field),{},{},{},{},{},{},*e};
        auto updated=std::get<player_update_accumulator::Output>(std::move(result));
        auto rendered=render(state_,completed_steps_,0,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        shared_fields_=std::move(updated.fields);samples_=0;++revision_;
        return std::get<Frame>(std::move(rendered));
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerPreparationOutput> Player::prepare_update(const player_update_prefix::Context& context){
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        auto result=player_update_prefix::apply(state_.controller.player,shared_fields_,context);
        if(auto* e=std::get_if<player_update_prefix::Error>(&result)){
            auto failure=error(PlayerErrorCode::update_prefix,"Player update prefix failed at "+
                std::visit([](const auto& cause){return std::string(cause.field);},e->cause));
            failure.update_prefix=*e;return failure;
        }
        auto updated=std::get<player_update_prefix::Output>(std::move(result));
        auto candidate=state_;candidate.controller.player=std::move(updated.player);
        auto rendered=render(candidate,completed_steps_,0,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerPreparationOutput output{std::get<Frame>(std::move(rendered)),updated.filter,std::move(updated.lookup)};
        state_=std::move(candidate);shared_fields_=std::move(updated.fields);samples_=0;++revision_;
        return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerItemPopOutput> Player::update_item_pop(player_world_lookup::Manager& manager,
    const item_pop_update::Context& context){
    static_assert(std::is_nothrow_move_assignable_v<player_world_lookup::Manager>);
    static_assert(std::is_nothrow_move_assignable_v<player_transition::GlobalFields>);
    static_assert(std::is_nothrow_move_constructible_v<PlayerResult<PlayerItemPopOutput>>);
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        // Pass the original borrowed Manager identity, never a replacement or
        // inferred singleton. The pure updater owns lazy dependency checks.
        auto result=item_pop_update::apply(manager,state_.controller.player.global,context);
        if(auto* e=std::get_if<item_pop_update::Error>(&result)){
            auto failure=error(PlayerErrorCode::item_pop_update,"Item-pop update failed at "+std::string(e->field));
            failure.item_pop_update=*e;return failure;
        }
        auto updated=std::get<item_pop_update::Output>(std::move(result));
        // Neither the global fields nor external Manager change this pose.
        auto rendered=render(state_,completed_steps_,samples_,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerResult<PlayerItemPopOutput> output{PlayerItemPopOutput{
            std::get<Frame>(std::move(rendered)),updated.visited,updated.clock_gated,updated.effects}};
        // Every potentially throwing operation is complete. The two owners
        // publish together within this synchronous call; no handoff remains.
        manager=std::move(updated.manager);
        state_.controller.player.global=std::move(updated.global);
        ++revision_;
        return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerAdmissionOutput> Player::prepare_to_admission(player_object_query::Collection& objects,
    const player_update_admission::Parameters& parameters){
    namespace admission=player_update_admission;
    static_assert(std::is_nothrow_move_assignable_v<player_object_query::Collection>);
    static_assert(std::is_nothrow_move_assignable_v<pa::State>);
    static_assert(std::is_nothrow_move_assignable_v<player_fields::Fields>);
    static_assert(std::is_nothrow_move_constructible_v<PlayerResult<PlayerAdmissionOutput>>);
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        auto result=admission::apply(state_.controller,state_.frame.animation.layers,shared_fields_,
            &objects,{parameters,constants_});
        if(auto* e=std::get_if<admission::Error>(&result)){
            auto failure=error(e->code==admission::ErrorCode::capacity?PlayerErrorCode::capacity:
                PlayerErrorCode::update_admission,"Player admission failed at "+std::string(e->field));
            failure.update_admission=*e;return failure;
        }
        auto updated=std::get<admission::Output>(std::move(result));
        auto candidate=state_;
        candidate.controller=std::move(updated.state);
        candidate.frame.animation.layers=std::move(updated.layers);
        auto rendered=render(candidate,completed_steps_,samples_,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerResult<PlayerAdmissionOutput> output{PlayerAdmissionOutput{
            std::get<Frame>(std::move(rendered)),std::move(updated.effects)}};
        // Every allocation, dependency and rendering step has succeeded.
        // These final moves cannot throw, so no partially published owner
        // can escape this synchronous transaction.
        if(updated.collection)objects=std::move(*updated.collection);
        state_=std::move(candidate);shared_fields_=std::move(updated.fields);++revision_;
        return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerDispatchOutput> Player::prepare_to_dispatch(player_object_query::Collection& objects,
    const player_update_admission::Parameters& admission,const player_update_statistics::Context& statistics){
    namespace continuation=player_update_continuation;
    static_assert(std::is_nothrow_move_assignable_v<player_object_query::Collection>);
    static_assert(std::is_nothrow_move_assignable_v<pa::State>);
    static_assert(std::is_nothrow_move_assignable_v<player_fields::Fields>);
    static_assert(std::is_nothrow_move_constructible_v<PlayerResult<PlayerDispatchOutput>>);
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        auto result=continuation::apply(state_.controller,state_.frame.animation.layers,shared_fields_,
            &objects,{admission,statistics,constants_});
        if(auto* e=std::get_if<continuation::Error>(&result)){
            auto failure=error(e->code==continuation::ErrorCode::capacity?PlayerErrorCode::capacity:
                PlayerErrorCode::update_continuation,"Player continuation failed at "+std::string(e->field));
            failure.update_continuation=*e;return failure;
        }
        auto completed=std::get<continuation::Output>(std::move(result));
        auto& prepared=completed.prepared;
        auto candidate=state_;
        candidate.controller=std::move(prepared.state);
        candidate.frame.animation.layers=std::move(prepared.layers);
        auto rendered=render(candidate,completed_steps_,samples_,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerResult<PlayerDispatchOutput> output{PlayerDispatchOutput{
            std::get<Frame>(std::move(rendered)),std::move(prepared.effects),
            completed.statistics,completed.returns}};
        // Neither the admission checkpoint nor the parameter-query prefix has
        // been published. All later dependencies and rendering have succeeded;
        // the remaining owner moves and result move cannot throw.
        if(prepared.collection)objects=std::move(*prepared.collection);
        state_=std::move(candidate);shared_fields_=std::move(prepared.fields);++revision_;
        return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerUpdateDispatchOutput> Player::prepare_and_dispatch(player_object_query::Collection& objects,
    const PlayerUpdateDispatchContext& context){
    return prepare_dispatch<PlayerUpdateDispatchOutput>(objects,context,{});
}
PlayerResult<PlayerUpdateAdjustmentOutput> Player::prepare_and_adjust(player_object_query::Collection& objects,
    const PlayerUpdateDispatchContext& context,std::optional<player_update_post_dispatch::Virtual10c> binding){
    return prepare_dispatch<PlayerUpdateAdjustmentOutput>(objects,context,binding);
}
PlayerResult<PlayerUpdateStatusOutput> Player::prepare_and_update_status(player_object_query::Collection& objects,
    const PlayerUpdateDispatchContext& context,std::optional<player_update_post_dispatch::Virtual10c> binding){
    return prepare_dispatch<PlayerUpdateStatusOutput>(objects,context,binding);
}
PlayerResult<PlayerUpdateNotificationsOutput> Player::prepare_and_update_notifications(player_object_query::Collection& objects,
    notification_queue::State& queue,const PlayerUpdateDispatchContext& context,
    std::optional<player_update_post_dispatch::Virtual10c> binding){
    return prepare_dispatch<PlayerUpdateNotificationsOutput>(objects,context,binding,&queue);
}
PlayerResult<PlayerUpdateDirectionOutput> Player::prepare_and_update_direction(player_object_query::Collection& objects,
    notification_queue::State& queue,const PlayerUpdateDispatchContext& context,
    const player_direction_refresh::Bindings& direction,std::optional<player_update_post_dispatch::Virtual10c> binding){
    return prepare_dispatch<PlayerUpdateDirectionOutput>(objects,context,binding,&queue,&direction);
}
template<class Output>PlayerResult<Output> Player::prepare_dispatch(player_object_query::Collection& objects,
    const PlayerUpdateDispatchContext& context,std::optional<player_update_post_dispatch::Virtual10c> binding,
    notification_queue::State* queue,const player_direction_refresh::Bindings* direction_bindings){
    static_assert(std::is_same_v<Output,PlayerUpdateDispatchOutput>||std::is_same_v<Output,PlayerUpdateAdjustmentOutput>||
        std::is_same_v<Output,PlayerUpdateStatusOutput>||std::is_same_v<Output,PlayerUpdateNotificationsOutput>||
        std::is_same_v<Output,PlayerUpdateDirectionOutput>);
    namespace sq=spatial_query;
    static_assert(std::is_nothrow_move_assignable_v<player_object_query::Collection>);
    static_assert(std::is_nothrow_move_assignable_v<sq::GridState>);
    static_assert(std::is_nothrow_move_assignable_v<pa::State>);
    static_assert(std::is_nothrow_move_assignable_v<player_fields::Fields>);
    static_assert(std::is_nothrow_move_assignable_v<player_state1::Fields>);
    static_assert(std::is_nothrow_move_assignable_v<notification_queue::State>);
    static_assert(std::is_nothrow_move_constructible_v<PlayerResult<Output>>);
    const auto grid_error=[](GridPublicationCode code,std::optional<std::uint32_t> slot={}){
        auto out=error(PlayerErrorCode::grid_publication,"Player grid publication plan is outside its explicit profile");
        out.grid_publication=GridPublicationError{code,slot};return out;
    };
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        const Resources resources{*data_,banks_};const mi::Resolver resolver{&resources,Resources::track};
        const motion_command::Globals unknown{};const auto& globals=data_->command_globals?*data_->command_globals:unknown;
        const player_update_dispatch::Resources dispatch{context.tables,banks_,globals,{context.templates,resolver},
            context.area_mapping,context.area_categories,{context.spatial_scene,context.area_enable_mask,context.query_limits}};
        const player_update_dispatch::Context dispatch_context{{context.admission,context.statistics,constants_},dispatch};
        player_update_post_dispatch::Effects adjustment{};
        player_status_update::Effects status{};
        player_status_notifications::Effects notifications{};
        player_update_direction::MaterialEffects material{};
        player_direction_refresh::Effects direction{};
        std::optional<notification_queue::State> queue_candidate;
        // Select one typed checkpoint. Each completed phase is unwrapped once
        // below, sharing resource bindings and the publication boundary.
        auto result=[&](){
            if constexpr(std::is_same_v<Output,PlayerUpdateDirectionOutput>){
                const player_update_post_dispatch::Context preceding{dispatch_context,binding};
                return pa::update_direction(state_,state1_fields_,shared_fields_,&objects,
                    {preceding,*direction_bindings},*queue,limits_);
            }else if constexpr(std::is_same_v<Output,PlayerUpdateNotificationsOutput>)
                return pa::update_notifications(state_,state1_fields_,shared_fields_,&objects,{dispatch_context,binding},*queue,limits_);
            else if constexpr(std::is_same_v<Output,PlayerUpdateStatusOutput>)
                return pa::update_status(state_,state1_fields_,shared_fields_,&objects,{dispatch_context,binding},limits_);
            else if constexpr(std::is_same_v<Output,PlayerUpdateAdjustmentOutput>)
                return pa::adjust_update(state_,state1_fields_,shared_fields_,&objects,{dispatch_context,binding},limits_);
            else return pa::dispatch_update(state_,state1_fields_,shared_fields_,&objects,dispatch_context,limits_);
        }();
        if(auto* e=std::get_if<pa::Error>(&result))return PlayerError{PlayerErrorCode::driver,
            "Player update dispatch phase "+std::to_string(unsigned(e->phase)),*e,{}};
        const auto unwrap=[&]<class Candidate>(auto&& self,Candidate candidate)->pa::UpdateDispatchOutput{
            if constexpr(std::is_same_v<Candidate,pa::UpdateDirectionOutput>){
                material=std::move(candidate.material);direction=std::move(candidate.effects);
                return self(self,std::move(candidate.notified));
            }else if constexpr(std::is_same_v<Candidate,pa::UpdateNotificationsOutput>){
                queue_candidate=std::move(candidate.queue);notifications=std::move(candidate.effects);
                return self(self,std::move(candidate.status));
            }else if constexpr(std::is_same_v<Candidate,pa::UpdateStatusOutput>){
                status=std::move(candidate.effects);return self(self,std::move(candidate.adjusted));
            }else if constexpr(std::is_same_v<Candidate,pa::UpdateAdjustmentOutput>){
                adjustment=std::move(candidate.effects);return self(self,std::move(candidate.dispatched));
            }else{
                static_assert(std::is_same_v<Candidate,pa::UpdateDispatchOutput>);
                return candidate;
            }
        };
        using Candidate=std::variant_alternative_t<0,decltype(result)>;
        auto updated=unwrap(unwrap,std::get<Candidate>(std::move(result)));
        const sq::HeightResult* height=nullptr;std::size_t samples=0;
        for(std::size_t i=0;i<updated.effects.dispatch_count;++i){
            const auto& effects=updated.effects.dispatches[i].effects;
            if(effects.samples>std::numeric_limits<std::size_t>::max()-samples)return capacity();
            samples+=effects.samples;
            if(effects.spatial&&effects.spatial->height){
                if(height)return grid_error(GridPublicationCode::query_profile);
                height=&*effects.spatial->height;
            }
        }
        // Build only the changed slots. Effects keep their own complete result;
        // no entire Scene copy, re-query or publication occurs before render.
        std::vector<sq::GridUpdate> grid_plan;
        if(height&&!height->query.grid_updates.empty()){
            if(!context.spatial_scene)return grid_error(GridPublicationCode::missing_scene);
            const auto& updates=height->query.grid_updates;
            std::size_t words=0;
            for(const auto& update:updates){
                if(update.slot>=context.spatial_scene->grid_states.size())return grid_error(GridPublicationCode::invalid_slot,update.slot);
                const auto& current=context.spatial_scene->grid_states[update.slot];
                if(current.resource.sha256()!=update.state.resource.sha256()||current.group!=update.state.group||
                    current.pair_marks.size()!=update.state.pair_marks.size())return grid_error(GridPublicationCode::source_binding,update.slot);
                if(update.state.pair_marks.size()>context.query_limits.runtime_words-words)return grid_error(GridPublicationCode::resource_limit,update.slot);
                words+=update.state.pair_marks.size();
            }
            grid_plan=updates;
            std::sort(grid_plan.begin(),grid_plan.end(),[](const auto& a,const auto& b){return a.slot<b.slot;});
            for(std::size_t i=1;i<grid_plan.size();++i)if(grid_plan[i-1].slot==grid_plan[i].slot)
                return grid_error(GridPublicationCode::duplicate_slot,grid_plan[i].slot);
        }
        auto rendered=render(updated.state,completed_steps_,samples,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerResult<Output> output=[&]()->Output{
            if constexpr(std::is_same_v<Output,PlayerUpdateDirectionOutput>)
                return {std::get<Frame>(std::move(rendered)),std::move(updated.effects),std::move(adjustment),std::move(status),std::move(notifications),std::move(material),std::move(direction)};
            else if constexpr(std::is_same_v<Output,PlayerUpdateNotificationsOutput>)
                return {std::get<Frame>(std::move(rendered)),std::move(updated.effects),std::move(adjustment),std::move(status),std::move(notifications)};
            else if constexpr(std::is_same_v<Output,PlayerUpdateStatusOutput>)
                return {std::get<Frame>(std::move(rendered)),std::move(updated.effects),std::move(adjustment),std::move(status)};
            else if constexpr(std::is_same_v<Output,PlayerUpdateAdjustmentOutput>)
                return {std::get<Frame>(std::move(rendered)),std::move(updated.effects),std::move(adjustment)};
            else return {std::get<Frame>(std::move(rendered)),std::move(updated.effects)};
        }();
        // No allocation, validation or throwing move remains after the first
        // publication. Every reached mutable owner belongs to this operation.
        if constexpr(std::is_same_v<Output,PlayerUpdateNotificationsOutput>||std::is_same_v<Output,PlayerUpdateDirectionOutput>)
            *queue=std::move(*queue_candidate);
        for(auto& update:grid_plan)context.spatial_scene->grid_states[update.slot]=std::move(update.state);
        if(updated.collection)objects=std::move(*updated.collection);
        state_=std::move(updated.state);shared_fields_=std::move(updated.shared_fields);
        state1_fields_=std::move(updated.action_fields);samples_=samples;++revision_;
        return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerPolicyOutput> Player::apply_policy(const PlayerPolicyInput& input,const PlayerPolicyContext& context){
    namespace pcs=player_controller_step;namespace ppi=player_policy_interpreter;
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        // State is private and all producing boundaries enforce the 2/8 profile
        // and cumulative storage limits before publication. Projection is fixed size.
        auto projected=pcs::selection_view(state_.controller,state_.frame.animation.layers);
        if(auto* e=std::get_if<pcs::Error>(&projected))return PlayerError{PlayerErrorCode::initial_state,
            "Player policy requires the canonical two active layers",pa::Error{pa::Phase::controller,*e},{}};
        auto selection=std::get<player_selection::State>(std::move(projected));auto fields=shared_fields_;
        if(input.channels){selection.transition.pbw.angles=input.channels->angles;selection.transition.pbw.magnitudes=input.channels->magnitudes;}
        if(input.field3d4)selection.transition.pbw.field3d4=*input.field3d4;
        for(std::size_t i=0;i<19;++i)if(input.flags3d8[i])
            fields.pbw.words3d8[i]=player_input_storage::split(input.flags3d8[i]);
        std::optional<std::uint16_t>* masks[]{&fields.pbw.field3a8,&fields.pbw.field3ac,&fields.pbw.field3b8,&fields.pbw.field3bc};
        for(std::size_t i=0;i<4;++i)if(input.masks[i])*masks[i]=input.masks[i];
        const ppi::Context facts{state_.frame.animation.layers[0].animation.layer,state_.controller.motion,
            context.delta_time_bits,constants_,context.reference,context.context_ad24,context.global_c8c,context.direction_bounds};
        auto result=ppi::apply(selection,fields,facts);
        if(auto* e=std::get_if<ppi::Error>(&result))return PlayerError{PlayerErrorCode::policy,
            "Player policy failed at "+std::string(e->field),{},{},*e};
        auto decision=std::get<ppi::Output>(std::move(result));auto candidate=state_;
        if(auto e=pcs::store_selection(candidate.controller,candidate.frame.animation.layers,decision.state))
            return PlayerError{PlayerErrorCode::initial_state,"Player policy projection extent",pa::Error{pa::Phase::controller,*e},{}};
        auto rendered=render(candidate,completed_steps_,0,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerPolicyOutput output{std::get<Frame>(std::move(rendered)),decision.returned,
            decision.visited,decision.visited_count,std::move(decision.transition)};
        state_=std::move(candidate);shared_fields_=std::move(decision.fields);samples_=0;++revision_;
        return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerOutput> Player::apply(const pa::Command& command,const mi::Templates& templates,const player_selection::AreaContext& area){return execute(&command,templates,nullptr,area);}
PlayerResult<PlayerState1Output> Player::dispatch_state1(const player_state1::Tables& tables,const mi::Templates& templates,
    const PlayerState1Context& context){
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max())return capacity();
        const Resources resources{*data_,banks_};const mi::Resolver resolver{&resources,Resources::track};
        const motion_command::Globals unknown{};const auto& globals=data_->command_globals?*data_->command_globals:unknown;
        const player_state1::ContinuationContext continuation{&constants_,&shared_fields_,
            context.reference,context.area,context.delta_time_bits,context.context_ad50,context.manager,context.spatial};
        auto result=pa::dispatch_state1(state_,state1_fields_,{tables,banks_,globals,{templates,resolver},continuation},limits_);
        if(auto* e=std::get_if<pa::Error>(&result))return PlayerError{PlayerErrorCode::driver,
            "Player state1 dispatch phase "+std::to_string(unsigned(e->phase)),*e,{}};
        auto updated=std::get<pa::State1Output>(std::move(result));
        auto rendered=render(updated.state,completed_steps_,updated.effects.samples,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerState1Output output{std::get<Frame>(std::move(rendered)),std::move(updated.effects)};
        state_=std::move(updated.state);state1_fields_=std::move(updated.fields);shared_fields_=std::move(updated.shared_fields);
        samples_=output.effects.samples;++revision_;return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerOutput> Player::step(const PlayerStep& frame){return execute(nullptr,frame.templates,&frame,{});}
PlayerResult<PlayerPositionOutput> Player::step_position(const PlayerStep& frame,
    std::optional<player_position_bridge::OrientationCallback> orientation,
    std::optional<player_position_bridge::OutputCallbacks> publication){
    namespace pb=player_position_bridge;
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max()||completed_steps_==std::numeric_limits<std::uint64_t>::max())return capacity();
        const Resources resources{*data_,banks_};const auto parameters=resources.parameters(frame);
        auto result=pb::apply(state_.frame,state_.hierarchy,state_.controller,shared_fields_,position_fields_,{parameters,orientation,publication},limits_);
        if(auto* e=std::get_if<pb::Error>(&result))return PlayerError{PlayerErrorCode::position,"Player position failed at "+std::string(e->field),{},{},{},*e};
        auto updated=std::get<pb::Output>(std::move(result));
        pa::State candidate{std::move(updated.frame),state_.inactive,std::move(updated.controller),std::move(updated.hierarchy)};
        const auto& animation=updated.effects.animation;
        const auto samples=animation?animation->model_samples+animation->joint_samples:0U;
        const auto completed=completed_steps_+(animation?1U:0U);
        auto rendered=render(candidate,completed,samples,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerPositionOutput output{std::get<Frame>(std::move(rendered)),std::move(updated.effects)};
        state_=std::move(candidate);shared_fields_=std::move(updated.shared);position_fields_=std::move(updated.fields);
        samples_=samples;completed_steps_=completed;++revision_;return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
PlayerResult<PlayerOutput> Player::tick(const pa::Command& command,const PlayerStep& frame,const player_selection::AreaContext& area){return execute(&command,frame.templates,&frame,area);}
PlayerResult<PlayerOutput> Player::execute(const pa::Command* command,const mi::Templates& templates,
    const PlayerStep* frame,const player_selection::AreaContext& area){
    try{
        if(revision_==std::numeric_limits<std::uint64_t>::max()||(frame&&completed_steps_==std::numeric_limits<std::uint64_t>::max()))return capacity();
        const Resources resources{*data_,banks_};const mi::Resolver resolver{&resources,Resources::track};
        const motion_command::Globals unknown{};const auto& globals=data_->command_globals?*data_->command_globals:unknown;
        const pa::CommandContext commands{banks_,globals,constants_,{templates,resolver},area};
        pa::Result result;
        if(frame){
            const auto parameters=resources.parameters(*frame);
            result=command?pa::tick(state_,*command,commands,parameters,limits_):pa::step(state_,parameters,limits_);
        }else result=pa::apply(state_,*command,commands,limits_);
        if(auto* e=std::get_if<pa::Error>(&result))return PlayerError{PlayerErrorCode::driver,"Player animation phase "+std::to_string(unsigned(e->phase)),*e,{}};
        auto updated=std::get<pa::Output>(std::move(result));std::size_t samples=updated.command?updated.command->samples:0;
        if(updated.frame)samples+=updated.frame->model_samples+updated.frame->joint_samples;
        auto rendered=render(updated.state,completed_steps_+(frame?1U:0U),samples,revision_+1);
        if(auto* e=std::get_if<PlayerError>(&rendered))return *e;
        PlayerOutput output{std::get<Frame>(std::move(rendered)),std::move(updated.command),std::move(updated.frame)};
        state_=std::move(updated.state);samples_=samples;++revision_;if(frame)++completed_steps_;
        return output;
    }catch(const std::bad_alloc&){return capacity();}catch(const std::length_error&){return capacity();}
}
}
