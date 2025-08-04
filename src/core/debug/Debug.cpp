/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/debug/Debug.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <cstdio>
#include <cstdarg>
#include <type_traits>

#if defined(HYP_UNIX)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#elif defined(HYP_WINDOWS)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <debugapi.h>
#endif

#define HYP_DEBUG_OUTPUT_STREAM stdout

namespace hyperion {
namespace debug {

HYP_API char* GetErrorStringBuffer()
{
    static char errorStringBuf[4096];

    return &errorStringBuf[0];
}

static const char* g_logTypeTable[] = {
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
static const char* g_logColourTable[] = {
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
HYP_DEPRECATED HYP_API void DebugLog_Write(LogType type, const char* fmt, ...)
{
    /* Coloured files are less that ideal */
    const int typeN = static_cast<std::underlying_type_t<LogType>>(type);
    fprintf(HYP_DEBUG_OUTPUT_STREAM, "[%s] ", g_logTypeTable[typeN]);

    va_list args;
    va_start(args, fmt);
    vfprintf(HYP_DEBUG_OUTPUT_STREAM, fmt, args);
    va_end(args);
}
#else
HYP_DEPRECATED HYP_API void DebugLog_Write(LogType type, const char* callee, uint32_t line, const char* fmt, ...)
{
    const int typeN = static_cast<std::underlying_type_t<LogType>>(type);
    /* Coloured files are less than ideal */
    if (HYP_DEBUG_OUTPUT_STREAM == stdout)
        printf("%s[%s]\33[0m ", g_logColourTable[typeN], g_logTypeTable[typeN]);
    else
        fprintf(HYP_DEBUG_OUTPUT_STREAM, "[%s] ", g_logTypeTable[typeN]);

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
    fputs("\n\n", HYP_DEBUG_OUTPUT_STREAM);
    fflush(HYP_DEBUG_OUTPUT_STREAM);
}

HYP_API void WriteToStandardError(const char* msg)
{
    fputs(msg, stderr);
    fflush(stderr);
}

HYP_API bool IsDebuggerAttached()
{
#ifdef HYP_WINDOWS
    return ::IsDebuggerPresent();
#elif defined(HYP_UNIX)
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    struct kinfo_proc info {};
    size_t size = sizeof(info);

    if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0)
    {
        return false;
    }

    // P_TRACED flag is set when a debugger is tracing the process.
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#endif
}

HYP_API void LogAssert(const char* str)
{
#ifdef HYP_DEBUG_MODE
    HYP_LOG_DYNAMIC(Core, Error, str);
    HYP_BREAKPOINT;
#else
    HYP_LOG_DYNAMIC(Core, Fatal, str);
#endif
}

} // namespace debug
} // namespace hyperion
