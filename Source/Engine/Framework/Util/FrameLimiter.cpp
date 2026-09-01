/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Framework/Util/FrameLimiter.hpp>

#include <algorithm>
#include <thread>

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <timeapi.h>
#pragma comment(lib, "winmm.lib")
#endif

namespace Hyperion {

FrameLimiter::FrameLimiter(int targetFps)
    : m_targetFps(targetFps)
{
    if (m_targetFps <= 0)
    {
        return;
    }

#ifdef _WIN32
    timeBeginPeriod(1);
#endif

    m_frameDuration = std::chrono::nanoseconds(1000000000LL / m_targetFps);
    m_nextFrameTime = Clock::now() + m_frameDuration;
}

FrameLimiter::~FrameLimiter()
{
#ifdef _WIN32
    timeEndPeriod(1);
#endif
}

void FrameLimiter::SetTargetFPS(int targetFps)
{
    if (m_targetFps == targetFps)
    {
        return;
    }

    targetFps = std::max(1, targetFps);

    m_frameDuration = std::chrono::nanoseconds(1000000000LL / targetFps);
    m_nextFrameTime = Clock::now() + m_frameDuration;

    m_targetFps = targetFps;
}

void FrameLimiter::Wait()
{
    if (m_targetFps <= 0)
    {
        return;
    }

    const auto now = Clock::now();

    const auto sleepUntil = m_nextFrameTime - std::chrono::milliseconds(2);
    if (sleepUntil > now)
    {
        std::this_thread::sleep_until(sleepUntil);
    }

    while (Clock::now() < m_nextFrameTime)
    {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_pause();
#elif defined(__aarch64__) || defined(_M_ARM64)
        __asm__ volatile("yield");
#endif
    }

    m_nextFrameTime += m_frameDuration;

    if (Clock::now() > m_nextFrameTime + std::chrono::milliseconds(50))
    {
        m_nextFrameTime = Clock::now() + m_frameDuration;
    }
}

} // namespace Hyperion
