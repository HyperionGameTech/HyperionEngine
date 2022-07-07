//
// Created by emd22 on 2022-02-14.
//

#include "Debug.hpp"
#include <cstdio>
#include <cstdarg>

/* Using a FILE* is definitely less than ideal,
 * But it's the easiest way to pass in a file and get
 * variadic parameters in without a lot of extra work. */
static FILE *debug_output_stream = stdout;
static const char *log_type_table[] = {
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
static const char *log_colour_table[] = {
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

void DebugSetOutputStream(FILE *stream) {
    debug_output_stream = stream;
}

#ifdef HYPERION_BUILD_RELEASE
void DebugLog_(LogType type, const char *fmt, ...) {
    /* Coloured files are less that ideal */
    const int type_n = static_cast<std::underlying_type<LogType>::type>(type);
    fprintf(debug_output_stream, "[%s] ", log_type_table[type_n]);

    va_list args;
    va_start(args, fmt);
    vfprintf(debug_output_stream, fmt, args);
    va_end(args);
}
#else
void DebugLog_(LogType type, const char *callee, uint32_t line, const char *fmt, ...) {
    const int type_n = static_cast<std::underlying_type<LogType>::type>(type);
    /* Coloured files are less than ideal */
    if (debug_output_stream == stdout)
        printf("%s[%s]\33[0m ", log_colour_table[type_n], log_type_table[type_n]);
    else
        fprintf(debug_output_stream, "[%s] ", log_type_table[type_n]);

    if (callee != nullptr)
        fprintf(debug_output_stream, "%s(line:%u): ", callee, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(debug_output_stream, fmt, args);
    va_end(args);
}

#endif
