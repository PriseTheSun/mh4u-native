#include "model_motion.hpp"
#include "picosha2.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <set>

namespace mh4u::model_motion {
namespace {
constexpr std::string_view magic="MH4UMC02";
struct Failure { Error error; };
[[noreturn]] void fail(ErrorCode code,std::string text,std::size_t offset=0) { throw Failure{{code,std::move(text),offset}}; }
bool valid_hash(std::string_view s) {
    return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});
}
bool valid_path(std::string_view path,std::size_t limit) {
    if(path.empty()||path.size()>limit||path.front()=='/'||path.back()=='/')return false;
    for(char c:path)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='/'||c=='.'||c=='_'||c=='-'))return false;
    std::size_t start=0;
    for(;;) {
        const auto end=path.find('/',start);const auto part=path.substr(start,end==path.npos?path.size()-start:end-start);
        if(part.empty()||part=="."||part=="..")return false;
        if(end==path.npos)return true;
        start=end+1;
    }
}
void validate_expected(const ExpectedPackage& expected,const Limits& limits) {
    if(!limits.package_bytes||!limits.bound_asset_bytes||!limits.actions||!limits.keys||!limits.path_bytes||limits.asset_bindings<2)
        fail(ErrorCode::resource_limit,"Limits must be positive");
    if(!valid_hash(expected.package_sha256)||!valid_hash(expected.gltf.sha256)||!valid_hash(expected.curves.sha256)||
       !valid_path(expected.gltf.relative_path,limits.path_bytes)||!valid_path(expected.curves.relative_path,limits.path_bytes)||
       expected.gltf.relative_path==expected.curves.relative_path||expected.actions.empty())
        fail(ErrorCode::invalid_expected,"Invalid package hash or asset bindings");
    if(expected.dependencies.size()>limits.asset_bindings-2)fail(ErrorCode::resource_limit,"Asset binding count exceeds limit");
    std::set<std::string> paths{expected.gltf.relative_path,expected.curves.relative_path};
    for(const auto& binding:expected.dependencies) {
        if(!valid_path(binding.relative_path,limits.path_bytes)||!valid_hash(binding.sha256))fail(ErrorCode::invalid_expected,"Invalid dependent asset binding");
        if(!paths.insert(binding.relative_path).second)fail(ErrorCode::duplicate_identity,"Asset binding path appears twice");
    }
    if(expected.actions.size()>limits.actions)fail(ErrorCode::resource_limit,"Expected actions exceed limit");
    std::set<std::pair<std::uint32_t,std::uint32_t>> seen;
    std::set<std::uint32_t> clips;
    for(const auto& a:expected.actions) {
        if(!a.identity.entry_id||!valid_hash(a.identity.lmt_sha256)||!a.frame_count||a.frame_count>1000000||
           a.loop_frame_raw< -1||a.loop_frame_raw>=static_cast<std::int32_t>(a.frame_count)||a.presence_mask>3)
            fail(ErrorCode::invalid_expected,"Invalid expected action identity/frame domain");
        if(!seen.emplace(a.identity.entry_id,a.identity.slot).second)fail(ErrorCode::duplicate_identity,"Expected entry/slot appears twice");
        if(!clips.insert(a.gltf_clip_index).second)fail(ErrorCode::duplicate_identity,"Expected glTF clip appears twice");
    }
}
class Reader {
public:
    explicit Reader(std::string_view input):bytes(input){}
    std::string_view take(std::size_t size) {
        if(size>bytes.size()-offset)fail(ErrorCode::malformed_package,"Truncated field",offset);
        auto out=bytes.substr(offset,size);offset+=size;return out;
    }
    std::uint32_t u32() {
        auto s=take(4);std::uint32_t v=0;
        for(unsigned i=0;i<4;++i)v|=std::uint32_t(static_cast<unsigned char>(s[i]))<<(8*i);
        return v;
    }
    std::int32_t i32() { return std::bit_cast<std::int32_t>(u32()); }
    double f64() {
        static_assert(std::numeric_limits<double>::is_iec559&&sizeof(double)==8);
        auto s=take(8);std::uint64_t v=0;
        for(unsigned i=0;i<8;++i)v|=std::uint64_t(static_cast<unsigned char>(s[i]))<<(8*i);
        const double out=std::bit_cast<double>(v);
        if(!std::isfinite(out))fail(ErrorCode::nonfinite_value,"Nonfinite key frame/component",offset-8);
        return out;
    }
    std::string hash() {
        constexpr char hex[]="0123456789abcdef";auto s=take(32);std::string result;result.reserve(64);
        for(unsigned char c:s){result.push_back(hex[c>>4]);result.push_back(hex[c&15]);}return result;
    }
    AssetBinding binding(const Limits& limits) {
        const auto length=u32();if(length>limits.path_bytes)fail(ErrorCode::resource_limit,"Path exceeds limit",offset-4);
        auto path=take(length);if(!valid_path(path,limits.path_bytes))fail(ErrorCode::malformed_package,"Noncanonical relative binding path",offset-length);
        return {std::string(path),hash()};
    }
    std::size_t remaining() const { return bytes.size()-offset; }
    std::string_view bytes;std::size_t offset{};
};
std::string read_file(const std::filesystem::path& path,std::size_t cap) {
    std::ifstream file(path,std::ios::binary|std::ios::ate);
    if(!file)fail(ErrorCode::io,"Cannot open selected local file");
    const auto size=file.tellg();
    if(size<0)fail(ErrorCode::io,"Cannot measure selected local file");
    if(static_cast<std::uintmax_t>(size)>cap||static_cast<std::uintmax_t>(size)>static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
        fail(ErrorCode::resource_limit,"File exceeds byte limit");
    std::string bytes(static_cast<std::size_t>(size),'\0');file.seekg(0);
    if(!file.read(bytes.data(),static_cast<std::streamsize>(bytes.size()))||file.peek()!=std::char_traits<char>::eof())
        fail(ErrorCode::io,"File changed size or read failed");
    return bytes;
}
std::size_t verify_asset(const std::filesystem::path& root,const AssetBinding& binding,std::size_t cap) {
    const auto path=std::filesystem::canonical(root/binding.relative_path);
    const auto relative=path.lexically_relative(root);
    if(relative.empty()||relative.is_absolute()||*relative.begin()=="..")fail(ErrorCode::binding_mismatch,"Bound asset escapes selected root");
    const auto bytes=read_file(path,cap);
    if(picosha2::hash256_hex_string(bytes)!=binding.sha256)fail(ErrorCode::binding_mismatch,"Bound asset bytes differ from selected hash");
    return bytes.size();
}
}

std::string_view error_name(ErrorCode code) noexcept {
    switch(code) {
#define CASE(value) case ErrorCode::value:return #value
    CASE(io);CASE(resource_limit);CASE(invalid_expected);CASE(package_hash_mismatch);
    CASE(binding_mismatch);CASE(unsupported_format);CASE(malformed_package);
    CASE(duplicate_identity);CASE(duplicate_component);CASE(nonfinite_value);
    CASE(invalid_timeline);CASE(missing_action);CASE(action_identity_mismatch);CASE(invalid_sample_frame);
#undef CASE
    }return "unknown";
}

Result<Package> Package::decode(std::string_view bytes,const ExpectedPackage& expected,Limits limits) {
    try {
        validate_expected(expected,limits);
        if(bytes.size()>limits.package_bytes)fail(ErrorCode::resource_limit,"Package exceeds byte limit");
        if(picosha2::hash256_hex_string(bytes.begin(),bytes.end())!=expected.package_sha256)
            fail(ErrorCode::package_hash_mismatch,"Package SHA-256 differs from independently selected identity");
        Reader r(bytes);
        if(r.take(8)!=magic||r.u32()!=0x01020304)fail(ErrorCode::unsupported_format,"Unsupported model curve version/endian marker");
        if(r.u32()!=bytes.size())fail(ErrorCode::malformed_package,"Declared package size differs from input",12);
        const auto count=r.u32();if(!count||count>limits.actions)fail(ErrorCode::resource_limit,"Action count outside limits",16);
        if(count!=expected.actions.size())fail(ErrorCode::action_identity_mismatch,"Action count differs from selected identities",16);
        if(r.u32()!=0)fail(ErrorCode::unsupported_format,"Nonzero reserved header",20);
        Package out;out.gltf_=r.binding(limits);out.curves_=r.binding(limits);
        if(out.gltf_!=expected.gltf||out.curves_!=expected.curves)fail(ErrorCode::binding_mismatch,"Serialized asset bindings differ from selected identities",r.offset);
        const auto dependencies=r.u32();
        if(dependencies>limits.asset_bindings-2)fail(ErrorCode::resource_limit,"Dependent binding count exceeds limit",r.offset-4);
        if(dependencies!=expected.dependencies.size())fail(ErrorCode::binding_mismatch,"Dependent binding count differs from expected inventory",r.offset-4);
        if(dependencies>r.remaining()/37)fail(ErrorCode::malformed_package,"Dependent binding count exceeds remaining bytes",r.offset-4);
        for(std::size_t i=0;i<dependencies;++i) {
            auto binding=r.binding(limits);
            if(binding!=expected.dependencies[i])fail(ErrorCode::binding_mismatch,"Dependent asset identity differs from expected inventory",r.offset);
            out.dependencies_.push_back(std::move(binding));
        }
        // Each action has at least 64 bytes, even with both channels absent.
        if(count>r.remaining()/64)fail(ErrorCode::malformed_package,"Action count exceeds remaining record bytes",r.offset);
        std::set<std::pair<std::uint32_t,std::uint32_t>> identities;
        std::size_t total_keys=0;
        for(std::size_t i=0;i<count;++i) {
            ActionInfo info;info.identity.entry_id=r.u32();info.identity.slot=r.u32();info.identity.lmt_sha256=r.hash();
            info.frame_count=r.u32();info.loop_frame_raw=r.i32();info.presence_mask=r.u32();const auto channel_count=r.u32();
            info.gltf_clip_index=r.u32();
            if(r.u32()!=0)fail(ErrorCode::unsupported_format,"Nonzero reserved action field",r.offset-4);
            if(!identities.emplace(info.identity.entry_id,info.identity.slot).second)fail(ErrorCode::duplicate_identity,"Repeated entry/slot",r.offset);
            if(info!=expected.actions[i])fail(ErrorCode::action_identity_mismatch,"Action identity/domain/presence differs from selected manifest",r.offset);
            if(channel_count>2||channel_count!=static_cast<std::uint32_t>(std::popcount(info.presence_mask)))fail(ErrorCode::malformed_package,"Channel count/presence mask disagree",r.offset);
            Action action;unsigned seen=0;
            for(std::uint32_t c=0;c<channel_count;++c) {
                const auto component=r.u32(),mode=r.u32();const auto ordinal=r.u32();(void)ordinal;
                const auto key_count=r.u32();
                if(component!=1&&component!=2)fail(ErrorCode::unsupported_format,"Unsupported channel component",r.offset-16);
                if(seen&component)fail(ErrorCode::duplicate_component,"Repeated channel component",r.offset-16);
                seen|=component;
                if(!(info.presence_mask&component))fail(ErrorCode::malformed_package,"Channel not present in declared mask",r.offset);
                if(mode>1||(component==2&&mode!=0))fail(ErrorCode::unsupported_format,"Only constant raw rotation and constant/linear translation supported",r.offset-12);
                const std::size_t dimension=component==1?3:4, stride=8*(dimension+1);
                if(!key_count||key_count>limits.keys-total_keys)fail(ErrorCode::resource_limit,"Key count exceeds aggregate limit",r.offset-4);
                if(key_count>r.remaining()/stride)fail(ErrorCode::malformed_package,"Key count exceeds remaining bytes",r.offset-4);
                total_keys+=key_count;Channel channel;channel.interpolation=mode;channel.keys.reserve(key_count);
                for(std::size_t k=0;k<key_count;++k) {
                    Key key;key.frame=r.f64();for(std::size_t j=0;j<dimension;++j)key.value[j]=r.f64();
                    if(key.frame<0||key.frame>info.frame_count-1||(!k&&key.frame!=0)||
                       (k&&key.frame<=channel.keys.back().frame))fail(ErrorCode::invalid_timeline,"Key frames must start at zero and strictly increase within [0,N-1]",r.offset-stride);
                    if(mode==0&&k&&key.value!=channel.keys.front().value)fail(ErrorCode::malformed_package,"Constant channel changes value",r.offset-stride);
                    channel.keys.push_back(key);
                }
                if(channel.keys.back().frame!=info.frame_count-1)fail(ErrorCode::invalid_timeline,"Last key must equal source N-1",r.offset);
                if(component==1)action.translation=std::move(channel);else action.rotation=std::move(channel);
            }
            out.info_.push_back(std::move(info));out.actions_.push_back(std::move(action));
        }
        if(r.remaining())fail(ErrorCode::malformed_package,"Trailing bytes after final action",r.offset);
        return out;
    }catch(const Failure& e){return e.error;}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Allocation failed",0};}
}

Result<Package> Package::load(const std::filesystem::path& binary,const ExpectedPackage& expected,
                              const std::filesystem::path& paired_asset_root,Limits limits) {
    try {
        validate_expected(expected,limits);
        auto decoded=decode(read_file(binary,limits.package_bytes),expected,limits);
        if(auto error=std::get_if<Error>(&decoded))return *error;
        const auto root=std::filesystem::canonical(paired_asset_root);
        auto remaining=limits.bound_asset_bytes;
        remaining-=verify_asset(root,expected.gltf,remaining);remaining-=verify_asset(root,expected.curves,remaining);
        for(const auto& binding:expected.dependencies)remaining-=verify_asset(root,binding,remaining);
        return decoded;
    }catch(const Failure& e){return e.error;}
    catch(const std::filesystem::filesystem_error&){return Error{ErrorCode::io,"Cannot resolve bound asset root/path",0};}
    catch(const std::bad_alloc&){return Error{ErrorCode::resource_limit,"Allocation failed",0};}
}

Result<Sample> Package::sample(const ActionIdentity& identity,double frame) const {
    const auto it=std::find_if(info_.begin(),info_.end(),[&](const auto& a){return a.identity.entry_id==identity.entry_id&&a.identity.slot==identity.slot;});
    if(it==info_.end())return Error{ErrorCode::missing_action,"Entry/slot is absent",0};
    if(it->identity!=identity)return Error{ErrorCode::action_identity_mismatch,"LMT hash differs for selected entry/slot",0};
    if(!std::isfinite(frame)||frame<0||frame>it->frame_count-1)return Error{ErrorCode::invalid_sample_frame,"Query is outside finite source [0,N-1]",0};
    const auto evaluate=[frame](const Channel& channel) {
        const auto right=std::upper_bound(channel.keys.begin(),channel.keys.end(),frame,[](double value,const Key& key){return value<key.frame;});
        const auto& left=*(right-1);
        if(channel.interpolation==0||right==channel.keys.end()||frame==left.frame)return left.value;
        const double t=(frame-left.frame)/(right->frame-left.frame);std::array<double,4> value{};
        for(std::size_t i=0;i<4;++i)value[i]=(1-t)*left.value[i]+t*right->value[i];
        return value;
    };
    Sample result;const auto& action=actions_[static_cast<std::size_t>(it-info_.begin())];
    if(action.translation){const auto value=evaluate(*action.translation);
        if(!std::all_of(value.begin(),value.end(),[](double x){return std::isfinite(x);}))
            return Error{ErrorCode::nonfinite_value,"Sample arithmetic produced a nonfinite result",0};
        result.translation=Vector{value[0],value[1],value[2]};}
    if(action.rotation)result.rotation=evaluate(*action.rotation);
    return result;
}
} // namespace mh4u::model_motion
