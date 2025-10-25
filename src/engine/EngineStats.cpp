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

namespace hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

static constexpr SizeType StatPoolBlockSize = 1024 * 1024; // 1 MB
static constexpr const char* RootStatGroupName = "Root";

static bool s_isInitializing = false;

static constexpr int NumReservedStatIds = 5;

static AtomicVar<int> s_nextStatId { NumReservedStatIds };

static constexpr int GameThreadBufferIndex = 0;
static constexpr int RenderThreadBufferIndex = 1;
static constexpr int NumPerThreadBuffers = 2;

static inline int GetBufferIndex()
{
    const ThreadId& currentThreadId = ThreadId::Current();

    if (currentThreadId == g_gameThread)
    {
        return GameThreadBufferIndex;
    }
    else if (currentThreadId == g_renderThread)
    {
        return RenderThreadBufferIndex;
    }
    else
    {
        AssertDebug(false);

        HYP_LOG(Engine, Warning, "EngineStatsRecorder accessed from unknown thread!");

        return GameThreadBufferIndex;
    }
}

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

#pragma region EngineStats

class EngineStats final
{
public:
    EngineStats();
    ~EngineStats();

    EngineStatBase* GetStat(UTF8StringView path) const;

    EngineStatGroup* root;
};

EngineStats::EngineStats()
    : root(nullptr)
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

        // If no separator was found, this is the last component
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
            // Reached the end of the path
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

#pragma endregion EngineStats

#pragma region EngineStatBase

EngineStatBase::EngineStatBase(EngineStatType type, UTF8StringView path, const ThreadId& ownerThreadId)
    : EngineStatBase(type, path, ownerThreadId, false)
{
}

EngineStatBase::EngineStatBase(EngineStatType type, UTF8StringView path, const ThreadId& ownerThreadId, bool skipPathParsing)
    : id(-1),
      type(type),
      ownerThreadId(ownerThreadId.IsValid() ? ownerThreadId : g_renderThread)
{
    if (skipPathParsing)
    {
        name = CreateNameFromDynamicString(path);
        return;
    }

    // Prevent recursive initialization during EngineStats construction
    if (s_isInitializing)
    {
        HYP_LOG(Engine, Warning, "Attempted to create stat '{}' during EngineStats initialization - deferring", path);
        name = CreateNameFromDynamicString(path);
        return;
    }

    // get / create statgroups as needed
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

        // If no separator was found, this is the last component (the stat name itself)
        if (!separatorFound)
        {
            statName = curr;
            break;
        }

        // check if stat group exists
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
            // create new stat group
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

    // Assign unique ID to this stat (not for groups)
    if (type != EST_GROUP)
    {
        id = s_nextStatId.Increment(1, MemoryOrder::RELAXED);
    }

    // Add this stat to the current group
    currentGroup->stats.PushBack(this);
}

#pragma endregion EngineStatBase

#pragma region EngineStatGroup

EngineStatGroup::~EngineStatGroup()
{
    for (EngineStatBase* stat : stats)
    {
        delete stat;
    }
}

#pragma endregion EngineStatGroup

#pragma region EngineStatsRecorderImpl

static FixedArray<TByteBuffer<Pool>, NumPerThreadBuffers> CreateStatsBuffers()
{
    ValueStorage<FixedArray<TByteBuffer<Pool>, NumPerThreadBuffers>> buffersStorage;

    for (uint32 i = 0; i < NumPerThreadBuffers; i++)
    {
        auto* buffer = new (buffersStorage.GetPointer()->Data() + i) TByteBuffer<Pool>(EngineStats_GetPool());
        buffer->SetSize(sizeof(double) * EngineStatsMaxStats * EngineStatsNumSamples);
    }

    return std::move(buffersStorage).Get();
}

struct EngineStatsRecorderImpl
{
    FixedArray<EngineStatsSnapshot, NumMultiBuffers> snapshots;
    FixedArray<TByteBuffer<Pool>, NumPerThreadBuffers> statsBuffers;
    GameCounter counter;
    double deltaAccum;
    uint32 numSamples;
    uint32 sampleIndex;

    EngineStatsRecorderImpl()
        : deltaAccum(0.0),
          numSamples(0),
          sampleIndex(0),
          statsBuffers(CreateStatsBuffers())
    {
        counter.delta = 1.0;
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
    return m_pImpl->snapshots[RenderApi::GetFrameIndex()];
}

const EngineStatsSnapshot& EngineStatsRecorder::GetCurrentSnapshot() const
{
    return m_pImpl->snapshots[RenderApi::GetFrameIndex()];
}

void EngineStatsRecorder::SetSampleData(int statId, uint32 sampleIdx, uint32 bufferIdx, double value)
{
    HYP_SCOPE;

    if (statId < 0 || statId >= int(EngineStatsMaxStats))
    {
        return;
    }

    TByteBuffer<Pool>& statsBuffer = m_pImpl->statsBuffers[bufferIdx];
    double* statSamples = reinterpret_cast<double*>(statsBuffer.Data()) + statId * EngineStatsNumSamples;

    statSamples[sampleIdx] = value;
}

double EngineStatsRecorder::GetSampleData(int statId, uint32 sampleIdx, uint32 bufferIdx) const
{
    HYP_SCOPE;

    if (statId < 0 || statId >= int(EngineStatsMaxStats))
    {
        return 0.0;
    }

    const TByteBuffer<Pool>& statsBuffer = m_pImpl->statsBuffers[bufferIdx];
    const double* statSamples = reinterpret_cast<const double*>(statsBuffer.Data()) + statId * EngineStatsNumSamples;

    return statSamples[sampleIdx];
}

void EngineStatsRecorder::RecordStat(int statId, EngineStatType type, double value)
{
    HYP_SCOPE;

    if (statId < 0 || statId >= int(EngineStatsMaxStats))
    {
        return;
    }

    const uint32 bufferIdx = GetBufferIndex();

    EngineStatsSnapshot& snapshot = m_pImpl->snapshots[RenderApi::GetFrameIndex()];

    EngineStatsSnapshotValue& snapshotValue = snapshot[statId];
    snapshotValue = {};
    snapshotValue.statId = statId;
    snapshotValue.type = type;
    snapshotValue.min = DBL_MAX;
    snapshotValue.max = -DBL_MAX;
    snapshotValue.avg = 0.0;
    snapshotValue.value = value;

    const uint32 sampleIdx = m_pImpl->sampleIndex % EngineStatsNumSamples;
    SetSampleData(statId, sampleIdx, bufferIdx, value);
}

double EngineStatsRecorder::CalculateFps(uint32 bufferIdx) const
{
    HYP_SCOPE;

    if (m_pImpl->numSamples < EngineStatsMinSamples)
    {
        return INFINITY;
    }

    const uint32 count = MathUtil::Min(m_pImpl->numSamples, EngineStatsNumSamples);

    double sum = 0.0;

    for (uint32 i = 0; i < count; i++)
    {
        sum += GetSampleData(StatIdMsPerFrame, i, bufferIdx) / 1000.0;
    }

    const double avgDelta = sum / double(count);
    return avgDelta > 0.0 ? 1.0 / avgDelta : INFINITY;
}

void EngineStatsRecorder::Prepare()
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    // set it all to zero
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

void EngineStatsRecorder::Advance()
{
    HYP_SCOPE;

    const uint32 bufferIdx = GetBufferIndex();

    m_pImpl->counter.NextTick();
    m_pImpl->deltaAccum += m_pImpl->counter.delta;

    const bool resetFrameStats = m_pImpl->counter.delta >= 1.0;
    const bool resetMinMax = resetFrameStats || m_pImpl->deltaAccum >= 1.0;

    // Reset frame stats if we have a significant delta between the last frame,
    // indicating we probably were paused (e.g in a breakpoint)
    if (resetFrameStats)
    {
        m_pImpl->counter = GameCounter();
        m_pImpl->counter.delta = 1.0;

        m_pImpl->numSamples = 0;
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

    snapshot[StatIdFps].value = CalculateFps(bufferIdx);

    for (int i = 0; i < NumReservedStatIds; ++i)
    {
        if (snapshot[i].type != EST_INVALID)
        {
            SetSampleData(i, m_pImpl->sampleIndex % EngineStatsNumSamples, bufferIdx, snapshot[i].value);
        }
    }

    // // Get deletion queue stats
    // if (g_safeDeleter)
    // {
    //     g_safeDeleter->GetCounterValues(
    //         snapshot.deletionQueueNumElements,
    //         snapshot.deletionQueueTotalBytes);
    // }

    // Calculate averages for all stats in this snapshot
    for (int statId = 0; statId < EngineStatsMaxStats; statId++)
    {
        EngineStatsSnapshotValue& snapshotValue = snapshot[statId];

        RecordStat(statId, snapshotValue.type, snapshotValue.value);

        if (m_pImpl->numSamples >= EngineStatsMinSamples)
        {
            const uint32 count = MathUtil::Min(m_pImpl->numSamples, EngineStatsNumSamples);
            double sum = 0.0;

            for (uint32 sampleIdx = 0; sampleIdx < count; sampleIdx++)
            {
                sum += GetSampleData(statId, sampleIdx, bufferIdx);
            }

            snapshotValue.avg = sum / double(count);
        }
        else
        {
            snapshotValue.avg = snapshotValue.value;
        }
    }
}

#pragma endregion EngineStatsRecorder

HYP_API void EngineStats_Initialize()
{
    (void)EngineStats_GetPool(); // ensure stat pool is initialized

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