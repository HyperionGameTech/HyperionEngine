/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_UTIL_LOGGER_HPP
#define HYPERION_UTIL_LOGGER_HPP

#include <core/logging/LoggerFwd.hpp>

#include <core/Name.hpp>
#include <core/debug/Debug.hpp>
#include <core/Defines.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Format.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/NotNullPtr.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/containers/Bitset.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/Util.hpp> // For HYP_PRETTY_FUNCTION_NAME

namespace hyperion {

enum class LogChannelFlags : uint32
{
    NONE    = 0x0
};

HYP_MAKE_ENUM_FLAGS(LogChannelFlags)

namespace logging {

struct LogMessage
{
    LogLevel                        level;
    StringView<StringType::UTF8>    message;
};

template <LogLevel Level>
static constexpr auto LogLevelToString()
{
    if constexpr (Level == LogLevel::DEBUG) {
        return HYP_STATIC_STRING("Debug");
    } else if constexpr (Level == LogLevel::INFO) {
        return HYP_STATIC_STRING("Info");
    } else if constexpr (Level == LogLevel::WARNING) {
        return HYP_STATIC_STRING("Warning");
    } else if constexpr (Level == LogLevel::ERR) {
        return HYP_STATIC_STRING("Error");
    } else if constexpr (Level == LogLevel::FATAL) {
        return HYP_STATIC_STRING("Fatal");
    } else {
        return HYP_STATIC_STRING("Unknown");
    }
}

class LogChannel
{
public:
    friend class Logger;

    HYP_API LogChannel(Name name);
    HYP_API LogChannel(Name name, LogChannel *parent_channel);
    LogChannel(const LogChannel &other)                 = delete;
    LogChannel &operator=(const LogChannel &other)      = delete;
    LogChannel(LogChannel &&other) noexcept             = delete;
    LogChannel &operator=(LogChannel &&other) noexcept  = delete;
    HYP_API ~LogChannel();

    /*! \brief Get the ID of this channel. */
    HYP_FORCE_INLINE uint32 GetID() const
        { return m_id; }

    /*! \brief Get the name of this channel. */
    HYP_FORCE_INLINE Name GetName() const
        { return m_name; }

    /*! \brief Get the flags for this channel. */
    HYP_FORCE_INLINE EnumFlags<LogChannelFlags> GetFlags() const
        { return m_flags; }

    /*! \brief Get a pointer to the parent channel, if one exists. */
    HYP_FORCE_INLINE LogChannel *GetParentChannel() const
        { return m_parent_channel; }

    HYP_FORCE_INLINE const Bitset &GetMaskBitset() const
        { return m_mask_bitset; }

private:
    uint32                      m_id;
    Name                        m_name;
    EnumFlags<LogChannelFlags>  m_flags;
    LogChannel                  *m_parent_channel;
    Bitset                      m_mask_bitset;
};

class ILoggerOutputStream
{
public:
    virtual ~ILoggerOutputStream() = default;

    virtual void Write(const LogChannel &channel, const LogMessage &message) = 0;
    virtual void WriteError(const LogChannel &channel, const LogMessage &message) = 0;
};

class HYP_API Logger
{
public:
    static constexpr uint32 max_channels = 64;

    static Logger &GetInstance();

    Logger();
    Logger(NotNullPtr<ILoggerOutputStream> output_stream);

    Logger(const Logger &other)                 = delete;
    Logger &operator=(const Logger &other)      = delete;
    Logger(Logger &&other) noexcept             = delete;
    Logger &operator=(Logger &&other) noexcept  = delete;
    ~Logger();

    void RegisterChannel(LogChannel *channel);

    const LogChannel *FindLogChannel(WeakName name) const;

    LogChannel *CreateDynamicLogChannel(Name name, LogChannel *parent_channel = nullptr);
    void DestroyDynamicLogChannel(Name name);
    void DestroyDynamicLogChannel(LogChannel *channel);

    bool IsChannelEnabled(const LogChannel &channel) const;

    void SetChannelEnabled(const LogChannel &channel, bool enabled);

    void Log(const LogChannel &channel, const LogMessage &message);

private:
    AtomicVar<uint64>                       m_log_mask;
    FixedArray<LogChannel *, max_channels>  m_log_channels;
    LinkedList<LogChannel>                  m_dynamic_log_channels;
    mutable Mutex                           m_dynamic_log_channels_mutex;
    NotNullPtr<ILoggerOutputStream>         m_output_stream;
};

namespace detail {

struct LogOnceHelper
{
    template <auto LogOnceFileName, int32 LogOnceLineNumber, auto LogOnceFunctionName, LogCategory Category, auto LogOnceFormatString, class... LogOnceArgTypes>
    static void ExecuteLogOnce(Logger &logger, const LogChannel &channel, LogOnceArgTypes &&... args)
    {
        static struct LogOnceHelper_Impl
        {
            LogOnceHelper_Impl(Logger &logger, const LogChannel &channel, LogOnceArgTypes &&... args)
            {
                Log_Internal< Category, LogOnceFunctionName, LogOnceFormatString >(logger, channel, std::forward<LogOnceArgTypes>(args)...);
            }
        } impl { logger, channel, std::forward<LogOnceArgTypes>(args)... };
    }
};

} // namespace detail

template <LogCategory Category, auto FunctionNameString, auto FormatString, class... Args>
static inline void Log_Internal(Logger &logger, const LogChannel &channel, Args &&... args)
{
    if constexpr (!Category.IsEnabled()) {
        return;
    }

    static const auto prefix_static_string = containers::helpers::Concat<
        StaticString("["),
        LogLevelToString< Category.GetLevel() >(),
        StaticString("] "),
        FunctionNameString,
        StaticString(": ")
    >::value;

    static const String prefix_string = String(prefix_static_string.Data());

    if (logger.IsChannelEnabled(channel)) {
        logger.Log(
            channel,
            LogMessage {
                Category.GetLevel(),
                prefix_string + utilities::Format<FormatString>(std::forward<Args>(args)...)
            }
        );
    }

    if constexpr (Category.GetFlags() & LogCategory::LOG_CATEGORY_FLAG_FATAL) {
        HYP_THROW("Fatal error logged");
    }
}

} // namespace logging

using logging::Logger;
using logging::LogChannel;
using logging::LogLevel;
using logging::LogCategory;

} // namespace hyperion

// Helper macros

// Must be used outside of function (in global scope)
#define HYP_DEFINE_LOG_CHANNEL(name) \
    hyperion::logging::LogChannel Log_##name(HYP_NAME_UNSAFE(name))

#define HYP_DEFINE_LOG_SUBCHANNEL(name, parent_name) \
    hyperion::logging::LogChannel Log_##name(HYP_NAME_UNSAFE(name), &Log_##parent_name)

// Undefine HYP_LOG if already defined (LoggerFwd could have defined it as an empty macro)
#ifdef HYP_LOG_ONCE
    #undef HYP_LOG_ONCE
#endif

#define HYP_LOG_ONCE(channel, category, fmt, ...) \
    do { \
        ::hyperion::logging::detail::LogOnceHelper::ExecuteLogOnce< HYP_STATIC_STRING(__FILE__), __LINE__, HYP_PRETTY_FUNCTION_NAME, hyperion::logging::category(), HYP_STATIC_STRING(fmt "\n") >(hyperion::logging::GetLogger(), Log_##channel, ##__VA_ARGS__); \
    } while (0)

#endif