/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/logging/Logger.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Thread.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Bitset.hpp>

#include <core/system/StackDump.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);
namespace logging {

class LogChannelIDGenerator
{
public:
    LogChannelIDGenerator()                                                     = default;
    LogChannelIDGenerator(const LogChannelIDGenerator &other)                   = delete;
    LogChannelIDGenerator &operator=(const LogChannelIDGenerator &other)        = delete;
    LogChannelIDGenerator(LogChannelIDGenerator &&other) noexcept               = delete;
    LogChannelIDGenerator &operator=(LogChannelIDGenerator &&other) noexcept    = delete;
    ~LogChannelIDGenerator()                                                    = default;

    [[nodiscard]]
    HYP_FORCE_INLINE
    uint32 Next()
    {
        return m_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
    }

private:
    AtomicVar<uint32>   m_counter { 0 };
};

static LogChannelIDGenerator g_log_channel_id_generator { };

#pragma region LogChannel

LogChannel::LogChannel(Name name)
    : m_id(g_log_channel_id_generator.Next()),
      m_name(name),
      m_flags(LogChannelFlags::NONE),
      m_parent_channel(nullptr),
      m_mask_bitset(1ull << m_id)
{
}

LogChannel::LogChannel(Name name, LogChannel *parent_channel)
    : LogChannel(name)
{
    m_parent_channel = parent_channel;

    if (m_parent_channel != nullptr) {
        m_mask_bitset |= m_parent_channel->m_mask_bitset;
    }
}

LogChannel::~LogChannel()
{
}

#pragma endregion LogChannel

#pragma region Logger

static Logger g_logger;

Logger &Logger::GetInstance()
{
    return g_logger;
}

Logger::Logger()
    : m_log_mask(uint64(-1))
{
}

Logger::Logger(Name context_name)
    : m_log_mask(uint64(-1))
{
}

void Logger::RegisterChannel(LogChannel *channel)
{
    AssertThrow(channel != nullptr);
    AssertThrow(channel->GetID() < m_log_channels.Size());

    m_log_channels[channel->GetID()] = channel;
}

bool Logger::IsChannelEnabled(const LogChannel &channel) const
{
    // @TODO: Come up with a more efficient solution that doesn't require atomics, looping, or dynamic bitsets

    const Bitset mask_value(m_log_mask.Get(MemoryOrder::ACQUIRE));
    const Bitset &channel_mask_bitset = channel.GetMaskBitset();

    return (mask_value & channel_mask_bitset) == channel_mask_bitset;
}

void Logger::SetChannelEnabled(const LogChannel &channel, bool enabled)
{
    if (enabled) {
        m_log_mask.BitOr(1ull << channel.GetID(), MemoryOrder::RELEASE);
    } else {
        m_log_mask.BitAnd(~(1ull << channel.GetID()), MemoryOrder::RELEASE);
    }
}

void Logger::Log(const LogChannel &channel, const LogMessage &message)
{
    // @TODO handle log level

    std::puts(*message.message);
}

String Logger::GetCurrentFunction()
{
    return StackDump(2).GetTrace().Front();
}

#pragma endregion Logger

} // namespace logging
} // namespace hyperion