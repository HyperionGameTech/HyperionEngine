//
// Created by emd22 on 2022-02-14.
//

#ifndef HYPERION_DEBUG_H
#define HYPERION_DEBUG_H

#include <ostream>
#include <string>
#include <iostream>


#ifndef HYPERION_BUILD_RELEASE

#if defined(__GNUC__) || defined(__GNUG__) || defined(__clang__)
#define HYP_DEBUG_FUNC_SHORT (__FUNCTION__)
#define HYP_DEBUG_FUNC       (__PRETTY_FUNCTION__)
#define HYP_DEBUG_LINE       (__LINE__)

#elif defined(_MSC_VER)
#define HYP_DEBUG_FUNC_SHORT (__FUNCTION__)
#define HYP_DEBUG_FUNC       (__FUNCSIG__)
#define HYP_DEBUG_LINE       (__LINE__)

#else
#define HYP_DEBUG_FUNC_SHORT ""
#define HYP_DEBUG_FUNC ""
#define HYP_DEBUG_LINE (0)

#endif
#endif /* HYPERION_BUILD_RELEASE */

enum class LogType : int {
    Info,
    Warn,
    Error,
    Fatal,
    Debug,
};

void DebugSetOutputStream(FILE *stream);

/* To keep final executable size down and avoid extra calculations
 * the release build will have function naming disabled. */

#ifdef HYPERION_BUILD_RELEASE
#define DebugLog(type, ...) \
    DebugLog_(type, __VA_ARGS__)

void DebugLog_(LogType type, const char *fmt, ...);
#else
//#define DebugLog(type, fmt) DebugLog(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, fmt)
#define DebugLog(type, ...) \
    DebugLog_(type, HYP_DEBUG_FUNC_SHORT, HYP_DEBUG_LINE, __VA_ARGS__);

void DebugLog_(LogType type, const char *callee, uint32_t line, const char *fmt, ...);
#endif

#define _DebugLogAssertion(level, cond) \
    { \
        const char *s = "*** assertion failed ***\n\t" #cond " evaluated to FALSE in file " __FILE__; \
        DebugLog(level, "%s", s); \
    }

#define _DebugLogAssertionMsg(level, cond, msg, ...) \
    { \
        const char *s = "*** assertion failed: " #msg " ***\n\t" #cond " evaluated to FALSE in file " __FILE__; \
        DebugLog(level, "%s\nMessage was: %s", s, msg, __VA_ARGS__); \
    }

#define AssertOrElse(level, cond, stmt) \
    { \
        if (!(cond)) { \
            _DebugLogAssertion(level, cond); \
            ##stmt; \
        } \
    }

#define AssertOrElseMsg(level, cond, stmt, msg, ...) \
    { \
        if (!(cond)) { \
            _DebugLogAssertionMsg(level, cond, msg, __VA_ARGS__); \
            ##stmt; \
        } \
    }

#define AssertThrow(cond) AssertOrElse(LogType::Error, cond, throw std::runtime_error("Assertion failed"))
#define AssertThrowMsg(cond, msg, ...) AssertOrElseMsg(LogType::Error, cond, throw std::runtime_error("Assertion failed"), msg, __VA_ARGS__)
#define AssertSoft(cond) AssertOrElse(LogType::Warn, cond, return);
#define AssertSoftMsg(cond, msg, ...) AssertOrElseMsg(LogType::Warn, cond, return, msg, __VA_ARGS__);
#define AssertReturn(cond, value) AssertOrElse(LogType::Warn, cond, return value);
#define AssertReturnMsg(cond, value, msg, ...) AssertOrElseMsg(LogType::Warn, cond, return value, msg, __VA_ARGS__);
#define AssertBreak(cond) AssertOrElse(LogType::Warn, cond, break);
#define AssertBreakMsg(cond, msg, ...) AssertOrElseMsg(LogType::Warn, cond, break, msg, __VA_ARGS__);
#define AssertContinue(cond) AssertOrElse(LogType::Warn, cond, continue);
#define AssertContinueMsg(cond, msg, ...) AssertOrElseMsg(LogType::Warn, cond, continue, msg, __VA_ARGS__);
#define AssertExit(cond) AssertOrElse(LogType::Fatal, cond, exit(1));
#define AssertExitMsg(cond, msg, ...) AssertOrElseMsg(LogType::Fatal, cond, exit(1), msg, __VA_ARGS__)
#define RagequitUnless(cond) AssertExit(cond)
#define RagequitUnlessMsg(cond, msg, ...) AssertExitMsg(cond, msg, __VA_ARGS__)

#define not_implemented AssertExitMsg(0, "Not implemented")
#define function_body_not_implemented { not_implemented; }
#define unexpected_value(value) AssertExitMsg(0, #value ": unexpected value")
#define unexpected_value_msg(value, msg) AssertExitMsg(0, #value ": " #msg)

#endif //HYPERION_DEBUG_H
