#pragma once
#include "peer.hpp"
#include "world.hpp"

namespace mh4u::net {
// Reusable, nonblocking loopback client. The caller owns SocketSystem and calls
// pump regularly; no subprocess, sleep, renderer dependency or stdout parsing.
// Destruction closes the socket only; call close()+pump for confirmed Bye.
class WorldClient {
public:
    WorldClient(std::uint16_t port,std::string name,Content verified_content,int handshake_ms=1000,int idle_ms=1500);
    void pump(); // poll timeout zero; <=32 decoded frames and one read/write.
    void motion(MotionInput); // bounded queue; only after ready().
    void select(std::uint16_t);
    void close(); // Requests bounded Bye handshake; keep pumping until done().
    bool ready() const { return peer_.ready&&!stopping_&&!done_; }
    bool done() const { return done_; }
    bool confirmed_bye() const { return confirmed_; }
    const std::string& error() const { return error_; }
    std::uint32_t id() const { return id_; }
    std::uint64_t session() const { return session_; }
    const std::optional<WorldSnapshot>& snapshot() const { return state_; }
private:
    Content content_;std::string name_;Peer peer_;
    int handshake_ms_,idle_ms_;
    std::uint32_t id_{};std::uint64_t session_{},motion_sequence_{},selection_sequence_{},ping_{};
    bool connecting_{true},stopping_{},bye_sent_{},done_{},confirmed_{};
    Time stop_since_{},last_ping_{};
    std::optional<WorldSnapshot> state_;
    std::string error_;
    void receive(Frame);
};
}
