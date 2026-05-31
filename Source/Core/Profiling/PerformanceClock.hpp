/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Types.hpp>

namespace Hyperion {
namespace profiling {

class CORE_API PerformanceClock
{
public:
    static uint64 Now();

    /*! Get time since the given timestamp in Milliseconds */
    static double TimeSince(uint64 timestamp);
    static double ToMilliseconds(uint64 timestamp);

    PerformanceClock();

    double ElapsedMs() const;

    void Start();
    void Stop();

private:
    uint64 m_startTime;
    uint64 m_endTime;
};

} // namespace profiling

using profiling::PerformanceClock;

} // namespace Hyperion
