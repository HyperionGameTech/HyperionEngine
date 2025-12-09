/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/logging/LoggerFwd.hpp>

#include <core/Name.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Format.hpp>
#include <core/utilities/Time.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/reflection/Handle.hpp>
#include <core/reflection/ObjectFwd.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Bitset.hpp>

#include <core/threading/AtomicVar.hpp>

#include <climits>

namespace hyperion {

HYP_API extern ANSIStringView GetCurrentThreadName();

namespace logging {

struct LogMessage
{
    LogLevel level;
    uint64 timestamp;
    Span<StringView<StringType::UTF8>> chunks;
};

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

    constexpr LogCategory()
        : value(0)
    {
    }

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

inline constexpr LogCategory LogCategory::Debug = LogCategory(LogLevel::DEBUG, 10000, LogCategory::LCF_ENABLED_IF_DEBUG_MODE);
inline constexpr LogCategory LogCategory::Warning = LogCategory(LogLevel::WARNING, 1000);
inline constexpr LogCategory LogCategory::Info = LogCategory(LogLevel::INFO, 100);
inline constexpr LogCategory LogCategory::Error = LogCategory(LogLevel::ERR, 10);
inline constexpr LogCategory LogCategory::Fatal = LogCategory(LogLevel::FATAL, 1, LogCategory::LCF_ENABLED | LogCategory::LCF_FATAL);

template <LogLevel Level>
static constexpr auto LogLevelToString()
{
    if constexpr (Level == LogLevel::DEBUG)
    {
        return HYP_STATIC_STRING("Debug");
    }
    else if constexpr (Level == LogLevel::INFO)
    {
        return HYP_STATIC_STRING("Info");
    }
    else if constexpr (Level == LogLevel::WARNING)
    {
        return HYP_STATIC_STRING("Warning");
    }
    else if constexpr (Level == LogLevel::ERR)
    {
        return HYP_STATIC_STRING("Error");
    }
    else if constexpr (Level == LogLevel::FATAL)
    {
        return HYP_STATIC_STRING("Fatal");
    }
    else
    {
        return HYP_STATIC_STRING("?");
    }
}

template <LogLevel Level>
static constexpr const char* LogLevelTermColor()
{
    // constexpr const char* table[uint32(LogLevel::MAX)] = {
    //     ""          // DEBUG
    //     "",         // INFO
    //     "\33[33m",  // WARNING
    //     "\33[31m",  // ERR
    //     "\33[31;4m" // FATAL
    // };

    // return table[uint32(Level)];

    if constexpr (Level == LogLevel::DEBUG)
    {
        return "";
    }
    else if constexpr (Level == LogLevel::INFO)
    {
        return "";
    }
    else if constexpr (Level == LogLevel::WARNING)
    {
        return "\33[33m";
    }
    else if constexpr (Level == LogLevel::ERR)
    {
        return "\33[31m";
    }
    else if constexpr (Level == LogLevel::FATAL)
    {
        return "\33[31;4m";
    }
    else
    {
        return "?";
    }
}

HYP_STRUCT()
class HYP_API LogChannel
{
public:
    HYP_STRUCT_BODY(LogChannel);

    friend class Logger;
    friend class LogChannelRegistrar;

    HYP_FIELD()
    uint32 id;

    HYP_FIELD()
    Name name;

    HYP_FIELD()
    LogChannel* parentChannel;

    HYP_FIELD(NoScriptBindings)
    Bitset maskBitset;

    LogChannel()
        : id(~0u),
          name(),
          parentChannel(nullptr),
          maskBitset()
    {
    }

    explicit LogChannel(Name name, LogChannel* parentChannel = nullptr);

    LogChannel(const LogChannel& other) = default;
    LogChannel& operator=(const LogChannel& other) = default;

    LogChannel(LogChannel&& other) noexcept = default;
    LogChannel& operator=(LogChannel&& other) noexcept = default;

    ~LogChannel() = default;
};

/*! \brief Function pointer type for logger output stream write functions.
 *  \param context User-defined context pointer.
 *  \param channel The log channel the message is being written to.
 *  \param message The log message to write.
 *  \return True to allow default output processing, false to suppress it.
 */
typedef bool (*LoggerWriteFnPtr)(void* context, const LogChannel& channel, const LogMessage& message);

class ILoggerOutputStream
{
public:
    virtual ~ILoggerOutputStream() = default;

    virtual int AddRedirect(const Bitset& channelMask, void* context, LoggerWriteFnPtr writeFnptr, LoggerWriteFnPtr writeErrorFnptr) = 0;
    virtual void RemoveRedirect(int id) = 0;

    virtual void Write(const LogChannel& channel, const LogMessage& message) = 0;
    virtual void WriteError(const LogChannel& channel, const LogMessage& message) = 0;

    virtual void Flush() = 0;
};

class LoggerImpl;

class HYP_API DynamicLogChannelHandle
{
    friend class Logger;

    DynamicLogChannelHandle(Logger* logger, LogChannel* channel, bool ownsAllocation)
        : m_logger(logger),
          m_channel(channel),
          m_ownsAllocation(ownsAllocation)
    {
    }

    DynamicLogChannelHandle(const DynamicLogChannelHandle& other) = delete;
    DynamicLogChannelHandle& operator=(const DynamicLogChannelHandle& other) = delete;

public:
    DynamicLogChannelHandle(DynamicLogChannelHandle&& other) noexcept
        : m_logger(other.m_logger),
          m_channel(other.m_channel),
          m_ownsAllocation(other.m_ownsAllocation)
    {
        other.m_logger = nullptr;
        other.m_channel = nullptr;
        other.m_ownsAllocation = false;
    }

    DynamicLogChannelHandle& operator=(DynamicLogChannelHandle&& other) noexcept;

    ~DynamicLogChannelHandle();

    LogChannel* Release()
    {
        LogChannel* channel = m_channel;

        m_logger = nullptr;
        m_channel = nullptr;
        m_ownsAllocation = false;

        return channel;
    }

private:
    Logger* m_logger;
    LogChannel* m_channel;
    bool m_ownsAllocation : 1;
};

HYP_CLASS()
class HYP_API Logger : public ObjectBase
{
    HYP_OBJECT_BODY(Logger);

public:
    using ChannelMask = uint64;

    static constexpr uint32 MaxChannels = sizeof(ChannelMask) * CHAR_BIT;

    HYP_METHOD()
    static const Handle<Logger>& GetInstance();

    Logger();
    Logger(ILoggerOutputStream& outputStream);

    Logger(const Logger& other) = delete;
    Logger& operator=(const Logger& other) = delete;
    Logger(Logger&& other) noexcept = delete;
    Logger& operator=(Logger&& other) noexcept = delete;
    ~Logger();

    ILoggerOutputStream* GetOutputStream() const;

    void RegisterChannel(LogChannel* channel);

    const LogChannel* FindLogChannel(StringHash name) const;

    DynamicLogChannelHandle CreateDynamicLogChannel(Name name, LogChannel* parentChannel = nullptr);
    DynamicLogChannelHandle CreateDynamicLogChannel(LogChannel& channel);
    void RemoveDynamicLogChannel(Name name);
    void RemoveDynamicLogChannel(LogChannel* channel);
    void RemoveDynamicLogChannel(DynamicLogChannelHandle& channelHandle);

    bool IsChannelEnabled(const LogChannel& channel) const;

    void SetChannelEnabled(const LogChannel& channel, bool enabled);

    void Log(const LogChannel& channel, const LogMessage& message);
    void LogFatal(const LogChannel& channel, const LogMessage& message);

    HYP_METHOD()
    void LogScript(const LogChannel& channel, const LogCategory& category, const String& message);

    void (*fatalErrorHook)(const char*);

private:
    Pimpl<LoggerImpl> m_impl;
};

struct LogOnceHelper
{
    template <auto LogOnceFileName, int32 LogOnceLineNumber, auto LogOnceFunctionName, auto CategoryArg, auto ChannelArg, auto LogOnceFormatString, class... LogOnceArgTypes>
    static void ExecuteLogOnce(Logger& logger, LogOnceArgTypes&&... args)
    {
        static volatile int64 timesExecutedCounter = 0;

        int64 count = AtomicIncrement(&timesExecutedCounter) - 1;

        if (count == 0)
        {
            LogStatic<CategoryArg, ChannelArg, LogOnceFormatString>(logger, std::forward<LogOnceArgTypes>(args)...);
        }
        else if ((uint32(count) & (uint32(count) - 1)) == 0)
        {
            LogStatic<CategoryArg, ChannelArg, LogOnceFormatString.template Concat<HYP_STATIC_STRING("\t... and {} more like this\n")>()>(logger, std::forward<LogOnceArgTypes>(args)..., uint32(count));
        }
    }
};

template <auto CategoryArg, auto ChannelArg, auto FormatString, class... Args>
inline void LogStatic(Logger& logger, Args&&... args)
{
    static constexpr const LogCategory& Category = *HYP_GET_CONST_ARG(CategoryArg);

    if constexpr (!Category.IsEnabled())
    {
        return;
    }

    constexpr const char* ColorCode = LogLevelTermColor<Category.GetLevel()>();

    static const LogChannel& s_channel = *HYP_GET_CONST_ARG(ChannelArg);
    static const String s_prefix = HYP_FORMAT("{}{} [{}]: ", ColorCode, s_channel.name, LogLevelToString<Category.GetLevel()>());

    if (logger.IsChannelEnabled(s_channel))
    {
        if constexpr (Memory::AreStaticStringsEqual(ColorCode, ""))
        {
            if constexpr (Category.GetFlags() & LogCategory::LCF_FATAL)
            {
                logger.LogFatal(s_channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, utilities::Format<FormatString>(std::forward<Args>(args)...) } } });

                HYP_UNREACHABLE();
            }
            else
            {
                logger.Log(s_channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, utilities::Format<FormatString>(std::forward<Args>(args)...) } } });
            }
        }
        else
        {
            if constexpr (Category.GetFlags() & LogCategory::LCF_FATAL)
            {
                logger.LogFatal(s_channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, utilities::Format<FormatString>(std::forward<Args>(args)...), "\33[0m" } } });

                HYP_UNREACHABLE();
            }
            else
            {
                logger.Log(s_channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, utilities::Format<FormatString>(std::forward<Args>(args)...), "\33[0m" } } });
            }
        }
    }
}

template <auto CategoryArg, auto FormatString, class... Args>
inline void LogStatic_Channel(Logger& logger, const LogChannel& channel, Args&&... args)
{
    static constexpr const LogCategory& Category = *HYP_GET_CONST_ARG(CategoryArg);

    if constexpr (!Category.IsEnabled())
    {
        return;
    }

    constexpr const char* ColorCode = LogLevelTermColor<Category.GetLevel()>();

    const String prefix = HYP_FORMAT("{}{} [{}]: ", ColorCode, channel.name, LogLevelToString<Category.GetLevel()>());

    if (logger.IsChannelEnabled(channel))
    {
        if constexpr (Category.GetFlags() & LogCategory::LCF_FATAL)
        {
            logger.LogFatal(channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, utilities::Format<FormatString>(std::forward<Args>(args)...), "\33[0m" } } });

            HYP_UNREACHABLE();
        }
        else
        {
            logger.Log(channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, utilities::Format<FormatString>(std::forward<Args>(args)...), "\33[0m" } } });
        }
    }
}

template <auto CategoryArg, auto ChannelArg>
inline void LogDynamic(Logger& logger, const char* str)
{
    static constexpr const LogCategory& Category = *HYP_GET_CONST_ARG(CategoryArg);

    if constexpr (!Category.IsEnabled())
    {
        return;
    }

    constexpr const char* ColorCode = LogLevelTermColor<Category.GetLevel()>();

    static const LogChannel& s_channel = *HYP_GET_CONST_ARG(ChannelArg);
    static const String s_prefix = HYP_FORMAT("{}{} [{}]: ", ColorCode, s_channel.name, LogLevelToString<Category.GetLevel()>());

    if (logger.IsChannelEnabled(s_channel))
    {
        if constexpr (Memory::AreStaticStringsEqual(ColorCode, ""))
        {
            if constexpr (Category.GetFlags() & LogCategory::LCF_FATAL)
            {
                logger.LogFatal(s_channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, str } } });

                HYP_UNREACHABLE();
            }
            else
            {
                logger.Log(s_channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, str } } });
            }
        }
        else
        {
            if constexpr (Category.GetFlags() & LogCategory::LCF_FATAL)
            {
                logger.LogFatal(s_channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, str, "\33[0m" } } });

                HYP_UNREACHABLE();
            }
            else
            {
                logger.Log(s_channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, str, "\33[0m" } } });
            }
        }
    }
}

class LogChannelRegistrar
{
public:
    static LogChannelRegistrar& GetInstance()
    {
        static LogChannelRegistrar s_instance;

        return s_instance;
    }

    void Register(LogChannel* channel)
    {
        AssertDebug(channel);

        m_channels.PushBack(channel);
    }

    void Register(LogChannel* channel, LogChannel* parentChannel)
    {
        AssertDebug(channel);

        channel->parentChannel = parentChannel;

        m_channels.PushBack(channel);
    }

    void RegisterAll();

private:
    Array<LogChannel*> m_channels;
};

struct LogChannelRegistration
{
    LogChannelRegistration(LogChannel* channel)
    {
        LogChannelRegistrar::GetInstance().Register(channel);
    }

    LogChannelRegistration(LogChannel* channel, LogChannel* parentChannel)
    {
        LogChannelRegistrar::GetInstance().Register(channel, parentChannel);
    }
};

#define DEFINE_LOG_CATEGORY_GLOBAL(name)       \
    inline constexpr const LogCategory& name() \
    {                                          \
        return LogCategory::name;              \
    }

DEFINE_LOG_CATEGORY_GLOBAL(Debug);
DEFINE_LOG_CATEGORY_GLOBAL(Warning);
DEFINE_LOG_CATEGORY_GLOBAL(Info);
DEFINE_LOG_CATEGORY_GLOBAL(Error);
DEFINE_LOG_CATEGORY_GLOBAL(Fatal);

#undef DEFINE_LOG_CATEGORY_GLOBAL

} // namespace logging

using logging::DynamicLogChannelHandle;
using logging::ILoggerOutputStream;
using logging::LogCategory;
using logging::LogChannel;
using logging::LogChannelRegistrar;
using logging::LogChannelRegistration;
using logging::Logger;
using logging::LogLevel;
using logging::LogMessage;

HYP_API extern Handle<Logger> g_logger;

} // namespace hyperion

// Helper macros

// Must be used outside of function (in global scope)
#define HYP_DEFINE_LOG_CHANNEL(name)                                           \
    hyperion::logging::LogChannel g_logChannel_##name { NAME(HYP_STR(name)) }; \
    static hyperion::logging::LogChannelRegistration g_logChannelRegistration_##name(&g_logChannel_##name)

#define HYP_DEFINE_LOG_SUBCHANNEL(name, parentName)                            \
    hyperion::logging::LogChannel g_logChannel_##name { NAME(HYP_STR(name)) }; \
    static hyperion::logging::LogChannelRegistration g_logChannelRegistration_##name(&g_logChannel_##name, &g_logChannel_##parentName)

// Undefine HYP_LOG if already defined (LoggerFwd could have defined it as an empty macro)
#ifdef HYP_LOG_ONCE
#undef HYP_LOG_ONCE
#endif

#define HYP_LOG_ONCE(channel, category, fmt, ...)                                                                                                                                                                                                                                                                         \
    do                                                                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                                                                     \
        ::hyperion::logging::LogOnceHelper::ExecuteLogOnce<HYP_STATIC_STRING(__FILE__), __LINE__, HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT), HYP_MAKE_CONST_ARG(&hyperion::logging::category()), HYP_MAKE_CONST_ARG(&g_logChannel_##channel), HYP_STATIC_STRING(fmt "\n")>(hyperion::logging::GetLogger(), ##__VA_ARGS__); \
    }                                                                                                                                                                                                                                                                                                                     \
    while (0)
