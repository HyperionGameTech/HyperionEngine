/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEBUG_HPP
#define HYPERION_DEBUG_HPP

#include <core/Defines.hpp>

#include <ostream>
#include <string>

#include <csignal>

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

/* To keep final executable size down and avoid extra calculations
 * the release build will have function naming disabled. */

#define DebugLogRaw(type, ...) \
    DebugLog_(type, nullptr, 0, __VA_ARGS__)

#ifdef HYP_DEBUG_MODE
    //#define DebugLog(type, fmt) DebugLog(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, fmt)
    #define DebugLog(type, ...) \
        DebugLog_(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, __VA_ARGS__)

    extern HYP_API void DebugLog_(LogType type, const char *callee, uint32_t line, const char *fmt, ...);
#else
    #define DebugLog(type, ...) \
        DebugLog_(type, __VA_ARGS__)

    extern HYP_API void DebugLog_(LogType type, const char *fmt, ...);
#endif

#define DebugLogAssertion(level, cond) \
    do { \
        const char *s = "*** assertion failed: (" #cond ") ***"; \
        DebugLog(level, "%s", s); \
        fflush(stdout); \
    } while (0)

#define AssertOrElse(level, cond, stmt) \
    do { \
        if (!(cond)) { \
            DebugLogAssertion(level, cond); \
            { stmt; } \
        } \
    } while (0)


#define AssertThrow(cond)                       AssertOrElse(LogType::Error, cond, HYP_THROW("Assertion failed"))

#ifdef HYP_DEBUG_MODE
    #define AssertDebug(cond)                   AssertThrow(cond)
#else
    #define AssertDebug(...)
#endif

#define DebugLogAssertionMsg(level, cond, msg, ...) \
    do { \
        DebugLog(level, "*** assertion failed: (" #cond ") ***\n\t" #msg "\n", ##__VA_ARGS__); \
    } while (0)

#define AssertOrElseMsg(level, cond, stmt, msg, ...) \
    do { \
        if (!(cond)) { \
            DebugLogAssertionMsg(level, cond, msg, ## __VA_ARGS__); \
            { stmt; } \
        } \
    } while (0)


#define AssertThrowMsg(cond, msg, ...)              AssertOrElseMsg(LogType::Error, cond, HYP_THROW("Assertion failed"), msg, ##__VA_ARGS__)
    
#ifdef HYP_DEBUG_MODE
    #define AssertDebugMsg(cond, msg, ...)          AssertThrowMsg(cond, msg, ##__VA_ARGS__)
#else
    #define AssertDebugMsg(...)
#endif

#define HYP_FAIL(msg, ...)                          AssertOrElseMsg(LogType::Error, false, HYP_THROW("Fatal error"), msg, ##__VA_ARGS__)

#ifdef HYP_DEBUG_MODE

template <class T>
struct EnsureValidPointerWrapper
{
    T *ptr;

    EnsureValidPointerWrapper(T *ptr)
        : ptr(ptr)
    {
    }

    HYP_FORCE_INLINE operator T*() const
    {
        AssertThrowMsg(ptr != nullptr, "EnsureValidPointer check failed");

        return ptr;
    }

    HYP_FORCE_INLINE T *operator->() const
    {
        AssertThrowMsg(ptr != nullptr, "EnsureValidPointer check failed");

        return ptr;
    }

    HYP_FORCE_INLINE T &operator*() const
    {
        AssertThrowMsg(ptr != nullptr, "EnsureValidPointer check failed");

        return *ptr;
    }
};

#define EnsureValidPointer(ptr) (EnsureValidPointerWrapper { ptr })

#else

#define EnsureValidPointer(ptr) ptr

#endif

#endif //HYPERION_DEBUG_HPP
