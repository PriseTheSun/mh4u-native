#include "notification_queue.hpp"

namespace mh4u::notification_queue {
namespace {
std::optional<Error> valid(const Slot& slot,Phase phase,std::size_t index) noexcept {
    if(std::holds_alternative<Unknown>(slot))return Error{phase,ErrorCode::unknown_slot,index};
    if(const auto* p=std::get_if<PoolIndex>(&slot);p&&p->value>=16)
        return Error{phase,ErrorCode::invalid_index,index};
    return {};
}
void put(Record& record,std::size_t offset,std::size_t width,Word value) noexcept {
    for(std::size_t i=0;i<width;++i)record[offset+i]=static_cast<std::uint8_t>(value>>(i*8));
}
}
Result enqueue(const State& source,const Request& request) noexcept {
    Output out{source,{},{}};
    std::size_t record=0;
    for(;record<16;++record){
        ++out.effects.pool_examined;
        const auto& active=source.pool[record][0];
        if(!active)return Error{Phase::pool,ErrorCode::unknown_byte,record};
        if(*active==0)break;
    }
    if(record==16)return out; // Slots/counter are unconsumed.
    const auto selected=PoolIndex{static_cast<std::uint8_t>(record)};
    out.effects.selected=selected;
    std::size_t slot=0;
    for(;slot<16;++slot){
        ++out.effects.slots_examined;
        if(const auto e=valid(source.slots[slot],Phase::slots,slot))return *e;
        if(std::holds_alternative<Absent>(source.slots[slot]))break;
    }
    if(slot==16){
        out.effects.cleanup_attempted=true;
        for(std::size_t i=0;i<16;++i){
            ++out.effects.cleanup_examined;
            if(const auto e=valid(out.queue.slots[i],Phase::cleanup,i))return *e;
            if(out.queue.slots[i]!=Slot{selected})continue;
            // Only the first equal identity is removed. Later duplicate aliases
            // survive, pointing at the same record that fill makes all-zero.
            for(std::size_t j=i;j+1<16;++j)out.queue.slots[j]=out.queue.slots[j+1];
            out.queue.slots.back()=Absent{};
            out.queue.pool[record].fill(std::uint8_t{0});
            if(!source.counter)return Error{Phase::cleanup,ErrorCode::unknown_counter,{}};
            out.queue.counter=*source.counter-Word{1};
            out.effects.cleanup_slot=static_cast<std::uint8_t>(i);
            break;
        }
        return out; // Cleanup never changes this original null return.
    }
    out.queue.slots[slot]=selected;
    if(!source.counter)return Error{Phase::counter,ErrorCode::unknown_counter,{}};
    out.queue.counter=*source.counter+Word{1};
    auto& body=out.queue.pool[record];
    put(body,0,1,1);put(body,1,1,request.arg0);
    put(body,0x34,2,request.arg1);put(body,0x36,2,request.arg2);put(body,0x38,4,request.arg3);
    for(std::size_t i=0;i<3;++i)put(body,0x3c+4*i,4,request.payload[i]);
    put(body,3,1,0);
    put(body,0x48,2,0xffff);put(body,0x4a,2,0xffff);
    for(std::size_t block=0;block<4;++block){
        if(block)put(body,0x4a+2*block,2,0xffff);
        for(std::size_t i=0;i<4;++i)put(body,0x54+16*block+4*i,4,0);
    }
    out.inserted=selected;
    return out;
}
}
