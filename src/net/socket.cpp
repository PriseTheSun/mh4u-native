#include "socket.hpp"
#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>
#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <unistd.h>
#endif
namespace mh4u::net {
namespace {
int error_code() {
#ifdef _WIN32
    return WSAGetLastError();
#else
    return errno;
#endif
}
bool pending(int code) {
#ifdef _WIN32
    return code==WSAEWOULDBLOCK||code==WSAEINPROGRESS||code==WSAEINTR;
#else
    return code==EAGAIN||code==EWOULDBLOCK||code==EINPROGRESS||code==EINTR;
#endif
}
[[noreturn]] void failure(const char* action) { throw std::runtime_error(std::string(action)+"_"+std::to_string(error_code())); }
sockaddr_in loopback(std::uint16_t port) { sockaddr_in address{}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK); address.sin_port=htons(port); return address; }
}
SocketSystem::SocketSystem() {
#ifdef _WIN32
    WSADATA data{}; if(WSAStartup(MAKEWORD(2,2),&data)) throw std::runtime_error("WSAStartup_failed");
#endif
}
SocketSystem::~SocketSystem() {
#ifdef _WIN32
    WSACleanup();
#endif
}
Socket::~Socket(){close();}
Socket::Socket(Socket&& other) noexcept:value_(std::exchange(other.value_,invalid)){}
Socket& Socket::operator=(Socket&& other) noexcept { if(this!=&other){close();value_=std::exchange(other.value_,invalid);}return *this; }
void Socket::close() {
    if(!valid())return;
#ifdef _WIN32
    closesocket(value_);
#else
    ::close(value_);
#endif
    value_=invalid;
}
void Socket::configure() {
#ifdef _WIN32
    u_long mode=1; if(ioctlsocket(value_,FIONBIO,&mode)) failure("nonblocking");
#else
    if(value_>=FD_SETSIZE) throw std::runtime_error("descriptor_select_limit");
    const int flags=fcntl(value_,F_GETFL,0); if(flags<0||fcntl(value_,F_SETFL,flags|O_NONBLOCK)<0)failure("nonblocking");
#endif
    int enabled=1,size=4096;
    if(setsockopt(value_,IPPROTO_TCP,TCP_NODELAY,reinterpret_cast<const char*>(&enabled),sizeof(enabled)))failure("nodelay");
    if(setsockopt(value_,SOL_SOCKET,SO_SNDBUF,reinterpret_cast<const char*>(&size),sizeof(size)))failure("send_buffer");
#ifdef __APPLE__
    if(setsockopt(value_,SOL_SOCKET,SO_NOSIGPIPE,&enabled,sizeof(enabled)))failure("nosigpipe");
#endif
}
int Socket::receive(std::span<std::uint8_t> bytes) {
    const int size=static_cast<int>(::recv(value_,reinterpret_cast<char*>(bytes.data()),static_cast<int>(bytes.size()),0));
    if(size<0) { if(pending(error_code()))return -1; failure("recv"); } return size;
}
int Socket::send(std::span<const std::uint8_t> bytes) {
    int flags=0;
#ifdef MSG_NOSIGNAL
    flags=MSG_NOSIGNAL;
#endif
    const int size=static_cast<int>(::send(value_,reinterpret_cast<const char*>(bytes.data()),static_cast<int>(bytes.size()),flags));
    if(size<0) { if(pending(error_code()))return -1; failure("send"); } return size;
}
Socket Socket::listen_loopback(std::uint16_t port) {
    Socket socket(::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)); if(!socket.valid())failure("socket"); socket.configure();
    const auto address=loopback(port);
    if(::bind(socket.value_,reinterpret_cast<const sockaddr*>(&address),sizeof(address)))failure("bind");
    if(::listen(socket.value_,8))failure("listen"); return socket;
}
Socket Socket::accept() {
    Socket result(::accept(value_,nullptr,nullptr));
    if(!result.valid()){if(pending(error_code()))return result;failure("accept");} result.configure(); return result;
}
std::uint16_t Socket::port() const {
    sockaddr_in address{};
#ifdef _WIN32
    int length=sizeof(address);
#else
    socklen_t length=sizeof(address);
#endif
    if(getsockname(value_,reinterpret_cast<sockaddr*>(&address),&length))failure("getsockname"); return ntohs(address.sin_port);
}
Socket Socket::begin_connect_loopback(std::uint16_t port) {
    Socket socket(::socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)); if(!socket.valid())failure("socket"); socket.configure(); const auto address=loopback(port);
    if(::connect(socket.value_,reinterpret_cast<const sockaddr*>(&address),sizeof(address))!=0&&!pending(error_code()))failure("connect");
    return socket;
}
void Socket::finish_connect() const {
    int error=0;
#ifdef _WIN32
    int length=sizeof(error);
#else
    socklen_t length=sizeof(error);
#endif
    if(getsockopt(value_,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&error),&length)||error)throw std::runtime_error("connect_failed_"+std::to_string(error));
}
Socket Socket::connect_loopback(std::uint16_t port,int timeout_ms) {
    auto socket=begin_connect_loopback(port);
    std::vector<PollItem> items{{&socket,true}}; poll(items,timeout_ms);
    if(!items[0].writable)throw std::runtime_error("connect_timeout");
    socket.finish_connect();return socket;
}

void poll(std::vector<PollItem>& items,int milliseconds) {
    fd_set reads,writes; FD_ZERO(&reads);FD_ZERO(&writes); Socket::Handle highest=0;
    if(items.size()>=FD_SETSIZE)throw std::runtime_error("select_capacity");
    for(auto& item:items) { item.readable=item.writable=false; FD_SET(item.socket->handle(),&reads); if(item.want_write)FD_SET(item.socket->handle(),&writes); highest=std::max(highest,item.socket->handle()); }
    timeval timeout{milliseconds/1000,(milliseconds%1000)*1000};
#ifdef _WIN32
    const int result=select(0,&reads,&writes,nullptr,&timeout);
#else
    const int result=select(highest+1,&reads,&writes,nullptr,&timeout);
#endif
    if(result<0) { if(pending(error_code()))return;failure("select"); }
    for(auto& item:items) { item.readable=FD_ISSET(item.socket->handle(),&reads)!=0; item.writable=FD_ISSET(item.socket->handle(),&writes)!=0; }
}
}
