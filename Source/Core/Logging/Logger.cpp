/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <Core/Logging/Logger.hpp>
#include <Core/Logging/LogChannels.hpp>

#include <Core/Threading/Thread.hpp>
#include <Core/Threading/Threads.hpp>
#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/ThreadLocalStorage.hpp>

#include <Core/Containers/Map.hpp>
#include <Core/Containers/Bitset.hpp>
#include <Core/Containers/List.hpp>

#include <Core/Functional/Proc.hpp>

#include <Core/Memory/ByteBuffer.hpp>
#include <Core/Memory/NotNullPtr.hpp>

#include <Core/Memory/Allocator/Allocator.hpp>
#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Config/Config.hpp>
#include <Core/Core.hpp>

#include <Core/IO/ByteWriter.hpp>

#include <Core/Utilities/ByteUtil.hpp>

#ifndef HYP_TOOL
#include <Logger.generated.inl>
#endif

#if HYP_ANDROID
#include <android/log.h>
#endif

namespace Hyperion {

CORE_API HYP_DECLARE_LOG_CHANNEL(Core);
CORE_API HYP_DECLARE_LOG_CHANNEL(Misc);
CORE_API HYP_DECLARE_LOG_CHANNEL(Temp);

namespace logging {

static volatile int32 s_maxLogChannelId = -1;
static bool s_registerAllCalled = false;

static bool s_isOutputStreamInitialized = false;

thread_local bool t_isVerboseLoggingEnabled = false;

thread_local uint64 t_localRedirectMask = 0;
thread_local uint32 t_localGeneration = 0;
thread_local void* t_localContexts[Logger::MaxChannels];
thread_local LoggerWriteFnPtr t_localWriteFnptrTable[Logger::MaxChannels];
thread_local LoggerWriteFnPtr t_localWriteErrorFnptrTable[Logger::MaxChannels];

CORE_API ANSIStringView GetCurrentThreadName()
{
    return *CurrentThreadId().GetName();
}

CORE_API bool IsVerboseLoggingEnabled()
{
    return (t_isVerboseLoggingEnabled || (t_isVerboseLoggingEnabled = CoreApi::GetGlobalConfig().Get("Logging.Verbose").ToBool(false)));
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

struct LoggerRedirect
{
    Bitset channelMask;
    void* context = nullptr;
    LoggerWriteFnPtr writeFn;
    LoggerWriteFnPtr writeErrorFn;
};

struct LoggerState
{
    FILE* m_output;
    FILE* m_outputError;

    void* m_contexts[Logger::MaxChannels];

    LoggerWriteFnPtr m_writeFnptrTable[Logger::MaxChannels];
    LoggerWriteFnPtr m_writeErrorFnptrTable[Logger::MaxChannels];

    Mutex m_mutex;
    TMap<int, LoggerRedirect> m_redirects;
    uint64 m_redirectEnabledMask;
    int m_redirectIdCounter;

    AtomicVar<uint32> m_generation { 0 };

    LoggerState()
    {
        m_output = stdout;
        m_outputError = stderr;
        m_redirectEnabledMask = 0;
        m_redirectIdCounter = -1;

        Assert(!s_isOutputStreamInitialized, "Only one instance off LoggerOutputStream can be active");
        s_isOutputStreamInitialized = true;

        Memory::Zero(m_contexts, sizeof(m_contexts));
        Memory::Zero(m_writeFnptrTable, sizeof(m_writeFnptrTable));
        Memory::Zero(m_writeErrorFnptrTable, sizeof(m_writeErrorFnptrTable));
    }
};

static LoggerState s_loggerState;

static void Write(const LogChannel& channel, const LogMessage& message)
{
    const LogChannel* pChannel = &channel;

    if (HYP_UNLIKELY(channel.id >= Logger::MaxChannels))
    {
        // log channel overflow! revert to Log_Misc
        //// \todo : Dynamic channels with ID >= MaxChannels should be checked using a dynamic bitset w/ mutex
        pChannel = &g_logChannel_Misc;
        return;
    }

    const uint32 currentGeneration = s_loggerState.m_generation.Get(MemoryOrder::RELAXED);

    if (HYP_UNLIKELY(t_localGeneration != currentGeneration))
    {
        t_localRedirectMask = s_loggerState.m_redirectEnabledMask;

        Memory::Copy(t_localContexts, s_loggerState.m_contexts, sizeof(s_loggerState.m_contexts));
        Memory::Copy(t_localWriteFnptrTable, s_loggerState.m_writeFnptrTable, sizeof(s_loggerState.m_writeFnptrTable));
        Memory::Copy(t_localWriteErrorFnptrTable, s_loggerState.m_writeErrorFnptrTable, sizeof(s_loggerState.m_writeErrorFnptrTable));

        t_localGeneration = currentGeneration;
    }

    void* redirectContext = nullptr;
    LoggerWriteFnPtr redirectFunction = nullptr;

    const bool isError = uint8(message.level) <= uint8(LogLevel::Warning);

    uint32 bitIndex;
    uint64 mask = pChannel->maskBitset.ToUInt64();

    while ((bitIndex = ByteUtil::HighestSetBitIndex(mask)) != -1)
    {
        if (t_localRedirectMask & (1ull << bitIndex))
        {
            redirectContext = t_localContexts[bitIndex];
            redirectFunction = isError ? t_localWriteErrorFnptrTable[bitIndex] : t_localWriteFnptrTable[bitIndex];

            break;
        }

        mask &= ~(1ull << bitIndex);
    }

    if (!redirectFunction || redirectFunction(redirectContext, *pChannel, message))
    {
        FILE* file = isError ? s_loggerState.m_output : s_loggerState.m_outputError;
        const bool isConsole = file == stdout || file == stderr;

#if HYP_ANDROID
        if (isConsole)
        {
            static constexpr android_LogPriority LogLevelToAndroidLogPriority[NumLogLevels] = {

                ANDROID_LOG_FATAL,   // Fatal
                ANDROID_LOG_ERROR,   // Error
                ANDROID_LOG_WARN,    // Warning
                ANDROID_LOG_INFO,    // Info
                ANDROID_LOG_VERBOSE, // Verbose
                ANDROID_LOG_DEBUG    // Debug
            };

            thread_local ByteBuffer* t_androidLogBuffer = nullptr;
            thread_local bool t_useAndroidLogger = false;

            if (HYP_UNLIKELY(t_androidLogBuffer == nullptr))
            {
                ThreadBase* thisThread = CurrentThreadObject();

                if (thisThread)
                {
                    t_androidLogBuffer = new ByteBuffer;

                    if (t_androidLogBuffer)
                    {
                        t_androidLogBuffer->SetCapacity(4096);

                        thisThread->AddOnExitCallback(
                            []()
                            {
                                delete t_androidLogBuffer;
                                t_androidLogBuffer = nullptr;
                            });

                        t_useAndroidLogger = true;
                    }
                    else
                    {
                        // alloc failed!
                        t_useAndroidLogger = false;
                    }
                }
                else
                {
                    // no thread object!
                    t_useAndroidLogger = false;
                }
            }

            if (HYP_LIKELY(t_useAndroidLogger))
            {
                size_t offset = 0;
                for (auto it = message.chunks.Begin(); it != message.chunks.End(); ++it)
                {
                    if (t_androidLogBuffer->Size() < offset + it->Size())
                    {
                        t_androidLogBuffer->SetSize((offset + it->Size()) * 2);
                    }

                    Memory::Copy(t_androidLogBuffer->Data() + offset, it->Data(), it->Size());
                    offset += it->Size();
                }

                // ensure null-terminated
                if (t_androidLogBuffer->Size() == offset)
                {
                    t_androidLogBuffer->SetSize(offset + 1);
                }

                t_androidLogBuffer->Data()[offset] = '\0';

                __android_log_write(LogLevelToAndroidLogPriority[uint8(message.level)], "HyperionEngine", reinterpret_cast<const char*>(t_androidLogBuffer->Data()));
            }
        }
        else
#endif
        {
#if HYP_DEBUG_MODE
            if (isConsole)
            {
                Span<const char> colorCode = LogLevelTermColor(message.level);
                std::fwrite(colorCode.Data(), 1, colorCode.Size(), file);
            }
#endif

            for (auto it = message.chunks.Begin(); it != message.chunks.End(); ++it)
            {
                std::fwrite(**it, 1, it->Size(), file);
            }

#if HYP_DEBUG_MODE
            if (isConsole)
            {
                static constexpr Span<const char> ResetCode = "\033[0m";
                std::fwrite(ResetCode.Data(), 1, ResetCode.Size(), file);
            }
#endif
        }
    }
}

static void WriteError(const LogChannel& channel, const LogMessage& message)
{
    Write(channel, message);
}

static void Flush()
{
    if (s_loggerState.m_output)
    {
        std::fflush(s_loggerState.m_output);
    }

    if (s_loggerState.m_outputError)
    {
        std::fflush(s_loggerState.m_outputError);
    }
}

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

    TMap<LogChannel*, uint32> indeg;
    TMap<LogChannel*, Array<LogChannel*>> children;
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

    size_t head = 0;

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

    LoggerImpl()
        : m_logMask(-1)
    {
        Fill(m_logChannels.Begin(), m_logChannels.End(), nullptr);
    }

    ~LoggerImpl() = default;

private:
    AtomicVar<Logger::ChannelMask> m_logMask;
    FixedArray<LogChannel*, Logger::MaxChannels> m_logChannels;

    Array<LogChannel*> m_dynamicLogChannels;
    mutable Mutex m_dynamicLogChannelsMutex;
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
    : m_impl(MakePimpl<LoggerImpl>()),
      fatalErrorHook(nullptr)
{
}

Logger::~Logger() = default;

int Logger::AddRedirect(const Bitset& channelMask, void* context, LoggerWriteFnPtr writeFnptr, LoggerWriteFnPtr writeErrorFnptr)
{
    AssertDebug(writeFnptr != nullptr || writeErrorFnptr != nullptr, "At least one of writeFnptr or writeErrorFnptr must be non-null");

    Mutex::Guard guard(s_loggerState.m_mutex);

    const int id = ++s_loggerState.m_redirectIdCounter;

    s_loggerState.m_redirects.Emplace(id, LoggerRedirect { channelMask, context, writeFnptr, writeErrorFnptr });

    for (Bitset::BitIndex bitIndex : channelMask)
    {
        if (bitIndex >= Logger::MaxChannels)
        {
            break;
        }

        s_loggerState.m_contexts[bitIndex] = context;
        s_loggerState.m_writeFnptrTable[bitIndex] = writeFnptr;
        s_loggerState.m_writeErrorFnptrTable[bitIndex] = writeErrorFnptr;

        s_loggerState.m_redirectEnabledMask |= (1ull << bitIndex);
    }

    s_loggerState.m_generation.Increment(1, MemoryOrder::RELEASE);

    return id;
}

void Logger::RemoveRedirect(int id)
{
    Mutex::Guard guard(s_loggerState.m_mutex);

    auto it = s_loggerState.m_redirects.Find(id);

    if (it == s_loggerState.m_redirects.End())
    {
        return;
    }

    for (Bitset::BitIndex bitIndex : it->second.channelMask)
    {
        if (s_loggerState.m_contexts[bitIndex] != it->second.context)
        {
            continue;
        }

        s_loggerState.m_contexts[bitIndex] = nullptr;
        s_loggerState.m_writeFnptrTable[bitIndex] = nullptr;
        s_loggerState.m_writeErrorFnptrTable[bitIndex] = nullptr;

        s_loggerState.m_redirectEnabledMask &= ~(1ull << bitIndex);
    }

    s_loggerState.m_redirects.Erase(it);

    s_loggerState.m_generation.Increment(1, MemoryOrder::RELEASE);
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

    auto it = m_impl->m_dynamicLogChannels.FindIf(
        [name](LogChannel* channel)
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
        WriteError(channel, message);
    }
    else
    {
        Write(channel, message);
    }
}

void Logger::LogFatal(const LogChannel& channel, const LogMessage& message)
{
    Log(channel, message);

    // flush the output stream to ensure that the message is written before we call the fatal error hook
    Flush();

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

    const LogChannel* pChannel = &channel;

    if (channel.id == ~0u || channel.id >= Logger::MaxChannels)
    {
        pChannel = &g_logChannel_Misc;
    }

    if (!IsChannelEnabled(*pChannel))
    {
        return;
    }

    UTF8StringView sv[2] = { UTF8StringView(message), NewlineChunk };

    LogMessage lm {};
    lm.level = level;
    lm.timestamp = uint64(Time::Now());
    lm.chunks = sv;

    // don't handle fatal here; scripts shouldn't be able to cause fatal errors

    Log(*pChannel, lm);
}

#pragma endregion Logger

} // namespace logging

namespace logging {

#if HYP_DEBUG_MODE
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
