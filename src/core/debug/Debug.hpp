/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEBUG_HPP
#define HYPERION_DEBUG_HPP

#include <core/Defines.hpp>

#include <csignal>

namespace hyperion {
namespace debug {

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

#define HYP_UNREACHABLE() \
    HYP_THROW("Unreachable code hit in function " HYP_STR(HYP_DEBUG_FUNC))

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

#define DebugLogRaw(type, ...) \
    debug::DebugLog_Write(type, nullptr, 0, __VA_ARGS__)

#define HYP_HAS_ARGS(...) HYP_HAS_ARGS_(__VA_ARGS__, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, )
#define HYP_HAS_ARGS_(a1, a2, a3, a4, a5, b1, b2, b3, b4, b5, c1, c2, c3, c4, c5, d1, d2, d3, d4, d5, e, N, ...) N

#define AssertThrow(...) HYP_CONCAT(HYP_ASSERT, HYP_HAS_ARGS(__VA_ARGS__))(__VA_ARGS__)

#define HYP_ASSERT0(expr) HYP_ASSERT1(expr, )

#define HYP_ASSERT1(expression, ...)                                                                              \
    do                                                                                                            \
    {                                                                                                             \
        if (HYP_UNLIKELY(!(expression)))                                                                          \
        {                                                                                                         \
            DebugLog(LogType::Error, "Assertion failed!\n\tCondition: " #expression "\n\tMessage: " __VA_ARGS__); \
            debug::DebugLog_FlushOutputStream();                                                                  \
            HYP_PRINT_STACK_TRACE();                                                                              \
            std::terminate();                                                                                     \
        }                                                                                                         \
    }                                                                                                             \
    while (0)

#ifdef HYP_DEBUG_MODE
#define AssertDebug(...) AssertThrow(__VA_ARGS__)
#else
#define AssertDebug(...)
#endif

#define AssertThrowMsg(...) AssertThrow(__VA_ARGS__)

#ifdef HYP_DEBUG_MODE
#define AssertDebugMsg(...) AssertThrowMsg(__VA_ARGS__)
#else
#define AssertDebugMsg(...)
#endif

#ifdef HYP_DEBUG_MODE
#define HYP_PRINT_STACK_TRACE() \
    debug::LogStackTrace()
#else
#define HYP_PRINT_STACK_TRACE()
#endif

#define HYP_FAIL(...) AssertThrow(false, ##__VA_ARGS__)

// Add to the body of virtual methods that should be overridden.
// Used to allow instances of the class to be created from the managed runtime for providing managed method implementations.
#define HYP_PURE_VIRTUAL() HYP_FAIL("Pure virtual function call: " HYP_STR(HYP_DEBUG_FUNC_SHORT) " is missing an implementation ")

#define AssertStatic(cond) static_assert((cond), "Static assertion failed: " #cond)
#define AssertStaticCond(use_static_assert, cond)                 \
    if constexpr ((use_static_assert))                            \
    {                                                             \
        static_assert((cond), "Static assertion failed: " #cond); \
    }                                                             \
    else                                                          \
    {                                                             \
        AssertThrow(cond);                                        \
    }
#define AssertStaticMsg(cond, msg) static_assert((cond), "Static assertion failed: " #cond "\n\t" #msg "\n")
#define AssertStaticMsgCond(use_static_assert, cond, msg)                          \
    if constexpr ((use_static_assert))                                             \
    {                                                                              \
        static_assert((cond), "Static assertion failed: " #cond "\n\t" #msg "\n"); \
    }                                                                              \
    else                                                                           \
    {                                                                              \
        AssertThrowMsg(cond, msg);                                                 \
    }

#endif // HYPERION_DEBUG_HPP
