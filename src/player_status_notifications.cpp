#include "player_status_notifications.hpp"

namespace mh4u::player_status_notifications {
Mode1Result mode1(const player_controller_step::PlayerFields& player,
    const notification_queue::State& source,std::uint32_t value) noexcept {
    Attempt attempt{};attempt.id=static_cast<std::uint16_t>(value);
    attempt.permitted=player_transition::local_control(player.global,player.pbw);
    if(!attempt.permitted)return Mode1Output{source,attempt};
    auto result=notification_queue::enqueue(source,{0,1,attempt.id,0,{0,0,0}});
    if(const auto* e=std::get_if<notification_queue::Error>(&result))
        return Error{Phase::mode1,ErrorCode::queue,"b8ba58",{},*e};
    auto out=std::get<notification_queue::Output>(result);
    attempt.queue=out.effects;attempt.inserted=out.inserted;
    return Mode1Output{out.queue,attempt};
}
namespace {
struct Engine {
    const player_controller_step::PlayerFields& player;
    const player_fields::Fields& fields;
    Output out;
    Phase phase=Phase::edges;
    template<class T>T need(const std::optional<T>& value,std::string_view field)const {
        if(!value)throw Error{phase,ErrorCode::unknown_field,field,{},{}};
        return *value;
    }
    void emit(std::uint16_t id){
        auto result=mode1(player,out.queue,id);
        if(auto* e=std::get_if<Error>(&result)){e->attempt=out.effects.count;throw *e;}
        const auto next=std::get<Mode1Output>(result);
        out.queue=next.queue;
        out.effects.attempts[out.effects.count++]=next.attempt;
    }
    void run(std::optional<std::uint8_t> inherited){
        const auto current=need(fields.pw.field1f8,"PW1f8"),previous=need(fields.pw.field200,"PW200");
        std::array<std::optional<std::uint8_t>,2> bytes{{{},inherited}};
        bool rising=false;
        for(std::size_t i=0;i<2;++i){
            bytes[i]=0;const auto mask=std::uint32_t{0x100000}<<i;
            if((current&mask)&&!(previous&mask)){rising=true;break;}
            if(!(current&mask)&&(previous&mask))bytes[i]=1;
        }
        const bool local=player_transition::local_control(player.global,player.pbw);
        if(local&&need(fields.pbw.field34c,"PBW34c")<=0){bytes={0,0};rising=false;}
        if(rising){
            phase=Phase::rising;
            const auto kind=need(fields.pw.field586,"PW586");std::uint16_t id=0;
            switch(kind){
            case 0x4d5:case 0x715:id=50;break;
            case 0x4d6:case 0x716:id=51;break;
            case 0x4d7:case 0x717:id=52;break;
            case 0x4d8:case 0x6bd:case 0x718:id=53;break;
            default:break;
            }
            emit(id);
        }
        phase=Phase::falling;
        for(std::size_t i=0;i<2;++i)if(need(bytes[i],i?"prior second byte":"first local byte")==1)emit(54);
        out.effects.second_byte=*bytes[1];
        phase=Phase::tail;
        const auto previous_first=need(fields.pw.field1fc,"PW1fc");
        const bool old_bit=(previous_first&0x20000)!=0,new_bit=(player.pw.field1f4&0x20000)!=0;
        if(!old_bit&&new_bit)emit(55);
        else if(old_bit&&!new_bit&&player_transition::local_control(player.global,player.pbw)&&
            need(fields.pbw.field34c,"PBW34c")>0)emit(56);
    }
};
}
Result apply(const player_controller_step::PlayerFields& player,const player_fields::Fields& fields,
    const notification_queue::State& queue,std::optional<std::uint8_t> prior_second_byte) noexcept {
    try{Engine engine{player,fields,{queue,{}}};engine.run(prior_second_byte);return engine.out;}
    catch(const Error& error){return error;}
}
}
