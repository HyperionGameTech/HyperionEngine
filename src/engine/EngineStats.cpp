/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <engine/EngineStats.hpp>

#include <core/math/MathUtil.hpp>

#include <core/threading/AtomicVar.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/util/SafeDeleter.hpp>

#include <EngineStats.generated.inl>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Engine);

static constexpr SizeType StatPoolBlockSize = 1 << 18;
static constexpr const char* RootStatGroupName = "Root";

static constexpr int NumReservedStatIds = 5;

static AtomicVar<int> s_nextStatId { NumReservedStatIds };

Pool* g_statPool;

static TByteBuffer<Pool> CreateSamplesBuffer()
{
    TByteBuffer<Pool> buf(g_statPool);
    buf.SetSize(sizeof(double) * EngineStatsMaxStats * EngineStatsNumSamples);
    return buf;
}

struct EngineStatsRecorderImpl
{
    EngineStatsSnapshot snapshots[RingBufferDepth];

    TByteBuffer<Pool> statsBuffer;

    GameCounter counter;
    double deltaAccum;
    uint32 numSamples;
    uint32 sampleIndex;

    EngineStatsRecorderImpl()
        : snapshots(),
          statsBuffer(CreateSamplesBuffer()),
          deltaAccum(0.0),
          numSamples(0),
          sampleIndex(0)
    {
        counter.delta = 1.0;
    }
};

struct DeferredInitStat
{
    EngineStatBase* stat;
    String path;
};

static Array<DeferredInitStat>& GetDeferredInitStats()
{
    static Array<DeferredInitStat> s_deferredInitStats;
    return s_deferredInitStats;
}

static void InitStat(EngineStats* stats, EngineStatBase* stat, UTF8StringView path)
{
    AssertDebug(stat != nullptr);

    if (!stats)
    {
        DeferredInitStat& dis = GetDeferredInitStats().EmplaceBack();
        dis.stat = stat;
        dis.path = path;

        return;
    }

    EngineStats& engineStats = *stats;

    EngineStatGroup* currentGroup = static_cast<EngineStatGroup*>(engineStats.root);
    Assert(currentGroup != nullptr);

    UTF8StringView remainingPath = path;
    UTF8StringView statName = path;

    while (remainingPath.Size() > 0)
    {
        UTF8StringView curr = remainingPath;
        SizeType characterIndex = 0;
        bool separatorFound = false;

        for (utf::Char32 ch : remainingPath)
        {
            if (ch == utf::Char32('/'))
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
            if (stat->name == StringHash(curr))
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

    stat->name = CreateNameFromDynamicString(statName);

    if (stat->type != EST_GROUP)
    {
        stat->id = s_nextStatId.Increment(1, MemoryOrder::RELAXED);
        engineStats.linearStats[stat->id] = stat;
    }

    currentGroup->stats.PushBack(stat);
}

#pragma region EngineStats

extern Handle<EngineStats> g_engineStats;

const Handle<EngineStats>& EngineStats::GetInstance()
{
    return g_engineStats;
}

// Global Engine stats singleton

EngineStats::EngineStats()
    : root(nullptr),
      linearStats {}
{
    // init pool
    AssertDebug(g_statPool == nullptr);
    g_statPool = new Pool(StatPoolBlockSize, PF_THREAD_SAFE, ThreadId::Invalid());

    m_impl = MakePimpl<EngineStatsRecorderImpl>();

    root = new EngineStatGroup(UTF8StringView(RootStatGroupName), true);

    Array<DeferredInitStat>& deferredInitStats = GetDeferredInitStats();
    if (deferredInitStats.Any())
    {
        for (DeferredInitStat& dis : deferredInitStats)
        {
            InitStat(this, dis.stat, UTF8StringView(dis.path));
        }

        deferredInitStats.Clear();
    }
}

EngineStats::~EngineStats()
{
    delete root;

    // destroy pool
    delete g_statPool;
    g_statPool = nullptr;
}

EngineStatBase* EngineStats::GetStat(UTF8StringView path) const
{
    HYP_SCOPE;

    static constexpr utf::Char32 PathSeparator = utf::Char32('/');

    EngineStatBase* currentStat = root;

    while (currentStat != nullptr && path.Size() > 0)
    {
        UTF8StringView curr = path;
        SizeType characterIndex = 0;
        bool separatorFound = false;

        for (utf::Char32 ch : path)
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
            if (stat->name == StringHash(curr))
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

double EngineStats::GetFps() const
{
    const EngineStatsSnapshot& snapshot = GetCurrentSnapshot();
    return snapshot[StatIdFps].value;
}

double EngineStats::GetMsPerFrame() const
{
    const EngineStatsSnapshot& snapshot = GetCurrentSnapshot();
    return snapshot[StatIdMsPerFrame].value;
}

double EngineStats::QueryStatValue(UTF8StringView path, double valueIfNotFound) const
{
    EngineStatBase* stat = GetStat(path);

    if (!stat)
    {
        return valueIfNotFound;
    }

    return stat->GetValue();
}

EngineStatsSnapshot& EngineStats::GetCurrentSnapshot()
{
    AssertOnThread(g_renderThread | g_simThread);
    return m_impl->snapshots[GetRingIndex()];
}

const EngineStatsSnapshot& EngineStats::GetCurrentSnapshot() const
{
    AssertOnThread(g_renderThread | g_simThread);
    return m_impl->snapshots[GetRingIndex()];
}

void EngineStats::SetSampleData(int statId, uint32 sampleIdx, double value)
{
    if (statId < 0 || statId >= int(EngineStatsMaxStats))
    {
        return;
    }

    double* base = reinterpret_cast<double*>(m_impl->statsBuffer.Data());
    base[statId * EngineStatsNumSamples + sampleIdx] = value;
}

double EngineStats::GetSampleData(int statId, uint32 sampleIdx) const
{
    if (statId < 0 || statId >= int(EngineStatsMaxStats))
    {
        return 0.0;
    }

    const double* base = reinterpret_cast<const double*>(m_impl->statsBuffer.Data());
    return base[statId * EngineStatsNumSamples + sampleIdx];
}

double EngineStats::CalculateFps() const
{
    if (m_impl->numSamples < EngineStatsMinSamples)
    {
        return INFINITY;
    }

    const uint32 count = MathUtil::Min(m_impl->numSamples, EngineStatsNumSamples);
    double sum = 0.0;

    for (uint32 i = 0; i < count; i++)
    {
        const uint32 idx = (m_impl->sampleIndex + EngineStatsNumSamples - 1 - i) % EngineStatsNumSamples;

        sum += GetSampleData(StatIdMsPerFrame, idx) / 1000.0;
    }

    const double avgDelta = sum / double(count);
    return avgDelta > 0.0 ? 1.0 / avgDelta : INFINITY;
}

void EngineStats::Prepare()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 sampleIdx = m_impl->sampleIndex % EngineStatsNumSamples;

    // clear sample data for this sample index
    for (int statId = 0; statId < EngineStatsMaxStats; statId++)
    {
        SetSampleData(statId, sampleIdx, 0.0);
    }
}

void EngineStats::RecordValueSet(const EngineStatsValueSet& valueSet)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    const uint32 sampleIdx = m_impl->sampleIndex % EngineStatsNumSamples;

    for (int statId = 0; statId < EngineStatsMaxStats; ++statId)
    {
        const double value = valueSet.values[statId];

        if (value != 0.0)
        {
            // Add to existing sample value (accumulate values from multiple sources)
            const double currentValue = GetSampleData(statId, sampleIdx);
            SetSampleData(statId, sampleIdx, currentValue + value);
        }
    }
}

void EngineStats::Advance()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    m_impl->counter.NextTick();
    m_impl->deltaAccum += m_impl->counter.delta;

    const bool resetFrameStats = (m_impl->counter.delta >= 1.0);
    const bool resetMinMax = resetFrameStats || (m_impl->deltaAccum >= 1.0);

    if (resetFrameStats)
    {
        m_impl->counter = GameCounter();
        m_impl->counter.delta = 1.0;

        m_impl->numSamples = 0;
        m_impl->sampleIndex = 0;
    }

    if (resetMinMax)
    {
        m_impl->deltaAccum = 0.0;
    }

    EngineStatsSnapshot& snapshot = m_impl->snapshots[GetRingIndex()];

    const double msPerFrame = m_impl->counter.delta * 1000.0;

    const uint32 sampleIdx = m_impl->sampleIndex % EngineStatsNumSamples;
    const uint32 prevSampleIdx = (sampleIdx + EngineStatsNumSamples - 1) % EngineStatsNumSamples;

    SetSampleData(StatIdMsPerFrame, sampleIdx, msPerFrame);
    SetSampleData(StatIdFps, sampleIdx, m_impl->counter.delta > 0.0 ? (1.0 / (m_impl->counter.delta)) : 0.0);

    EngineStats& engineStats = *g_engineStats;

    // integrate values into sample data
    for (int statId = NumReservedStatIds; statId < EngineStatsMaxStats; ++statId)
    {
        EngineStatBase* stat = engineStats.linearStats[statId];

        if (!stat)
        {
            continue;
        }

        double value = stat->GetValue();
        stat->Reset();

        const double currValue = GetSampleData(statId, sampleIdx);
        SetSampleData(statId, sampleIdx, value + currValue);
    }

    const uint32 actualNumSamples = MathUtil::Min(m_impl->numSamples + 1u, EngineStatsNumSamples);

    // Update snapshot values
    for (int statId = 0; statId < EngineStatsMaxStats; ++statId)
    {
        EngineStatsSnapshotValue& statSnapshot = snapshot.values[statId];

        if (statId >= NumReservedStatIds)
        {
            EngineStatBase* stat = engineStats.linearStats[statId];
            if (!stat)
            {
                continue;
            }

            statSnapshot.type = stat->type;
        }
        else
        {
            statSnapshot.type = EST_COUNTER;
        }

        // Calculate min, max, avg from samples
        double sum = 0.0;
        double minVal = DBL_MAX;
        double maxVal = -DBL_MAX;

        for (uint32 i = 0; i < actualNumSamples; ++i)
        {
            const uint32 idx = (sampleIdx + EngineStatsNumSamples - i) % EngineStatsNumSamples;

            double sampleValue = GetSampleData(statId, idx);
            sum += sampleValue;

            minVal = MathUtil::Min(minVal, sampleValue);
            maxVal = MathUtil::Max(maxVal, sampleValue);
        }

        const double currentValue = GetSampleData(statId, sampleIdx);

        // If we don't have enough samples yet, use current value for min/max
        if (m_impl->numSamples == 0)
        {
            minVal = currentValue;
            maxVal = currentValue;
        }

        statSnapshot.value = currentValue;
        statSnapshot.min = minVal;
        statSnapshot.max = maxVal;
        statSnapshot.avg = actualNumSamples > 0 ? (sum / double(actualNumSamples)) : 0.0;
    }

    m_impl->numSamples = MathUtil::Min<uint32>(m_impl->numSamples + 1u, EngineStatsNumSamples);
    m_impl->sampleIndex = (m_impl->sampleIndex + 1) % EngineStatsNumSamples;
}

#pragma endregion EngineStats

#pragma region EngineStatBase

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

    InitStat(g_engineStats.Get(), this, path);
}

EngineStatGroup::~EngineStatGroup()
{
    for (EngineStatBase* stat : stats)
    {
        delete stat;
    }
}

#pragma endregion EngineStatBase

} // namespace Hyperion
