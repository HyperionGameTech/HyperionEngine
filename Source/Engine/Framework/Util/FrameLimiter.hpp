/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <Core/Memory/Pimpl.hpp>

#include <chrono>

namespace Hyperion {

class ENGINE_API FrameLimiter
{
public:
    explicit FrameLimiter(int targetFps);
    ~FrameLimiter();

    FrameLimiter(const FrameLimiter &other) = delete;
    FrameLimiter &operator=(const FrameLimiter &other) = delete;

    void SetTargetRate(int targetFps);
    void Wait();

private:
    using Clock = std::chrono::steady_clock;
    
    int m_targetFps;

    Clock::time_point m_nextFrameTime;
    std::chrono::nanoseconds m_frameDuration;

#ifdef _WIN32
    Pimpl<void> m_win32SetTimerPeriodState;
#endif // _WIN32
};

} // namespace Hyperion
