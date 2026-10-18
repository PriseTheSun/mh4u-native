#include "motion_events.hpp"
#include "motion_event_kernel.hpp"
#include "picosha2.h"
#include <algorithm>
#include <fstream>
#include <limits>
#include <set>

namespace mh4u::motion_events {
namespace {
struct Failure { Error error; };
[[noreturn]] void fail(ErrorCode c,std::string message,std::size_t offset=0) { throw Failure{{c,std::move(message),offset}}; }
bool hash_valid(std::string_view text) {
    return text.size()==64&&std::all_of(text.begin(),text.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool code_valid(std::string_view text) {
    return std::all_of(text.begin(),text.end(),[](char c){return (c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_';});
}
void validate_action(const ActionInfo& a,const Limits& limits,ErrorCode error) {
    if(a.rejection_code.size()>limits.rejection_code_bytes)fail(ErrorCode::resource_limit,"Rejection code exceeds limit");
    if(static_cast<unsigned>(a.state)>3||!code_valid(a.rejection_code))fail(error,"Unknown action state/rejection code");
    if(a.state==ActionState::empty_slot) {
        if(a.frame_count)fail(error,"Empty slot has a frame count");
    } else if(!a.frame_count||!*a.frame_count||*a.frame_count>1000000)fail(error,"Invalid action frame count");
    if((a.state==ActionState::rejected)!=!a.rejection_code.empty())fail(error,"Rejection state/code disagree");
    const auto expected=a.state==ActionState::parsed?GroupState::present:
                        a.state==ActionState::events_absent?GroupState::absent:GroupState::unavailable;
    if(!std::all_of(a.groups.begin(),a.groups.end(),[&](auto g){return g==expected;}))
        fail(error,"Group states differ from the owner action state");
}
void validate_expected(const ExpectedPackage& e,const Limits& l) {
    if(!l.package_bytes||!l.sources||!l.actions||!l.records||!l.rejection_code_bytes)
        fail(ErrorCode::resource_limit,"Limits must be positive");
    if(!hash_valid(e.package_sha256)||!hash_valid(e.inventory_sha256)||e.sources.empty()||e.actions.empty())
        fail(ErrorCode::invalid_expected,"Invalid expected hashes or empty identity sets");
    if(e.sources.size()>l.sources||e.actions.size()>l.actions)fail(ErrorCode::resource_limit,"Expected identity count exceeds limits");
    std::set<std::uint32_t> sources;std::size_t slots=0;
    for(const auto& s:e.sources) {
        if(!s.entry_id||!hash_valid(s.lmt_sha256)||!s.slot_count)fail(ErrorCode::invalid_expected,"Invalid expected source");
        if(!sources.insert(s.entry_id).second)fail(ErrorCode::duplicate_identity,"Repeated expected source entry");
        if(s.slot_count>l.actions-slots)fail(ErrorCode::resource_limit,"Expected slot count exceeds limit");
        slots+=s.slot_count;
    }
    if(slots!=e.actions.size())fail(ErrorCode::invalid_expected,"Every source slot must have one explicit state");
    std::set<std::pair<std::uint32_t,std::uint32_t>> seen;
    for(const auto& a:e.actions) {
        validate_action(a,l,ErrorCode::invalid_expected);
        const auto s=std::find_if(e.sources.begin(),e.sources.end(),[&](const auto& v){return v.entry_id==a.identity.entry_id;});
        if(s==e.sources.end()||s->lmt_sha256!=a.identity.lmt_sha256||a.identity.slot>=s->slot_count)
            fail(ErrorCode::invalid_expected,"Expected action/source mismatch");
        if(!seen.emplace(a.identity.entry_id,a.identity.slot).second)fail(ErrorCode::duplicate_identity,"Repeated expected slot");
    }
}
class Reader {
public:
    explicit Reader(std::string_view bytes):bytes_(bytes){}
    std::size_t offset{};
    std::size_t remaining() const { return bytes_.size()-offset; }
    std::string_view take(std::size_t count) {
        if(count>remaining())fail(ErrorCode::malformed_package,"Truncated field",offset);
        const auto out=bytes_.substr(offset,count);offset+=count;return out;
    }
    std::uint32_t u32() { auto s=take(4);std::uint32_t out=0;for(unsigned i=0;i<4;++i)out|=std::uint32_t(static_cast<unsigned char>(s[i]))<<(8*i);return out; }
    std::uint16_t u16() { auto s=take(2);return static_cast<std::uint16_t>(static_cast<unsigned char>(s[0])|(static_cast<unsigned char>(s[1])<<8)); }
    std::string hash() {
        constexpr char hex[]="0123456789abcdef";std::string out;out.reserve(64);
        for(unsigned char c:take(32)){out.push_back(hex[c>>4]);out.push_back(hex[c&15]);}return out;
    }
private:
    std::string_view bytes_;
};
std::string read_file(const std::filesystem::path& path,std::size_t cap) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)fail(ErrorCode::io,"Cannot open selected binary");
    const auto size=file.tellg();if(size<0)fail(ErrorCode::io,"Cannot measure selected binary");
    if(static_cast<std::uintmax_t>(size)>cap||static_cast<std::uintmax_t>(size)>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        fail(ErrorCode::resource_limit,"Binary exceeds byte limit");
    std::string bytes(static_cast<std::size_t>(size),'\0');file.seekg(0);
    if(!file.read(bytes.data(),static_cast<std::streamsize>(bytes.size()))||file.peek()!=std::char_traits<char>::eof())
        fail(ErrorCode::io,"Binary changed size or read failed");
    return bytes;
}
Result<std::uint32_t> accumulate(const Group& g,std::int32_t begin,std::int32_t end) {
    if(!g.kernel_supported)return Error{ErrorCode::unsupported_accumulator,"Signed cumulative overflow is outside this kernel profile",0};
    return detail::accumulate_validated(g.records,begin,end);
}
} // namespace

std::string_view error_name(ErrorCode code) noexcept {
    switch(code) {
#define C(x) case ErrorCode::x:return #x
    C(io);C(resource_limit);C(invalid_expected);C(package_hash_mismatch);C(binding_mismatch);
    C(unsupported_format);C(malformed_package);C(duplicate_identity);C(duplicate_group);
    C(missing_action);C(action_identity_mismatch);C(empty_slot);C(events_absent);C(events_rejected);
    C(invalid_group);C(group_unavailable);C(invalid_interval);C(unsupported_accumulator);
#undef C
    }return "unknown";
}
Result<Package> Package::decode(std::string_view bytes,const ExpectedPackage& expected,Limits limits) {
    try {
        validate_expected(expected,limits);
        if(bytes.size()>limits.package_bytes)fail(ErrorCode::resource_limit,"Package exceeds byte limit");
        if(picosha2::hash256_hex_string(bytes.begin(),bytes.end())!=expected.package_sha256)
            fail(ErrorCode::package_hash_mismatch,"Package differs from independently selected SHA-256");
        Reader r(bytes);
        if(r.take(8)!="MH4UME01"||r.u32()!=0x01020304)fail(ErrorCode::unsupported_format,"Unsupported event package/version/endian marker");
        if(r.u32()!=bytes.size())fail(ErrorCode::malformed_package,"Declared size differs from bytes",12);
        const auto sources=r.u32(),actions=r.u32();
        if(!sources||sources>limits.sources||!actions||actions>limits.actions)fail(ErrorCode::resource_limit,"Source/action count exceeds limit",16);
        if(sources!=expected.sources.size()||actions!=expected.actions.size())fail(ErrorCode::binding_mismatch,"Source/action counts differ from selected identities",16);
        if(r.u32()!=1||r.u32()!=0)fail(ErrorCode::unsupported_format,"Unsupported kernel profile/reserved word",24);
        Package out;out.inventory_sha_=r.hash();
        if(out.inventory_sha_!=expected.inventory_sha256)fail(ErrorCode::binding_mismatch,"Inventory identity differs",32);
        if(sources>r.remaining()/44)fail(ErrorCode::malformed_package,"Source count exceeds remaining bytes",r.offset);
        std::set<std::uint32_t> source_entries;
        for(std::size_t i=0;i<sources;++i) {
            SourceInfo source;source.entry_id=r.u32();source.lmt_sha256=r.hash();source.slot_count=r.u32();
            if(r.u32()!=0)fail(ErrorCode::unsupported_format,"Nonzero source reserved word",r.offset-4);
            if(!source_entries.insert(source.entry_id).second)fail(ErrorCode::duplicate_identity,"Repeated serialized source",r.offset-44);
            if(source!=expected.sources[i])fail(ErrorCode::binding_mismatch,"Source identity differs from selected source",r.offset-44);
            out.sources_.push_back(std::move(source));
        }
        // Minimum action:32-byte descriptor and four16-byte group descriptors.
        if(actions>r.remaining()/96)fail(ErrorCode::malformed_package,"Action count exceeds remaining bytes",r.offset);
        std::size_t total_records=0;std::set<std::pair<std::uint32_t,std::uint32_t>> seen;
        for(std::size_t i=0;i<actions;++i) {
            const auto source_index=r.u32(),slot=r.u32(),frames=r.u32(),state=r.u32(),reason_size=r.u32(),group_count=r.u32();
            if(r.u32()!=0||r.u32()!=0)fail(ErrorCode::unsupported_format,"Nonzero action reserved word",r.offset-8);
            if(source_index>=out.sources_.size()||state>3||group_count!=4)fail(ErrorCode::malformed_package,"Invalid source index/state/group count",r.offset-32);
            if(reason_size>limits.rejection_code_bytes)fail(ErrorCode::resource_limit,"Rejection code exceeds limit",r.offset-16);
            const auto& source=out.sources_[source_index];ActionInfo info;
            info.identity={source.entry_id,slot,source.lmt_sha256};info.state=static_cast<ActionState>(state);
            if(frames)info.frame_count=frames;
            info.rejection_code=std::string(r.take(reason_size));
            if(!seen.emplace(source.entry_id,slot).second)fail(ErrorCode::duplicate_identity,"Repeated serialized slot",r.offset);
            std::array<Group,4> groups;unsigned group_seen=0;
            for(unsigned g=0;g<4;++g) {
                const auto ordinal=r.u32(),presence=r.u32(),count=r.u32(),reserved=r.u32();
                if(ordinal>=4||presence>2)fail(ErrorCode::malformed_package,"Invalid group ordinal/presence",r.offset-16);
                if(group_seen&(1U<<ordinal))fail(ErrorCode::duplicate_group,"Repeated group ordinal",r.offset-16);
                group_seen|=1U<<ordinal;info.groups[ordinal]=static_cast<GroupState>(presence);
                if(reserved)fail(ErrorCode::unsupported_format,"Nonzero group reserved word",r.offset-4);
                if(presence!=2) {
                    if(count)fail(ErrorCode::malformed_package,"Unavailable/absent group has records",r.offset-8);
                    continue;
                }
                if(count>limits.records-total_records)fail(ErrorCode::resource_limit,"Aggregate event record count exceeds limit",r.offset-8);
                if(r.remaining()<64||count>(r.remaining()-64)/8)fail(ErrorCode::malformed_package,"Event record count exceeds remaining bytes",r.offset-8);
                auto& group=groups[ordinal];for(auto& value:group.prefix_raw)value=r.u16();
                total_records+=count;group.records.reserve(count);
                for(std::size_t n=0;n<count;++n) {
                    Record record{r.u32(),r.u32()};group.duration_sum+=record.duration;group.records.push_back(record);
                }
                group.kernel_supported=group.duration_sum<=static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max());
            }
            validate_action(info,limits,ErrorCode::malformed_package);
            if(info!=expected.actions[i])fail(ErrorCode::action_identity_mismatch,"Action state/domain/identity differs from selected inventory",r.offset);
            out.info_.push_back(std::move(info));out.groups_.push_back(std::move(groups));
        }
        if(r.remaining())fail(ErrorCode::malformed_package,"Trailing bytes",r.offset);
        return out;
    }catch(const Failure& e){return e.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Allocation failed",0};}
}
Result<Package> Package::load(const std::filesystem::path& file,const ExpectedPackage& expected,Limits limits) {
    try {validate_expected(expected,limits);return decode(read_file(file,limits.package_bytes),expected,limits);}
    catch(const Failure& e){return e.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Allocation failed",0};}
}
Result<std::size_t> Package::find(const ActionIdentity& identity) const {
    const auto it=std::find_if(info_.begin(),info_.end(),[&](const auto& a){return a.identity.entry_id==identity.entry_id&&a.identity.slot==identity.slot;});
    if(it==info_.end())return Error{ErrorCode::missing_action,"Entry/slot is outside the selected inventory",0};
    if(it->identity!=identity)return Error{ErrorCode::action_identity_mismatch,"Selected LMT hash differs",0};
    return static_cast<std::size_t>(it-info_.begin());
}
Result<const Group*> Package::group(const ActionIdentity& identity,std::uint32_t ordinal) const {
    const auto found=find(identity);if(auto e=std::get_if<Error>(&found))return *e;
    const auto index=std::get<std::size_t>(found);const auto& info=info_[index];
    if(ordinal>=4)return Error{ErrorCode::invalid_group,"Group ordinal must be0..3",0};
    if(info.state==ActionState::empty_slot)return Error{ErrorCode::empty_slot,"Source slot has no action header",0};
    if(info.state==ActionState::events_absent)return Error{ErrorCode::events_absent,"Source action event pointer is absent",0};
    if(info.state==ActionState::rejected)return Error{ErrorCode::events_rejected,info.rejection_code,0};
    if(info.groups[ordinal]!=GroupState::present)return Error{ErrorCode::group_unavailable,"Group interpretation is unavailable",0};
    return &groups_[index][ordinal];
}
Result<std::uint32_t> Package::kernel_range(const ActionIdentity& identity,std::uint32_t ordinal,std::int32_t begin,std::int32_t end) const {
    const auto result=group(identity,ordinal);if(auto e=std::get_if<Error>(&result))return *e;
    return accumulate(*std::get<const Group*>(result),begin,end);
}
Result<std::optional<PrefixHit>> Package::first_prefix(const ActionIdentity& identity,std::uint32_t ordinal,std::uint32_t mask) const {
    const auto result=group(identity,ordinal);if(auto e=std::get_if<Error>(&result))return *e;
    const auto& prefix=std::get<const Group*>(result)->prefix_raw;
    for(std::uint32_t bit=0;bit<32;++bit)
        if(mask&(std::uint32_t{1}<<bit))return std::optional<PrefixHit>{{bit,prefix[bit]}};
    return std::optional<PrefixHit>{};
}
Result<std::uint32_t> Package::range(const ActionIdentity& identity,std::uint32_t ordinal,std::int64_t begin,std::int64_t end) const {
    const auto result=group(identity,ordinal);if(auto e=std::get_if<Error>(&result))return *e;
    const auto index=std::get<std::size_t>(find(identity));
    if(begin<0||begin>end||end>=*info_[index].frame_count)return Error{ErrorCode::invalid_interval,"Expected inclusive ordered source interval in[0,N-1]",0};
    return accumulate(*std::get<const Group*>(result),static_cast<std::int32_t>(begin),static_cast<std::int32_t>(end));
}
} // namespace mh4u::motion_events
