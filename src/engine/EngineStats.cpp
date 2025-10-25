/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <engine/EngineStats.hpp>
#include <engine/EngineGlobals.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/math/MathUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <rendering/RenderGlobalState.hpp>

#include <cfloat>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

static constexpr SizeType StatPoolBlockSize = 1024 * 1024;
static constexpr const char* RootStatGroupName = "Root";
static constexpr int NumReservedStatIds = 5;

Pool* g_statPool = nullptr;
EngineStatsRecorder* g_engineStatsRecorder = nullptr;

HYP_API Pool* EngineStats_GetPool()
{
    static struct Initializer
    {
        Initializer()
        {
            if (g_statPool != nullptr)
            {
                return;
            }

            g_statPool = new Pool(StatPoolBlockSize, PF_THREAD_SAFE, ThreadId::Invalid());
        }
    } s_initializer;

    return g_statPool;
}

static inline EngineStatThreadType GetThreadType(const ThreadId& threadId)
{
    if (threadId == g_renderThread)
    {
        return ESTT_RENDER;
    }

    if (threadId == g_gameThread)
    {
        return ESTT_GAME;
    }

    AssertDebug(false, "Invalid thread to get ThreadType : {}", threadId.GetName());

    return ESTT_INVALID;
}

#pragma region EngineStats tree / registration

class EngineStats final
{
public:
    EngineStats();
    ~EngineStats();

    EngineStatBase* GetStat(UTF8StringView path) const;

    EngineStatGroup* root;
    FixedArray<EngineStatBase*, EngineStatsMaxStats> linearStats;
};

static bool s_isInitializing = false;
static int s_nextStatId = NumReservedStatIds;

EngineStats::EngineStats()
    : root(nullptr),
      linearStats { nullptr }
{
    s_isInitializing = true;
    root = new EngineStatGroup(UTF8StringView(RootStatGroupName), true);
    s_isInitializing = false;
}

EngineStats::~EngineStats()
{
    delete root;
}

static EngineStats& GetGlobalEngineStats()
{
    static EngineStats s_globalEngineStats;
    return s_globalEngineStats;
}

EngineStatBase::EngineStatBase(EngineStatType inType, UTF8StringView path, EngineStatThreadType threadType)
    : id(-1),
      name(),
      type(inType),
      threadType(threadType)
{
    // Parse path and register into the tree; also assign id.
    EngineStats& engineStats = GetGlobalEngineStats();

    EngineStatGroup* currentGroup = engineStats.root;
    Assert(currentGroup != nullptr);

    UTF8StringView remainingPath = path;
    UTF8StringView statName = path;

    while (remainingPath.Size() > 0)
    {
        UTF8StringView curr = remainingPath;
        SizeType characterIndex = 0;
        bool separatorFound = false;

        for (utf::u32char ch : remainingPath)
        {
            if (ch == utf::u32char('/'))
            {
                curr = remainingPath.Substr(0, characterIndex);
                remainingPath = remainingPath.Substr(characterIndex + 1, SizeType(-1));
                separatorFound = true;
                break;
            }

            ++characterIndex;
        }

        if (!separatorFound)
        {
            statName = curr;
            break;
        }

        EngineStatBase* foundStat = nullptr;

        for (EngineStatBase* stat : currentGroup->stats)
        {
            if (stat->name == WeakName(curr))
            {
                foundStat = stat;
                break;
            }
        }

        if (!foundStat)
        {
            EngineStatGroup* newGroup = new EngineStatGroup(curr, true);
            currentGroup->stats.PushBack(newGroup);
            currentGroup = newGroup;
        }
        else if (foundStat->type == EST_GROUP)
        {
            currentGroup = static_cast<EngineStatGroup*>(foundStat);
        }
        else
        {
            HYP_LOG(Engine, Warning, "Stat group '{}' not found; found non-group stat instead!", curr);
            break;
        }
    }

    name = CreateNameFromDynamicString(statName);

    if (type != EST_GROUP)
    {
        id = s_nextStatId++;
        engineStats.linearStats[id] = this;
    }

    currentGroup->stats.PushBack(this);
}

EngineStatBase::EngineStatBase(EngineStatType inType, UTF8StringView path, EngineStatThreadType threadType, bool /*skipPathParsing*/)
    : id(-1),
      name(CreateNameFromDynamicString(path)),
      type(inType),
      threadType(threadType)
{
    // Only used by EngineStatGroup internal constructor
}

EngineStatGroup::~EngineStatGroup()
{
    for (EngineStatBase* stat : stats)
    {
        delete stat;
    }
}

EngineStatBase* EngineStats::GetStat(UTF8StringView path) const
{
    static constexpr utf::u32char PathSeparator = utf::u32char('/');

    EngineStatBase* currentStat = root;

    while (currentStat != nullptr && path.Size() > 0)
    {
        UTF8StringView curr = path;
        SizeType characterIndex = 0;
        bool separatorFound = false;

        for (utf::u32char ch : path)
        {
            if (ch == PathSeparator)
            {
                curr = path.Substr(0, characterIndex);
                path = path.Substr(characterIndex + 1, SizeType(-1));
                separatorFound = true;
                break;
            }

            ++characterIndex;
        }

        if (!separatorFound)
        {
            path = UTF8StringView();
        }

        EngineStatGroup* currentGroup = currentStat->type == EST_GROUP
            ? static_cast<EngineStatGroup*>(currentStat)
            : nullptr;

        if (!currentGroup)
        {
            HYP_LOG(Engine, Warning, "Stat '{}' is not a group, cannot access child stat '{}'", currentStat->name, curr);
            return nullptr;
        }

        bool found = false;

        for (EngineStatBase* stat : currentGroup->stats)
        {
            if (stat->name == WeakName(curr))
            {
                currentStat = stat;
                found = true;
                break;
            }
        }

        if (!found)
        {
            HYP_LOG(Engine, Warning, "Stat '{}' not found in group '{}'", curr, currentGroup->name);
            return nullptr;
        }

        if (path.Size() == 0)
        {
            return currentStat;
        }
    }

    return nullptr;
}

#pragma endregion EngineStats tree / registration

#pragma region Buffering and sampling

class StatWriteBuffer
{
public:
    StatWriteBuffer()
    {
        for (int i = 0; i < NumMultiBuffers; ++i)
        {
            m_values[i] = 0.0;
            m_written[i] = false;
        }
    }

    HYP_FORCE_INLINE void Write(uint8 idx, double v)
    {
        m_values[idx] = v;
        m_written[idx] = true;
    }

    HYP_FORCE_INLINE bool TryRead(uint8 idx, double& out) const
    {
        if (!m_written[idx])
            return false;
        out = m_values[idx];
        return true;
    }

    HYP_FORCE_INLINE void ClearWritten(uint8 idx)
    {
        m_written[idx] = false;
    }

private:
    double m_values[NumMultiBuffers];
    bool m_written[NumMultiBuffers];
};

static constexpr int SamplesPerStat = EngineStatsNumSamples;

struct RenderSamples
{
    RenderSamples()
        : count(0),
          writeIndex(0)
    {
        for (int i = 0; i < SamplesPerStat; ++i)
        {
            data[i] = 0.0;
        }
    }

    void Push(double v)
    {
        data[writeIndex] = v;
        writeIndex = (writeIndex + 1) % SamplesPerStat;
        if (count < (uint32)SamplesPerStat)
        {
            ++count;
        }
    }

    double Avg() const
    {
        if (count == 0)
            return 0.0;
        double sum = 0.0;
        for (uint32 i = 0; i < count; ++i)
            sum += data[i];
        return sum / double(count);
    }

    double data[SamplesPerStat];
    uint32 count;
    uint32 writeIndex;
};

#pragma endregion Buffering and sampling

#pragma region EngineStatsRecorderImpl

static TByteBuffer<Pool> CreateSamplesBuffer()
{
    TByteBuffer<Pool> buf(EngineStats_GetPool());
    buf.SetSize(sizeof(double) * EngineStatsMaxStats * EngineStatsNumSamples);
    return buf;
}

struct EngineStatsRecorderImpl
{
    //// Render thread only stuff
    EngineStatsSnapshot snapshot;
    EngineStatsSnapshot prevSnapshot;

    // Rolling samples window
    RenderSamples samples[EngineStatsMaxStats];

    // Clock for ms/frame
    GameCounter msCounter;
    double deltaAccum = 0.0;
    ////

    // Per-stat write buffers (filled during Publish passes)
    // One thread may access elems at a given time.
    // E.g game thread writes to stat X while render thread only from stat Y.
    /// Each thread must call Publish() to make its writes visible to the recorder.
    StatWriteBuffer writeBuffers[EngineStatsMaxStats];

    uint32 numSamples = 0;
    uint32 sampleIndex = 0;

    bool suppressed = false;

    EngineStatsRecorderImpl()
    {
        for (int i = 0; i < EngineStatsMaxStats; ++i)
        {
            snapshot[i] = {};
            snapshot[i].statId = i;
            snapshot[i].type = EST_INVALID;
            snapshot[i].min = DBL_MAX;
            snapshot[i].max = -DBL_MAX;
        }
    }
};

#pragma endregion EngineStatsRecorderImpl

#pragma region EngineStatsRecorder

EngineStatsRecorder::EngineStatsRecorder()
    : m_pImpl(MakePimpl<EngineStatsRecorderImpl>())
{
}

EngineStatsSnapshot& EngineStatsRecorder::GetCurrentSnapshot()
{
    return m_pImpl->snapshot;
}

const EngineStatsSnapshot& EngineStatsRecorder::GetCurrentSnapshot() const
{
    return m_pImpl->snapshot;
}

void EngineStatsRecorder::Suppress()
{
    m_pImpl->suppressed = true;
}

void EngineStatsRecorder::Unsuppress()
{
    m_pImpl->suppressed = false;
}

void EngineStatsRecorder::CalculateFps(uint32 sampleIdx, struct EngineStatsSnapshotValue& out) const
{
    const auto& msWin = m_pImpl->samples[StatIdMsPerFrame];

    if (msWin.count == 0)
    {
        out.value = 0.0;
        out.min = 0.0;
        out.max = 0.0;
        out.avg = 0.0;
        return;
    }

    const double lastMs = msWin.data[sampleIdx];
    out.value = (lastMs > 0.0) ? (1000.0 / lastMs) : 0.0;

    double sumMs = 0.0;
    double minMs = DBL_MAX;  // fastest frame in window
    double maxMs = -DBL_MAX; // slowest frame in window
    uint32 used = 0;

    for (uint32 i = 0; i < msWin.count; ++i)
    {
        const uint32 idx = (sampleIdx + i) % EngineStatsNumSamples;
        const double ms = msWin.data[idx];

        if (ms <= 0.0)
        {
            continue;
        }

        sumMs += ms;
        minMs = MathUtil::Min(minMs, ms);
        maxMs = MathUtil::Max(maxMs, ms);
        ++used;
    }

    if (used == 0)
    {
        out.min = 0.0;
        out.max = 0.0;
        out.avg = 0.0;
        return;
    }

    const double avgMs = sumMs / double(used);

    out.avg = (avgMs > 0.0) ? (1000.0 / avgMs) : 0.0; // frames / totalSeconds
    out.min = (maxMs > 0.0) ? (1000.0 / maxMs) : 0.0; // lowest fps = 1 / largest ms
    out.max = (minMs > 0.0) ? (1000.0 / minMs) : 0.0; // highest fps = 1 / smallest ms
}

// Zero-cost at frame start; resets snapshot and carries prev for fallback
void EngineStatsRecorder::Prepare()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    m_pImpl->prevSnapshot = m_pImpl->snapshot;

    EngineStats& engineStats = GetGlobalEngineStats();

    for (int i = 0; i < EngineStatsMaxStats; ++i)
    {
        EngineStatBase* stat = engineStats.linearStats[i];

        auto& s = m_pImpl->snapshot[i];
        s = {};
        s.statId = i;
        s.type = stat ? stat->type : EST_INVALID;
        s.value = 0.0;
        s.min = DBL_MAX;
        s.max = -DBL_MAX;
        s.avg = 0.0;
    }
}

void EngineStatsRecorder::BeginGameStatsFrame()
{
}

void EngineStatsRecorder::PublishGameChannel()
{
    const EngineStatThreadType threadType = GetThreadType(ThreadId::Current());
    const uint8 frameIdx = uint8(RenderApi::GetFrameIndex());

    for (EngineStatBase* stat : GetGlobalEngineStats().linearStats)
    {
        if (!stat || stat->id < 0 || stat->id >= EngineStatsMaxStats)
            continue;
        if (stat->threadType != threadType)
            continue;

        const double v = stat->GetValue();
        m_pImpl->writeBuffers[stat->id].Write(frameIdx, v);

        stat->Reset();
    }
}

void EngineStatsRecorder::Advance()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    m_pImpl->msCounter.NextTick();
    m_pImpl->deltaAccum += m_pImpl->msCounter.delta;

    const bool resetFrameStats = m_pImpl->msCounter.delta >= 1.0;
    const bool resetMinMax = resetFrameStats || m_pImpl->deltaAccum >= 1.0;

    // reset frame stats if we have a signficant delta between the last frame,
    // indicating we probably were paused (e.g in a breakpoint)
    if (resetFrameStats)
    {
        m_pImpl->msCounter = GameCounter();
        m_pImpl->msCounter.delta = 1.0;

        m_pImpl->numSamples = 0;
        m_pImpl->sampleIndex = 0;
    }

    const uint8 frameIdx = uint8(RenderApi::GetFrameIndex());

    // Update ms/frame stat
    const double msPerFrame = m_pImpl->msCounter.delta * 1000.0;
    m_pImpl->writeBuffers[StatIdMsPerFrame].Write(frameIdx, msPerFrame);

    // Publish render-owned stats into buffers for this frame
    for (EngineStatBase* stat : GetGlobalEngineStats().linearStats)
    {
        if (!stat || stat->id < 0 || stat->id >= EngineStatsMaxStats)
            continue;
        if (stat->threadType != ESTT_RENDER)
            continue;

        const double v = stat->GetValue();
        m_pImpl->writeBuffers[stat->id].Write(frameIdx, v);
        stat->Reset();
    }

    // Populate snapshot from buffers (fallback to previous snapshot if not written)
    for (int statId = 0; statId < EngineStatsMaxStats; statId++)
    {
        double v = 0.0;

        auto& dst = m_pImpl->snapshot[statId];

        if (!m_pImpl->writeBuffers[statId].TryRead(frameIdx, v))
        {
            v = m_pImpl->prevSnapshot[statId].value;
        }

        m_pImpl->writeBuffers[statId].ClearWritten(frameIdx);

        dst.value = v;

        dst.min = MathUtil::Min(dst.min, v);
        dst.max = MathUtil::Max(dst.max, v);

        m_pImpl->samples[statId].Push(v);
        dst.avg = m_pImpl->samples[statId].Avg();
    }

    const uint32 fpsSampleIdx = m_pImpl->sampleIndex;

    m_pImpl->numSamples = MathUtil::Min<uint32>(m_pImpl->numSamples + 1, EngineStatsNumSamples);
    m_pImpl->sampleIndex = (m_pImpl->sampleIndex + 1) % EngineStatsNumSamples;

    // FPS derived from ms-per-frame rolling window
    {
        auto& f = m_pImpl->snapshot[StatIdFps];

        CalculateFps(fpsSampleIdx, f);
    }
}

#pragma endregion EngineStatsRecorder

#pragma region Init / Shutdown

HYP_API void EngineStats_Initialize()
{
    (void)EngineStats_GetPool();

    if (!g_engineStatsRecorder)
    {
        g_engineStatsRecorder = new EngineStatsRecorder();
    }
}

HYP_API void EngineStats_Shutdown()
{
    if (g_engineStatsRecorder != nullptr)
    {
        delete g_engineStatsRecorder;
        g_engineStatsRecorder = nullptr;
    }

    if (g_statPool != nullptr)
    {
        delete g_statPool;
        g_statPool = nullptr;
    }
}

#pragma endregion Init / Shutdown

} // namespace hyperion
