#pragma once
#include <cstdint>
#include <span>
#include <vector>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif
namespace mh4u::net {
class SocketSystem {
public: SocketSystem(); ~SocketSystem(); SocketSystem(const SocketSystem&)=delete; SocketSystem& operator=(const SocketSystem&)=delete;
};
class Socket {
public:
#ifdef _WIN32
    using Handle=SOCKET; static constexpr Handle invalid=INVALID_SOCKET;
#else
    using Handle=int; static constexpr Handle invalid=-1;
#endif
    Socket()=default; explicit Socket(Handle value):value_(value){}
    ~Socket(); Socket(const Socket&)=delete; Socket& operator=(const Socket&)=delete;
    Socket(Socket&&) noexcept; Socket& operator=(Socket&&) noexcept;
    Handle handle() const {return value_;}
    bool valid() const {return value_!=invalid;}
    void close(); void configure();
    // -1 is would-block, zero is orderly EOF for receive.
    int receive(std::span<std::uint8_t>); int send(std::span<const std::uint8_t>);
    static Socket listen_loopback(std::uint16_t port);
    static Socket begin_connect_loopback(std::uint16_t port);
    void finish_connect() const; // Only after writable/readable readiness.
    static Socket connect_loopback(std::uint16_t port,int timeout_ms);
    Socket accept(); std::uint16_t port() const;
private: Handle value_=invalid;
};
struct PollItem { const Socket* socket; bool want_write=false,readable=false,writable=false; };
void poll(std::vector<PollItem>&,int milliseconds);
}
