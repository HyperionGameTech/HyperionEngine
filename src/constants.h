#ifndef HYPERION_CONSTANTS_H
#define HYPERION_CONSTANTS_H

#include <types.h>

namespace hyperion {

constexpr uint max_frames_in_flight = 2;

template <class ...T>
constexpr bool resolution_failure = false;

} // namespace hyperion

#endif
