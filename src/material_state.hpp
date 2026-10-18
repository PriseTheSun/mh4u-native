#pragma once
#include "binary32_arithmetic.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

namespace mh4u::material_state {
using Word=std::uint32_t;
using Byte=std::optional<std::uint8_t>;
template<std::size_t N> using Bytes=std::array<Byte,N>;
struct Unknown {bool operator==(const Unknown&)const=default;};
struct Absent {bool operator==(const Absent&)const=default;};
template<class Tag> struct Index {Word value;bool operator==(const Index&)const=default;};
using MaterialIndex=Index<struct MaterialTag>;
using ReferenceIndex=Index<struct ReferenceTag>;
using DescriptorIndex=Index<struct DescriptorTag>;
using PropertyTableIndex=Index<struct PropertyTableTag>;
using BufferIndex=Index<struct BufferTag>;
using ResourceIndex=Index<struct ResourceTag>;
using ShaderTableIndex=Index<struct ShaderTableTag>;
template<class I> using Link=std::variant<Unknown,Absent,I>;
using ReferenceLink=Link<ReferenceIndex>;
struct BufferLocation {BufferIndex buffer;Word offset{};std::uint8_t tag{};bool operator==(const BufferLocation&)const=default;};
struct Opaque {std::uint64_t identity;bool operator==(const Opaque&)const=default;};
using PropertyValue=std::variant<Unknown,Absent,ReferenceIndex,BufferLocation,Opaque>;
struct Property {std::optional<Word> control{};PropertyValue value{};std::optional<Word> word8{};bool operator==(const Property&)const=default;};
struct PropertyTable {std::vector<Property> records;bool operator==(const PropertyTable&)const=default;};
struct Descriptor {std::optional<Word> id{};Link<DescriptorIndex> parent{};bool operator==(const Descriptor&)const=default;};
enum class Binding {material_std};
struct Material {
    std::optional<Binding> binding{};
    Link<DescriptorIndex> descriptor{}; // Concrete virtual14 result, never a guessed class.
    Bytes<0x2c> bytes04{}; // +04..2f, including timestamp4 and packed word18.
    Link<PropertyTableIndex> parameters{}; // +30, sole canonical pointer owner.
    Bytes<0x38> bytes34{}; // +34..6b, includes four shader slots +48..67.
    ReferenceLink shader{}; // +6c, sole canonical pointer owner.
    Bytes<0x98> packed70{}; // +70..107; raw packed words/bytes, not GPU objects.
    bool operator==(const Material&)const=default;
};
struct Reference {
    Bytes<0x10> bytes00{}; // Includes lifecycle word4, refcount8 and property word0c.
    Link<ShaderTableIndex> shader_table{}; // +10; consumed only by 637f44.
    Bytes<0x14> bytes14{}; // +14..27, including flags24.
    bool operator==(const Reference&)const=default;
};
struct ShaderTable {
    std::optional<Word> count{};
    // Entry identities are not exposed: entries are immutable here and only
    // their word4 is consumed. No pointer replacement or callback occurs.
    std::vector<std::optional<Word>> entry_flags;
    bool operator==(const ShaderTable&)const=default;
};
struct Buffer {std::vector<Byte> bytes;bool operator==(const Buffer&)const=default;};
enum class ResourceBinding {rmaterial};
struct MaterialResource {
    std::optional<ResourceBinding> binding{};
    std::optional<Word> count{};
    std::vector<Link<MaterialIndex>> materials;
    bool operator==(const MaterialResource&)const=default;
};
struct State {
    std::vector<Material> materials;
    std::vector<Descriptor> descriptors;
    std::vector<PropertyTable> properties;
    std::vector<Buffer> buffers;
    std::vector<Reference> references;
    std::vector<ShaderTable> shader_tables;
    std::vector<MaterialResource> resources;
    bool operator==(const State&)const=default;
};
struct Object {
    std::vector<Link<ResourceIndex>> banks;
    std::optional<Word> destination_count{};
    std::vector<Link<MaterialIndex>> destinations;
    std::optional<Word> flags104{};
    std::array<ReferenceLink,3> caches{};
    bool operator==(const Object&)const=default;
};
struct Context {
    std::array<std::optional<Word>,4> channel_templates{};
    std::optional<Word> release_threshold{},retirement_delay{},buffer_word_count{};
};
// Explicit allocation/work policy, not claimed source format cardinalities.
struct Limits {std::size_t representation_bytes=16U<<20,work=1U<<20;};
enum class Phase {capacity,resource,destination,descriptor,shader,property,copy,refresh,channels,release,caches};
enum class ErrorCode {capacity,unknown,absent,extent,unsupported_binding,invalid_identity,descriptor_cycle,unsupported_destructor,unsupported_retirement,unsupported_overlap,arithmetic};
struct Error {Phase phase;ErrorCode code;std::string_view field;std::optional<Word> index{};std::optional<binary32_arithmetic::Error> arithmetic{};};
struct Effects {std::size_t destinations{},compatible{},shader_rebindings{},property_setters{},refreshes{},copies{},copied_bytes{},releases{},acquires{},cache_clears{};bool resource_gate{};};
struct Output {State state;Object object;Effects effects;};
using Result=std::variant<Output,Error>;
// Pools are disjoint allocations: Material-as-Reference and cross-pool aliasing
// are outside this representation. Producers must reject those aliases, never
// create a second mutable identity. Aliases within each pool remain canonical.
// Exclusive synchronous snapshots. No concurrency, clone, destruction,
// retirement, allocator or renderer callback is substituted. Resource/descriptor
// presence and initialized bytes are explicit and validated only when consumed.
// Bounds/copy budget are checked before cloning the caller's canonical state.
[[nodiscard]] Result apply_bank(const State&,const Object&,Word bank,Word exclusion,const Context&,Limits={});
[[nodiscard]] Result set_channels(const State&,MaterialIndex,const std::array<std::optional<Word>,4>&,const Context&,Limits={});
[[nodiscard]] Result refresh(const State&,MaterialIndex,const Context&,Limits={});
} // namespace mh4u::material_state
