#include "player_position_bridge.hpp"
#include <bit>
#include <cfenv>
#include <iostream>
#include <stdexcept>

namespace {
namespace pb=mh4u::player_position_bridge;
namespace pa=mh4u::player_animation;
namespace jp=mh4u::joint_pose;
using Word=std::uint32_t;
constexpr Word one=0x3f800000U;
std::size_t checks{};
void check(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
Word bits(float f){return std::bit_cast<Word>(f);}
template<class T,class... U>T take(std::variant<U...> result){
    if(auto* value=std::get_if<T>(&result))return std::move(*value);
    if(auto* error=std::get_if<pb::Error>(&result)){
        std::string detail;
        if(error->publication)std::visit([&](const auto& e){detail=" publication: "+std::string(e.message);},error->publication->cause);
        throw std::runtime_error("Unexpected error at "+std::string(error->field)+" code "+std::to_string(int(error->code))+detail);
    }
    throw std::runtime_error("Unexpected success");
}
struct Fixture {
    mh4u::model_animation_frame::State frame;
    mh4u::model_animation_output::Hierarchy hierarchy;
    mh4u::player_controller_step::State controller;
    mh4u::player_fields::Fields shared;
    pb::Fields fields;
    pa::FrameContext context;
    Fixture(){
        frame.animation.layers.resize(1);auto& layer=frame.animation.layers[0].animation.layer;
        layer.frame=3;layer.previous_frame=2;layer.speed=2;
        frame.model.position={bits(99),bits(98),bits(97),0x7fc00001};
        frame.model.rotation=frame.model.rotation_delta={0,0,0,one};frame.model.scale=frame.model.null_scale={one,one,one,0};
        hierarchy.flags4=0x50;hierarchy.order=std::vector<std::uint8_t>{};
        controller.player.pbw.field20=0xabcd0000;controller.player.pbw.field24=0x12340000;controller.player.pbw.field28=0xffff0000;
        controller.player.fieldsd10={bits(1),bits(2),bits(3)};
        controller.motion.fieldsd48[4]=bits(5);
        shared.pbw.field10_bits=bits(17);shared.pbw.field14_bits=bits(-23);shared.pbw.field18_bits=bits(91);shared.pbw.field1c_bits=0x7fc00002;
        shared.pbw.field478=0x12345678;shared.pw.field838_bits=0x7fc00003;
        fields={std::uint8_t(1),std::uint8_t(77),bits(0),bits(0),one,2U,std::uint16_t(4)};
        context.pending.model_reference=jp::Null{};context.pending.initialization.model_delta_time_bits=one;
        context.output.model=jp::Null{};context.output.parent=jp::Null{};
        context.output.reference_basis=mh4u::motion_transform_math::Basis{one,0,0,0,0,one,0,0,0,0,one,0};
        context.output.reference_translation=jp::Bits4{0,0,0,one};
    }
    pb::Result run(std::optional<pb::OrientationCallback> orientation=pb::OrientationCallback::b05328,
                   std::optional<pb::OutputCallbacks> publication=pb::OutputCallbacks::standard_95b490_95b940,
                   pa::Limits limits={})const{
        return pb::apply(frame,hierarchy,controller,shared,fields,{context,orientation,publication},limits);
    }
};
void run(){
    Fixture f;const auto out=take<pb::Output>(f.run());
    check(out.effects.publication_skipped&&out.effects.animation&&!out.effects.early_return,"Source publication gate");
    check(out.frame.model.position==mh4u::motion_transform_math::Bits4{bits(17),bits(-23),bits(91),0},"PBW copy before frame");
    check(out.shared.pbw.field1c_bits==0&&out.shared.pbw.field10_bits==bits(17),"PBW suffix writes W");
    check(out.hierarchy.flags4==0x850,"d2c flag update");
    check(out.controller.player.pbw.field20==0&&out.controller.player.pbw.field24==0&&out.controller.player.pbw.field28==0,"Source angle narrowing");
    check(out.controller.motion.fieldsd48[1]==bits(2)&&out.controller.motion.fieldsd48[2]==bits(3),"Prior frame copies");
    check(out.controller.motion.fieldsd48[3]==bits(5)&&out.controller.motion.fieldsd48[4]==bits(2),"Source step history");
    check(out.fields.fieldd44==0&&out.shared.pbw.field478==f.shared.pbw.field478&&out.shared.pw==f.shared.pw,"Untouched facts retained");
    check(f.frame.model.position[0]==bits(99)&&f.shared.pbw.field1c_bits==0x7fc00002&&f.controller.player.pbw.field20==0xabcd0000,"Input unchanged");
    auto early=f;early.fields.fieldd64=8;early.fields.fieldd2c=0;early.fields.euler_mode.reset();early.fields.fieldd30_bits.reset();early.fields.fieldd34_bits.reset();early.fields.fieldd38_bits.reset();early.frame.animation.layers.clear();early.shared.pbw.field10_bits=0x7fc00008;
    const auto skipped=take<pb::Output>(early.run({},{}));
    check(skipped.effects.early_return&&!skipped.effects.animation&&skipped.frame.model==early.frame.model,"Early return ignores frame/orientation");
    check(skipped.controller.player==early.controller.player&&skipped.controller.motion==early.controller.motion&&skipped.shared.pbw.field10_bits==0x7fc00008&&skipped.shared.pbw.field1c_bits==0,"Early raw PBW copy");
    check(skipped.hierarchy.flags4==0x50&&skipped.fields.fieldd44==77,"Early gate still applies d2c flag only");
    early.shared.pbw.field10_bits.reset();check(take<pb::Error>(early.run()).field=="PBW10","Prefix consumes PBW before gate");
    auto missing=f;missing.fields.fieldd2c.reset();check(take<pb::Error>(missing.run()).field=="d2c","Missing flag");
    missing=f;missing.fields.fieldd64.reset();check(take<pb::Error>(missing.run()).field=="d64","Missing gates");
    check(take<pb::Error>(f.run({})).field=="virtual80","Unknown orientation callback");
    missing=f;missing.fields.euler_mode.reset();check(take<pb::Error>(missing.run()).code==pb::ErrorCode::orientation,"Unknown Euler mode");
    missing=f;missing.fields.euler_mode=6;check(take<pb::Error>(missing.run()).orientation->code==mh4u::player_orientation::ErrorCode::unsupported_mode,"Unsupported Euler mode");
    check(take<pb::Output>(f.run(pb::OrientationCallback::b05328,{})).effects.publication_skipped,"Skipped output ignores callbacks");
    missing=f;missing.fields.fieldd64=0;const auto publication_failure=take<pb::Error>(missing.run(pb::OrientationCallback::b05328,{}));
    check(publication_failure.field=="virtual4c/50"&&missing.frame.model==f.frame.model&&missing.shared==f.shared,"Late unknown output is transactional");
    auto timers=f;timers.fields.fieldd30_bits=bits(.5F);timers.fields.fieldd34_bits=bits(.5F);timers.fields.fieldd38_bits=bits(.25F);
    auto timed=take<pb::Output>(timers.run());
    check(timed.fields.fieldd30_bits==0&&timed.fields.fieldd34_bits==0,"Positive timers clamp at expiration");
    check((timed.frame.animation.layers[0].animation.layer.attributes&0x11U)==0x11U,"Expiration still sets current-frame attributes");
    check(timed.fields.fieldd38_bits==one&&timed.effects.interpolated_position,"Blend consumed and reset");
    timers=f;timers.fields.fieldd30_bits=bits(-1);timers.fields.fieldd34_bits=bits(-2);timed=take<pb::Output>(timers.run());
    check(timed.fields.fieldd30_bits==bits(-1)&&timed.fields.fieldd34_bits==bits(-2),"Negative timers remain raw");
    auto crossing=f;crossing.controller.motion.fieldsd48[6]=0x8002;crossing.frame.animation.layers[0].animation.layer.remaining_interpolation=.5F;
    auto crossed=take<pb::Output>(crossing.run());
    check(crossed.frame.animation.layers[0].animation.layer.frame==4&&crossed.frame.animation.layers[0].animation.layer.previous_frame==4,"Interpolation overrun source correction");
    check(crossed.controller.motion.fieldsd48[3]==bits(1),"Overrun replaces historical step");
    crossing.controller.motion.fieldsd48[6]=0x8000;
    check(take<pb::Error>(crossing.run()).field=="b06b38","Unsupported active blend route");
    auto direction=f;direction.fields.fieldd64=3;
    check(take<pb::Output>(direction.run()).effects.publication_skipped,"Unchanged QY does not invent direction refresh");
    auto invalid=f;invalid.fields.fieldd34_bits=0x7fc00000;
    check(take<pb::Error>(invalid.run()).arithmetic->code==mh4u::motion_transform_math::ErrorCode::nonfinite_operand,"Consumed NaN timer rejected");
    invalid=f;invalid.context.pending.initialization.model_delta_time_bits=0x7f800000;
    check(take<pb::Error>(invalid.run()).code==pb::ErrorCode::arithmetic,"Consumed nonfinite delta rejected");
    invalid=f;invalid.fields.fieldd64=0;invalid.controller.player.fieldsd10[2]=0x7fc00000;
    check(take<pb::Error>(invalid.run()).field=="publication offset","Consumed publication offset rejected");
    invalid.fields.fieldd64=2;
    check(take<pb::Output>(invalid.run()).effects.publication_skipped,"Skipped publication preserves unused nonfinite offset");
    invalid=f;invalid.context.pending.model_reference=jp::Unknown{};
    check(take<pb::Error>(invalid.run()).animation->code==mh4u::model_animation_frame::ErrorCode::unknown_model,"Frame failure retained");
    pa::Limits limits;limits.frame.layers=0;check(take<pb::Error>(f.run(pb::OrientationCallback::b05328,{},limits)).code==pb::ErrorCode::resource_limit,"Layer work limit");
    invalid=f;invalid.frame.animation.layers.clear();check(take<pb::Error>(invalid.run()).code==pb::ErrorCode::layer_extent,"Missing physical layer0");
    const int rounding=std::fegetround();check(std::fesetround(FE_DOWNWARD)==0,"Set test rounding");
    const auto wrong=f.run();check(std::fesetround(rounding)==0,"Restore rounding");
    check(take<pb::Error>(wrong).orientation->arithmetic->code==mh4u::motion_transform_math::ErrorCode::floating_environment,"Unsupported rounding rejected");
    auto published=f;published.fields.fieldd64=0;published.hierarchy.attachment_mode_high16=0x4321;const auto finished=take<pb::Output>(published.run());
    check(finished.frame.model.position==mh4u::motion_transform_math::Bits4{bits(18),bits(-21),bits(94),0},"Model offset applied after PBW update");
    check(finished.shared.pbw.field10_bits==bits(17)&&!finished.effects.publication_skipped,"PBW excludes model presentation offset");
    check(finished.hierarchy.attachment_mode_high16==0x4321&&finished.fields.euler_mode==4,"Separate model38 halfword owners survive publication");
}
}
int main(){try{run();std::cout<<"{\"status\":\"passed\",\"checks\":"<<checks<<"}\n";return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
