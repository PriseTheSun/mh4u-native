#pragma once
#include "player_policy_table.hpp"
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace mh4u::player_transition {
// Raw numeric fields retain their source offsets. They are not gameplay names.
// Pointers are not accepted: this profile fixes the concrete uPlayer virtual.
struct PlayerFields {
    std::uint32_t field3b8{};
    std::array<std::uint32_t,3> fieldsd10{};
    bool operator==(const PlayerFields&) const=default;
};
struct PbwFields {
    std::array<std::uint8_t,3> fields5{};
    std::uint32_t field20{},field24{},field28{};
    std::uint8_t field30{},field31{},field33{};
    std::array<std::uint8_t,4> fields2f8{};
    std::uint8_t field371{}; std::uint16_t field372{};
    std::uint8_t field377{}; std::uint16_t field378{};
    std::uint32_t field39c{};
    std::array<std::uint16_t,2> angles{},magnitudes{};
    std::uint32_t field3d4{}; std::uint8_t field451{};
    bool operator==(const PbwFields&) const=default;
};
struct PwFields {
    std::array<std::uint8_t,2> fieldsc{};
    std::uint32_t field4c{},field54{},field58{}; std::uint8_t field79{};
    std::uint32_t fieldb0{}; std::uint16_t fieldbc{};
    std::uint32_t fieldc4{},fielddc{},fielde8{},fieldec{},fieldf0{},fieldf4{},fieldf8{},fieldfc{};
    player_policy_table::State policy; // Sole owner of pw+100..148.
    std::uint8_t field17c{};
    std::uint32_t field1f4{}; std::uint8_t field261{};
    std::array<std::uint8_t,2> fields290{};
    std::uint32_t field334{}; std::uint8_t field33b{};
    std::array<std::uint8_t,2> fields35d{},fields3f5{};
    std::array<std::uint8_t,4> fields488{}; std::uint32_t field48c{};
    std::uint8_t field490{}; std::uint16_t field4c2{}; std::uint32_t field504{};
    std::uint8_t field50d{},field5e4{},field6c0{},field9d8{};
    bool operator==(const PwFields&) const=default;
};
struct GlobalFields {
    std::uint16_t field22{};
    std::uint8_t field2b{},field2f{},field5d{};
    std::optional<std::uint8_t> fieldd72{}; // Same fb6b7c object, consumed by 2b1f04.
    bool operator==(const GlobalFields&) const=default;
};
// Source d04924; shares canonical global/pbw fields with all consumers.
bool local_control(const GlobalFields&,const PbwFields&) noexcept;
struct State {
    PlayerFields player; PbwFields pbw; PwFields pw; GlobalFields global;
    bool operator==(const State&) const=default;
};
struct Request {
    // Full ARM argument bits are used in tests; only stores narrow to u8/u16.
    std::uint32_t state_arg{},action_arg{},flags_arg{};
};
struct ObservedDispatchCall {
    // Original b66f0c was entered and returned through global+5d==0.
    // This is a record of that call, never an instruction to dispatch later.
    std::uint32_t selector{},flags_argument{};
    bool operator==(const ObservedDispatchCall&) const=default;
};
struct Output {
    State state;
    std::uint32_t effective_flags{};
    std::optional<ObservedDispatchCall> returned_dispatch_call;
};
enum class ErrorCode {
    invalid_expected,constants_identity,unsupported_constants,type_table_extent,
    optional_dispatch_body,early_optional_helpers,old_state7_helpers,type13_helpers,
    angular_reference,unsupported_float_environment,sector_failure
};
struct Error {ErrorCode code;std::string_view message;};
template<class T>using Result=std::variant<T,Error>;
std::string_view error_name(ErrorCode) noexcept;
struct ExpectedConstants {std::string package_sha256,source_elf_sha256;};
class Constants {
public:
    // 88-byte little-endian MH4UPT01; SHA selected independently of its bytes.
    static Result<Constants> decode(std::span<const std::uint8_t>,const ExpectedConstants&);
private:
    Constants()=default;
    std::array<std::uint8_t,32> type_bytes_{};
    std::uint32_t multiplier_{},scale_bits_{},power_bits_{},angle_offset_{};
    friend Result<Output> apply(const State&,const Request&,const Constants&);
};
// Pure transactional numeric profile of b1c050 with real-helper semantics.
// Validates every excluded gate before producing state. No callback/gameplay.
Result<Output> apply(const State&,const Request&,const Constants&);
} // namespace mh4u::player_transition
