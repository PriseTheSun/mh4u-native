#include "player_policy_table.hpp"
#include "picosha2.h"
#include <algorithm>
#include <atomic>
#include <new>
#include <stdexcept>
#include <string>
#include <vector>

namespace mh4u::player_policy_table {
struct Table::Data {std::vector<std::uint8_t> bytes;std::vector<Record> records;Coverage coverage;std::string digest;std::uint64_t identity;};
namespace {
std::uint32_t integer_float(std::int32_t value)noexcept{
    if(value==0)return 0;
    const auto raw=std::bit_cast<std::uint32_t>(value);const auto sign=raw&0x80000000U;
    const auto magnitude=sign?0U-raw:raw;auto exponent=31U-static_cast<unsigned>(std::countl_zero(magnitude));
    std::uint32_t significand;
    if(exponent<=23)significand=magnitude<<(23-exponent);
    else{
        const auto shift=exponent-23;significand=magnitude>>shift;
        const auto remainder=magnitude&((1U<<shift)-1U);const auto halfway=1U<<(shift-1);
        if(remainder>halfway||(remainder==halfway&&(significand&1)))++significand;
        if(significand==0x1000000){significand>>=1;++exponent;}
    }
    return sign|((exponent+127U)<<23)|(significand&0x7fffffU);
}
}
Result<Table> Table::decode(std::span<const std::uint8_t> bytes,std::string_view expected){
    try{
        if(bytes.size()>maximum_source_bytes)return Error{ErrorCode::source_limit,"Carrier exceeds bounded49-slot profile",maximum_source_bytes};
        if(expected.size()!=64||!std::all_of(expected.begin(),expected.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');}))return Error{ErrorCode::invalid_expected,"Expected SHA256 must be64 lowercase hexadecimal digits"};
        const auto digest=picosha2::hash256_hex_string(bytes.begin(),bytes.end());
        if(digest!=expected)return Error{ErrorCode::hash_mismatch,"Policy bytes differ from expected SHA256"};
        std::size_t count=0,offset=0;std::uint8_t next=0;Termination termination;
        for(;;){
            if(offset>=bytes.size())return Error{ErrorCode::incomplete_first_byte,"Missing consumed first byte, including slot49 at cap",offset};
            next=bytes[offset];
            if(next==0xff){termination=Termination::sentinel;break;}
            if(count==maximum_records){termination=Termination::cap;break;}
            if(bytes.size()-offset<record_bytes)return Error{ErrorCode::incomplete_record,"Policy record is truncated",offset};
            ++count;offset+=record_bytes;
        }
        auto data=std::make_shared<Data>();data->bytes.assign(bytes.begin(),bytes.end());data->digest=digest;
        data->coverage={termination,count,offset+1,next};data->records.reserve(count);
        for(std::size_t i=0;i<count;++i){Record r;std::copy_n(bytes.begin()+static_cast<std::ptrdiff_t>(i*record_bytes),record_bytes,r.raw.begin());data->records.push_back(r);}
        static std::atomic<std::uint64_t> next_identity{1};auto token=next_identity.load(std::memory_order_relaxed);
        for(;;){if(!token)return Error{ErrorCode::identity_exhausted,"Policy table snapshot identities exhausted"};if(next_identity.compare_exchange_weak(token,token+1,std::memory_order_relaxed))break;}
        data->identity=token;return Table{std::move(data)};
    }catch(const std::bad_alloc&){return Error{ErrorCode::capacity,"Policy table allocation failed"};}catch(const std::length_error&){return Error{ErrorCode::capacity,"Policy table allocation exceeds capacity"};}
}
std::span<const std::uint8_t> Table::bytes()const noexcept{return data_->bytes;}
std::span<const Record> Table::records()const noexcept{return data_->records;}
Coverage Table::coverage()const noexcept{return data_->coverage;}
std::string_view Table::sha256()const noexcept{return data_->digest;}
std::uint64_t Table::identity()const noexcept{return data_->identity;}
State install(TableReference table,std::int32_t delay)noexcept{
    State state{};state.table=std::move(table);state.timer140_bits=integer_float(delay);return state;
}
}
