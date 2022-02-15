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

#endif //HYPERION_DEBUG_H
