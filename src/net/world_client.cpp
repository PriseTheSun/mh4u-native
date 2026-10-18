#include "world_client.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace mh4u::net {
WorldClient::WorldClient(std::uint16_t port,std::string name,Content content,int handshake_ms,int idle_ms)
    :content_(std::move(content)),name_(std::move(name)),peer_(Socket::begin_connect_loopback(port),world_protocol_version),handshake_ms_(handshake_ms),idle_ms_(idle_ms) {
    if(!port||!valid_name(name_)||handshake_ms<100||handshake_ms>5000||idle_ms<500||idle_ms>10000)throw std::runtime_error("invalid_world_client_options");
    Writer hello;hello.raw(content_.digest);hello.text(name_);peer_.queue(Message::hello,hello);
}
void WorldClient::motion(MotionInput input) {
    if(!ready())throw std::runtime_error("world_client_not_ready");
    if(motion_sequence_==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("motion_sequence_overflow");
    Writer body;body.number(motion_sequence_+1,8);write_motion(body,input);peer_.queue(Message::motion,body);++motion_sequence_;
}
void WorldClient::select(std::uint16_t choice) {
    if(!ready())throw std::runtime_error("world_client_not_ready");
    if(choice!=0xffff&&choice>=content_.clips.size())throw std::runtime_error("local_selection_range");
    if(selection_sequence_==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("intent_sequence_overflow");
    Writer body;body.number(selection_sequence_+1,8);body.number(choice,2);peer_.queue(Message::intent,body);++selection_sequence_;
}
void WorldClient::close() {if(!done_&&!stopping_){stopping_=true;stop_since_=Clock::now();}}
void WorldClient::receive(Frame frame) {
    Reader body{frame.payload};
    if(frame.type==Message::reject){const auto reason=body.text();body.finish();throw std::runtime_error("server_reject:"+reason);}
    if(!peer_.ready) {
        if(frame.type!=Message::welcome)throw std::runtime_error("welcome_required");
        id_=static_cast<std::uint32_t>(body.number(4));const auto clips=body.number(2);session_=body.number(8);const auto digest=body.raw(32);const auto tick_ms=body.number(2),hold_ms=body.number(2);body.finish();
        if(!id_||clips!=content_.clips.size()||!std::equal(digest.begin(),digest.end(),content_.digest.begin())||tick_ms!=world_tick_ms||hold_ms!=world_hold_ms)throw std::runtime_error("welcome_identity");
        peer_.ready=true;
    } else if(frame.type==Message::state) {
        auto next=read_world_snapshot(body,content_.clips.size());
        if(next.session!=session_||next.digest!=content_.digest)throw std::runtime_error("world_state_identity");
        if(state_&&(next.revision<=state_->revision||next.tick<state_->tick))throw std::runtime_error("world_state_order");
        bool member=false;
        for(const auto& p:next.participants) {
            if(p.id==id_) {
                if(p.name!=name_||p.accepted>motion_sequence_||p.selection_acknowledged>selection_sequence_)throw std::runtime_error("state_identity");
                member=true;
            }
            if(state_)for(const auto& before:state_->participants)if(before.id==p.id&&
                (before.name!=p.name||p.accepted<before.accepted||p.applied<before.applied||p.selection_acknowledged<before.selection_acknowledged))throw std::runtime_error("participant_state_order");
        }
        if(!member)throw std::runtime_error("self_missing_from_state");state_=std::move(next);
    } else if(frame.type==Message::pong) {
        const auto nonce=body.number(8);body.finish();if(!nonce||nonce>ping_)throw std::runtime_error("invalid_pong");
    } else if(frame.type==Message::bye) {body.finish();confirmed_=true;done_=true;}
    else throw std::runtime_error("unexpected_server_message");
    peer_.last=Clock::now();
}
void WorldClient::pump() {
    if(done_)return;
    try {
        const auto now=Clock::now();
        std::vector<PollItem> items{{&peer_.socket,connecting_||!peer_.output.empty()}};poll(items,0);
        if(connecting_&&(items[0].writable||items[0].readable)){peer_.socket.finish_connect();connecting_=false;}
        if(!connecting_) {
            if(items[0].writable)peer_.flush();
            if(items[0].readable)peer_.read();
            for(int i=0;i<32&&!done_;++i){auto frame=peer_.next(now);if(!frame)break;receive(std::move(*frame));}
        }
        if(!done_) {
            if(!peer_.ready&&milliseconds(now,peer_.connected)>handshake_ms_)throw std::runtime_error("handshake_timeout");
            if(peer_.ready&&milliseconds(now,peer_.last)>idle_ms_)throw std::runtime_error("server_timeout");
            if(peer_.partial_since!=Time{}&&milliseconds(now,peer_.partial_since)>handshake_ms_)throw std::runtime_error("partial_frame_timeout");
            if(stopping_&&peer_.ready&&!bye_sent_){peer_.queue(Message::bye);bye_sent_=true;}
            if(stopping_&&milliseconds(now,stop_since_)>500)throw std::runtime_error("bye_timeout");
            if(peer_.ready&&!stopping_&&(last_ping_==Time{}||milliseconds(now,last_ping_)>=250)) {
                if(ping_==std::numeric_limits<std::uint64_t>::max())throw std::runtime_error("ping_overflow");
                Writer ping;ping.number(++ping_,8);peer_.queue(Message::ping,ping);last_ping_=now;
            }
        }
    } catch(const std::exception& error) {error_=error.what();done_=true;}
    if(done_)peer_.socket.close();
}
}
