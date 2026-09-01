/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Defines.hpp>

#include <chrono>

namespace Hyperion {

class ENGINE_API FrameLimiter
{
public:
    FrameLimiter(int targetFps);
    ~FrameLimiter();

    FrameLimiter(const FrameLimiter &other) = delete;
    FrameLimiter &operator=(const FrameLimiter &other) = delete;

    void SetTargetFPS(int targetFps);
    void Wait();

private:
    using Clock = std::chrono::steady_clock;

    Clock::time_point m_nextFrameTime;
    std::chrono::nanoseconds m_frameDuration;
    int m_targetFps;
};

} // namespace Hyperion
