#pragma once
#include "../player_contact_update.hpp"

// Original entry-PC projection shared by standalone and continuous adapters.
namespace mh4u::test_contact_effects {
inline std::optional<player_contact_update::CallKind> call(std::uint32_t pc){using C=player_contact_update::CallKind;switch(pc){case 0xb6ab74:return C::integrate;case 0xb162a8:return C::position;case 0xb4620c:return C::attribute_mask;case 0xb43194:return C::wrapper_flags;case 0xd04924:return C::local_control;case 0x2ba230:return C::height_wrapper;case 0x2bb178:return C::height_query;case 0xb07b90:return C::area_match;case 0xd0ce2c:return C::radius;case 0xd0c9f8:return C::manager_selector;default:return std::nullopt;}}
}
