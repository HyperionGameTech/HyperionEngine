/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <core/logging/Logger.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Bitset.hpp>

#include <core/utilities/ByteUtil.hpp>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);

namespace logging {

HYP_API Logger &GetLogger()
{
    return Logger::GetInstance();
}

class LogChannelIDGenerator
{
public:
    LogChannelIDGenerator()                                                     = default;
    LogChannelIDGenerator(const LogChannelIDGenerator &other)                   = delete;
    LogChannelIDGenerator &operator=(const LogChannelIDGenerator &other)        = delete;
    LogChannelIDGenerator(LogChannelIDGenerator &&other) noexcept               = delete;
    LogChannelIDGenerator &operator=(LogChannelIDGenerator &&other) noexcept    = delete;
    ~LogChannelIDGenerator()                                                    = default;

    HYP_NODISCARD HYP_FORCE_INLINE uint32 Next()
    {
        return m_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
    }

private:
    AtomicVar<uint32>   m_counter { 0 };
};

static LogChannelIDGenerator g_log_channel_id_generator { };

#pragma region LoggerOutputStream

struct LoggerRedirect
{
    Bitset              channel_mask;
    void                *context = nullptr;
    LoggerWriteFnPtr    write_fnptr;
    LoggerWriteFnPtr    write_error_fnptr;
};

class BasicLoggerOutputStream : public ILoggerOutputStream
{
public:
    static constexpr uint64 write_flag = 0x1;
    static constexpr uint64 read_mask = uint64(-1) & ~write_flag;

    static BasicLoggerOutputStream &GetDefaultInstance()
    {
        static BasicLoggerOutputStream instance { stdout, stderr };

        return instance;
    }

    BasicLoggerOutputStream(FILE *output, FILE *output_error)
        : m_output(output),
          m_output_error(output_error),
          m_rw_marker(0),
          m_redirect_enabled_mask(0),
          m_redirect_id_counter(-1)
    {
        for (uint32 i = 0; i < Logger::max_channels; i++) {
            m_contexts[i] = (void *)this;
            m_write_fnptr_table[i] = &Write_Static;
            m_write_error_fnptr_table[i] = &WriteError_Static;
        }
    }

    virtual ~BasicLoggerOutputStream() override = default;

    // @FIXME: Needs to redirect all CHILD channels as well, not parent channels!
    virtual int AddRedirect(const Bitset &channel_mask, void *context, LoggerWriteFnPtr write_fnptr, LoggerWriteFnPtr write_error_fnptr) override
    {
        Mutex::Guard guard(m_mutex);

        uint64 state = m_rw_marker.BitOr(write_flag, MemoryOrder::ACQUIRE);
        while (state & read_mask) {
            state = m_rw_marker.Get(MemoryOrder::ACQUIRE);
            Threads::Sleep(0);
        }
        
        const int id = ++m_redirect_id_counter;

        m_redirects.Emplace(id, LoggerRedirect { channel_mask, context, write_fnptr, write_error_fnptr });

        for (Bitset::BitIndex bit_index : channel_mask) {
            m_contexts[bit_index] = context;
            m_write_fnptr_table[bit_index] = write_fnptr;
            m_write_error_fnptr_table[bit_index] = write_error_fnptr;
        }

        m_redirect_enabled_mask.BitOr(1ull << channel_mask.LastSetBitIndex(), MemoryOrder::RELEASE);

        m_rw_marker.BitAnd(~write_flag, MemoryOrder::RELEASE);

        return id;
    }

    virtual void RemoveRedirect(int id) override
    {
        Mutex::Guard guard(m_mutex);

        uint64 state = m_rw_marker.BitOr(write_flag, MemoryOrder::ACQUIRE);
        while (state & read_mask) {
            state = m_rw_marker.Get(MemoryOrder::ACQUIRE);
            Threads::Sleep(0);
        }

        auto it = m_redirects.Find(id);

        if (it == m_redirects.End()) {
            return;
        }

        for (Bitset::BitIndex bit_index : it->second.channel_mask) {
            if (m_contexts[bit_index] != it->second.context) {
                continue;
            }

            m_contexts[bit_index] = (void *)this;
            m_write_fnptr_table[bit_index] = &Write_Static;
            m_write_error_fnptr_table[bit_index] = &WriteError_Static;
        }

        m_redirects.Erase(it);

        m_redirect_enabled_mask.BitAnd(~(1ull << it->second.channel_mask.LastSetBitIndex()), MemoryOrder::RELEASE);

        m_rw_marker.BitAnd(~write_flag, MemoryOrder::RELEASE);
    }

    virtual void Write(const LogChannel &channel, const LogMessage &message) override
    {
        const LogChannel *channel_ptr = &channel;

        if (channel.GetID() >= Logger::max_channels) {
            channel_ptr = &Log_Core;
            return;
        }

        // set read flags
        uint64 state;
        while (((state = m_rw_marker.Increment(2, MemoryOrder::ACQUIRE)) & write_flag)) {
            m_rw_marker.Decrement(2, MemoryOrder::RELAXED);
            // wait for write flag to be released
            Threads::Sleep(0);
        }

        void *context = m_contexts[channel_ptr->GetID()];
        LoggerWriteFnPtr fnptr = m_write_fnptr_table[channel_ptr->GetID()];

        uint32 bit_index;
        uint64 mask = channel_ptr->GetMaskBitset().ToUInt64() & ~(1ull << channel_ptr->GetID());
    
        while ((bit_index = ByteUtil::HighestSetBitIndex(mask)) != -1) {
            if (m_redirect_enabled_mask.Get(MemoryOrder::ACQUIRE) & (1ull << bit_index)) {
                context = m_contexts[bit_index];
                fnptr = m_write_fnptr_table[bit_index];

                break;
            }
    
            mask &= ~(1ull << bit_index);
        }

        fnptr(context, *channel_ptr, message);

        m_rw_marker.Decrement(2, MemoryOrder::RELEASE);
    }

    virtual void WriteError(const LogChannel &channel, const LogMessage &message) override
    {
        const LogChannel *channel_ptr = &channel;

        if (channel.GetID() >= Logger::max_channels) {
            channel_ptr = &Log_Core;
            return;
        }

        // set read flags
        uint64 state;
        while (((state = m_rw_marker.Increment(2, MemoryOrder::ACQUIRE)) & write_flag)) {
            m_rw_marker.Decrement(2, MemoryOrder::RELAXED);
            // wait for write flag to be released
            Threads::Sleep(0);
        }

        void *context = m_contexts[channel_ptr->GetID()];
        LoggerWriteFnPtr fnptr = m_write_error_fnptr_table[channel_ptr->GetID()];

        uint32 bit_index;
        uint64 mask = channel_ptr->GetMaskBitset().ToUInt64() & ~(1ull << channel_ptr->GetID());
    
        while ((bit_index = ByteUtil::HighestSetBitIndex(mask)) != -1) {
            if (m_redirect_enabled_mask.Get(MemoryOrder::ACQUIRE) & (1ull << bit_index)) {
                context = m_contexts[bit_index];
                fnptr = m_write_error_fnptr_table[bit_index];

                break;
            }
    
            mask &= ~(1ull << bit_index);
        }
        
        fnptr(context, *channel_ptr, message);

        m_rw_marker.Decrement(2, MemoryOrder::RELEASE);
    }

private:
    static void Write_Static(void *context, const LogChannel &channel, const LogMessage &message)
    {
        std::fwrite(*message.message, 1, message.message.Size(), ((BasicLoggerOutputStream *)context)->m_output);
    }

    static void WriteError_Static(void *context, const LogChannel &channel, const LogMessage &message)
    {
        std::fwrite(*message.message, 1, message.message.Size(), ((BasicLoggerOutputStream *)context)->m_output_error);
    }

    AtomicVar<uint64>               m_rw_marker;

    FILE                            *m_output;
    FILE                            *m_output_error;

    void                            *m_contexts[Logger::max_channels];

    LoggerWriteFnPtr                m_write_fnptr_table[Logger::max_channels];
    LoggerWriteFnPtr                m_write_error_fnptr_table[Logger::max_channels];

    Mutex                           m_mutex;
    HashMap<int, LoggerRedirect>    m_redirects;
    AtomicVar<uint64>               m_redirect_enabled_mask;
    int                             m_redirect_id_counter;
};

#pragma endregion LoggerOutputStream

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

Logger &Logger::GetInstance()
{
    static Logger instance;

    return instance;
}

Logger::Logger()
    : Logger(&BasicLoggerOutputStream::GetDefaultInstance())
{
}

Logger::Logger(NotNullPtr<ILoggerOutputStream> output_stream)
    : m_log_mask(uint64(-1)),
      m_output_stream(output_stream)
{
}

Logger::~Logger() = default;

const LogChannel *Logger::FindLogChannel(WeakName name) const
{
    for (LogChannel *channel : m_log_channels) {
        if (!channel) {
            break;
        }

        if (channel->GetName() == name) {
            return channel;
        }
    }

    Mutex::Guard guard(m_dynamic_log_channels_mutex);

    for (const LogChannel &channel : m_dynamic_log_channels) {
        return &channel;
    }

    return nullptr;
}

void Logger::RegisterChannel(LogChannel *channel)
{
    AssertThrow(channel != nullptr);
    AssertThrow(channel->GetID() < m_log_channels.Size());

    m_log_channels[channel->GetID()] = channel;
}

LogChannel *Logger::CreateDynamicLogChannel(Name name, LogChannel *parent_channel)
{
    Mutex::Guard guard(m_dynamic_log_channels_mutex);

    return &m_dynamic_log_channels.EmplaceBack(name, parent_channel);
}

void Logger::DestroyDynamicLogChannel(Name name)
{
    Mutex::Guard guard(m_dynamic_log_channels_mutex);

    auto it = m_dynamic_log_channels.FindIf([name](const auto &item)
    {
        return item.GetName() == name;
    });

    if (it == m_dynamic_log_channels.End()) {
        return;
    }

    m_dynamic_log_channels.Erase(it);
}

void Logger::DestroyDynamicLogChannel(LogChannel *channel)
{
    Mutex::Guard guard(m_dynamic_log_channels_mutex);

    auto it = m_dynamic_log_channels.FindIf([channel](const auto &item)
    {
        return &item == channel;
    });

    if (it == m_dynamic_log_channels.End()) {
        return;
    }

    m_dynamic_log_channels.Erase(it);
}

bool Logger::IsChannelEnabled(const LogChannel &channel) const
{
    const uint64 channel_mask_bitset = channel.GetMaskBitset().ToUInt64();

    return (m_log_mask.Get(MemoryOrder::RELAXED) & channel_mask_bitset) == channel_mask_bitset;
}

void Logger::SetChannelEnabled(const LogChannel &channel, bool enabled)
{
    if (enabled) {
        m_log_mask.BitOr(1ull << channel.GetID(), MemoryOrder::RELAXED);
    } else {
        m_log_mask.BitAnd(~(1ull << channel.GetID()), MemoryOrder::RELAXED);
    }
}

void Logger::Log(const LogChannel &channel, const LogMessage &message)
{
    if (uint32(message.level) >= uint32(LogLevel::WARNING)) {
        m_output_stream->WriteError(channel, message);
    } else {
        m_output_stream->Write(channel, message);
    }
}

#pragma endregion Logger

} // namespace logging
} // namespace hyperion