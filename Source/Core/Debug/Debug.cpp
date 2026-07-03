/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <Core/Debug/Debug.hpp>

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <cstdio>
#include <cstdarg>
#include <type_traits>

#if HYP_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <debugapi.h>
#elif HYP_APPLE
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#elif HYP_LINUX
#include <sys/types.h>
#include <unistd.h>
#endif

#define HYP_DEBUG_OUTPUT_STREAM stdout

namespace Hyperion {
namespace debug {

char* GetErrorStringBuffer()
{
    static char errorStringBuf[4096];

    return &errorStringBuf[0];
}

static const char* s_logTypeTable[] = {
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
static const char* s_logColorTable[] = {
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
HYP_DEPRECATED void DebugLog_Write(LogType type, const char* fmt, ...)
{
    /* Coloured files are less that ideal */
    const int typeN = static_cast<std::underlying_type_t<LogType>>(type);
    fprintf(HYP_DEBUG_OUTPUT_STREAM, "[%s] ", s_logTypeTable[typeN]);

    va_list args;
    va_start(args, fmt);
    vfprintf(HYP_DEBUG_OUTPUT_STREAM, fmt, args);
    va_end(args);
}
#else
HYP_DEPRECATED void DebugLog_Write(LogType type, const char* callee, uint32_t line, const char* fmt, ...)
{
    const int typeN = static_cast<std::underlying_type_t<LogType>>(type);
    /* Coloured files are less than ideal */
    if (HYP_DEBUG_OUTPUT_STREAM == stdout)
        printf("%s[%s]\33[0m ", s_logColorTable[typeN], s_logTypeTable[typeN]);
    else
        fprintf(HYP_DEBUG_OUTPUT_STREAM, "[%s] ", s_logTypeTable[typeN]);

    if (callee != nullptr)
        fprintf(HYP_DEBUG_OUTPUT_STREAM, "%s(line:%u): ", callee, line);

    va_list args;
    va_start(args, fmt);
    vfprintf(HYP_DEBUG_OUTPUT_STREAM, fmt, args);
    va_end(args);
}
#endif

CORE_API void DebugLog_FlushOutputStream()
{
    fputs("\n\n", HYP_DEBUG_OUTPUT_STREAM);
    fflush(HYP_DEBUG_OUTPUT_STREAM);
}

CORE_API void WriteToStandardError(const char* msg)
{
    fputs(msg, stderr);
    fflush(stderr);
}

CORE_API bool IsDebuggerAttached()
{
#if HYP_WINDOWS
    return ::IsDebuggerPresent();
#elif HYP_APPLE
    int mib[4] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    struct kinfo_proc info {};
    size_t size = sizeof(info);

    if (sysctl(mib, 4, &info, &size, nullptr, 0) != 0)
    {
        return false;
    }

    // P_TRACED flag is set when a debugger is tracing the process.
    return (info.kp_proc.p_flag & P_TRACED) != 0;
#else
    // @TODO: Implement for HYP_LINUX
    return false;
#endif
}

void LogAssert(const char* str)
{
    //if (HYP_UNLIKELY(!Logger::GetInstance())) // logger system not yet initialized
    //{
        std::fprintf(HYP_DEBUG_OUTPUT_STREAM, "%s\n", str);
        std::fflush(HYP_DEBUG_OUTPUT_STREAM);

    //    if (IsDebuggerAttached())
    //    {
    //        HYP_BREAKPOINT;

    //        return;
    //    }

    //    TerminateProgram();

    //    return;
    //}

#if HYP_DEBUG_MODE
    HYP_LOG_DYNAMIC(Core, Error, str);
    std::fflush(HYP_DEBUG_OUTPUT_STREAM);

    if (IsDebuggerAttached())
    {
        HYP_BREAKPOINT;
    }
    // allow continuation
    return;
#endif

    HYP_LOG_DYNAMIC(Core, Fatal, str);
}

void TerminateProgram()
{
    std::terminate();
}

} // namespace debug
} // namespace Hyperion
