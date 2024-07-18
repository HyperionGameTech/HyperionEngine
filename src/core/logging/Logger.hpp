/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#ifndef HYPERION_UTIL_LOGGER_HPP
#define HYPERION_UTIL_LOGGER_HPP

#include <core/logging/LoggerFwd.hpp>

#include <core/Name.hpp>
#include <core/system/Debug.hpp>
#include <core/Defines.hpp>

#include <core/utilities/StringView.hpp>
#include <core/utilities/Format.hpp>
#include <core/utilities/EnumFlags.hpp>

#include <core/memory/ByteBuffer.hpp>

#include <core/containers/String.hpp>
#include <core/containers/HashMap.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/containers/Bitset.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>

namespace hyperion {

enum class LogChannelFlags : uint32
{
    NONE    = 0x0
};

HYP_MAKE_ENUM_FLAGS(LogChannelFlags)

namespace logging {

class Logger;

enum LogLevel : uint32
{
    DEBUG,
    INFO,
    WARNING,
    ERR,
    FATAL
};

struct LogMessage
{
    StringView<StringType::UTF8>    message;
};

class HYP_API LogChannel
{
public:
    friend class Logger;

    LogChannel(Name name);
    LogChannel(Name name, LogChannel *parent_channel);
    LogChannel(const LogChannel &other)                 = delete;
    LogChannel &operator=(const LogChannel &other)      = delete;
    LogChannel(LogChannel &&other) noexcept             = delete;
    LogChannel &operator=(LogChannel &&other) noexcept  = delete;
    ~LogChannel();

    /*! \brief Get the ID of this channel. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 GetID() const
        { return m_id; }

    /*! \brief Get the name of this channel. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    Name GetName() const
        { return m_name; }

    /*! \brief Get the flags for this channel. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    EnumFlags<LogChannelFlags> GetFlags() const
        { return m_flags; }

    /*! \brief Get a pointer to the parent channel, if one exists. */
    [[nodiscard]]
    HYP_FORCE_INLINE
    LogChannel *GetParentChannel() const
        { return m_parent_channel; }

    [[nodiscard]]
    HYP_FORCE_INLINE
    const Bitset &GetMaskBitset() const
        { return m_mask_bitset; }

private:
    uint32                      m_id;
    Name                        m_name;
    EnumFlags<LogChannelFlags>  m_flags;
    LogChannel                  *m_parent_channel;
    Bitset                      m_mask_bitset;
};

class HYP_API Logger
{
    template <LogLevel Level>
    static constexpr auto LogLevelToString()
    {
        if constexpr (Level == LogLevel::DEBUG) {
            return StaticString("Debug");
        } else if constexpr (Level == LogLevel::INFO) {
            return StaticString("Info");
        } else if constexpr (Level == LogLevel::WARNING) {
            return StaticString("Warning");
        } else if constexpr (Level == LogLevel::ERR) {
            return StaticString("Error");
        } else if constexpr (Level == LogLevel::FATAL) {
            return StaticString("Fatal");
        } else {
            return StaticString("Unknown");
        }
    }

public:
    static constexpr uint max_channels = 64;

    static Logger &GetInstance();

    Logger();
    Logger(Name context_name);
    Logger(const Logger &other)                 = delete;
    Logger &operator=(const Logger &other)      = delete;
    Logger(Logger &&other) noexcept             = delete;
    Logger &operator=(Logger &&other) noexcept  = delete;
    ~Logger()                                   = default;

    void RegisterChannel(LogChannel *channel);

    LogChannel *CreateDynamicLogChannel(Name name, LogChannel *parent_channel = nullptr);
    void DestroyDynamicLogChannel(Name name);
    void DestroyDynamicLogChannel(LogChannel *channel);

    bool IsChannelEnabled(const LogChannel &channel) const;

    void SetChannelEnabled(const LogChannel &channel, bool enabled);

    template <LogLevel Level, auto FunctionNameString, auto FormatString, class... Args>
    void Log(const LogChannel &channel, Args &&... args)
    {
        static const auto prefix_static_string = containers::helpers::Concat<
            StaticString("["),
            LogLevelToString< Level >(),
            StaticString("] "),
            FunctionNameString,
            StaticString(": ")
        >::value;

        static const String prefix_string = String(prefix_static_string.Data());

        if (IsChannelEnabled(channel)) {
            Log(
                channel,
                LogMessage {
                    prefix_string + utilities::Format<FormatString>(std::forward<Args>(args)...)
                }
            );
        }
    }

private:
    static String GetCurrentFunction();

    void Log(const LogChannel &channel, const LogMessage &message);

    AtomicVar<uint64>                       m_log_mask;
    FixedArray<LogChannel *, max_channels>  m_log_channels;
    LinkedList<LogChannel>                  m_dynamic_log_channels;
    Mutex                                   m_dynamic_log_channels_mutex;
};

} // namespace logging

using logging::Logger;
using logging::LogChannel;
using logging::LogLevel;

} // namespace hyperion

// Helper macros

// Must be used outside of function (in global scope)
#define HYP_DEFINE_LOG_CHANNEL(name) \
    hyperion::logging::LogChannel Log_##name(HYP_NAME_UNSAFE(name))

#define HYP_DEFINE_LOG_SUBCHANNEL(name, parent_name) \
    hyperion::logging::LogChannel Log_##name(HYP_NAME_UNSAFE(name), &Log_##parent_name)

// Undefine HYP_LOG if already defined (LoggerFwd could have defined it as an empty macro)
#ifdef HYP_LOG
    #undef HYP_LOG
    #undef HYP_LOG_ONCE
#endif

#ifdef HYP_MSVC
    #define HYP_LOG(channel, level, fmt, ...) \
        hyperion::logging::Logger::GetInstance().Log< level, HYP_PRETTY_FUNCTION_NAME, hyperion::StaticString< sizeof(fmt) >(fmt) >(hyperion::Log_##channel, __VA_ARGS__)

    #define HYP_LOG_ONCE(channel, level, fmt, ...) \
        do { \
            static bool HYP_CONCAT(_log_once_, __LINE__) = false; \
            if (!HYP_CONCAT(_log_once_, __LINE__)) { \
                HYP_CONCAT(_log_once_, __LINE__) = true; \
                HYP_LOG(channel, level, fmt, __VA_ARGS__); \
            } \
        } while (0)
#else
    #define HYP_LOG(channel, level, fmt, ...) \
        hyperion::logging::Logger::GetInstance().Log< level, HYP_PRETTY_FUNCTION_NAME, hyperion::StaticString(fmt) >(hyperion::Log_##channel __VA_OPT__(,) __VA_ARGS__)

    #define HYP_LOG_ONCE(channel, level, fmt, ...) \
        do { \
            static bool HYP_CONCAT(_log_once_, __LINE__) = false; \
            if (!HYP_CONCAT(_log_once_, __LINE__)) { \
                HYP_CONCAT(_log_once_, __LINE__) = true; \
                HYP_LOG(channel, level, fmt __VA_OPT__(,) __VA_ARGS__); \
            } \
        } while (0)
#endif

#endif