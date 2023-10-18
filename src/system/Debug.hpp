//
// Created by emd22 on 2022-02-14.
//

#ifndef HYPERION_DEBUG_H
#define HYPERION_DEBUG_H

#include <util/Defines.hpp>

#include <ostream>
#include <string>
#include <iostream>

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

#ifdef HYPERION_BUILD_RELEASE
#define DebugLog(type, ...) \
    DebugLog_(type, __VA_ARGS__)

void DebugLog_(LogType type, const char *fmt, ...);
#else
//#define DebugLog(type, fmt) DebugLog(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, fmt)
#define DebugLog(type, ...) \
    DebugLog_(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, __VA_ARGS__)

void DebugLog_(LogType type, const char *callee, uint32_t line, const char *fmt, ...);
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


#define AssertThrow(cond)                      AssertOrElse(LogType::Error, cond, HYP_THROW("Assertion failed"))
#define AssertSoft(cond)                       AssertOrElse(LogType::Warn, cond, (void)0)
#define AssertReturn(cond, value)              AssertOrElse(LogType::Warn, cond, return (value))
#define AssertBreak(cond)                      AssertOrElse(LogType::Warn, cond, break)
#define AssertContinue(cond)                   AssertOrElse(LogType::Warn, cond, continue)
#define AssertExit(cond)                       AssertOrElse(LogType::Fatal, cond, exit(1))

#if defined(HYP_MSVC) && HYP_MSVC

#define DebugLogAssertionMsg(level, cond, msg, ...) \
    do { \
        DebugLog(level, "*** assertion failed: (" #cond ") ***\n\t" msg "\n", __VA_ARGS__); \
    } while (0)

#define AssertOrElseMsg(level, cond, stmt, msg, ...) \
    do { \
        if (!(cond)) { \
            DebugLogAssertionMsg(level, cond, msg, __VA_ARGS__); \
            { stmt; } \
        } \
    } while (0)

#define AssertThrowMsg(cond, msg, ...)         AssertOrElseMsg(LogType::Error, cond, HYP_THROW("Assertion failed"), msg, __VA_ARGS__)
#define AssertSoftMsg(cond, msg, ...)          AssertOrElseMsg(LogType::Warn, cond, (void)0, msg, __VA_ARGS__)
#define AssertReturnMsg(cond, value, msg, ...) AssertOrElseMsg(LogType::Warn, cond, return value, msg, __VA_ARGS__)
#define AssertBreakMsg(cond, msg, ...)         AssertOrElseMsg(LogType::Warn, cond, break, msg, __VA_ARGS__)
#define AssertContinueMsg(cond, msg, ...)      AssertOrElseMsg(LogType::Warn, cond, continue, msg, __VA_ARGS__)
#define AssertExitMsg(cond, msg, ...)          AssertOrElseMsg(LogType::Fatal, cond, exit(1), msg, __VA_ARGS__)
#else

#define DebugLogAssertionMsg(level, cond, msg, ...) \
    do { \
        DebugLog(level, "*** assertion failed: (" #cond ") ***\n\t" #msg "\n" __VA_OPT__(,) __VA_ARGS__); \
    } while (0)

#define AssertOrElseMsg(level, cond, stmt, msg, ...) \
    do { \
        if (!(cond)) { \
            DebugLogAssertionMsg(level, cond, msg __VA_OPT__(,) __VA_ARGS__); \
            { stmt; } \
        } \
    } while (0)

#define AssertThrowMsg(cond, msg, ...)              AssertOrElseMsg(LogType::Error, cond, HYP_THROW("Assertion failed"), msg __VA_OPT__(,) __VA_ARGS__)
#define AssertSoftMsg(cond, msg, ...)               AssertOrElseMsg(LogType::Warn, cond, (void)0, msg __VA_OPT__(,) __VA_ARGS__)
#define AssertReturnMsg(cond, value, msg, ...)      AssertOrElseMsg(LogType::Warn, cond, return value, msg __VA_OPT__(,) __VA_ARGS__)
#define AssertBreakMsg(cond, msg, ...)              AssertOrElseMsg(LogType::Warn, cond, break, msg __VA_OPT__(,) __VA_ARGS__)
#define AssertContinueMsg(cond, msg, ...)           AssertOrElseMsg(LogType::Warn, cond, continue, msg __VA_OPT__(,) __VA_ARGS__)
#define AssertExitMsg(cond, msg, ...)               AssertOrElseMsg(LogType::Fatal, cond, exit(1), msg __VA_OPT__(,) __VA_ARGS__)
#endif

/* Deprecated */
#define unexpected_value_msg(value, msg) AssertExitMsg(0, "%s", #value ": " #msg)

#endif //HYPERION_DEBUG_H
