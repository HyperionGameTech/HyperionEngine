/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <core/logging/Logger.hpp>

#include <core/threading/Thread.hpp>
#include <core/threading/Threads.hpp>
#include <core/threading/Semaphore.hpp>
#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>

#include <core/containers/HashMap.hpp>
#include <core/containers/Bitset.hpp>
#include <core/containers/LinkedList.hpp>

#include <core/functional/Proc.hpp>

#include <core/memory/ByteBuffer.hpp>
#include <core/memory/NotNullPtr.hpp>

#include <core/io/ByteWriter.hpp>

#include <core/utilities/ByteUtil.hpp>

#include <numeric>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);
HYP_DECLARE_LOG_CHANNEL(Misc);
HYP_DECLARE_LOG_CHANNEL(Temp);

namespace logging {

static volatile int32 g_maxLogChannelId = -1;
static bool g_registerAllCalled = false;

HYP_API Logger& GetLogger()
{
    return Logger::GetInstance();
}

HYP_API ANSIStringView GetCurrentThreadName()
{
    return *Threads::CurrentThreadId().GetName();
}

class LogChannelIdGenerator
{
public:
    LogChannelIdGenerator() = default;
    LogChannelIdGenerator(const LogChannelIdGenerator& other) = delete;
    LogChannelIdGenerator& operator=(const LogChannelIdGenerator& other) = delete;
    LogChannelIdGenerator(LogChannelIdGenerator&& other) noexcept = delete;
    LogChannelIdGenerator& operator=(LogChannelIdGenerator&& other) noexcept = delete;
    ~LogChannelIdGenerator() = default;

    HYP_NODISCARD HYP_FORCE_INLINE uint32 Next()
    {
        return m_counter.Increment(1, MemoryOrder::ACQUIRE_RELEASE);
    }

private:
    AtomicVar<uint32> m_counter { 0 };
};

static LogChannelIdGenerator g_logChannelIdGenerator {};

#pragma region LoggerOutputStream

struct LoggerRedirect
{
    Bitset channelMask;
    void* context = nullptr;
    LoggerWriteFnPtr writeFnptr;
    LoggerWriteFnPtr writeErrorFnptr;
};

class BasicLoggerOutputStream : public ILoggerOutputStream
{
public:
    static constexpr uint64 writeFlag = 0x1;
    static constexpr uint64 readMask = uint64(-1) & ~writeFlag;

    static BasicLoggerOutputStream& GetDefaultInstance()
    {
        static BasicLoggerOutputStream instance { stdout, stderr };

        return instance;
    }

    BasicLoggerOutputStream(FILE* output, FILE* outputError)
        : m_output(output),
          m_outputError(outputError),
          m_rwMarker(0),
          m_redirectEnabledMask(0),
          m_redirectIdCounter(-1)
    {
        for (uint32 i = 0; i < Logger::maxChannels; i++)
        {
            m_contexts[i] = (void*)this;
            m_writeFnptrTable[i] = &Write_Static;
            m_writeErrorFnptrTable[i] = &WriteError_Static;
        }
    }

    virtual ~BasicLoggerOutputStream() override = default;

    // @FIXME: Needs to redirect all CHILD channels as well, not parent channels!
    virtual int AddRedirect(const Bitset& channelMask, void* context, LoggerWriteFnPtr writeFnptr, LoggerWriteFnPtr writeErrorFnptr) override
    {
        Mutex::Guard guard(m_mutex);

        uint64 state = m_rwMarker.BitOr(writeFlag, MemoryOrder::ACQUIRE);
        while (state & readMask)
        {
            state = m_rwMarker.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }

        const int id = ++m_redirectIdCounter;

        m_redirects.Emplace(id, LoggerRedirect { channelMask, context, writeFnptr, writeErrorFnptr });

        for (Bitset::BitIndex bitIndex : channelMask)
        {
            m_contexts[bitIndex] = context;
            m_writeFnptrTable[bitIndex] = writeFnptr;
            m_writeErrorFnptrTable[bitIndex] = writeErrorFnptr;
        }

        m_redirectEnabledMask |= (1ull << channelMask.LastSetBitIndex());

        m_rwMarker.BitAnd(~writeFlag, MemoryOrder::RELEASE);

        return id;
    }

    virtual void RemoveRedirect(int id) override
    {
        Mutex::Guard guard(m_mutex);

        uint64 state = m_rwMarker.BitOr(writeFlag, MemoryOrder::ACQUIRE);
        while (state & readMask)
        {
            state = m_rwMarker.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }

        auto it = m_redirects.Find(id);

        if (it == m_redirects.End())
        {
            return;
        }

        for (Bitset::BitIndex bitIndex : it->second.channelMask)
        {
            if (m_contexts[bitIndex] != it->second.context)
            {
                continue;
            }

            m_contexts[bitIndex] = (void*)this;
            m_writeFnptrTable[bitIndex] = &Write_Static;
            m_writeErrorFnptrTable[bitIndex] = &WriteError_Static;
        }

        m_redirects.Erase(it);

        m_redirectEnabledMask &= ~(1ull << it->second.channelMask.LastSetBitIndex());

        m_rwMarker.BitAnd(~writeFlag, MemoryOrder::RELEASE);
    }

    virtual void Write(const LogChannel& channel, const LogMessage& message) override
    {
        const LogChannel* channelPtr = &channel;

        if (channel.Id() >= Logger::maxChannels)
        {
            // log channel overflow! revert to Log_Misc
            /// @TODO: Dynamic channels with ID >= maxChannels should be checked using a dynamic bitset w/ mutex
            channelPtr = &g_logChannel_Misc;
            return;
        }

        uint64 rwMarkerState;

        do
        {
            rwMarkerState = m_rwMarker.Increment(2, MemoryOrder::ACQUIRE);

            if (HYP_UNLIKELY(rwMarkerState & writeFlag))
            {
                m_rwMarker.Decrement(2, MemoryOrder::RELAXED);

                // spin to wait for write flag to be released
                HYP_WAIT_IDLE();
            }
        }
        while (HYP_UNLIKELY(rwMarkerState & writeFlag));

        void* context = m_contexts[channelPtr->Id()];
        LoggerWriteFnPtr fnptr = m_writeFnptrTable[channelPtr->Id()];

        uint32 bitIndex;
        uint64 mask = channelPtr->GetMaskBitset().ToUInt64() & ~(1ull << channelPtr->Id());

        while ((bitIndex = ByteUtil::HighestSetBitIndex(mask)) != -1)
        {
            if (m_redirectEnabledMask & (1ull << bitIndex))
            {
                context = m_contexts[bitIndex];
                fnptr = m_writeFnptrTable[bitIndex];

                break;
            }

            mask &= ~(1ull << bitIndex);
        }

        fnptr(context, *channelPtr, message);

        m_rwMarker.Decrement(2, MemoryOrder::RELEASE);
    }

    virtual void WriteError(const LogChannel& channel, const LogMessage& message) override
    {
        const LogChannel* channelPtr = &channel;

        if (channel.Id() >= Logger::maxChannels)
        {
            // log channel overflow! revert to Log_Misc
            channelPtr = &g_logChannel_Misc;
            return;
        }

        uint64 rwMarkerState;

        do
        {
            rwMarkerState = m_rwMarker.Increment(2, MemoryOrder::ACQUIRE);

            if (HYP_UNLIKELY(rwMarkerState & writeFlag))
            {
                m_rwMarker.Decrement(2, MemoryOrder::RELAXED);

                // spin to wait for write flag to be released
                HYP_WAIT_IDLE();
            }
        }
        while (HYP_UNLIKELY(rwMarkerState & writeFlag));

        void* context = m_contexts[channelPtr->Id()];
        LoggerWriteFnPtr fnptr = m_writeErrorFnptrTable[channelPtr->Id()];

        uint32 bitIndex;
        uint64 mask = channelPtr->GetMaskBitset().ToUInt64() & ~(1ull << channelPtr->Id());

        while ((bitIndex = ByteUtil::HighestSetBitIndex(mask)) != -1)
        {
            if (m_redirectEnabledMask & (1ull << bitIndex))
            {
                context = m_contexts[bitIndex];
                fnptr = m_writeErrorFnptrTable[bitIndex];

                break;
            }

            mask &= ~(1ull << bitIndex);
        }

        fnptr(context, *channelPtr, message);

        m_rwMarker.Decrement(2, MemoryOrder::RELEASE);
    }

    virtual void Flush() override
    {
        if (m_output)
        {
            std::fflush(m_output);
        }

        if (m_outputError)
        {
            std::fflush(m_outputError);
        }
    }

private:
    static void Write_Static(void* context, const LogChannel& channel, const LogMessage& message)
    {
        for (auto it = message.chunks.Begin(); it != message.chunks.End(); ++it)
        {
            std::fwrite(**it, 1, it->Size(), ((BasicLoggerOutputStream*)context)->m_output);
        }
    }

    static void WriteError_Static(void* context, const LogChannel& channel, const LogMessage& message)
    {
        for (auto it = message.chunks.Begin(); it != message.chunks.End(); ++it)
        {
            std::fwrite(**it, 1, it->Size(), ((BasicLoggerOutputStream*)context)->m_outputError);
        }
    }

    AtomicVar<uint64> m_rwMarker;

    FILE* m_output;
    FILE* m_outputError;

    void* m_contexts[Logger::maxChannels];

    LoggerWriteFnPtr m_writeFnptrTable[Logger::maxChannels];
    LoggerWriteFnPtr m_writeErrorFnptrTable[Logger::maxChannels];

    Mutex m_mutex;
    HashMap<int, LoggerRedirect> m_redirects;
    uint64 m_redirectEnabledMask;
    int m_redirectIdCounter;
};

#pragma endregion LoggerOutputStream

#pragma region DynamicLogChannelHandle

DynamicLogChannelHandle::~DynamicLogChannelHandle()
{
    if (m_logger != nullptr)
    {
        m_logger->RemoveDynamicLogChannel(*this);
    }

    if (m_ownsAllocation)
    {
        delete m_channel;
    }
}

DynamicLogChannelHandle& DynamicLogChannelHandle::operator=(DynamicLogChannelHandle&& other) noexcept
{
    if (this == &other || m_channel == other.m_channel)
    {
        return *this;
    }

    if (m_logger != nullptr)
    {
        m_logger->RemoveDynamicLogChannel(*this);
    }

    if (m_ownsAllocation)
    {
        delete m_channel;
    }

    m_logger = other.m_logger;
    m_channel = other.m_channel;
    m_ownsAllocation = other.m_ownsAllocation;

    other.m_logger = nullptr;
    other.m_channel = nullptr;
    other.m_ownsAllocation = false;

    return *this;
}

#pragma endregion DynamicLogChannelHandle

#pragma region LogChannel

LogChannel::LogChannel(Name name, LogChannel* parentChannel)
    : m_id(~0u),
      m_name(name),
      m_flags(LogChannelFlags::NONE),
      m_parentChannel(parentChannel),
      m_maskBitset(0)
{
}

#pragma endregion LogChannel

#pragma region LogChannelRegistrar

void LogChannelRegistrar::RegisterAll()
{
    if (g_registerAllCalled)
    {
        return;
    }

    g_registerAllCalled = true;

    const Array<LogChannel*>& channels = m_channels;
    const uint32 n = uint32(channels.Size());

    HashMap<LogChannel*, uint32> indeg;
    HashMap<LogChannel*, Array<LogChannel*>> children;
    indeg.Reserve(n);
    children.Reserve(n);

    for (LogChannel* channel : channels)
    {
        indeg[channel] = 0;
    }

    for (LogChannel* channel : channels)
    {
        if (channel->m_parentChannel)
        {
            children[channel->m_parentChannel].PushBack(channel);
            ++indeg[channel];
        }
    }

    Array<LogChannel*> queue;
    queue.Reserve(n);

    for (LogChannel* channel : channels)
    {
        if (indeg[channel] == 0)
        {
            queue.PushBack(channel);
        }
    }

    Array<LogChannel*> order;
    order.Reserve(n);

    SizeType head = 0;

    while (head < queue.Size())
    {
        LogChannel* u = queue[head++];
        order.PushBack(u);

        Array<LogChannel*>& subchannels = children[u];

        for (LogChannel* subchannel : subchannels)
        {
            if (--indeg[subchannel] == 0)
            {
                queue.PushBack(subchannel);
            }
        }
    }

    Assert(order.Size() == n);

    for (uint32 i = 0; i < n; i++)
    {
        order[i]->m_id = i;
    }

    // kept alive until program exit
    static Array<DynamicLogChannelHandle> dynamicLogChannelHandles;

    for (LogChannel* channel : m_channels)
    {
        Assert(channel->m_id != ~0u);

        channel->m_maskBitset = Bitset();
        channel->m_maskBitset.Set(channel->m_id, true);

        if (channel->m_parentChannel != nullptr)
        {
            Assert(channel->m_parentChannel->m_id != ~0u);

            channel->m_maskBitset |= channel->m_parentChannel->m_maskBitset;
        }

        if (channel->m_id < hyperion::logging::Logger::maxChannels)
        {
            continue;
        }

        // out of slots, need to store dynamic
        dynamicLogChannelHandles.PushBack(Logger::GetInstance().CreateDynamicLogChannel(*channel));
    }

    m_channels.Clear();
}

#pragma endregion LogChannelRegistrar

#pragma region LoggerImpl

class LoggerImpl
{
public:
    friend class Logger;

    LoggerImpl(ILoggerOutputStream& outputStream)
        : m_logMask(-1),
          m_outputStream(&outputStream)
    {
        Fill(m_logChannels.Begin(), m_logChannels.End(), nullptr);
    }

    ~LoggerImpl()
    {
    }

private:
    AtomicVar<Logger::ChannelMask> m_logMask;
    FixedArray<LogChannel*, Logger::maxChannels> m_logChannels;
    Array<LogChannel*> m_dynamicLogChannels;
    mutable Mutex m_dynamicLogChannelsMutex;
    NotNullPtr<ILoggerOutputStream> m_outputStream;
};

#pragma endregion LoggerImpl

#pragma region Logger

Logger& Logger::GetInstance()
{
    static Logger instance;

    return instance;
}

Logger::Logger()
    : Logger(BasicLoggerOutputStream::GetDefaultInstance())
{
}

Logger::Logger(ILoggerOutputStream& outputStream)
    : m_pImpl(MakePimpl<LoggerImpl>(outputStream)),
      fatalErrorHook(nullptr)
{
}

Logger::~Logger() = default;

ILoggerOutputStream* Logger::GetOutputStream() const
{
    return m_pImpl->m_outputStream;
}

const LogChannel* Logger::FindLogChannel(WeakName name) const
{
    for (LogChannel* channel : m_pImpl->m_logChannels)
    {
        if (!channel)
        {
            break;
        }

        if (channel->GetName() == name)
        {
            return channel;
        }
    }

    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    for (LogChannel* channel : m_pImpl->m_dynamicLogChannels)
    {
        AssertDebug(channel != nullptr);

        if (channel->GetName() == name)
        {
            return channel;
        }

        return channel;
    }

    return nullptr;
}

void Logger::RegisterChannel(LogChannel* channel)
{
    HYP_CORE_ASSERT(channel != nullptr);
    HYP_CORE_ASSERT(channel->Id() < m_pImpl->m_logChannels.Size());

    m_pImpl->m_logChannels[channel->Id()] = channel;
}

DynamicLogChannelHandle Logger::CreateDynamicLogChannel(Name name, LogChannel* parentChannel)
{
    AssertDebug(g_registerAllCalled);

    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    LogChannel* channel = m_pImpl->m_dynamicLogChannels.PushBack(new LogChannel(name));

    channel->m_id = AtomicIncrement(&g_maxLogChannelId);
    channel->m_maskBitset = Bitset(1ull << channel->m_id);

    if (parentChannel)
    {
        AssertDebug(parentChannel->m_id != ~0u, "Parent channel must be registered if it exists!");

        channel->m_parentChannel = parentChannel;
        channel->m_maskBitset |= parentChannel->m_maskBitset;
    }

    return DynamicLogChannelHandle(this, channel, /* ownsAllocation */ true);
}

DynamicLogChannelHandle Logger::CreateDynamicLogChannel(LogChannel& channel)
{
    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    m_pImpl->m_dynamicLogChannels.PushBack(&channel);

    if (channel.m_id == ~0u)
    {
        AssertDebug(g_registerAllCalled);

        channel.m_id = AtomicIncrement(&g_maxLogChannelId);
    }

    channel.m_maskBitset = Bitset(1ull << channel.m_id);

    return DynamicLogChannelHandle(this, &channel, /* ownsAllocation */ false);
}

void Logger::RemoveDynamicLogChannel(Name name)
{
    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    auto it = m_pImpl->m_dynamicLogChannels.FindIf([name](LogChannel* channel)
        {
            return channel->GetName() == name;
        });

    if (it == m_pImpl->m_dynamicLogChannels.End())
    {
        return;
    }

    m_pImpl->m_dynamicLogChannels.Erase(it);
}

void Logger::RemoveDynamicLogChannel(LogChannel* channel)
{
    Mutex::Guard guard(m_pImpl->m_dynamicLogChannelsMutex);

    auto it = m_pImpl->m_dynamicLogChannels.Find(channel);

    if (it == m_pImpl->m_dynamicLogChannels.End())
    {
        return;
    }

    m_pImpl->m_dynamicLogChannels.Erase(it);
}

void Logger::RemoveDynamicLogChannel(DynamicLogChannelHandle& channelHandle)
{
    if (!channelHandle.m_channel)
    {
        return;
    }

    RemoveDynamicLogChannel(channelHandle.m_channel);

    channelHandle.m_logger = nullptr;
    channelHandle.m_channel = nullptr;
}

bool Logger::IsChannelEnabled(const LogChannel& channel) const
{
    const uint64 channelMaskBitset = channel.GetMaskBitset().ToUInt64();

    return (m_pImpl->m_logMask.Get(MemoryOrder::RELAXED) & channelMaskBitset) == channelMaskBitset;
}

void Logger::SetChannelEnabled(const LogChannel& channel, bool enabled)
{
    if (enabled)
    {
        m_pImpl->m_logMask.BitOr(1ull << channel.Id(), MemoryOrder::RELAXED);
    }
    else
    {
        m_pImpl->m_logMask.BitAnd(~(1ull << channel.Id()), MemoryOrder::RELAXED);
    }
}

void Logger::Log(const LogChannel& channel, const LogMessage& message)
{
    if (uint32(message.level) >= uint32(LogLevel::WARNING))
    {
        m_pImpl->m_outputStream->WriteError(channel, message);
    }
    else
    {
        m_pImpl->m_outputStream->Write(channel, message);
    }
}

void Logger::LogFatal(const LogChannel& channel, const LogMessage& message)
{
    Log(channel, message);

    // flush the output stream to ensure that the message is written before we call the fatal error hook
    m_pImpl->m_outputStream->Flush();

    MemoryByteWriter writer;

    for (const auto& chunk : message.chunks)
    {
        writer.Write(chunk.Data(), chunk.Size());
    }

    writer.Write(uint8(0)); // Null-terminate the message

    const String messageStr { writer.GetBuffer().ToByteView() };

    if (fatalErrorHook)
    {
        fatalErrorHook(messageStr.Data());
    }
}

#pragma endregion Logger

} // namespace logging

namespace logging {

HYP_API void LogTemp(Logger& logger, const char* str)
{
    LogDynamic<Debug(), HYP_MAKE_CONST_ARG(&g_logChannel_Temp)>(logger, str);
}

} // namespace logging

} // namespace hyperion
