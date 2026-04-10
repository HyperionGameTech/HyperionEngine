#pragma once

#include <Core/FxDefines.hpp>
#include <Core/FxLog.hpp>
#include <Renderer/Backend/RxUtil.hpp>

void FxTerminate();

#ifdef FX_PLATFORM_WINDOWS
#define FX_BREAKPOINT __debugbreak()
#elif FX_PLATFORM_MACOS
#define FX_BREAKPOINT __builtin_debugtrap()
#endif

template <typename... TTypes>
void FxPanic(const char* module, const char* fmt, TTypes&&... items)
{
    FxLogFatal("An irrecoverable error has occurred");

    if (module != nullptr) {
        FxLogFatal("{:s}: ", module);
    }

    FxLogFatal(fmt, std::forward<TTypes>(items)...);

    FxTerminate();
}

template <typename... TTypes>
void FxPanicVulkan(const char* module, const char* fmt, VkResult result, TTypes&&... items)
{
    FxLogFatal("An irrecoverable error has occurred");

    if (module != nullptr) {
        FxLogFatal("{:s}: ", module);
    }

    FxLogFatal(fmt, std::forward<TTypes>(items)...);
    FxLogFatal("=> Vulkan Err: {:s}", RxUtil::ResultToStr(result));

    FxTerminate();
}

#define FxModulePanic(...)       FxPanic(FxModuleName__, __VA_ARGS__)
#define FxModulePanicVulkan(...) FxPanicVulkan(FxModuleName__, __VA_ARGS__)

#define FxAssert(cond)                                                                                                 \
    if (!(cond)) {                                                                                                     \
        FxLogFatal("An assertion failed (Cond: {:s}) at ({:s}:{:d})", #cond, __FILE__, __LINE__);                      \
        FxPanic(__func__, "Assertion failed!", 0);                                                                     \
    }

#define FxAssertMsg(cond_, msg_)                                                                                       \
    if (!(cond_)) {                                                                                                    \
        FxLogFatal("An assertion failed (Cond: {:s}) at ({:s}:{:d})", #cond_, __FILE__, __LINE__);                     \
        FxLogFatal("Assertion Msg: {:s}", msg_);                                                                       \
        FxPanic(__func__, "Assertion failed!", 0);                                                                     \
    }

#if defined(FX_BUILD_DEBUG) && !defined(FX_NO_DEBUG_ASSERTS)
#define FxDebugAssert(cond)                                                                                            \
    {                                                                                                                  \
        if (!(cond)) {                                                                                                 \
            FxLogFatal("=== DEBUG ASSERTION FAILED ===");                                                              \
            FxLogFatal("An assertion failed (Cond: {:s}) at ({:s}:{:d})", #cond, __FILE__, __LINE__);                  \
            FxPanic(__func__, "Debug assertion failed!");                                                              \
        }                                                                                                              \
    }
#else
#define FxDebugAssert(cond)
#endif
