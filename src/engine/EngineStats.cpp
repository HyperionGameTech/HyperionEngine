/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <engine/EngineStats.hpp>
#include <engine/EngineGlobals.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/math/MathUtil.hpp>
#include <core/profiling/ProfileScope.hpp>

#include <core/threading/AtomicVar.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/util/SafeDeleter.hpp>

#include <cfloat>

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

static constexpr SizeType StatPoolBlockSize = 1024 * 1024;
static constexpr const char* RootStatGroupName = "Root";

static bool s_isInitializing = false;

static constexpr int NumReservedStatIds = 5;

static AtomicVar<int> s_nextStatId { NumReservedStatIds };

Pool* g_statPool;
EngineStatsRecorder* g_engineStatsRecorder;

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

#pragma region EngineStats tree

class EngineStats final
{
public:
    EngineStats();
    ~EngineStats();

    EngineStatBase* GetStat(UTF8StringView path) const;

    EngineStatGroup* root;
    FixedArray<EngineStatBase*, EngineStatsMaxStats> linearStats;
};

EngineStats::EngineStats()
    : root(nullptr),
      linearStats { }
{
    s_isInitializing = true;
    root = new EngineStatGroup(UTF8StringView(RootStatGroupName), true);
    s_isInitializing = false;
}

EngineStats::~EngineStats()
{
    delete root;
}

EngineStatBase* EngineStats::GetStat(UTF8StringView path) const
{
    HYP_SCOPE;

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

static EngineStats& GetGlobalEngineStats()
{
    static EngineStats s_globalEngineStats;
    return s_globalEngineStats;
}

EngineStatBase::EngineStatBase(EngineStatType type, UTF8StringView path, EngineStatThreadType threadType)
    : EngineStatBase(type, path, threadType, false)
{
}

EngineStatBase::EngineStatBase(EngineStatType type, UTF8StringView path, EngineStatThreadType threadType, bool skipPathParsing)
    : id(-1),
      type(type),
      threadType(threadType)
{
    if (skipPathParsing)
    {
        name = CreateNameFromDynamicString(path);
        return;
    }

    if (s_isInitializing)
    {
        HYP_LOG(Engine, Warning, "Attempted to create stat '{}' during EngineStats initialization - deferring", path);
        name = CreateNameFromDynamicString(path);
        return;
    }

    EngineStats& engineStats = GetGlobalEngineStats();

    EngineStatGroup* currentGroup = static_cast<EngineStatGroup*>(engineStats.root);
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
        id = s_nextStatId.Increment(1, MemoryOrder::RELAXED);
        engineStats.linearStats[id] = this;
    }

    currentGroup->stats.PushBack(this);
}

EngineStatGroup::~EngineStatGroup()
{
    for (EngineStatBase* stat : stats)
    {
        delete stat;
    }
}

#pragma endregion EngineStats tree

#pragma region Multi-buffer channels

struct StatAccumulator
{
    double last = 0.0;
    double min  = DBL_MAX;
    double max  = -DBL_MAX;
    double sum  = 0.0;
    uint32 count = 0;

    void Reset()
    {
        last = 0.0;
        min  = DBL_MAX;
        max  = -DBL_MAX;
        sum  = 0.0;
        count = 0;
    }

    void Push(double v)
    {
        last = v;
        min = MathUtil::Min(min, v);
        max = MathUtil::Max(max, v);
        sum += v;
        count += 1;
    }

    double Avg() const
    {
        return count ? (sum / double(count)) : last;
    }
};

struct StatBuffer
{
    StatAccumulator acc[EngineStatsMaxStats];

    void ResetAll()
    {
        for (int i = 0; i < EngineStatsMaxStats; ++i)
        {
            acc[i].Reset();
        }
    }
};

// One writer per channel, one reader (render). No atomics; relies on external semaphore ordering.
class EngineStatChannel
{
public:
    EngineStatChannel()
        : m_writeIdx(0),
          m_publishedIdx(255)
    {
        for (int i = 0; i < NumMultiBuffers; ++i)
        {
            m_buffers[i].ResetAll();
        }
    }

    void BeginFrame(uint8 idx)
    {
        m_writeIdx = idx;
        m_buffers[m_writeIdx].ResetAll();
    }

    void Record(int statId, double value)
    {
        if (HYP_UNLIKELY(statId < 0 || statId >= EngineStatsMaxStats))
        {
            return;
        }
        m_buffers[m_writeIdx].acc[statId].Push(value);
    }

    void Publish()
    {
        m_publishedIdx = m_writeIdx;
    }

    const StatBuffer* AcquirePublished() const
    {
        return (m_publishedIdx < NumMultiBuffers) ? &m_buffers[m_publishedIdx] : nullptr;
    }

    const StatBuffer* CurrentWriterBuffer() const
    {
        return &m_buffers[m_writeIdx];
    }

private:
    StatBuffer m_buffers[NumMultiBuffers];
    uint8 m_writeIdx;       // writer thread only
    uint8 m_publishedIdx;   // written by writer before signaling; read by reader after wait
};

struct MergeState
{
    double sum[EngineStatsMaxStats];
    uint32 count[EngineStatsMaxStats];

    void Reset()
    {
        for (int i = 0; i < EngineStatsMaxStats; ++i)
        {
            sum[i] = 0.0;
            count[i] = 0;
        }
    }
};

static void MergeFromBuffer(EngineStatsSnapshot& dst, const StatBuffer* buffer, MergeState& state)
{
    if (!buffer)
    {
        return;
    }

    for (int i = 0; i < EngineStatsMaxStats; ++i)
    {
        const StatAccumulator& a = buffer->acc[i];

        if (a.count == 0 && a.min == DBL_MAX && a.max == -DBL_MAX && a.last == 0.0)
        {
            continue;
        }

        EngineStatsSnapshotValue& s = dst[i];

        s.value = a.count ? a.last : s.value;
        s.min = MathUtil::Min(s.min, a.min);
        s.max = MathUtil::Max(s.max, a.max);

        state.sum[i]   += a.sum;
        state.count[i] += a.count;
    }
}

static void FinalizeMergedAverages(EngineStatsSnapshot& dst, const MergeState& state)
{
    for (int i = 0; i < EngineStatsMaxStats; ++i)
    {
        EngineStatsSnapshotValue& s = dst[i];

        if (state.count[i] != 0u)
        {
            s.avg = state.sum[i] / double(state.count[i]);
        }
        else
        {
            s.avg = s.value;
        }
    }
}

#pragma endregion Multi-buffer channels

#pragma region Recorder internals

static TByteBuffer<Pool> CreateSamplesBuffer()
{
    TByteBuffer<Pool> buf(EngineStats_GetPool());
    buf.SetSize(sizeof(double) * EngineStatsMaxStats * EngineStatsNumSamples);
    return buf;
}

struct EngineStatsRecorderImpl
{
    FixedArray<EngineStatsSnapshot, NumMultiBuffers> snapshots;

    // Single render-owned rolling window of samples
    TByteBuffer<Pool> statsBuffer;

    EngineStatChannel gameChannel;
    EngineStatChannel renderChannel;

    GameCounter counter;
    double deltaAccum;
    uint32 numSamples;
    uint32 sampleIndex;

    EngineStatsRecorderImpl()
        : statsBuffer(CreateSamplesBuffer()),
          counter(),
          deltaAccum(0.0),
          numSamples(0),
          sampleIndex(0)
    {
        counter.delta = 1.0;
    }
};

#pragma endregion Recorder internals

#pragma region EngineStatsRecorder

EngineStatsRecorder::EngineStatsRecorder()
    : m_pImpl(MakePimpl<EngineStatsRecorderImpl>())
{
}

EngineStatsSnapshot& EngineStatsRecorder::GetCurrentSnapshot()
{
    return m_pImpl->snapshots[RenderApi::GetFrameIndex()];
}

const EngineStatsSnapshot& EngineStatsRecorder::GetCurrentSnapshot() const
{
    return m_pImpl->snapshots[RenderApi::GetFrameIndex()];
}

void EngineStatsRecorder::SetSampleData(int statId, uint32 sampleIdx, double value)
{
    if (statId < 0 || statId >= int(EngineStatsMaxStats))
    {
        return;
    }

    double* base = reinterpret_cast<double*>(m_pImpl->statsBuffer.Data());
    base[statId * EngineStatsNumSamples + sampleIdx] = value;
}

double EngineStatsRecorder::GetSampleData(int statId, uint32 sampleIdx) const
{
    if (statId < 0 || statId >= int(EngineStatsMaxStats))
    {
        return 0.0;
    }

    const double* base = reinterpret_cast<const double*>(m_pImpl->statsBuffer.Data());
    return base[statId * EngineStatsNumSamples + sampleIdx];
}

double EngineStatsRecorder::CalculateFps() const
{
    if (m_pImpl->numSamples < EngineStatsMinSamples)
    {
        return INFINITY;
    }

    const uint32 count = MathUtil::Min(m_pImpl->numSamples, EngineStatsNumSamples);
    double sum = 0.0;

    for (uint32 i = 0; i < count; i++)
    {
        sum += GetSampleData(StatIdMsPerFrame, i) / 1000.0;
    }

    const double avgDelta = sum / double(count);
    return avgDelta > 0.0 ? 1.0 / avgDelta : INFINITY;
}

void EngineStatsRecorder::Prepare()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    m_pImpl->renderChannel.BeginFrame(uint8(RenderApi::GetFrameIndex()));

    EngineStatsSnapshot& snapshot = m_pImpl->snapshots[RenderApi::GetFrameIndex()];

    for (int i = 0; i < EngineStatsMaxStats; i++)
    {
        snapshot[i] = {};
        snapshot[i].statId = i;
        snapshot[i].type = EST_MAX;
        snapshot[i].value = 0.0;
        snapshot[i].min = DBL_MAX;
        snapshot[i].max = -DBL_MAX;
        snapshot[i].avg = 0.0;
    }
}

void EngineStatsRecorder::BeginGameStatsFrame()
{
    m_pImpl->gameChannel.BeginFrame(uint8(RenderApi::GetFrameIndex()));
}

void EngineStatsRecorder::PublishGameChannel()
{
    m_pImpl->gameChannel.Publish();
}

void EngineStatsRecorder::Advance()
{
    HYP_SCOPE;

    m_pImpl->counter.NextTick();
    m_pImpl->deltaAccum += m_pImpl->counter.delta;

    const bool resetFrameStats = (m_pImpl->counter.delta >= 1.0);
    const bool resetMinMax = resetFrameStats || (m_pImpl->deltaAccum >= 1.0);

    if (resetFrameStats)
    {
        m_pImpl->counter = GameCounter();
        m_pImpl->counter.delta = 1.0;

        m_pImpl->numSamples  = 0;
        m_pImpl->sampleIndex = 0;
    }

    EngineStatsSnapshot& snapshot = m_pImpl->snapshots[RenderApi::GetFrameIndex()];

    const double msPerFrame = m_pImpl->counter.delta * 1000.0;
    snapshot[StatIdMsPerFrame].value = msPerFrame;

    if (resetMinMax)
    {
        snapshot[StatIdMsPerFrame].max = msPerFrame;
        snapshot[StatIdMsPerFrame].min = msPerFrame;
        m_pImpl->deltaAccum = 0.0;
    }
    else
    {
        snapshot[StatIdMsPerFrame].max = MathUtil::Max(snapshot[StatIdMsPerFrame].max, msPerFrame);
        snapshot[StatIdMsPerFrame].min = MathUtil::Min(snapshot[StatIdMsPerFrame].min, msPerFrame);
    }

    // Seal render stats for this frame
    m_pImpl->renderChannel.Publish();

    // Merge game + render channel buffers into the snapshot
    const StatBuffer* gameBuffer = m_pImpl->gameChannel.AcquirePublished();
    const StatBuffer* renderBuffer = m_pImpl->renderChannel.CurrentWriterBuffer();

    MergeState mergeState;
    mergeState.Reset();

    MergeFromBuffer(snapshot, gameBuffer, mergeState);
    MergeFromBuffer(snapshot, renderBuffer, mergeState);
    FinalizeMergedAverages(snapshot, mergeState);

    const uint32 sampleIdx = m_pImpl->sampleIndex % EngineStatsNumSamples;

    SetSampleData(StatIdMsPerFrame, sampleIdx, snapshot[StatIdMsPerFrame].value);

    for (int statId = 0; statId < EngineStatsMaxStats; ++statId)
    {
        if (statId == StatIdMsPerFrame)
        {
            continue;
        }

        if (mergeState.count[statId] != 0u)
        {
            SetSampleData(statId, sampleIdx, snapshot[statId].value);
        }
        else
        {
            if (m_pImpl->numSamples == 0)
            {
                SetSampleData(statId, sampleIdx, snapshot[statId].value);
            }
            else
            {
                const uint32 prev = (sampleIdx + EngineStatsNumSamples - 1) % EngineStatsNumSamples;
                SetSampleData(statId, sampleIdx, GetSampleData(statId, prev));
            }
        }
    }

    // FPS derived from the rolling ms/frame window
    snapshot[StatIdFps].value = CalculateFps();
    snapshot[StatIdFps].avg = snapshot[StatIdFps].value;
    snapshot[StatIdFps].min = snapshot[StatIdFps].value;
    snapshot[StatIdFps].max = snapshot[StatIdFps].value;

    m_pImpl->numSamples = MathUtil::Min<uint32>(m_pImpl->numSamples + 1u, EngineStatsNumSamples);
    m_pImpl->sampleIndex++;
}

#pragma endregion EngineStatsRecorder

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

} // namespace hyperion
