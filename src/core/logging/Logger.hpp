/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/logging/LoggerFwd.hpp>

#include <core/Name.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Format.hpp>
#include <core/utilities/EnumFlags.hpp>
#include <core/utilities/Time.hpp>

#include <core/memory/Pimpl.hpp>

#include <core/containers/String.hpp>
#include <core/containers/Bitset.hpp>

#include <core/threading/AtomicVar.hpp>

namespace hyperion {

enum class LogChannelFlags : uint32
{
    NONE = 0x0
};

HYP_MAKE_ENUM_FLAGS(LogChannelFlags)

namespace logging {

struct LogMessage
{
    LogLevel level;
    uint64 timestamp;
    Span<StringView<StringType::UTF8>> chunks;
};

HYP_API extern ANSIStringView GetCurrentThreadName();

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

class LogChannel
{
public:
    friend class Logger;

    HYP_API LogChannel(Name name);
    HYP_API LogChannel(Name name, LogChannel* parentChannel);
    LogChannel(const LogChannel& other) = delete;
    LogChannel& operator=(const LogChannel& other) = delete;
    LogChannel(LogChannel&& other) noexcept = delete;
    LogChannel& operator=(LogChannel&& other) noexcept = delete;
    HYP_API ~LogChannel();

    /*! \brief Get the integral identifier of this channel. */
    HYP_FORCE_INLINE uint32 Id() const
    {
        return m_id;
    }

    /*! \brief Get the name of this channel. */
    HYP_FORCE_INLINE Name GetName() const
    {
        return m_name;
    }

    /*! \brief Get the flags for this channel. */
    HYP_FORCE_INLINE EnumFlags<LogChannelFlags> GetFlags() const
    {
        return m_flags;
    }

    /*! \brief Get a pointer to the parent channel, if one exists. */
    HYP_FORCE_INLINE LogChannel* GetParentChannel() const
    {
        return m_parentChannel;
    }

    HYP_FORCE_INLINE const Bitset& GetMaskBitset() const
    {
        return m_maskBitset;
    }

private:
    uint32 m_id;
    Name m_name;
    EnumFlags<LogChannelFlags> m_flags;
    LogChannel* m_parentChannel;
    Bitset m_maskBitset;
};

using LoggerWriteFnPtr = void (*)(void* context, const LogChannel& channel, const LogMessage& message);

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

class HYP_API Logger
{
public:
    using ChannelMask = uint64;

    static constexpr uint32 maxChannels = sizeof(ChannelMask) * CHAR_BIT;

    static Logger& GetInstance();

    Logger();
    Logger(ILoggerOutputStream& outputStream);

    Logger(const Logger& other) = delete;
    Logger& operator=(const Logger& other) = delete;
    Logger(Logger&& other) noexcept = delete;
    Logger& operator=(Logger&& other) noexcept = delete;
    ~Logger();

    ILoggerOutputStream* GetOutputStream() const;

    void RegisterChannel(LogChannel* channel);

    const LogChannel* FindLogChannel(WeakName name) const;

    LogChannel* CreateDynamicLogChannel(Name name, LogChannel* parentChannel = nullptr);
    void DestroyDynamicLogChannel(Name name);
    void DestroyDynamicLogChannel(LogChannel* channel);

    bool IsChannelEnabled(const LogChannel& channel) const;

    void SetChannelEnabled(const LogChannel& channel, bool enabled);

    void Log(const LogChannel& channel, const LogMessage& message);
    void LogFatal(const LogChannel& channel, const LogMessage& message);

    void (*fatalErrorHook)(const char*);

private:
    Pimpl<LoggerImpl> m_pImpl;
};

struct LogOnceHelper
{
    template <auto LogOnceFileName, int32 LogOnceLineNumber, auto LogOnceFunctionName, LogCategory Category, auto ChannelArg, auto LogOnceFormatString, class... LogOnceArgTypes>
    static void ExecuteLogOnce(Logger& logger, LogOnceArgTypes&&... args)
    {
        static volatile int64 timesExecutedCounter = 0;

        int64 count = AtomicIncrement(&timesExecutedCounter) - 1;

        if (count == 0)
        {
            LogStatic<Category, ChannelArg, LogOnceFormatString>(logger, std::forward<LogOnceArgTypes>(args)...);
        }
        else if ((uint32(count) & (uint32(count) - 1)) == 0)
        {
            LogStatic<Category, ChannelArg, LogOnceFormatString.template Concat<HYP_STATIC_STRING("\t... and {} more like this\n")>()>(logger, std::forward<LogOnceArgTypes>(args)..., uint32(count));
        }
    }
};

template <LogCategory Category, auto ChannelArg, auto FormatString, class... Args>
inline void LogStatic(Logger& logger, Args&&... args)
{
    if constexpr (!Category.IsEnabled())
    {
        return;
    }

    constexpr const char* colorCode = LogLevelTermColor<Category.GetLevel()>();

    static const LogChannel& channel = *HYP_GET_CONST_ARG(ChannelArg);
    static const String prefix = HYP_FORMAT("{}{} [{}]: ", colorCode, channel.GetName(), LogLevelToString<Category.GetLevel()>());

    if (logger.IsChannelEnabled(channel))
    {
        if constexpr (Memory::AreStaticStringsEqual(colorCode, ""))
        {
            if constexpr (Category.GetFlags() & LogCategory::LCF_FATAL)
            {
                logger.LogFatal(channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, utilities::Format<FormatString>(std::forward<Args>(args)...) } } });

                HYP_UNREACHABLE();
            }
            else
            {
                logger.Log(channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, utilities::Format<FormatString>(std::forward<Args>(args)...) } } });
            }
        }
        else
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
}

template <LogCategory Category, auto FormatString, class... Args>
inline void LogStatic_Channel(Logger& logger, const LogChannel& channel, Args&&... args)
{
    if constexpr (!Category.IsEnabled())
    {
        return;
    }

    constexpr const char* colorCode = LogLevelTermColor<Category.GetLevel()>();

    const String prefix = HYP_FORMAT("{}{} [{}]: ", colorCode, channel.GetName(), LogLevelToString<Category.GetLevel()>());

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

template <LogCategory Category, auto ChannelArg>
inline void LogDynamic(Logger& logger, const char* str)
{
    if constexpr (!Category.IsEnabled())
    {
        return;
    }

    constexpr const char* colorCode = LogLevelTermColor<Category.GetLevel()>();

    static const LogChannel& channel = *HYP_GET_CONST_ARG(ChannelArg);
    static const String prefix = HYP_FORMAT("{}{} [{}]: ", colorCode, channel.GetName(), LogLevelToString<Category.GetLevel()>());

    if (logger.IsChannelEnabled(channel))
    {
        if constexpr (Memory::AreStaticStringsEqual(colorCode, ""))
        {
            if constexpr (Category.GetFlags() & LogCategory::LCF_FATAL)
            {
                logger.LogFatal(channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, str } } });

                HYP_UNREACHABLE();
            }
            else
            {
                logger.Log(channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, str } } });
            }
        }
        else
        {
            if constexpr (Category.GetFlags() & LogCategory::LCF_FATAL)
            {
                logger.LogFatal(channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, str, "\33[0m" } } });

                HYP_UNREACHABLE();
            }
            else
            {
                logger.Log(channel, LogMessage { Category.GetLevel(), Time::Now().ToMilliseconds(), Span<StringView<StringType::UTF8>> { { prefix, str, "\33[0m" } } });
            }
        }
    }
}

} // namespace logging

using logging::ILoggerOutputStream;
using logging::LogCategory;
using logging::LogChannel;
using logging::Logger;
using logging::LogLevel;
using logging::LogMessage;

} // namespace hyperion

// Helper macros

// Must be used outside of function (in global scope)
#define HYP_DEFINE_LOG_CHANNEL(name) \
    hyperion::logging::LogChannel Log_##name(HYP_NAME_UNSAFE(name))

#define HYP_DEFINE_LOG_SUBCHANNEL(name, parentName) \
    hyperion::logging::LogChannel Log_##name(HYP_NAME_UNSAFE(name), &Log_##parentName)

// Undefine HYP_LOG if already defined (LoggerFwd could have defined it as an empty macro)
#ifdef HYP_LOG_ONCE
#undef HYP_LOG_ONCE
#endif

#define HYP_LOG_ONCE(channel, category, fmt, ...)                                                                                                                                                                                                                                           \
    do                                                                                                                                                                                                                                                                                      \
    {                                                                                                                                                                                                                                                                                       \
        ::hyperion::logging::LogOnceHelper::ExecuteLogOnce<HYP_STATIC_STRING(__FILE__), __LINE__, HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT), hyperion::logging::category(), HYP_MAKE_CONST_ARG(&Log_##channel), HYP_STATIC_STRING(fmt "\n")>(hyperion::logging::GetLogger(), ##__VA_ARGS__); \
    }                                                                                                                                                                                                                                                                                       \
    while (0)
