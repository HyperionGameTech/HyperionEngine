/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/StaticString.hpp>

#include <core/Types.hpp>

#define HYP_DECLARE_LOG_CHANNEL(name) \
    extern Hyperion::logging::LogChannel g_logChannel_##name

namespace Hyperion {
namespace logging {

class Logger;
class LogChannel;
enum class LogLevel : uint8;

#define DECLARE_LOG_LEVEL_GLOBAL(name) \
    extern constexpr LogLevel name();

DECLARE_LOG_LEVEL_GLOBAL(Fatal);
DECLARE_LOG_LEVEL_GLOBAL(Error);
DECLARE_LOG_LEVEL_GLOBAL(Warning);
DECLARE_LOG_LEVEL_GLOBAL(Info);
DECLARE_LOG_LEVEL_GLOBAL(Verbose);
DECLARE_LOG_LEVEL_GLOBAL(Debug);

#undef DECLARE_LOG_LEVEL_GLOBAL

template <LogLevel Level, auto ChannelArg, auto FormatString, auto FileName, int LineNumber, class... Args>
inline void LogStatic(Logger& logger, Args&&... args);

template <LogLevel Level, auto FormatString, auto FileName, int LineNumber, class... Args>
inline void LogStatic_Channel(Logger& logger, const LogChannel& channel, Args&&... args);

template <LogLevel Level>
inline void LogDynamic(Logger& logger, const LogChannel& channel, const char* fileName, int lineNumber, const char* str);

#ifdef HYP_DEBUG_MODE
HYP_API extern void LogTemp(Logger& logger, const char* str, const char* fileName, int lineNumber);
#endif

extern Logger& GetLogger();

} // namespace logging

using logging::LogChannel;
using logging::LogLevel;

HYP_DECLARE_LOG_CHANNEL(Core);

} // namespace Hyperion

#ifdef HYP_LOG
#error "HYP_LOG already defined!"
#endif

#define HYP_LOG(channel, level, fmt, ...) \
    Hyperion::logging::LogStatic<Hyperion::logging::LogLevel::level, HYP_MAKE_CONST_ARG(&g_logChannel_##channel), HYP_STATIC_STRING(fmt "\n"), HYP_STATIC_STRING(__FILE__), (__LINE__)>(Hyperion::logging::GetLogger(), ##__VA_ARGS__)

#define HYP_LOG_DYNAMIC(channel, level, str) \
    Hyperion::logging::LogDynamic<Hyperion::logging::LogLevel::level>(Hyperion::logging::GetLogger(), g_logChannel_##channel, HYP_STATIC_STRING(__FILE__).Data(), (__LINE__), str)

#ifdef HYP_DEBUG_MODE
#define HYP_LOG_TEMP(fmt, ...) \
    Hyperion::logging::LogTemp(Hyperion::logging::GetLogger(), &(HYP_FORMAT(fmt, ##__VA_ARGS__))[0], HYP_STATIC_STRING(__FILE__).Data(), (__LINE__))
#else
#define HYP_LOG_TEMP(fmt, ...)
#endif

#ifndef HYP_LOG_ONCE
#define HYP_LOG_ONCE(channel, category, fmt, ...)
#endif
