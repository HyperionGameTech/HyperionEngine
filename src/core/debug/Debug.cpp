/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/debug/Debug.hpp>

#include <cstdio>
#include <cstdarg>
#include <type_traits>

#define HYP_DEBUG_OUTPUT_STREAM stdout

namespace hyperion {
namespace debug {

static const char *g_log_type_table[] = {
        "INFO",
        "WARN",
        "ERROR",
        "FATAL",
        "DEBUG",

        "VKINFO",
        "VKWARN",
        "VKERROR",
        "VKDEBUG",
        nullptr
};

/* Colours increase happiness by 200% */
static const char *g_log_colour_table[] = {
        "\33[34m",
        "\33[33m",
        "\33[31m",
        "\33[31;4m",
        "\33[32;4m",

        "\33[1;34m",
        "\33[1;33m",
        "\33[1;31m",
        "\33[1;32m",

        nullptr
};

#ifndef HYP_DEBUG_MODE
HYP_API void DebugLog_Write(LogType type, const char *fmt, ...) {
    /* Coloured files are less that ideal */
    const int type_n = static_cast<std::underlying_type<LogType>::type>(type);
    fprintf(HYP_DEBUG_OUTPUT_STREAM, "[%s] ", g_log_type_table[type_n]);

    va_list args;
    va_start(args, fmt);
    vfprintf(HYP_DEBUG_OUTPUT_STREAM, fmt, args);
    va_end(args);
}
#else
HYP_API void DebugLog_Write(LogType type, const char *callee, uint32_t line, const char *fmt, ...) {
    const int type_n = static_cast<std::underlying_type<LogType>::type>(type);
    /* Coloured files are less than ideal */
    if (HYP_DEBUG_OUTPUT_STREAM == stdout)
        printf("%s[%s]\33[0m ", g_log_colour_table[type_n], g_log_type_table[type_n]);
    else
        fprintf(HYP_DEBUG_OUTPUT_STREAM, "[%s] ", g_log_type_table[type_n]);

    if (callee != nullptr)
        fprintf(HYP_DEBUG_OUTPUT_STREAM, "%s(line:%u): ", callee, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(HYP_DEBUG_OUTPUT_STREAM, fmt, args);
    va_end(args);
}
#endif

HYP_API void DebugLog_FlushOutputStream()
{
    fflush(HYP_DEBUG_OUTPUT_STREAM);
}

HYP_API void WriteToStandardError(const char *msg)
{
    fputs(msg, stderr);
    fflush(stderr);
}

} // namespace debug
} // namespace hyperion