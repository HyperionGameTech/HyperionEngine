/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <Core/logging/LoggerFwd.hpp>

#include <Core/Name.hpp>

#include <Core/utilities/StringView.hpp>
#include <Core/utilities/Format.hpp>
#include <Core/utilities/Time.hpp>

#include <Core/memory/Pimpl.hpp>

#include <Core/reflection/Handle.hpp>
#include <Core/reflection/ObjectFwd.hpp>

#include <Core/containers/String.hpp>
#include <Core/containers/Bitset.hpp>

#include <Core/threading/AtomicVar.hpp>

#include <climits>
#include <source_location>

namespace Hyperion {

namespace logging {

HYP_API extern ANSIStringView GetCurrentThreadName();
HYP_API extern bool IsVerboseLoggingEnabled();

struct LogMessage
{
    LogLevel level;
    uint64 timestamp;
    Span<StringView<StringType::UTF8>> chunks;
    const char* fileName;
    int lineNumber;
};

HYP_ENUM()
enum class LogLevel : uint8
{
    Fatal,
    Error,
    Warning,
    Info,
    Verbose,
    Debug
};

constexpr uint8 NumLogLevels = uint8(LogLevel::Debug) + 1;

template <LogLevel Level>
static constexpr auto LogLevelToString()
{
    if constexpr (Level == LogLevel::Info)
    {
        return HYP_STATIC_STRING("Info");
    }
    else if constexpr (Level == LogLevel::Warning)
    {
        return HYP_STATIC_STRING("Warning");
    }
    else if constexpr (Level == LogLevel::Error)
    {
        return HYP_STATIC_STRING("Error");
    }
    else if constexpr (Level == LogLevel::Fatal)
    {
        return HYP_STATIC_STRING("Fatal");
    }
    else if constexpr (Level == LogLevel::Debug)
    {
        return HYP_STATIC_STRING("Debug");
    }
    else if constexpr (Level == LogLevel::Verbose)
    {
        return HYP_STATIC_STRING("Verbose");
    }
    else
    {
        return HYP_STATIC_STRING("?");
    }
}

static constexpr Span<const char> LogLevelTermColor(LogLevel logLevel)
{
    constexpr Span<const char> ColorTable[] = {
        "\033[91m"  // Fatal
        "\033[31m", // Error
        "\033[33m"  // Warning
    };

    if (uint8(logLevel) >= uint8(std::size(ColorTable)))
    {
        constexpr Span<const char> DefaultColor = "\033[0m";
        
        return DefaultColor;
    }
    else
    {
        return ColorTable[uint8(logLevel)];
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

    static Logger& GetInstance();

    HYP_METHOD()
    static Handle<Logger> MakeScriptLogger();

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
    void LogScript(const LogChannel& channel, LogLevel level, const String& message);

    void (*fatalErrorHook)(const char*);

private:
    Pimpl<LoggerImpl> m_impl;
};

inline Logger& GetLogger()
{
    return Logger::GetInstance();
}

struct LogOnceHelper
{
    template <auto LogOnceFileName, int32 LogOnceLineNumber, auto LogOnceFunctionName, LogLevel Level, auto ChannelArg, auto LogOnceFormatString, class... LogOnceArgTypes>
    static void ExecuteLogOnce(Logger& logger, LogOnceArgTypes&&... args)
    {
        static volatile int64 timesExecutedCounter = 0;

        int64 count = AtomicIncrement(&timesExecutedCounter) - 1;

        if (count == 0)
        {
            LogStatic<Level, ChannelArg, LogOnceFormatString, HYP_STATIC_STRING(""), 0>(logger, std::forward<LogOnceArgTypes>(args)...);
        }
        else if ((uint32(count) & (uint32(count) - 1)) == 0)
        {
            LogStatic<Level, ChannelArg, LogOnceFormatString.template Concat<HYP_STATIC_STRING("\t... and {} more like this\n")>(), HYP_STATIC_STRING(""), 0>(logger, std::forward<LogOnceArgTypes>(args)..., uint32(count));
        }
    }
};

template <LogLevel Level, auto ChannelArg, auto FormatString, auto FileName, int LineNumber, class... Args>
inline void LogStatic(Logger& logger, Args&&... args)
{
    static const LogChannel& s_channel = *HYP_GET_CONST_ARG(ChannelArg);
    static const String s_prefix = HYP_FORMAT("{} [{}]: ", s_channel.name, LogLevelToString<Level>());

    if (logger.IsChannelEnabled(s_channel))
    {
#ifndef HYP_DEBUG_MODE
        if constexpr (Level == LogLevel::Debug)
            return;
#endif

        if constexpr (Level == LogLevel::Verbose)
        {
            if (!IsVerboseLoggingEnabled())
                return;
        }
        else if constexpr (Level == LogLevel::Fatal)
        {
            logger.LogFatal(s_channel, LogMessage { Level, Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, utilities::Format<FormatString>(std::forward<Args>(args)...) } }, FileName.Data(), LineNumber });

            HYP_UNREACHABLE();
        }
        else
        {
            logger.Log(s_channel, LogMessage { Level, Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, utilities::Format<FormatString>(std::forward<Args>(args)...) } }, FileName.Data(), LineNumber });
        }
    }
}

template <LogLevel Level, auto FormatString, auto FileName, int LineNumber, class... Args>
inline void LogStatic_Channel(Logger& logger, const LogChannel& channel, Args&&... args)
{
    const String prefix = HYP_FORMAT("{} [{}]: ", channel.name, LogLevelToString<Level>());

    if (logger.IsChannelEnabled(channel))
    {
#ifndef HYP_DEBUG_MODE
        if constexpr (Level == LogLevel::Debug)
            return;
#endif

        if constexpr (Level == LogLevel::Verbose)
        {
            if (!IsVerboseLoggingEnabled())
                return;
        }
        else if constexpr (Level == LogLevel::Fatal)
        {
            logger.LogFatal(channel, LogMessage { Level, Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, utilities::Format<FormatString>(std::forward<Args>(args)...) } }, FileName.Data(), LineNumber });

            HYP_UNREACHABLE();
        }
        else
        {
            logger.Log(channel, LogMessage { Level, Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, utilities::Format<FormatString>(std::forward<Args>(args)...) } }, FileName.Data(), LineNumber });
        }
    }
}

template <LogLevel Level>
inline void LogDynamic(Logger& logger, const LogChannel& channel, const char* fileName, int lineNumber, const char* str)
{
    const String s_prefix = HYP_FORMAT("{} [{}]: ", channel.name, LogLevelToString<Level>());

    if (logger.IsChannelEnabled(channel))
    {
#ifndef HYP_DEBUG_MODE
        if constexpr (Level == LogLevel::Debug)
            return;
#endif

        if constexpr (Level == LogLevel::Verbose)
        {
            if (!IsVerboseLoggingEnabled())
                return;
        }
        else if constexpr (Level == LogLevel::Fatal)
        {
            logger.LogFatal(channel, LogMessage { Level, Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, str } }, fileName, lineNumber });

            HYP_UNREACHABLE();
        }
        else
        {
            logger.Log(channel, LogMessage { Level, Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { s_prefix, str } }, fileName, lineNumber });
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

#define DEFINE_LOG_LEVEL_GLOBAL(name)           \
    inline constexpr LogLevel name()            \
    {                                           \
        return LogLevel::name;                  \
    }

DEFINE_LOG_LEVEL_GLOBAL(Fatal);
DEFINE_LOG_LEVEL_GLOBAL(Error);
DEFINE_LOG_LEVEL_GLOBAL(Warning);
DEFINE_LOG_LEVEL_GLOBAL(Info);
DEFINE_LOG_LEVEL_GLOBAL(Verbose);
DEFINE_LOG_LEVEL_GLOBAL(Debug);

#undef DEFINE_LOG_LEVEL_GLOBAL

} // namespace logging

using logging::DynamicLogChannelHandle;
using logging::ILoggerOutputStream;
using logging::LogChannel;
using logging::LogChannelRegistrar;
using logging::LogChannelRegistration;
using logging::Logger;
using logging::LogLevel;
using logging::LogMessage;

} // namespace Hyperion

// Helper macros

// Must be used outside of function (in global scope)
#define HYP_DEFINE_LOG_CHANNEL(name)                                           \
    Hyperion::logging::LogChannel g_logChannel_##name { NAME(HYP_STR(name)) }; \
    static Hyperion::logging::LogChannelRegistration g_logChannelRegistration_##name(&g_logChannel_##name)

#define HYP_DEFINE_LOG_SUBCHANNEL(name, parentName)                            \
    Hyperion::logging::LogChannel g_logChannel_##name { NAME(HYP_STR(name)) }; \
    static Hyperion::logging::LogChannelRegistration g_logChannelRegistration_##name(&g_logChannel_##name, &g_logChannel_##parentName)

// Undefine HYP_LOG if already defined (LoggerFwd could have defined it as an empty macro)
#ifdef HYP_LOG_ONCE
#undef HYP_LOG_ONCE
#endif

#define HYP_LOG_ONCE(channel, level, fmt, ...)                                                                                                                                                                                                                                                                         \
    do                                                                                                                                                                                                                                                                                                                    \
    {                                                                                                                                                                                                                                                                                                                     \
        ::Hyperion::logging::LogOnceHelper::ExecuteLogOnce<HYP_STATIC_STRING(__FILE__), __LINE__, HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT), Hyperion::logging::LogLevel::level, HYP_MAKE_CONST_ARG(&g_logChannel_##channel), HYP_STATIC_STRING(fmt "\n")>(Hyperion::logging::GetLogger(), ##__VA_ARGS__); \
    }                                                                                                                                                                                                                                                                                                                     \
    while (0)
