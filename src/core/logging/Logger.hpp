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
    friend class LogChannelRegistrar;

    LogChannel(Name name, LogChannel* parentChannel = nullptr);
    LogChannel(const LogChannel& other) = delete;
    LogChannel& operator=(const LogChannel& other) = delete;
    LogChannel(LogChannel&& other) noexcept = delete;
    LogChannel& operator=(LogChannel&& other) noexcept = delete;
    ~LogChannel() = default;

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

    DynamicLogChannelHandle CreateDynamicLogChannel(Name name, LogChannel* parentChannel = nullptr);
    DynamicLogChannelHandle CreateDynamicLogChannel(LogChannel& channel);
    void RemoveDynamicLogChannel(Name name);
    void RemoveDynamicLogChannel(LogChannel* channel);
    void RemoveDynamicLogChannel(DynamicLogChannelHandle& channelHandle);

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

class LogChannelRegistrar
{
public:
    static LogChannelRegistrar& GetInstance()
    {
        static LogChannelRegistrar instance;
        return instance;
    }

    void Register(LogChannel* channel)
    {
        AssertDebug(channel);

        m_channels.PushBack(channel);
    }

    void Register(LogChannel* channel, LogChannel* parentChannel)
    {
        AssertDebug(channel);

        channel->m_parentChannel = parentChannel;

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

} // namespace hyperion

// Helper macros

// Must be used outside of function (in global scope)
#define HYP_DEFINE_LOG_CHANNEL(name)                                            \
    hyperion::logging::LogChannel g_logChannel_##name(HYP_NAME(HYP_STR(name))); \
    static hyperion::logging::LogChannelRegistration g_logChannelRegistration_##name(&g_logChannel_##name)

#define HYP_DEFINE_LOG_SUBCHANNEL(name, parentName)                             \
    hyperion::logging::LogChannel g_logChannel_##name(HYP_NAME(HYP_STR(name))); \
    static hyperion::logging::LogChannelRegistration g_logChannelRegistration_##name(&g_logChannel_##name, &g_logChannel_##parentName)

// Undefine HYP_LOG if already defined (LoggerFwd could have defined it as an empty macro)
#ifdef HYP_LOG_ONCE
#undef HYP_LOG_ONCE
#endif

#define HYP_LOG_ONCE(channel, category, fmt, ...)                                                                                                                                                                                                                                                    \
    do                                                                                                                                                                                                                                                                                               \
    {                                                                                                                                                                                                                                                                                                \
        ::hyperion::logging::LogOnceHelper::ExecuteLogOnce<HYP_STATIC_STRING(__FILE__), __LINE__, HYP_STATIC_STRING(HYP_FUNCTION_NAME_LIT), hyperion::logging::category(), HYP_MAKE_CONST_ARG(&g_logChannel_##channel), HYP_STATIC_STRING(fmt "\n")>(hyperion::logging::GetLogger(), ##__VA_ARGS__); \
    }                                                                                                                                                                                                                                                                                                \
    while (0)
