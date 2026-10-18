#pragma once
#include "player_object_query.hpp"
#include "player_direction_refresh.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <variant>

namespace mh4u::player_manager_update {
namespace pc=player_controller_step;
namespace pf=player_fields;
namespace oq=player_object_query;
namespace pr=player_reference;
using Word=std::uint32_t;
struct Unknown {bool operator==(const Unknown&)const=default;};
struct Absent {bool operator==(const Absent&)const=default;};
struct Caller {bool operator==(const Caller&)const=default;};
struct ForeignPlayer {
    const pc::PlayerFields* player{};
    const pf::Fields* fields{};
    bool operator==(const ForeignPlayer&)const=default;
};
using PlayerLink=std::variant<Unknown,Absent,Caller,ForeignPlayer>;
// Singleton1057294 only, unrelated to input-filter managerfbeffc.
struct ManagerState {
    std::optional<std::uint8_t> field112b{},field1203{},field15ff{},field15a4{},field1645{};
    std::optional<Word> field102c{},field1040{},field1044{},field1048{},field104c{};
    std::array<std::optional<std::uint8_t>,6> fields1050{};
    bool operator==(const ManagerState&)const=default;
};
struct ManagerView {const ManagerState* state{};PlayerLink player;}; // +e18
using Manager=std::variant<Unknown,Absent,ManagerView>;
struct Context {
    std::span<const motion_pending::Layer> layers; // Player244 aliases layer0.selector.
    oq::Manager objects;
    Manager manager;
    pr::Reference reference; // The same manager+1c fact supplied to admission.
    std::optional<player_direction_refresh::Virtual84> virtual84{};
};
enum class CallKind {selector,slot,enabled,angle,local_control,manager_gate,
    flag6f9,type_group,action97,action145,toggle,manager_start,descriptor,
    descriptor_predicate,global_gate,vertical_update,vertical_heading,vertical_value};
struct Call {CallKind kind;Word argument{};bool foreign{};bool operator==(const Call&)const=default;};
struct Effects {
    // Bounded source-call observation, not callbacks or a requested execution list.
    std::array<Call,256> calls{};
    std::uint16_t call_count{};
    std::optional<oq::ObjectIndex> selected_object{};
    std::uint8_t slots_examined{},enabled_queries{},accepted_objects{};
    bool caller_vertical_written{},foreign_vertical_written{};
};
struct ForeignPatch {ForeignPlayer identity;Word fielddc{},fielde0_bits{};};
struct Output {
    pc::PlayerFields player;
    pf::Fields fields;
    std::optional<ManagerState> manager{};
    std::optional<ForeignPatch> foreign{};
    Effects effects;
};
enum class Phase {collection,manager,vertical};
enum class ErrorCode {unknown_field,absent_manager,absent_player,invalid_identity,
    unsupported_binding,arithmetic,reference,collection,capacity};
struct Error {
    Phase phase;ErrorCode code;std::string_view field;
    std::variant<std::monostate,binary32_arithmetic::Error,pr::Error,oq::Error> cause{};
};
using Result=std::variant<Output,Error>;
// Complete b11f3c, complete b24c44, and their ordered caller pair. No animation
// clock, manager startup, reference producer or Player publication is inferred.
// Global for every reached Player is the caller's sole global snapshot.
// Borrowed foreign identity is resolved lazily; only PWdc/e0 are returned as a
// patch. It is never copied over the caller or published by these operations.
[[nodiscard]] Result collection_phase(const pc::PlayerFields&,const pf::Fields&,const Context&);
[[nodiscard]] Result manager_phase(const pc::PlayerFields&,const pf::Fields&,const Context&);
[[nodiscard]] Result apply(const pc::PlayerFields&,const pf::Fields&,const Context&);
} // namespace mh4u::player_manager_update
