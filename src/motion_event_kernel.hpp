#pragma once
#include "motion_events.hpp"
#include <span>

namespace mh4u::motion_events::detail {
// Shared numeric kernel of the setter and later event queries. The owning
// boundary must prove every cumulative duration <= INT32_MAX before calling.
// This internal function neither parses a source nor repairs its intervals.
inline std::uint32_t accumulate_validated(std::span<const Record> records,
                                         std::int32_t begin,std::int32_t end) noexcept {
    std::int64_t cumulative=0;std::uint32_t mask=0;
    for(const auto& record:records) {
        cumulative+=record.duration;
        if(cumulative>begin)mask|=record.mask;
        if(cumulative>end)break;
    }
    return mask;
}
} // namespace mh4u::motion_events::detail
