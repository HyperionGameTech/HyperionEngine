/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <csignal>
#include <cstdio>

namespace hyperion {
namespace debug {

HYP_API extern char* GetErrorStringBuffer();

enum class LogType : int
{
    Info,
    Warn,
    Error,
    Fatal,
    Debug,

    RenInfo,
    RenWarn,
    RenError,
    RenDebug,
};

#ifdef HYP_DEBUG_MODE
// #define DebugLog(type, fmt) DebugLog(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, fmt)
#define DebugLog(type, ...) \
    debug::DebugLog_Write(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, __VA_ARGS__)

extern HYP_API void DebugLog_Write(LogType type, const char* callee, unsigned int line, const char* fmt, ...);
#else
#define DebugLog(type, ...) \
    debug::DebugLog_Write(type, __VA_ARGS__)

extern HYP_API void DebugLog_Write(LogType type, const char* fmt, ...);
#endif

extern HYP_API void DebugLog_FlushOutputStream();

extern HYP_API void LogStackTrace(int depth = 10);

extern HYP_API void WriteToStandardError(const char* msg);

extern HYP_API bool IsDebuggerAttached();

extern HYP_API void LogAssert(const char* str);

} // namespace debug

using debug::LogType;
} // namespace hyperion

#if defined(HYP_USE_EXCEPTIONS) && HYP_USE_EXCEPTIONS
#define HYP_THROW(msg) throw ::std::runtime_error(msg)
#else
#ifdef HYP_DEBUG_MODE
#define HYP_THROW(msg)                        \
    do                                        \
    {                                         \
        debug::WriteToStandardError(&msg[0]); \
        HYP_PRINT_STACK_TRACE();              \
        std::terminate();                     \
    }                                         \
    while (0)
#else
#define HYP_THROW(msg) std::terminate()
#endif
#endif

#define HYP_UNREACHABLE() HYP_FAIL("Expected this section to be unreached!")

#if defined(HYP_USE_EXCEPTIONS) && HYP_USE_EXCEPTIONS
#define HYP_NOT_IMPLEMENTED()                                            \
    do                                                                   \
    {                                                                    \
        HYP_THROW("Function not implemented: " HYP_STR(HYP_DEBUG_FUNC)); \
        std::terminate();                                                \
    }                                                                    \
    while (0)
#else
#define HYP_NOT_IMPLEMENTED() HYP_THROW("Not implemented: " HYP_STR(HYP_DEBUG_FUNC))
#endif

// obsolete
#define HYP_NOT_IMPLEMENTED_VOID() HYP_NOT_IMPLEMENTED()
#define HYP_EXPAND(x) x

#define DebugLogRaw(type, ...) \
    debug::DebugLog_Write(type, nullptr, 0, __VA_ARGS__)

#define HYP_HAS_ARGS(...) HYP_HAS_ARGS_(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, )
#define HYP_HAS_ARGS_(a1, a2, a3, a4, a5, b1, b2, b3, b4, b5, c1, c2, c3, c4, c5, d1, d2, d3, d4, d5, e, N, ...) N

#define HYP_SELECT_ASSERT(n) HYP_CONCAT(HYP_ASSERT, n)

// #define Assert(...) \
//     HYP_EXPAND(                                                                 \
//         HYP_SELECT_ASSERT( HYP_EXPAND( HYP_HAS_ARGS(__VA_ARGS__) ) )            \
//     )(__VA_ARGS__)

#define HYP_ASSERT0(expr) HYP_ASSERT1(expr, )

#define HYP_ASSERT1(expression, ...)                                                                              \
    do                                                                                                            \
    {                                                                                                             \
        if (HYP_UNLIKELY(!(expression)))                                                                          \
        {                                                                                                         \
            auto format = HYP_FORMAT("Assertion failed!\n\tCondition: " #expression "\n\tMessage: " __VA_ARGS__); \
            debug::LogAssert(&format[0]);                                                                         \
            debug::DebugLog_FlushOutputStream();                                                                  \
                                                                                                                  \
            if (debug::IsDebuggerAttached())                                                                      \
                HYP_BREAKPOINT;                                                                                   \
            else                                                                                                  \
            {                                                                                                     \
                HYP_PRINT_STACK_TRACE();                                                                          \
                std::terminate();                                                                                 \
            }                                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    while (0)

#if defined(HYP_MSVC) && defined(_MSVC_TRADITIONAL) && _MSVC_TRADITIONAL
#error "Traditional MSVC mode is not supported. Please use the new MSVC preprocessor."
#else
#define Assert(...) HYP_CONCAT(HYP_ASSERT, HYP_HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)
#endif

#ifdef HYP_DEBUG_MODE
#define AssertDebug(...) Assert(__VA_ARGS__)
#else
#define AssertDebug(...) (void)(__VA_ARGS__)
#endif

#ifdef HYP_DEBUG_MODE
// Assert used for internal Hyperion libraries. Uses a simple printf-style format string, rather than the internal Hyperion formatting library.
// Opt to use this macro over AssertDebug() and Assert() to not pollute dependency on including logging headers.
// These assertions are stripped from released builds.
#define HYP_CORE_ASSERT(cond, ...)                                                                                                                             \
    do                                                                                                                                                         \
    {                                                                                                                                                          \
        if (HYP_UNLIKELY(!(cond)))                                                                                                                             \
        {                                                                                                                                                      \
            std::snprintf(debug::GetErrorStringBuffer(), 4096, "Assertion failed in Hyperion core library!\n\tCondition: " #cond "\n\tMessage: " __VA_ARGS__); \
            debug::LogAssert(debug::GetErrorStringBuffer());                                                                                                   \
            HYP_PRINT_STACK_TRACE();                                                                                                                           \
            std::terminate();                                                                                                                                  \
        }                                                                                                                                                      \
    }                                                                                                                                                          \
    while (0)
#else
#define HYP_CORE_ASSERT(...) (void)(cond)
#endif

#ifdef HYP_DEBUG_MODE
#define HYP_PRINT_STACK_TRACE() \
    debug::LogStackTrace()
#else
#define HYP_PRINT_STACK_TRACE()
#endif

#define HYP_FAIL(...)                                                                                                  \
    do                                                                                                                 \
    {                                                                                                                  \
        HYP_PRINT_STACK_TRACE();                                                                                       \
        Assert(0, "\n\nAn engine crash has been triggered!\n" __VA_ARGS__);                                            \
        debug::DebugLog_FlushOutputStream();                                                                           \
                                                                                                                       \
        std::terminate();                                                                                              \
    }                                                                                                                  \
    while (0)

// Add to the body of virtual methods that should be overridden.
// Used to allow instances of the class to be created from the managed runtime for providing managed method implementations.
#define HYP_PURE_VIRTUAL() HYP_FAIL("Pure virtual function call: " HYP_STR(HYP_DEBUG_FUNC_SHORT) " is missing an implementation ")

#define AssertStatic(cond) static_assert((cond), "Static assertion failed: " #cond)
#define AssertStaticCond(useStaticAssert, cond)                   \
    if constexpr ((useStaticAssert))                              \
    {                                                             \
        static_assert((cond), "Static assertion failed: " #cond); \
    }                                                             \
    else                                                          \
    {                                                             \
        Assert(cond);                                             \
    }
#define AssertStaticMsg(cond, msg) static_assert((cond), "Static assertion failed: " #cond "\n\t" #msg "\n")
#define AssertStaticMsgCond(useStaticAssert, cond, msg)                            \
    if constexpr ((useStaticAssert))                                               \
    {                                                                              \
        static_assert((cond), "Static assertion failed: " #cond "\n\t" #msg "\n"); \
    }                                                                              \
    else                                                                           \
    {                                                                              \
        Assert(cond, msg);                                                         \
    }
