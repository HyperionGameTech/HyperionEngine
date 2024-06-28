/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_PERFORMANCE_CLOCK_HPP
#define HYPERION_PERFORMANCE_CLOCK_HPP

#include <core/Defines.hpp>

#include <Types.hpp>

namespace hyperion {

class HYP_API PerformanceClock
{
public:
    static uint64 Now();

    /*! Get time since the given timestamp in microseconds */
    static uint64 TimeSince(uint64 microseconds);
};

} // namespace hyperion

#endif