/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Defines.hpp>

#include <core/containers/StaticString.hpp>

#include <core/Types.hpp>

#define HYP_DECLARE_LOG_CHANNEL(name) \
    extern hyperion::logging::LogChannel g_logChannel_##name

namespace hyperion {
namespace logging {

class Logger;
class LogChannel;

HYP_ENUM()
enum LogLevel : uint32
{
    DEBUG = 0,
    INFO,
    WARNING,
    ERR,
    FATAL,

    MAX
};

HYP_STRUCT()
struct LogCategory
{
    HYP_STRUCT_BODY(LogCategory);

    enum LogCategoryFlags : uint8
    {
        LCF_NONE = 0x0,
        LCF_ENABLED = 0x1,
#ifdef HYP_DEBUG_MODE
        LCF_ENABLED_IF_DEBUG_MODE = LCF_ENABLED,
#else
        LCF_ENABLED_IF_DEBUG_MODE = LCF_NONE,
#endif
        LCF_FATAL = 0x4,

        LCF_DEFAULT = LCF_ENABLED
    };

    constexpr LogCategory(LogLevel level, uint16 priority, uint8 flags = LCF_DEFAULT)
        : value(uint32(flags) | (uint32(priority) << 8) | (uint32(level) << 24))
    {
    }

    constexpr LogCategory(const LogCategory& other)
        : value(other.value)
    {
    }

    LogCategory& operator=(const LogCategory& other)
    {
        value = other.value;

        return *this;
    }

    HYP_FORCE_INLINE constexpr bool operator==(const LogCategory& other) const
    {
        return value == other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator!=(const LogCategory& other) const
    {
        return value != other.value;
    }

    HYP_FORCE_INLINE constexpr bool operator<(const LogCategory& other) const
    {
        return GetPriority() < other.GetPriority();
    }

    HYP_FORCE_INLINE constexpr uint8 GetFlags() const
    {
        return value & 0xFF;
    }

    HYP_FORCE_INLINE constexpr uint16 GetPriority() const
    {
        return uint16((value >> 8) & 0xFFFF);
    }

    HYP_FORCE_INLINE constexpr LogLevel GetLevel() const
    {
        return LogLevel((value >> 24) & 0xFF);
    }

    HYP_FORCE_INLINE constexpr bool IsEnabled() const
    {
        return (GetFlags() & LCF_ENABLED) != 0;
    }

    HYP_FIELD()
    static const LogCategory Debug;

    HYP_FIELD()
    static const LogCategory Warning;

    HYP_FIELD()
    static const LogCategory Info;

    HYP_FIELD()
    static const LogCategory Error;

    HYP_FIELD()
    static const LogCategory Fatal;

    uint32 value;
};

#define DEFINE_LOG_CATEGORY_GLOBAL(name)    \
    static inline const LogCategory& name() \
    {                                       \
        return LogCategory::name;           \
    }

DEFINE_LOG_CATEGORY_GLOBAL(Debug);
DEFINE_LOG_CATEGORY_GLOBAL(Warning);
DEFINE_LOG_CATEGORY_GLOBAL(Info);
DEFINE_LOG_CATEGORY_GLOBAL(Error);
DEFINE_LOG_CATEGORY_GLOBAL(Fatal);

#undef DEFINE_LOG_CATEGORY_GLOBAL

template <auto CategoryArg, auto ChannelArg, auto FormatString, class... Args>
inline void LogStatic(Logger& logger, Args&&... args);

template <auto Category, auto FormatString, class... Args>
inline void LogStatic_Channel(Logger& logger, const LogChannel& channel, Args&&... args);

template <auto CategoryArg, auto ChannelArg>
inline void LogDynamic(Logger& logger, const char* str);

HYP_API extern void LogTemp(Logger& logger, const char* str);

HYP_API extern Logger& GetLogger();

} // namespace logging

using logging::LogChannel;

HYP_DECLARE_LOG_CHANNEL(Core);

} // namespace hyperion

#ifdef HYP_LOG
#error "HYP_LOG already defined!"
#endif

#define HYP_LOG(channel, category, fmt, ...) \
    hyperion::logging::LogStatic<HYP_MAKE_CONST_ARG(&LogCategory::category), HYP_MAKE_CONST_ARG(&g_logChannel_##channel), HYP_STATIC_STRING(fmt "\n")>(hyperion::logging::GetLogger(), ##__VA_ARGS__)

#define HYP_LOG_DYNAMIC(channel, category, str) \
    hyperion::logging::LogDynamic<HYP_MAKE_CONST_ARG(&LogCategory::category), HYP_MAKE_CONST_ARG(&g_logChannel_##channel)>(hyperion::logging::GetLogger(), str)

#ifdef HYP_DEBUG_MODE
#define HYP_LOG_TEMP(fmt, ...) \
    hyperion::logging::LogTemp(hyperion::logging::GetLogger(), &(HYP_FORMAT(fmt "\n", ##__VA_ARGS__))[0])
#else
#define HYP_LOG_TEMP(fmt, ...)
#endif

#ifndef HYP_LOG_ONCE
#define HYP_LOG_ONCE(channel, category, fmt, ...)
#endif
