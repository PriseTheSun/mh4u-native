#pragma once
#include "socket.hpp"
#include "wire.hpp"
#include <chrono>
#include <utility>

namespace mh4u::net {
using Clock=std::chrono::steady_clock;
using Time=Clock::time_point;
inline long long milliseconds(Time a,Time b) { return std::chrono::duration_cast<std::chrono::milliseconds>(a-b).count(); }
struct Peer {
    Socket socket;std::vector<std::uint8_t> input,output;
    Time connected=Clock::now(),last=connected,window=connected,closing_since{},partial_since{};
    std::uint64_t received=0,transmitted=0,last_intent=0;
    std::uint32_t id=0;std::string name,reason;unsigned frames=0;
    bool ready=false,closing=false,dead=false;
    std::uint16_t version=protocol_version;
    explicit Peer(Socket socket_,std::uint16_t protocol=protocol_version):socket(std::move(socket_)),version(protocol){}
    void queue(Message type,const Writer& payload={}) {
        auto encoded=frame(type,payload,version);if(encoded.size()>max_queue-output.size())throw std::runtime_error("outbound_queue_limit");output.insert(output.end(),encoded.begin(),encoded.end());
    }
    void read() {
        std::array<std::uint8_t,4096> buffer{};const auto count=socket.receive(buffer);
        if(count==0)throw std::runtime_error("peer_eof");if(count<0)return;
        if(std::uint64_t(count)>16*1024*1024-received)throw std::runtime_error("cumulative_byte_limit");received+=static_cast<std::uint64_t>(count);
        if(std::size_t(count)>max_queue-input.size())throw std::runtime_error("inbound_queue_limit");input.insert(input.end(),buffer.begin(),buffer.begin()+count);
    }
    void flush() {
        if(output.empty())return;
        if(output.size()>16*1024*1024-transmitted)throw std::runtime_error("cumulative_send_limit");
        const int count=socket.send(output);if(count==0)throw std::runtime_error("send_eof");
        if(count>0){transmitted+=static_cast<std::uint64_t>(count);output.erase(output.begin(),output.begin()+count);}
    }
    std::optional<Frame> next(Time now) {
        const auto result=take_frame(input,version);
        if(!result){if(!input.empty()&&partial_since==Time{})partial_since=now;return result;}
        partial_since={};
        if(milliseconds(now,window)>=1000){window=now;frames=0;}
        if(++frames>128)throw std::runtime_error("message_rate_limit");return result;
    }
};
}
