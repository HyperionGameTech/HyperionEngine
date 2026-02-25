/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#include <Core/logging/Logger.hpp>
#include <Core/logging/LogChannels.hpp>

#include <Core/threading/Thread.hpp>
#include <Core/threading/Threads.hpp>
#include <Core/threading/Mutex.hpp>
#include <Core/threading/AtomicVar.hpp>

#include <Core/containers/HashMap.hpp>
#include <Core/containers/Bitset.hpp>
#include <Core/containers/LinkedList.hpp>

#include <Core/functional/Proc.hpp>

#include <Core/memory/ByteBuffer.hpp>
#include <Core/memory/NotNullPtr.hpp>

#include <Core/config/Config.hpp>
#include <Core/Core.hpp>

#include <Core/io/ByteWriter.hpp>

#include <Core/utilities/ByteUtil.hpp>

#ifndef HYP_TOOL
#include <Logger.generated.inl>
#endif

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Core);
HYP_DECLARE_LOG_CHANNEL(Misc);
HYP_DECLARE_LOG_CHANNEL(Temp);
HYP_DECLARE_LOG_CHANNEL(Script);

namespace logging {

static volatile int32 s_maxLogChannelId = -1;
static bool s_registerAllCalled = false;

HYP_API ANSIStringView GetCurrentThreadName()
{
    return *CurrentThreadId().GetName();
}

HYP_API bool IsVerboseLoggingEnabled()
{
    static const ConfigurationValue& s_cfgVerboseLoggingEnabled = CoreApi::GetGlobalConfig().Get("Logging.Verbose");

    return s_cfgVerboseLoggingEnabled.ToBool(false);
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

static LogChannelIdGenerator s_logChannelIdGenerator {};

#pragma region LoggerOutputStream

struct LoggerRedirect
{
    Bitset channelMask;
    void* context = nullptr;
    LoggerWriteFnPtr writeFn;
    LoggerWriteFnPtr writeErrorFn;
};

class BasicLoggerOutputStream : public ILoggerOutputStream
{
public:
    static constexpr uint64 WriteFlag = 0x1;
    static constexpr uint64 ReadMask = uint64(-1) & ~WriteFlag;

    static BasicLoggerOutputStream& GetDefaultInstance()
    {
        static BasicLoggerOutputStream s_instance { stdout, stderr };

        return s_instance;
    }

    BasicLoggerOutputStream(FILE* output, FILE* outputError)
        : m_output(output),
          m_outputError(outputError),
          m_rwMarker(0),
          m_redirectEnabledMask(0),
          m_redirectIdCounter(-1)
    {
        Memory::Fill(m_contexts, 0, sizeof(m_contexts));
        Memory::Fill(m_writeFnptrTable, 0, sizeof(m_writeFnptrTable));
        Memory::Fill(m_writeErrorFnptrTable, 0, sizeof(m_writeErrorFnptrTable));
    }

    virtual ~BasicLoggerOutputStream() override = default;

    // @FIXME: Needs to redirect all CHILD channels as well, not parent channels!
    virtual int AddRedirect(const Bitset& channelMask, void* context, LoggerWriteFnPtr writeFnptr, LoggerWriteFnPtr writeErrorFnptr) override
    {
        AssertDebug(writeFnptr != nullptr || writeErrorFnptr != nullptr, "At least one of writeFnptr or writeErrorFnptr must be non-null");

        Mutex::Guard guard(m_mutex);

        uint64 state = m_rwMarker.BitOr(WriteFlag, MemoryOrder::ACQUIRE);
        while (state & ReadMask)
        {
            state = m_rwMarker.Get(MemoryOrder::ACQUIRE);
            HYP_WAIT_IDLE();
        }

        const int id = ++m_redirectIdCounter;

        m_redirects.Emplace(id, LoggerRedirect { channelMask, context, writeFnptr, writeErrorFnptr });

        for (Bitset::BitIndex bitIndex : channelMask)
        {
            if (bitIndex >= Logger::MaxChannels)
            {
                break;
            }

            m_contexts[bitIndex] = context;
            m_writeFnptrTable[bitIndex] = writeFnptr;
            m_writeErrorFnptrTable[bitIndex] = writeErrorFnptr;

            m_redirectEnabledMask |= (1ull << bitIndex);
        }

        m_rwMarker.BitAnd(~WriteFlag, MemoryOrder::RELEASE);

        return id;
    }

    virtual void RemoveRedirect(int id) override
    {
        Mutex::Guard guard(m_mutex);

        uint64 state = m_rwMarker.BitOr(WriteFlag, MemoryOrder::ACQUIRE);
        while (state & ReadMask)
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

            m_contexts[bitIndex] = nullptr;
            m_writeFnptrTable[bitIndex] = nullptr;
            m_writeErrorFnptrTable[bitIndex] = nullptr;

            m_redirectEnabledMask &= ~(1ull << bitIndex);
        }

        m_redirects.Erase(it);

        m_rwMarker.BitAnd(~WriteFlag, MemoryOrder::RELEASE);
    }

    virtual void Write(const LogChannel& channel, const LogMessage& message) override
    {
        const LogChannel* channelPtr = &channel;

        if (channel.id >= Logger::MaxChannels)
        {
            // log channel overflow! revert to Log_Misc
            //// \todo : Dynamic channels with ID >= MaxChannels should be checked using a dynamic bitset w/ mutex
            channelPtr = &g_logChannel_Misc;
            return;
        }

        uint64 rwMarkerState;

        do
        {
            rwMarkerState = m_rwMarker.Increment(2, MemoryOrder::ACQUIRE);

            if (HYP_UNLIKELY(rwMarkerState & WriteFlag))
            {
                m_rwMarker.Decrement(2, MemoryOrder::RELAXED);

                // spin to wait for write flag to be released
                HYP_WAIT_IDLE();
            }
        }
        while (HYP_UNLIKELY(rwMarkerState & WriteFlag));

        void* redirectContext = nullptr;
        LoggerWriteFnPtr redirectFunction = nullptr;

        uint32 bitIndex;
        uint64 mask = channelPtr->maskBitset.ToUInt64();

        while ((bitIndex = ByteUtil::HighestSetBitIndex(mask)) != -1)
        {
            if (m_redirectEnabledMask & (1ull << bitIndex))
            {
                redirectContext = m_contexts[bitIndex];
                redirectFunction = m_writeFnptrTable[bitIndex];

                break;
            }

            mask &= ~(1ull << bitIndex);
        }

        if (!redirectFunction || redirectFunction(redirectContext, *channelPtr, message))
        {
#ifdef HYP_DEBUG_MODE
            const bool isStandardOutput = (m_output == stdout || m_output == stderr);
            if (isStandardOutput)
            {
                Span<const char> colorCode = LogLevelTermColor(message.level);
                std::fwrite(colorCode.Data(), 1, colorCode.Size(), m_output);
            }
#endif

            for (auto it = message.chunks.Begin(); it != message.chunks.End(); ++it)
            {
                std::fwrite(**it, 1, it->Size(), m_output);
            }

#ifdef HYP_DEBUG_MODE
            if (isStandardOutput)
            {
                static constexpr Span<const char> ResetCode = "\033[0m";
                std::fwrite(ResetCode.Data(), 1, ResetCode.Size(), m_output);
            }
#endif
        }

        m_rwMarker.Decrement(2, MemoryOrder::RELEASE);
    }

    virtual void WriteError(const LogChannel& channel, const LogMessage& message) override
    {
        const LogChannel* channelPtr = &channel;

        if (channel.id >= Logger::MaxChannels)
        {
            // log channel overflow! revert to Log_Misc
            channelPtr = &g_logChannel_Misc;
            return;
        }

        uint64 rwMarkerState;

        do
        {
            rwMarkerState = m_rwMarker.Increment(2, MemoryOrder::ACQUIRE);

            if (HYP_UNLIKELY(rwMarkerState & WriteFlag))
            {
                m_rwMarker.Decrement(2, MemoryOrder::RELAXED);

                // spin to wait for write flag to be released
                HYP_WAIT_IDLE();
            }
        }
        while (HYP_UNLIKELY(rwMarkerState & WriteFlag));

        void* redirectContext = nullptr;
        LoggerWriteFnPtr redirectFunction = nullptr;

        uint32 bitIndex;
        uint64 mask = channelPtr->maskBitset.ToUInt64();

        while ((bitIndex = ByteUtil::HighestSetBitIndex(mask)) != -1)
        {
            if (m_redirectEnabledMask & (1ull << bitIndex))
            {
                redirectContext = m_contexts[bitIndex];
                redirectFunction = m_writeErrorFnptrTable[bitIndex];

                break;
            }

            mask &= ~(1ull << bitIndex);
        }

        if (!redirectFunction || redirectFunction(redirectContext, *channelPtr, message))
        {
#ifdef HYP_DEBUG_MODE
            const bool isStandardOutput = (m_output == stdout || m_output == stderr);
            if (isStandardOutput)
            {
                Span<const char> colorCode = LogLevelTermColor(message.level);
                std::fwrite(colorCode.Data(), 1, colorCode.Size(), m_outputError);
            }
#endif

            for (auto it = message.chunks.Begin(); it != message.chunks.End(); ++it)
            {
                std::fwrite(**it, 1, it->Size(), m_outputError);
            }

#ifdef HYP_DEBUG_MODE
            if (isStandardOutput)
            {
                static constexpr Span<const char> ResetCode = "\033[0m";
                std::fwrite(ResetCode.Data(), 1, ResetCode.Size(), m_outputError);
            }
#endif
        }

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
    AtomicVar<uint64> m_rwMarker;

    FILE* m_output;
    FILE* m_outputError;

    void* m_contexts[Logger::MaxChannels];

    LoggerWriteFnPtr m_writeFnptrTable[Logger::MaxChannels];
    LoggerWriteFnPtr m_writeErrorFnptrTable[Logger::MaxChannels];

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
    : id(~0u),
      name(name),
      parentChannel(parentChannel),
      maskBitset()
{
}

#pragma endregion LogChannel

#pragma region LogChannelRegistrar

void LogChannelRegistrar::RegisterAll()
{
    if (s_registerAllCalled)
    {
        return;
    }

    s_registerAllCalled = true;

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
        if (channel->parentChannel)
        {
            children[channel->parentChannel].PushBack(channel);
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
        order[i]->id = i;
    }

    // kept alive until program exit
    static Array<DynamicLogChannelHandle> s_dynamicLogChannelHandles;

    for (LogChannel* channel : m_channels)
    {
        Assert(channel->id != ~0u);

        channel->maskBitset = Bitset();
        channel->maskBitset.Set(channel->id, true);

        if (channel->parentChannel != nullptr)
        {
            Assert(channel->parentChannel->id != ~0u);

            channel->maskBitset |= channel->parentChannel->maskBitset;
        }

        if (channel->id < Hyperion::logging::Logger::MaxChannels)
        {
            continue;
        }

        // out of slots, need to store dynamic
        s_dynamicLogChannelHandles.PushBack(Logger::GetInstance().CreateDynamicLogChannel(*channel));
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
    FixedArray<LogChannel*, Logger::MaxChannels> m_logChannels;
    Array<LogChannel*> m_dynamicLogChannels;
    mutable Mutex m_dynamicLogChannelsMutex;
    NotNullPtr<ILoggerOutputStream> m_outputStream;
};

#pragma endregion LoggerImpl

#pragma region Logger

Logger& Logger::GetInstance()
{
    static Logger s_instance;
    return s_instance;
}

Handle<Logger> Logger::MakeScriptLogger()
{
#ifdef HYP_TOOL
    return Handle<Logger>::Null();
#else
    return MakeHandle<Logger>();
#endif
}

Logger::Logger()
    : Logger(BasicLoggerOutputStream::GetDefaultInstance())
{
}

Logger::Logger(ILoggerOutputStream& outputStream)
    : m_impl(MakePimpl<LoggerImpl>(outputStream)),
      fatalErrorHook(nullptr)
{
}

Logger::~Logger() = default;

ILoggerOutputStream* Logger::GetOutputStream() const
{
    return m_impl->m_outputStream;
}

const LogChannel* Logger::FindLogChannel(StringHash name) const
{
    for (LogChannel* channel : m_impl->m_logChannels)
    {
        if (!channel)
        {
            break;
        }

        if (channel->name == name)
        {
            return channel;
        }
    }

    Mutex::Guard guard(m_impl->m_dynamicLogChannelsMutex);

    for (LogChannel* channel : m_impl->m_dynamicLogChannels)
    {
        AssertDebug(channel != nullptr);

        if (channel->name == name)
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
    HYP_CORE_ASSERT(channel->id < m_impl->m_logChannels.Size());

    m_impl->m_logChannels[channel->id] = channel;
}

DynamicLogChannelHandle Logger::CreateDynamicLogChannel(Name name, LogChannel* parentChannel)
{
    AssertDebug(s_registerAllCalled);

    Mutex::Guard guard(m_impl->m_dynamicLogChannelsMutex);

    LogChannel* channel = m_impl->m_dynamicLogChannels.PushBack(new LogChannel(name));

    channel->id = AtomicIncrement(&s_maxLogChannelId);
    channel->maskBitset = Bitset(1ull << channel->id);

    if (parentChannel)
    {
        AssertDebug(parentChannel->id != ~0u, "Parent channel must be registered if it exists!");

        channel->parentChannel = parentChannel;
        channel->maskBitset |= parentChannel->maskBitset;
    }

    return DynamicLogChannelHandle(this, channel, /* ownsAllocation */ true);
}

DynamicLogChannelHandle Logger::CreateDynamicLogChannel(LogChannel& channel)
{
    Mutex::Guard guard(m_impl->m_dynamicLogChannelsMutex);

    m_impl->m_dynamicLogChannels.PushBack(&channel);

    if (channel.id == ~0u)
    {
        AssertDebug(s_registerAllCalled);

        channel.id = AtomicIncrement(&s_maxLogChannelId);
    }

    channel.maskBitset = Bitset(1ull << channel.id);

    return DynamicLogChannelHandle(this, &channel, /* ownsAllocation */ false);
}

void Logger::RemoveDynamicLogChannel(Name name)
{
    Mutex::Guard guard(m_impl->m_dynamicLogChannelsMutex);

    auto it = m_impl->m_dynamicLogChannels.FindIf([name](LogChannel* channel)
        {
            return channel->name == name;
        });

    if (it == m_impl->m_dynamicLogChannels.End())
    {
        return;
    }

    m_impl->m_dynamicLogChannels.Erase(it);
}

void Logger::RemoveDynamicLogChannel(LogChannel* channel)
{
    Mutex::Guard guard(m_impl->m_dynamicLogChannelsMutex);

    auto it = m_impl->m_dynamicLogChannels.Find(channel);

    if (it == m_impl->m_dynamicLogChannels.End())
    {
        return;
    }

    m_impl->m_dynamicLogChannels.Erase(it);
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
    const uint64 channelMaskBitset = channel.maskBitset.ToUInt64();

    return (m_impl->m_logMask.Get(MemoryOrder::RELAXED) & channelMaskBitset) == channelMaskBitset;
}

void Logger::SetChannelEnabled(const LogChannel& channel, bool enabled)
{
    if (enabled)
    {
        m_impl->m_logMask.BitOr(1ull << channel.id, MemoryOrder::RELAXED);
    }
    else
    {
        m_impl->m_logMask.BitAnd(~(1ull << channel.id), MemoryOrder::RELAXED);
    }
}

void Logger::Log(const LogChannel& channel, const LogMessage& message)
{
    if (uint32(message.level) <= uint32(LogLevel::Warning))
    {
        m_impl->m_outputStream->WriteError(channel, message);
    }
    else
    {
        m_impl->m_outputStream->Write(channel, message);
    }
}

void Logger::LogFatal(const LogChannel& channel, const LogMessage& message)
{
    Log(channel, message);

    // flush the output stream to ensure that the message is written before we call the fatal error hook
    m_impl->m_outputStream->Flush();

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
    else
    {
        debug::TerminateProgram();
    }

    HYP_UNREACHABLE();
}

void Logger::LogScript(const LogChannel& channel, LogLevel level, const String& message)
{
    constexpr UTF8StringView NewlineChunk = UTF8StringView("\n");

    const LogChannel* channelPtr = &channel;

    if (channel.id == ~0u)
    {
        // default to script channel if not set
        channelPtr = &g_logChannel_Script;
    }
    else if (channel.id >= Logger::MaxChannels)
    {
        // log channel overflow! revert to Log_Misc
        channelPtr = &g_logChannel_Misc;
    }

    if (!IsChannelEnabled(*channelPtr))
    {
        return;
    }

    UTF8StringView sv[2] = { UTF8StringView(message), NewlineChunk };

    LogMessage lm {};
    lm.level = level;
    lm.timestamp = uint64(Time::Now());
    lm.chunks = sv;

    // don't handle fatal here; scripts shouldn't be able to cause fatal errors

    Log(*channelPtr, lm);
}

#pragma endregion Logger

} // namespace logging

namespace logging {

#ifdef HYP_DEBUG_MODE
void LogTemp(Logger& logger, const char* str, const char* fileName, int lineNumber)
{
    static constexpr LogLevel Level = LogLevel::Debug;

    static const LogChannel& s_channel = g_logChannel_Temp;
    static const String s_prefix = HYP_FORMAT("{} [{}]: ", s_channel.name, LogLevelToString<Level>());

    FixedArray<UTF8StringView, 3> views { s_prefix, str, "\n" };

    LogMessage lm {};
    lm.level = Level;
    lm.timestamp = uint64(Time::Now());
    lm.chunks = views.ToSpan();
    lm.fileName = fileName;
    lm.lineNumber = lineNumber;

    logger.Log(s_channel, lm);
}
#endif

} // namespace logging

} // namespace Hyperion
