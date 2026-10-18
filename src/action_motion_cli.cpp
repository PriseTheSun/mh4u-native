#include "action_motion.hpp"
#ifdef MH4U_HAS_MODEL_MOTION
#include "model_motion_identity.hpp"
#endif
#ifdef MH4U_HAS_MOTION_EVENTS
#include "motion_events_identity.hpp"
#endif
#include <charconv>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <stdexcept>
#include <string>

namespace {
template<class T>T number(const std::string& value) {
    T result{};const auto [end,error]=std::from_chars(value.data(),value.data()+value.size(),result);
    if(error!=std::errc{}||end!=value.data()+value.size())throw std::runtime_error("invalid_number");return result;
}
std::filesystem::path path(const std::string& value) {
    return std::filesystem::path(std::u8string(value.begin(),value.end()));
}
template<std::size_t N>void values(const std::array<float,N>& v) {for(const auto x:v)std::cout<<'\t'<<x;}
}
int main(int argc,char** argv) {
    try {
        std::filesystem::path binary,root,events_binary;
        int action=-1,ticks=1;float speed=1,start=0;bool repeat=false;
        for(int i=1;i<argc;++i) {
            const std::string option=argv[i];
            if(option=="--repeat"){repeat=true;continue;}
            if(option=="--help") {std::cout<<"mh4u-action-motion --package model-motion.bin --paired-root PATH --action INDEX --ticks N [--step SOURCE_FRAMES --start SOURCE_FRAME --repeat --events-package motion-events.bin]\nExplicit diagnostic state; optional event masks without callbacks or gameplay. Original timing policy remains unknown.\n";return 0;}
            if(i+1==argc)throw std::runtime_error("missing_option_value");const std::string value=argv[++i];
            if(option=="--package")binary=path(value);else if(option=="--paired-root")root=path(value);
            else if(option=="--events-package")events_binary=path(value);
            else if(option=="--action")action=number<int>(value);else if(option=="--ticks")ticks=number<int>(value);
            else if(option=="--step")speed=number<float>(value);else if(option=="--start")start=number<float>(value);
            else throw std::runtime_error("unknown_option");
        }
        if(binary.empty()||root.empty()||action<0||ticks<0||ticks>100000||!std::isfinite(speed)||!std::isfinite(start))throw std::runtime_error("invalid_options");
#ifdef MH4U_HAS_MODEL_MOTION
        auto loaded=mh4u::model_motion::Package::load(binary,mh4u::model_motion::generated::expected_package(),root);
        if(const auto* error=std::get_if<mh4u::model_motion::Error>(&loaded))throw std::runtime_error(error->message);
        const auto& package=std::get<mh4u::model_motion::Package>(loaded);
        if(static_cast<std::size_t>(action)>=package.actions().size())throw std::runtime_error("action_range");
        std::optional<mh4u::motion_events::Package> events;
        if(!events_binary.empty()) {
#ifdef MH4U_HAS_MOTION_EVENTS
            auto parsed=mh4u::motion_events::Package::load(events_binary,mh4u::motion_events::generated::expected_package());
            if(const auto* error=std::get_if<mh4u::motion_events::Error>(&parsed))throw std::runtime_error(error->message);
            events.emplace(std::move(std::get<mh4u::motion_events::Package>(parsed)));
#else
            throw std::runtime_error("local_motion_events_identity_not_generated");
#endif
        }
        mh4u::action_motion::Player player(package,package.actions()[action].identity,{start,speed,repeat,{{0,0,0},{0,0,0,1},{1,1,1},{1,1,1}}},events?&*events:nullptr);
        const auto& a=player.action();
        std::cout<<"PROFILE\t"<<(events?"diagnostic-one-layer-events-v1":"diagnostic-one-layer-v1")<<"\nACTION\t"<<a.identity.entry_id<<'\t'<<a.identity.slot<<'\t'<<a.gltf_clip_index<<"\n";
        std::cout<<std::setprecision(9);
        const auto report=[&](int tick,const mh4u::action_motion::Frame& frame) {
            const auto& p=frame.state.pre;const auto& c=p.clock;
            std::cout<<"TICK\t"<<tick<<'\t'<<frame.joint_source_frame<<'\t'<<c.frame<<'\t'<<c.internal_flags<<'\t'<<c.previous_flags<<'\t'<<frame.loop_iterations;
            values(p.model.position);values(p.model.rotation);values(c.previous_translation);values(c.previous_rotation);
            values(frame.state.prediction_delta.translation);values(frame.state.prediction_delta.rotation);std::cout<<'\n';
            if(events){std::cout<<"EVENTS\t"<<tick;for(auto v:c.event_masks)std::cout<<'\t'<<v;for(auto v:c.previous_event_masks)std::cout<<'\t'<<v;std::cout<<'\n';}
        };
        report(0,player.current());for(int i=1;i<=ticks;++i)report(i,player.tick());
        return 0;
#else
        throw std::runtime_error("local_motion_identity_not_generated");
#endif
    } catch(const std::exception& error) {std::cerr<<"ERROR "<<error.what()<<'\n';return 1;}
}
