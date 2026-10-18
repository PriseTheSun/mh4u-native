#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <variant>

namespace mh4u::player_policy_table {
inline constexpr std::size_t record_bytes=22,maximum_records=48;
// Bounded carrier profile: up to48 interpreted records and one full trailing
// slot. The original loop reads byte0 of slot49 before checking its cap.
inline constexpr std::size_t maximum_source_bytes=(maximum_records+1)*record_bytes;
struct Record {
    std::array<std::uint8_t,record_bytes> raw{};
    std::uint8_t field00()const noexcept{return raw[0];}
    std::uint8_t field01()const noexcept{return raw[1];}
    std::uint16_t field02()const noexcept{return word(2);}
    std::uint16_t field04()const noexcept{return word(4);}
    std::uint16_t field06()const noexcept{return word(6);}
    std::uint16_t field08()const noexcept{return word(8);}
    std::uint16_t field0a()const noexcept{return word(10);}
    std::uint16_t field0c()const noexcept{return word(12);}
    std::uint16_t field0e()const noexcept{return word(14);}
    std::int16_t field10()const noexcept{return std::bit_cast<std::int16_t>(word(16));}
    std::int16_t field12()const noexcept{return std::bit_cast<std::int16_t>(word(18));}
    std::int16_t field14()const noexcept{return std::bit_cast<std::int16_t>(word(20));}
    bool operator==(const Record&)const=default;
private:
    std::uint16_t word(std::size_t offset)const noexcept{return static_cast<std::uint16_t>(raw[offset]|(std::uint16_t(raw[offset+1])<<8));}
};
enum class Termination {sentinel,cap};
struct Coverage {
    Termination termination;std::size_t records{},examined_bytes{};
    std::uint8_t next_first_byte{}; // Always observed, including after48 records.
    bool operator==(const Coverage&)const=default;
};
enum class ErrorCode {invalid_expected,hash_mismatch,source_limit,incomplete_first_byte,incomplete_record,identity_exhausted,capacity};
struct Error {ErrorCode code;std::string_view message;std::size_t byte_offset{};};
template<class T>using Result=std::variant<T,Error>;
class Table {
public:
    static Result<Table> decode(std::span<const std::uint8_t>,std::string_view expected_sha256);
    std::span<const std::uint8_t> bytes()const noexcept;
    std::span<const Record> records()const noexcept;
    Coverage coverage()const noexcept;
    std::string_view sha256()const noexcept;
    std::uint64_t identity()const noexcept;
    bool operator==(const Table& other)const noexcept{return identity()==other.identity();}
private:
    struct Data;explicit Table(std::shared_ptr<const Data> data):data_(std::move(data)){}
    std::shared_ptr<const Data> data_;
};
struct Unknown {bool operator==(const Unknown&)const=default;};
struct Absent {bool operator==(const Absent&)const=default;};
using TableReference=std::variant<Unknown,Absent,Table>;
struct State {
    TableReference table=Unknown{}; //pw+100; no emulated address retained.
    std::uint16_t field104{},field106{},field108{};
    std::array<std::uint8_t,48> latches{}; //pw+10a..139
    std::uint32_t timer140_bits{},timer144_bits{};
    std::uint8_t field148{};
    bool operator==(const State&)const=default;
};
// Complete b6a97c effect on declared fields. No previous field is read.
// The table argument is assigned without dereference: Unknown/Absent preserved.
// Signed integer -> binary32 round-to-nearest-even, implemented with integers;
// host floating environment is irrelevant. Does not model FPSCR exception flags.
State install(TableReference,std::int32_t delay)noexcept;
}
