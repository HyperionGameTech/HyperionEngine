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
    //#define DebugLog(type, fmt) DebugLog(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, fmt)
    #define DebugLog(type, ...) \
        debug::DebugLog_(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, __VA_ARGS__)

    extern HYP_API void DebugLog_(LogType type, const char *callee, uint32_t line, const char *fmt, ...);
#else
    #define DebugLog(type, ...) \
        debug::DebugLog_(type, __VA_ARGS__)

    extern HYP_API void DebugLog_(LogType type, const char *fmt, ...);
#endif

extern HYP_API void LogStackTrace(int depth = 10);

} // namespace debug

using debug::LogType;

} // namespace hyperion

#if defined(HYP_USE_EXCEPTIONS) && HYP_USE_EXCEPTIONS
    #define HYP_THROW(msg) throw ::std::runtime_error(msg)
#else
    #ifdef HYP_DEBUG_MODE
        #define HYP_THROW(msg) \
            do { \
                HYP_PRINT_STACK_TRACE(); \
                std::terminate(); \
            } while (0)
    #else
        #define HYP_THROW(msg) std::terminate()
    #endif
#endif

#define HYP_UNREACHABLE() \
    HYP_THROW("Unreachable code hit in function " HYP_DEBUG_FUNC)

#if defined(HYP_USE_EXCEPTIONS) && HYP_USE_EXCEPTIONS
    #define HYP_NOT_IMPLEMENTED() do { HYP_THROW("Function not implemented: " HYP_DEBUG_FUNC); std::terminate(); } while (0)
#else
    #define HYP_NOT_IMPLEMENTED() HYP_THROW("Function not implemented: " HYP_DEBUG_FUNC)
#endif

// obsolete
#define HYP_NOT_IMPLEMENTED_VOID() HYP_NOT_IMPLEMENTED()

#define DebugLogRaw(type, ...) \
    debug::DebugLog_(type, nullptr, 0, __VA_ARGS__)

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

#define AssertThrow(cond) AssertOrElse(LogType::Error, cond, HYP_THROW("Assertion failed"))

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

#ifdef HYP_DEBUG_MODE
    #define HYP_PRINT_STACK_TRACE() \
        debug::LogStackTrace()
#else
    #define HYP_PRINT_STACK_TRACE()
#endif

#define HYP_FAIL(msg, ...)                          AssertOrElseMsg(LogType::Error, false, HYP_THROW("Fatal error"), msg, ##__VA_ARGS__)

// Add to the body of virtual methods that should be overridden.
// Used to allow instances of the class to be created from the managed runtime for providing managed method implementations.
#define HYP_PURE_VIRTUAL()                          HYP_FAIL("Pure virtual function call: " HYP_STR(HYP_DEBUG_FUNC_SHORT) " is missing an implementation ")

#define AssertStatic(cond) static_assert((cond), "Static assertion failed: " #cond)
#define AssertStaticCond(use_static_assert, cond) \
    if constexpr ((use_static_assert)) { \
        static_assert((cond), "Static assertion failed: " #cond); \
    } else { \
        AssertThrow(cond); \
    }
#define AssertStaticMsg(cond, msg) static_assert((cond), "Static assertion failed: " #cond "\n\t" #msg "\n")
#define AssertStaticMsgCond(use_static_assert, cond, msg) \
    if constexpr ((use_static_assert)) { \
        static_assert((cond), "Static assertion failed: " #cond "\n\t" #msg "\n"); \
    } else { \
        AssertThrowMsg(cond, msg); \
    }

#endif //HYPERION_DEBUG_HPP
