#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>
namespace mh4u::net {
inline constexpr std::uint16_t protocol_version=1;
inline constexpr std::size_t max_payload=512,max_queue=8192;
enum class Message:std::uint8_t { hello=1,welcome,state,intent,ping,pong,bye,reject,motion };
struct Writer {
    std::vector<std::uint8_t> bytes;
    void number(std::uint64_t value,unsigned size) { for(unsigned i=size;i>0;--i) bytes.push_back(static_cast<std::uint8_t>(value>>((i-1)*8))); }
    void real(double value) { if(!std::isfinite(value)) throw std::runtime_error("nonfinite_wire_number");number(std::bit_cast<std::uint64_t>(value),8); }
    void raw(std::span<const std::uint8_t> value) { bytes.insert(bytes.end(),value.begin(),value.end()); }
    void text(const std::string& value) { if(value.size()>255) throw std::runtime_error("string_limit"); number(value.size(),1); bytes.insert(bytes.end(),value.begin(),value.end()); }
};
struct Reader {
    std::span<const std::uint8_t> bytes; std::size_t position=0;
    std::uint64_t number(unsigned size) {
        if(size>8||position>bytes.size()||size>bytes.size()-position) throw std::runtime_error("truncated_message");
        std::uint64_t value=0; while(size--) value=(value<<8)|bytes[position++]; return value;
    }
    double real() { const double value=std::bit_cast<double>(number(8));if(!std::isfinite(value))throw std::runtime_error("nonfinite_wire_number");return value; }
    std::span<const std::uint8_t> raw(std::size_t count) {
        if(position>bytes.size()||count>bytes.size()-position) throw std::runtime_error("truncated_message");
        const auto value=bytes.subspan(position,count); position+=count; return value;
    }
    std::string text() { const auto value=raw(static_cast<std::size_t>(number(1))); return {value.begin(),value.end()}; }
    void finish() const { if(position!=bytes.size()) throw std::runtime_error("trailing_payload"); }
};
struct Frame { Message type; std::vector<std::uint8_t> payload; };
inline std::vector<std::uint8_t> frame(Message type,const Writer& body,std::uint16_t version=protocol_version) {
    if(body.bytes.size()>max_payload) throw std::runtime_error("payload_limit");
    Writer output; output.raw(std::array<std::uint8_t,4>{'M','4','N','L'}); output.number(version,2);
    output.number(static_cast<std::uint8_t>(type),1); output.number(0,1); output.number(body.bytes.size(),4); output.raw(body.bytes); return output.bytes;
}
inline std::optional<Frame> take_frame(std::vector<std::uint8_t>& bytes,std::uint16_t version=protocol_version) {
    if(bytes.size()<12) return std::nullopt;
    Reader header{std::span<const std::uint8_t>(bytes).first(12)};
    if(header.number(4)!=0x4d344e4c) throw std::runtime_error("bad_magic");
    if(header.number(2)!=version) throw std::runtime_error("protocol_version");
    const auto type=header.number(1); if(type<1||type>(version==2?9:8)) throw std::runtime_error("unknown_message");
    if(header.number(1)!=0) throw std::runtime_error("unsupported_flags");
    const auto length=static_cast<std::size_t>(header.number(4)); if(length>max_payload) throw std::runtime_error("payload_limit");
    if(bytes.size()<12+length) return std::nullopt;
    Frame result{static_cast<Message>(type),{bytes.begin()+12,bytes.begin()+12+length}};
    bytes.erase(bytes.begin(),bytes.begin()+12+length); return result;
}
inline bool valid_name(const std::string& name) {
    if(name.empty()||name.size()>24) return false;
    for(unsigned char c:name) if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-')) return false;
    return true;
}
}
