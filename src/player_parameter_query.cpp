#include "player_parameter_query.hpp"
#include <algorithm>
#include <limits>

namespace mh4u::player_parameter_query {
namespace {
namespace pf=player_fields;
constexpr std::int32_t signed16(std::uint32_t v) noexcept {
    const auto low=v&0xffffU;
    return low<0x8000U?static_cast<std::int32_t>(low):static_cast<std::int32_t>(low)-65536;
}
constexpr std::int32_t signed8(std::uint32_t v) noexcept {
    return v<128?static_cast<std::int32_t>(v):static_cast<std::int32_t>(v)-256;
}
constexpr bool category24(std::uint32_t k) noexcept {
    return k>=7&&k<=20&&k!=11&&k!=12&&k!=16;
}
constexpr bool category40(std::uint32_t k) noexcept {return k==11||k==12||k==16;}
constexpr std::uint32_t category(std::uint32_t k) noexcept {
    return k>=1&&k<=6?0:k>=7&&k<=20?1:255;
}
constexpr std::int32_t floor_shift(std::int32_t v,unsigned n) noexcept {
    const auto magnitude=static_cast<std::int64_t>(v);
    return static_cast<std::int32_t>(magnitude>=0?magnitude/(std::int64_t{1}<<n):
        -((-magnitude+(std::int64_t{1}<<n)-1)/(std::int64_t{1}<<n)));
}
struct View {const Binding* binding;Table table;std::uint32_t kind;std::size_t offset;};
struct Engine {
    const pf::Fields& input;
    const Resources& resources;
    Output out;
    std::size_t current{};
    Engine(const pf::Fields& in,const Resources& r):input(in),resources(r),out{in,0,{}}{}
    [[noreturn]] void fail(ErrorCode code,std::string_view field,Table table=Table::none,
        std::uint32_t kind=0,std::size_t offset=0,std::size_t size=0) const {
        throw Error{code,field,current,table,kind,offset,size};
    }
    std::uint32_t descriptor(std::size_t offset,unsigned size=1) const {
        std::uint32_t value=0;
        for(unsigned i=0;i<size;++i) {
            const auto b=input.pbw.descriptors[current][offset+i];
            if(!b)fail(ErrorCode::unknown_field,"PBW descriptor byte",Table::none,0,offset+i,1);
            value|=std::uint32_t{*b}<<(8*i);
        }
        return value;
    }
    const Bytes* binding(const View& v,bool nullable=false) const {
        if(std::holds_alternative<Unknown>(*v.binding))
            fail(ErrorCode::unknown_resource,"Resource pointer",v.table,v.kind,v.offset);
        if(std::holds_alternative<Absent>(*v.binding)) {
            if(nullable)return nullptr;
            fail(ErrorCode::absent_resource,"Resource dereference",v.table,v.kind,v.offset);
        }
        return &std::get<Bytes>(*v.binding);
    }
    std::uint32_t read(const View& v,std::size_t offset,unsigned size=1) const {
        const auto& bytes=*binding(v);
        // Digest is provenance supplied by the immutable owner, not a per-query
        // hash assertion. Validate its shape only when the source is consumed.
        if(bytes.sha256.size()!=64||!std::all_of(bytes.sha256.begin(),bytes.sha256.end(),
            [](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');}))
            fail(ErrorCode::invalid_provenance,"Source SHA-256",v.table,v.kind,v.offset+offset,size);
        if(bytes.bytes.size()>std::numeric_limits<std::uint32_t>::max()||
            v.offset>bytes.bytes.size()||offset>bytes.bytes.size()-v.offset||
            size>bytes.bytes.size()-v.offset-offset)
            fail(ErrorCode::resource_extent,"Resource byte range",v.table,v.kind,v.offset+offset,size);
        std::uint32_t result=0;
        for(unsigned i=0;i<size;++i)
            result|=std::uint32_t{std::to_integer<std::uint8_t>(bytes.bytes[v.offset+offset+i])}<<(8*i);
        return result;
    }
    std::uint32_t table(const Binding& b,Table t,std::size_t offset,unsigned size=1) const {
        return read({&b,t,0,0},offset,size);
    }
    View record(std::uint32_t k,bool primary=false) const {
        if(primary||category(k)==0) {
            if(category(k)!=0)fail(ErrorCode::resource_extent,"Primary category",Table::primary40,k);
            const View v{&resources.primary40[k],Table::primary40,k,0};
            if(!binding(v,true))return v;
            return {v.binding,v.table,k,40*descriptor(2,2)};
        }
        if(category24(k)) {
            const View v{&resources.records24[k],Table::records24,k,0};
            if(!binding(v,true))return v;
            return {v.binding,v.table,k,24*descriptor(2,2)};
        }
        const View v{&resources.records40[k],Table::records40,k,0};
        if(!binding(v,true))return v;
        return {v.binding,v.table,k,40*descriptor(2,2)};
    }
    bool generated(std::uint32_t k) const {
        if(category(k)!=0||k==6)return false;
        const auto v=record(k,true);
        return binding(v,true)&&(read(v,6)&0x40U)!=0;
    }
    bool special(std::uint32_t k) const {
        if(category24(k)) {
            const auto v=record(k);return binding(v,true)&&(read(v,22)&0x80U)!=0;
        }
        if(category40(k)) {
            const auto v=record(k);return binding(v,true)&&(read(v,28)&0x80U)!=0;
        }
        if(category(k)!=0||k==6)return false;
        const auto v=record(k,true);
        return binding(v,true)&&(read(v,6)&0xc0U)!=0;
    }
    std::int32_t conditional_bonus(std::uint32_t k,std::int32_t value) const {
        if((k!=11&&k!=12)||(descriptor(1)&0x20U)==0)return value;
        // 2f1e38/54 and 2f4924/68: signed product, ASR, sign correction.
        // Products stay in int32 for every source signed16 operand.
        const auto product=value*(k==11?0x8889:0x6667);
        const auto quotient=floor_shift(product,k==11?19:18)-floor_shift(product,31);
        return signed16(static_cast<std::uint32_t>(value+quotient));
    }
    std::int32_t special6(std::uint32_t k) const {
        if(!category24(k)&&!category40(k))return 0;
        const auto index=(descriptor(14,2)&3U)+4*(descriptor(12,2)>>8);
        auto value=signed16(table(resources.special12,Table::special12,12*index+(category24(k)?0:2),2));
        if(k==7||k==9) {
            const auto variant=descriptor(14,2)>>8;
            if(variant==1||variant==2)value=signed16(static_cast<std::uint32_t>(value+(variant==1?10:20)));
            return value;
        }
        return conditional_bonus(k,value);
    }
    std::int32_t special0(std::uint32_t k) const {
        if(category(k)==0)
            return signed16(table(resources.special4,Table::special4,4*(descriptor(12,2)>>8),2));
        const auto index=(descriptor(14,2)&3U)+4*(descriptor(12,2)>>8);
        auto value=signed16(table(resources.special12,Table::special12,12*index+4,2));
        if(k==8||k==10) {
            const auto variant=descriptor(14,2)>>8;
            const auto extra=variant==1?10:variant==2||variant==3?20:variant==4?35:0;
            if(extra)value=signed16(static_cast<std::uint32_t>(value+extra));
        }
        return value;
    }
    std::int32_t global_value(std::uint32_t selector) const {
        const auto index=descriptor(22,2);
        if(index>=420)return 0;
        constexpr std::array<std::size_t,6> offsets{0,1,2,3,5,4};
        const auto value=table(resources.global,Table::global,0x3366+8*index+offsets[selector]);
        return selector?signed8(value):static_cast<std::int32_t>(value);
    }
    std::int32_t first6(std::uint32_t k) const {
        // Special branch skips the +20 modifier (2f8f30 ->2f8fac).
        if(special(k))return special6(k);
        std::int32_t value=0;
        if(category24(k))value=signed16(read(record(k),8,2));
        else if(category40(k))value=conditional_bonus(k,signed16(read(record(k),12,2)));
        if(k>=7&&k<=20&&(descriptor(18,2)>>14)==1)
            // VCVT/VADD/VCVT is exact for signed16+20. No FP operation is
            // performed here, so host rounding/FZ is not a hidden dependency.
            value=signed16(static_cast<std::uint32_t>(value+20));
        return value;
    }
    std::int32_t first0(std::uint32_t k) const {
        // Both special branches bypass the +60 modifier in the caller.
        if(generated(k))return global_value(0);
        if(special(k))return special0(k);
        std::int32_t value=1;
        if(category(k)==0||category24(k)||category40(k)) {
            const auto v=record(k);
            value=binding(v,true)?static_cast<std::int32_t>(read(v,category24(k)?10:14)):
                category24(k)?0:1;
        }
        if(k>=7&&k<=20&&(descriptor(18,2)>>14)==2)value=signed16(static_cast<std::uint32_t>(value+60));
        return value;
    }
    std::int32_t component(std::uint32_t k,std::uint32_t selector) const {
        if(generated(k))return global_value(selector);
        if(special(k)) {
            constexpr std::array<std::size_t,6> offsets{0,0,2,4,8,6};
            return signed16(table(resources.special10,Table::special10,
                10*(descriptor(12,2)&255U)+offsets[selector],2));
        }
        return signed8(read(record(k,true),11+selector));
    }
    std::int32_t secondary0(std::uint32_t k) const {
        if(descriptor(2,2)==0)return 0;
        const auto v=record(k,true);
        const bool is_generated=generated(k),is_special=!is_generated&&special(k);
        std::array<std::uint32_t,8> counts{};
        if(is_generated||is_special) {
            // Original copies all40 bytes before replacing these eight bytes.
            // Retain the source extent check even for dead copied lanes.
            for(unsigned i=0;i<40;++i)(void)read(v,i);
            const auto offset=is_generated?0x3304+8*(descriptor(20,2)&255U):
                8*(table(resources.special4,Table::special4,4*(descriptor(12,2)>>8)+2,2)&255U);
            const auto& b=is_generated?resources.global:resources.growth8;
            const auto t=is_generated?Table::global:Table::growth8;
            for(unsigned i=0;i<8;++i)counts[i]=table(b,t,offset+i);
        } else for(unsigned i=0;i<8;++i)counts[i]=read(v,20+i);
        // 2f509c uses the original record, and special (including generated)
        // returns35. Its ordinary sum is narrowed to u8 BEFORE min63.
        std::uint32_t level_limit=35;
        if(!is_generated&&!is_special) {
            std::uint32_t sum=0;for(auto n:counts)sum+=n;
            level_limit=std::min(sum&255U,63U);
        }
        const auto level=std::min(descriptor(1),level_limit);
        const auto step_table=(read(v,4)&0x10U)?1U:0U;
        std::uint32_t value=is_generated?static_cast<std::uint32_t>(global_value(0)):
            is_special?static_cast<std::uint32_t>(special0(k))&0xffffU:read(v,7);
        std::uint32_t step=1;
        for(unsigned i=0;i<8;++i) {
            for(std::uint32_t j=0;j<counts[i]&&step<=level;++j,++step) {
                const auto amount=signed8(table(resources.growth_steps[step_table],Table::growth_steps,i));
                value=static_cast<std::uint32_t>(static_cast<std::int32_t>(value)+amount)&0xffffU;
            }
            if(counts[i]!=0&&step>level)break;
        }
        return signed16(value);
    }
    void flag(std::uint32_t k,std::uint32_t mask,bool set) {
        const auto previous=out.fields.pbw.words9404[k];
        if(!previous)fail(ErrorCode::unknown_field,"PBW9404 indexed flags",Table::none,k,0x9404+4*k,4);
        const auto next=set?*previous|mask:*previous&~mask;
        out.fields.pbw.words9404[k]=next;
        out.effects.writes[out.effects.write_count++]={static_cast<std::uint8_t>(k),*previous,next};
    }
    Output run(std::uint32_t selector) {
        if(selector>=19)return out;
        if(selector>6)fail(ErrorCode::unsupported_selector,"2f8e04 selector");
        std::int32_t value=0;
        if(selector==6) {
            const auto k=descriptor(0);value=first6(k);
            const auto cap=signed16(table(resources.caps_by_kind,Table::caps_by_kind,2*k,2));
            flag(k,0x40,value>=cap);out.value=value>=cap?cap:value;return out;
        }
        if(selector==0) {
            const auto k=descriptor(0);value=first0(k);flag(k,1,value>=180);value=std::min(value,180);
        }
        for(current=1;current<7;++current) {
            const auto k=descriptor(0);if(category(k)!=0||k==6)continue;
            if(selector==0) {
                const auto component_value=secondary0(k);
                const auto cap=signed16(table(resources.caps_by_kind,Table::caps_by_kind,2*current,2));
                flag(k,1,component_value>=cap);
                value=signed16(static_cast<std::uint32_t>(value+std::min(component_value,cap)));
            } else {
                const auto component_value=component(k,selector);
                // Source copies12 bytes before reading the selected halfword.
                for(unsigned i=0;i<12;++i)(void)table(resources.query_caps,Table::query_caps,i);
                const auto cap=signed16(table(resources.query_caps,Table::query_caps,2*selector,2));
                flag(k,1U<<selector,component_value>=cap);
                if(component_value>=cap)value=cap; // MOV r4,r0 at2f957c.
                value=signed16(static_cast<std::uint32_t>(value+component_value));
            }
        }
        out.value=value;return out;
    }
};
} // namespace
Result apply(const player_fields::Fields& fields,std::uint32_t selector,const Resources& resources) noexcept {
    try {return Engine(fields,resources).run(selector);}catch(const Error& error){return error;}
}
} // namespace mh4u::player_parameter_query
