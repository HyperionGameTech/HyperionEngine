/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/Bricks/GlimmerBrickWorld.hpp>
#include <Rendering/Glimmer/Bricks/GlimmerBrickCVars.hpp>
#include <Rendering/Glimmer/GlimmerTechnique.hpp>
#include <Rendering/Glimmer/GlimmerPass.hpp>
#include <Rendering/Glimmer/GlimmerSceneInstances.hpp>
#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/Framebuffer.hpp>
#include <Rendering/GBuffer.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/GpuImage.hpp>
#include <Rendering/GpuImageView.hpp>
#include <Rendering/PlaceholderData.hpp>
#include <Rendering/ShaderManager.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/ShaderPropertyDictionary.hpp>

#include <Scene/Camera/Camera.hpp>

#include <Framework/EngineStats.hpp>
#include <Framework/View.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

#include <algorithm>
#include <iterator>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static EngineStatGpuTimer s_statGlimmerBrickBuild("Rendering/GPU/Glimmer/BrickBuild");
static EngineStatGpuTimer s_statGlimmerBrickLight("Rendering/GPU/Glimmer/BrickLight");
static EngineStatGpuTimer s_statGlimmerBrickDebug("Rendering/GPU/Glimmer/BrickDebug");
static EngineStatGpuTimer s_statGlimmerBrickPassClear("Rendering/GPU/Glimmer/BrickPasses/Clear");
static EngineStatGpuTimer s_statGlimmerBrickPassBin("Rendering/GPU/Glimmer/BrickPasses/Bin");
static EngineStatGpuTimer s_statGlimmerBrickPassAlloc("Rendering/GPU/Glimmer/BrickPasses/Alloc");
static EngineStatGpuTimer s_statGlimmerBrickPassCount("Rendering/GPU/Glimmer/BrickPasses/Count");
static EngineStatGpuTimer s_statGlimmerBrickPassPrefix("Rendering/GPU/Glimmer/BrickPasses/Prefix");
static EngineStatGpuTimer s_statGlimmerBrickPassGather("Rendering/GPU/Glimmer/BrickPasses/Gather");
static EngineStatGpuTimer s_statGlimmerBrickPassTiles("Rendering/GPU/Glimmer/BrickPasses/Tiles");
static EngineStatGpuTimer s_statGlimmerBrickPassLight("Rendering/GPU/Glimmer/BrickPasses/Light");

// in BuildPass order
static StaticShaderPropertyId s_propBrickModes[] = {
    ShaderProperty(NAME("MODE"), NAME("INIT")),
    ShaderProperty(NAME("MODE"), NAME("BEGIN")),
    ShaderProperty(NAME("MODE"), NAME("CLEAR")),
    ShaderProperty(NAME("MODE"), NAME("BIN")),
    ShaderProperty(NAME("MODE"), NAME("ALLOC")),
    ShaderProperty(NAME("MODE"), NAME("ARGS")),
    ShaderProperty(NAME("MODE"), NAME("COUNT")),
    ShaderProperty(NAME("MODE"), NAME("PREFIX")),
    ShaderProperty(NAME("MODE"), NAME("GATHER")),
    ShaderProperty(NAME("MODE"), NAME("TILES")),
    ShaderProperty(NAME("MODE"), NAME("LIVE")),
    ShaderProperty(NAME("MODE"), NAME("GROW"))
};

static StaticShaderPropertyId s_propBrickLightModeLight { ShaderProperty(NAME("MODE"), NAME("LIGHT")) };
static StaticShaderPropertyId s_propBrickLightModeMips { ShaderProperty(NAME("MODE"), NAME("MIPS")) };

static constexpr uint32 BrickGroupSize = 64;
static constexpr uint32 BrickTableSize = GlimmerBrickGridXZ * GlimmerBrickGridY * GlimmerBrickGridXZ * 3;
static constexpr uint32 BrickListNodeCapacity = 1u << 20;
static constexpr uint32 BrickListNodeWords = 3;

static constexpr uint32 PoolBricks = 0;
static constexpr uint32 PoolPages = 1;
static constexpr uint32 PoolCards = 2;

static constexpr uint32 InitialPoolBricks = 4096;
static constexpr uint32 InitialPoolPages = 16384;
static constexpr uint32 InitialPoolCards = 16384;

static constexpr double IdealPagesPerBrick = 3.0;
static constexpr double IdealCardsPerBrick = 2.5;

static constexpr uint32 RelightBricksPerFrame = 128;
static constexpr uint32 RelightTrickleBricks = 16;
static constexpr float RelightBurstChange = 0.05f;

static_assert(InitialPoolBricks <= GlimmerBrickMaxPoolBricks && InitialPoolPages <= GlimmerBrickMaxPoolPages && InitialPoolCards <= GlimmerBrickMaxPoolCards);

// NOTE must match GlimmerBricks.hlsli
static constexpr uint32 BrickNumCounters = 16;
static constexpr uint32 BrickCounterBatchDroppedNodes = 15;
static constexpr uint32 BrickSlotCounters = 8;
static constexpr uint32 BrickSlotFailedBricks = 0;
static constexpr uint32 BrickSlotFailedPages = 1;
static constexpr uint32 BrickSlotFailedCards = 2;
static constexpr uint32 BrickSlotBricks = 3;
static constexpr uint32 BrickSlotPages = 4;
static constexpr uint32 BrickSlotCards = 5;
static constexpr uint32 BrickColumnsPerBrick = GlimmerBrickSide * GlimmerBrickSide;
static constexpr uint32 BrickChunkBricks = GlimmerBrickChunkCells * GlimmerBrickChunkCells * GlimmerBrickChunkCells * 3;
// the counters, then each slot's counters, each slot's list heads, then the batch's build list of two words a brick (NOTE must match GlimmerBricks.hlsli)
static constexpr uint32 BrickCountersWords = BrickNumCounters + GlimmerBrickMaxChunkBuildsPerFrame * (BrickSlotCounters + BrickChunkBricks * 3);
static constexpr uint32 BrickReadbackWords = BrickNumCounters + GlimmerBrickMaxChunkBuildsPerFrame * BrickSlotCounters;

// NOTE must match GlimmerBrickBuild.hlsl
static constexpr uint32 BrickArgsColumns = 0;
static constexpr uint32 BrickArgsTiles = 3;
static constexpr uint32 BrickArgsPrefix = 6;
static constexpr uint32 BrickArgsMips = 9;
static constexpr uint32 BrickArgsWords = BrickArgsMips + 3 * (GlimmerBrickMips - GlimmerBrickCardFirstMip);

static constexpr int32 WindowRecenterChunks = 2;
static constexpr uint32 MaxEmptyChunkClearsPerFrame = 32;
static constexpr uint32 PoolLogIntervalFrames = 300;
static constexpr uint32 MinPoolIncrease = 64;
static constexpr uint32 LightViewFrames = 2;
static constexpr int32 ChunkCells = int32(GlimmerBrickChunkCells);
static constexpr float ChunkSize = GlimmerBrickCellSize * float(GlimmerBrickChunkCells);
static constexpr float ChangeMargin = 0.25f;
static constexpr float ShadowBiasColumns = 0.5f;
static constexpr float ShadowDistance = 500.0f;

static const Vec3i ChunkDims = Vec3i(int32(GlimmerBrickChunksXZ), int32(GlimmerBrickChunksY), int32(GlimmerBrickChunksXZ));

struct GlimmerBrickBuildConstants
{
    GlimmerBrickShaderData bricks;
    Vec4i boxMin;
    Vec4i boxExtent;
    Vec4u counts;  // x = span chunks, y = groups along x, z = pool bricks, w = list node capacity
    Vec4u options; // x = 1 to sample the textures, y = the LIVE step, z = chunks in the batch, w = 1 to give bricks cards
    Vec4i chunkMins[GlimmerBrickMaxChunkBuildsPerFrame];
};

struct GlimmerBrickLightConstants
{
    GlimmerBrickShaderData bricks;
    Vec4u range;  // x = first place in the live list, y = bricks, z = 1 when they are the build list's, w = mip level
    Vec4f params; // x = columns off its surface a shadow ray starts, y = how far a shadow ray looks
};

struct GlimmerBrickDebugConstants
{
    Vec4u dimensionsMode;
    Vec4f params;
    Vec4u info;
    GlimmerBrickShaderData bricks;
};

#pragma region GlimmerBrickWorld

Vec3u GlimmerBrickWorld::s_largestPools = Vec3u::Zero();

const GlimmerBrickWorld::PoolBuffer GlimmerBrickWorld::s_poolBuffers[] = {
    { &GlimmerBrickWorld::m_indexBuffer, PoolBricks, GlimmerBrickIndexStride, false, NAME("GlimmerBrickIndex") },
    { &GlimmerBrickWorld::m_freeListBuffer, PoolBricks, 1, false, NAME("GlimmerBrickFreeList") },
    { &GlimmerBrickWorld::m_liveListBuffer, PoolBricks, 1, false, NAME("GlimmerBrickLiveList") },
    { &GlimmerBrickWorld::m_crossingsBuffer, PoolPages, GlimmerBrickPageCrossings * 2, false, NAME("GlimmerBrickCrossings") },
    { &GlimmerBrickWorld::m_materialBuffer, PoolPages, GlimmerBrickPageCrossings, false, NAME("GlimmerBrickMaterial") },
    { &GlimmerBrickWorld::m_pageFreeListBuffer, PoolPages, 1, false, NAME("GlimmerBrickPageFreeList") },
    { &GlimmerBrickWorld::m_crossingLightBuffer, PoolPages, GlimmerBrickPageCrossings, true, NAME("GlimmerBrickCrossingLight") },
    { &GlimmerBrickWorld::m_cardFreeListBuffer, PoolCards, 1, false, NAME("GlimmerBrickCardFreeList") },
    { &GlimmerBrickWorld::m_radianceBuffer, PoolCards, GlimmerBrickCardTexels, true, NAME("GlimmerBrickRadiance") }
};

GlimmerBrickWorld::GlimmerBrickWorld()
    : m_framesSinceLightView(LightViewFrames),
      m_isTextured(g_cvGlimmerBricksTextured.Get())
{
    const float columnWidth = GlimmerBrickCellSize / float(GlimmerBrickSide);

    m_shaderData.params = Vec4f(GlimmerBrickCellSize, 1.0f / GlimmerBrickCellSize, columnWidth, 1.0f / columnWidth);
}

GlimmerBrickWorld::~GlimmerBrickWorld()
{
    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        (this->*poolBuffer.buffer).Shutdown();
    }

    m_tableBuffer.Shutdown();
    m_countersBuffer.Shutdown();
    m_listNodesBuffer.Shutdown();
    m_dispatchArgsBuffer.Shutdown();
    m_placeholderBuffer.Shutdown();

    for (GpuBufferRef& readbackBuffer : m_readbackBuffers)
    {
        EnqueueDeletion(std::move(readbackBuffer));
    }
}

uint32 GlimmerBrickWorld::ChunkSlot(const Vec3i& coord)
{
    const uint32 x = uint32(coord.x) & (GlimmerBrickChunksXZ - 1);
    const uint32 y = uint32(coord.y) & (GlimmerBrickChunksY - 1);
    const uint32 z = uint32(coord.z) & (GlimmerBrickChunksXZ - 1);

    return (z * GlimmerBrickChunksY + y) * GlimmerBrickChunksXZ + x;
}

static RWStructuredBuffer CreateBrickBuffer(size_t numWords, Name name)
{
    RWStructuredBuffer buffer = GlimmerHelpers::CreateGlimmerRWBuffer(sizeof(uint32), numWords);

#ifdef HYP_RHI_DEBUG_NAMES
    buffer.gpuBuffer->SetDebugName(name);
#endif

    return buffer;
}

// how far the sun and sky have moved from one to the other, as a share of all of the other
static float LightChange(const GlimmerLightShaderData& from, const GlimmerLightShaderData& to)
{
    float change = 0.0f;
    float total = 0.0f;

    const auto addTerm = [&](const Vec4f& fromWeight, const Vec4f& toWeight)
    {
        for (int channel = 0; channel < 3; channel++)
        {
            change += MathUtil::Abs(toWeight[channel] - fromWeight[channel]);
            total += MathUtil::Abs(toWeight[channel]);
        }
    };

    for (uint32 term = 0; term < GlimmerLightTerms; term++)
    {
        addTerm(from.bounceWeights[term], to.bounceWeights[term]);
        addTerm(from.directWeights[term], to.directWeights[term]);
    }

    return total > 1e-6f ? change / total : (change > 0.0f ? 1.0f : 0.0f);
}

// a crossing's light and a card's texels are only held while lit
uint64 GlimmerBrickWorld::PoolBytes(const Vec3u& pools, bool isLit)
{
    uint64 numWords = 0;

    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        numWords += isLit || !poolBuffer.isLitOnly ? uint64(pools[poolBuffer.pool]) * poolBuffer.wordsPerItem : 0;
    }

    return numWords * sizeof(uint32);
}

Vec3u GlimmerBrickWorld::GetInitialPools() const
{
    const Vec3u maxima = Vec3u(GlimmerBrickMaxPoolBricks, GlimmerBrickMaxPoolPages, GlimmerBrickMaxPoolCards);
    const uint64 budgetBytes = uint64(MathUtil::Max(g_cvGlimmerBricksPoolBudgetMB.Get(), 0)) * 1024ull * 1024ull;
    const uint64 initialBytes = MathUtil::Min(uint64(MathUtil::Max(g_cvGlimmerBricksInitialPoolMB.Get(), 0)) * 1024ull * 1024ull, budgetBytes);

    Vec3u pools = Vec3u(InitialPoolBricks, InitialPoolPages, InitialPoolCards);

    if (initialBytes != 0)
    {
        const auto getIdeal = [&](uint32 bricks)
        {
            return Vec3u(
                bricks,
                uint32(MathUtil::Min(std::ceil(IdealPagesPerBrick * double(bricks)), double(maxima.y))),
                uint32(MathUtil::Min(std::ceil(IdealCardsPerBrick * double(bricks)), double(maxima.z))));
        };

        uint32 low = 0;
        uint32 high = maxima.x;

        while (low < high)
        {
            const uint32 middle = low + (high - low + 1) / 2;

            if (PoolBytes(getIdeal(middle), true) <= initialBytes)
            {
                low = middle;
            }
            else
            {
                high = middle - 1;
            }
        }

        const Vec3u sized = getIdeal(low);

        for (int pool = 0; pool < 3; pool++)
        {
            pools[pool] = MathUtil::Max(pools[pool], sized[pool]);
        }
    }

    for (int pool = 0; pool < 3; pool++)
    {
        pools[pool] = MathUtil::Min(MathUtil::Max(pools[pool], s_largestPools[pool]), maxima[pool]);
    }

    return pools;
}

void GlimmerBrickWorld::CreateResources()
{
    m_tableBuffer = CreateBrickBuffer(BrickTableSize, NAME("GlimmerBrickTable"));
    m_countersBuffer = CreateBrickBuffer(BrickCountersWords, NAME("GlimmerBrickCounters"));
    m_listNodesBuffer = CreateBrickBuffer(size_t(BrickListNodeCapacity) * BrickListNodeWords, NAME("GlimmerBrickListNodes"));
    m_placeholderBuffer = CreateBrickBuffer(4, NAME("GlimmerBrickPlaceholder"));

    m_dispatchArgsBuffer = IndirectArgsBuffer(BrickArgsWords, sizeof(uint32), RawBufferFlags::GpuOnly);
    m_dispatchArgsBuffer.Initialize();

    GlimmerHelpers::ClearGlimmerBuffer(m_dispatchArgsBuffer);

#ifdef HYP_RHI_DEBUG_NAMES
    m_dispatchArgsBuffer.gpuBuffer->SetDebugName(NAME("GlimmerBrickDispatchArgs"));
#endif

    m_pools = GetInitialPools();

    CreatePoolBuffers(false);

    if (m_isLit)
    {
        CreatePoolBuffers(true);
    }
}

void GlimmerBrickWorld::CreatePoolBuffers(bool isLitOnly)
{
    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        if (poolBuffer.isLitOnly == isLitOnly)
        {
            this->*poolBuffer.buffer = CreateBrickBuffer(size_t(m_pools[poolBuffer.pool]) * poolBuffer.wordsPerItem, poolBuffer.name);
        }
    }
}

void GlimmerBrickWorld::ReleaseLitResources()
{
    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        if (poolBuffer.isLitOnly)
        {
            (this->*poolBuffer.buffer).Shutdown();
        }
    }
}

void GlimmerBrickWorld::GrowPoolResources(Frame* frame, const Vec3u& grown)
{
    CommandRecorder& cr = frame->cr;

    const Vec3u old = m_pools;

    for (const PoolBuffer& poolBuffer : s_poolBuffers)
    {
        if (grown[poolBuffer.pool] == old[poolBuffer.pool] || (poolBuffer.isLitOnly && !m_isLit))
        {
            continue;
        }

        RWStructuredBuffer& buffer = this->*poolBuffer.buffer;
        RWStructuredBuffer grownBuffer = CreateBrickBuffer(size_t(grown[poolBuffer.pool]) * poolBuffer.wordsPerItem, poolBuffer.name);

        cr << InsertBarrier(buffer, ResourceState::CopySrc, ShaderModuleType::Compute);
        cr << InsertBarrier(grownBuffer, ResourceState::CopyDst, ShaderModuleType::Compute);
        cr << CopyBuffer(buffer.gpuBuffer, grownBuffer.gpuBuffer, uint32(size_t(old[poolBuffer.pool]) * poolBuffer.wordsPerItem * sizeof(uint32)));

        // the old one is deleted once the frames using it are done
        buffer = std::move(grownBuffer);
    }

    m_pools = grown;

    for (int pool = 0; pool < 3; pool++)
    {
        s_largestPools[pool] = MathUtil::Max(s_largestPools[pool], grown[pool]);
    }

    const uint32 numAdded = MathUtil::Max(grown.x - old.x, MathUtil::Max(grown.y - old.y, grown.z - old.z));

    Dispatch(frame, BuildPass::Grow, Box { Vec3i(int32(old.x), int32(old.y), int32(old.z)), Vec3i::Zero() }, numAdded);
}

bool GlimmerBrickWorld::GrowPools(const Vec3u& needed)
{
    const Vec3u current = m_pools;
    const Vec3u maxima = Vec3u(GlimmerBrickMaxPoolBricks, GlimmerBrickMaxPoolPages, GlimmerBrickMaxPoolCards);

    const uint64 budgetBytes = uint64(MathUtil::Max(g_cvGlimmerBricksPoolBudgetMB.Get(), 0)) * 1024ull * 1024ull;

    // no pool grows past its share of the most bricks the budget holds, with pages and cards in the proportion the chunks built in full use them
    Vec3u caps = maxima;

    uint64 heldBricks = 0;
    uint64 heldPages = 0;
    uint64 heldCards = 0;

    for (const Chunk& chunk : m_chunks)
    {
        if (chunk.hasBricks && !chunk.isShort)
        {
            heldBricks += chunk.numBricks;
            heldPages += chunk.numPages;
            heldCards += chunk.numCards;
        }
    }

    if (heldBricks != 0)
    {
        const double pagesPerBrick = double(heldPages) / double(heldBricks);
        const double cardsPerBrick = double(heldCards) / double(heldBricks);

        const auto balancedAt = [&](uint32 bricks)
        {
            return Vec3u(
                bricks,
                uint32(MathUtil::Min(std::ceil(pagesPerBrick * double(bricks)), double(maxima.y))),
                uint32(MathUtil::Min(std::ceil(cardsPerBrick * double(bricks)), double(maxima.z))));
        };

        uint32 low = 0;
        uint32 high = maxima.x;

        while (low < high)
        {
            const uint32 middle = low + (high - low + 1) / 2;
            const Vec3u balanced = balancedAt(middle);

            if (PoolBytes(balanced, m_isLit) <= budgetBytes)
            {
                low = middle;
            }
            else
            {
                high = middle - 1;
            }
        }

        const Vec3u balanced = balancedAt(low);

        for (int pool = 0; pool < 3; pool++)
        {
            caps[pool] = MathUtil::Max(balanced[pool], current[pool]);
        }
    }

    Vec3u grown = m_grownPools == Vec3u::Zero() ? current : m_grownPools;

    for (int pool = 0; pool < 3; pool++)
    {
        if (needed[pool] == 0 || grown[pool] != current[pool])
        {
            continue;
        }

        // twice the size, or what the batch fell short by and a quarter more if that's bigger, or as much more as the budget leaves room for
        const uint64 wanted = MathUtil::Max(uint64(current[pool]) * 2, uint64(current[pool]) + uint64(needed[pool]) + uint64(needed[pool]) / 4);
        uint32 increase = uint32(MathUtil::Min(wanted, uint64(caps[pool]))) - current[pool];

        for (; increase >= MinPoolIncrease; increase /= 2)
        {
            Vec3u candidate = grown;
            candidate[pool] = current[pool] + increase;

            if (PoolBytes(candidate, m_isLit) <= budgetBytes)
            {
                grown = candidate;

                break;
            }
        }
    }

    if (grown == current)
    {
        return m_grownPools != Vec3u::Zero();
    }

    if (grown != m_grownPools)
    {
        HYP_LOG(Rendering, Info, "Glimmer: brick pools ran short; growing to {} bricks, {} pages and {} cards ({} MB)",
            grown.x, grown.y, grown.z, PoolBytes(grown, m_isLit) / (1024ull * 1024ull));
    }

    m_grownPools = grown;

    return true;
}

void GlimmerBrickWorld::InsertBarriers(Frame* frame, bool isForWriting)
{
    CommandRecorder& cr = frame->cr;

    const ResourceState state = isForWriting ? ResourceState::UnorderedAccess : ResourceState::ShaderResource;

    cr << InsertBarrier(m_tableBuffer, state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_indexBuffer, state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_crossingsBuffer, state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_materialBuffer, state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_countersBuffer, state, ShaderModuleType::Compute);
    cr << InsertBarrier(m_liveListBuffer, state, ShaderModuleType::Compute);

    if (isForWriting)
    {
        cr << InsertBarrier(m_freeListBuffer, state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_pageFreeListBuffer, state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_cardFreeListBuffer, state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_listNodesBuffer, state, ShaderModuleType::Compute);
        cr << InsertBarrier(m_placeholderBuffer, state, ShaderModuleType::Compute);
    }
}

void GlimmerBrickWorld::Dispatch(Frame* frame, BuildPass pass, const Box& box, uint32 numThreads, uint32 step)
{
    const Vec3u groups = RenderHelpers::WrapComputeGroupCount((numThreads + BrickGroupSize - 1) / BrickGroupSize);

    SetBuildPass(frame, pass, box, groups.x, step);

    frame->cr << DispatchCompute(groups);
}

void GlimmerBrickWorld::DispatchIndirect(Frame* frame, BuildPass pass, uint32 argsOffset)
{
    SetBuildPass(frame, pass, Box {}, 0, 0);

    frame->cr << DispatchComputeIndirect(m_dispatchArgsBuffer, argsOffset * sizeof(uint32));
}

uint32 GlimmerBrickWorld::BindBuffers(CommandRecorder& cr, uint32 uniformIndex, EnumFlags<GlimmerBrickBindings> bindings) const
{
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickTableBuffer"_sh, m_tableBuffer);
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickIndexBuffer"_sh, m_indexBuffer);
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingsBuffer"_sh, m_crossingsBuffer);
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickMaterialBuffer"_sh, m_materialBuffer);

    if (bindings & GlimmerBrickBindings::Counters)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCountersBuffer"_sh, m_countersBuffer);
    }

    if (bindings & GlimmerBrickBindings::Light)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickRadianceBuffer"_sh, (m_isLit ? m_radianceBuffer : m_placeholderBuffer));
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCrossingLightBuffer"_sh, (m_isLit ? m_crossingLightBuffer : m_placeholderBuffer));
    }

    return uniformIndex;
}

void GlimmerBrickWorld::SetBuildPass(Frame* frame, BuildPass pass, const Box& box, uint32 groupsX, uint32 step)
{
    static_assert(std::size(s_propBrickModes) == uint32(BuildPass::Max));

    CommandRecorder& cr = frame->cr;

    GlimmerBrickBuildConstants constants {};
    constants.bricks = m_shaderData;
    constants.boxMin = Vec4i(box.min, int32(m_pools.z));
    constants.boxExtent = Vec4i(box.extent, int32(m_pools.y));
    constants.counts = Vec4u(m_sceneInstances->GetNumSpanChunks(), groupsX, m_pools.x, BrickListNodeCapacity);
    constants.options = Vec4u(m_isTextured ? 1u : 0u, step, m_numBatchChunks, m_isLit ? 1u : 0u);

    for (uint32 slot = 0; slot < m_numBatchChunks; slot++)
    {
        constants.chunkMins[slot] = m_batchChunkMins[slot];
    }

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    ShaderPropertySet shaderProperties;
    shaderProperties.Add(s_propBrickModes[uint32(pass)]);

    InsertBarriers(frame, true);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickBuild"), shaderProperties));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));

    uniformIndex = BindBuffers(cr, uniformIndex, GlimmerBrickBindings::Counters);

    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickFreeListBuffer"_sh, m_freeListBuffer);
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickPageFreeListBuffer"_sh, m_pageFreeListBuffer);
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickCardFreeListBuffer"_sh, m_cardFreeListBuffer);
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickListNodesBuffer"_sh, m_listNodesBuffer);
    cr << SetShaderUniform(uniformIndex++, "GlimmerBrickLiveListBuffer"_sh, m_liveListBuffer);
    cr << SetShaderUniform(uniformIndex++, "SpanInstancesBuffer"_sh, m_sceneInstances->GetSpanInstancesBuffer());
    cr << SetShaderUniform(uniformIndex++, "SpanChunksBuffer"_sh, m_sceneInstances->GetSpanChunksBuffer());
    cr << SetShaderUniform(uniformIndex++, "GlimmerTrianglesBuffer"_sh, m_triangleCache->GetTrianglesBuffer());
    cr << SetShaderUniform(uniformIndex++, "GlimmerTriangleTexcoordsBuffer"_sh, m_triangleCache->GetTexcoordsBuffer());
    cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
    cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());

    // the args are read by the passes they size, so only the args pass writes them
    if (pass == BuildPass::Args)
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickDispatchArgsBuffer"_sh, m_dispatchArgsBuffer);
    }
    else
    {
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickDispatchArgsBuffer"_sh, m_placeholderBuffer);
    }
}

void GlimmerBrickWorld::MoveWindow(Frame* frame, const Vec3i& originChunk)
{
    const Vec3i oldOrigin = m_originChunk;

    const bool overlapsOld = m_hasWindow
        && MathUtil::Abs(originChunk.x - oldOrigin.x) < ChunkDims.x
        && MathUtil::Abs(originChunk.y - oldOrigin.y) < ChunkDims.y
        && MathUtil::Abs(originChunk.z - oldOrigin.z) < ChunkDims.z;

    m_originChunk = originChunk;
    m_shaderData.origin = Vec4i(originChunk * ChunkCells, 1);

    if (!overlapsOld)
    {
        Dispatch(frame, BuildPass::Init, Box {}, MathUtil::Max(MathUtil::Max(BrickTableSize, m_pools.z), MathUtil::Max(m_pools.x, m_pools.y)));
    }
    else
    {
        // the slabs entering the window share table slots with the ones leaving it
        Vec3i remainingMin = originChunk;
        Vec3i remainingMax = originChunk + ChunkDims;

        for (int axis = 0; axis < 3; axis++)
        {
            Vec3i enteringMin = remainingMin;
            Vec3i enteringMax = remainingMax;

            if (originChunk[axis] > oldOrigin[axis])
            {
                enteringMin[axis] = oldOrigin[axis] + ChunkDims[axis];
                remainingMax[axis] = enteringMin[axis];
            }
            else if (originChunk[axis] < oldOrigin[axis])
            {
                enteringMax[axis] = oldOrigin[axis];
                remainingMin[axis] = enteringMax[axis];
            }
            else
            {
                continue;
            }

            const Box box { enteringMin * ChunkCells, (enteringMax - enteringMin) * ChunkCells };

            Dispatch(frame, BuildPass::Clear, box, uint32(box.extent.x * box.extent.y * box.extent.z) * 3);
        }
    }

    for (int32 z = 0; z < ChunkDims.z; z++)
    {
        for (int32 y = 0; y < ChunkDims.y; y++)
        {
            for (int32 x = 0; x < ChunkDims.x; x++)
            {
                const Vec3i coord = originChunk + Vec3i(x, y, z);

                Chunk& chunk = m_chunks[ChunkSlot(coord)];

                // what was starved may fit now that a slab has left
                chunk.isStarved = false;

                if (overlapsOld && chunk.coord == coord)
                {
                    continue;
                }

                chunk.Empty();
                chunk.coord = coord;
                chunk.isBuilt = false;
                chunk.isDirty = true;
                chunk.buildSerial++;
            }
        }
    }

    m_hasWindow = true;
}

void GlimmerBrickWorld::MarkAllDirty()
{
    for (Chunk& chunk : m_chunks)
    {
        chunk.isDirty = true;
        chunk.isStarved = false;
    }
}

void GlimmerBrickWorld::MarkSceneChanges()
{
    const uint32 generation = m_sceneInstances->GetGeneration();

    if (generation == m_seenGeneration)
    {
        return;
    }

    GlimmerSceneChanges changes;

    const bool isEverywhere = m_seenGeneration == ~0u || !m_sceneInstances->GetChangesSince(m_seenGeneration, changes);
    m_seenGeneration = generation;

    if (isEverywhere)
    {
        MarkAllDirty();

        return;
    }

    for (Chunk& chunk : m_chunks)
    {
        const Vec3f chunkMin = Vec3f(chunk.coord) * ChunkSize - Vec3f(ChangeMargin);
        const Vec3f chunkMax = chunkMin + Vec3f(ChunkSize + 2.0f * ChangeMargin);

        for (const GlimmerSceneChange& change : changes)
        {
            if ((chunk.isDirty && !chunk.isStarved) || change.isLodOnly)
            {
                continue;
            }

            if (change.bounds.Overlaps(BoundingBox(chunkMin, chunkMax)))
            {
                chunk.isDirty = true;
                chunk.isStarved = false;
            }
        }
    }
}

void GlimmerBrickWorld::MarkOccupiedChunks()
{
    for (Chunk& chunk : m_chunks)
    {
        chunk.hasGeometry = false;
    }

    const Vec3i windowMax = m_originChunk + ChunkDims - Vec3i(1);

    for (const GlimmerSceneInstances::SpanKey& spanKey : m_sceneInstances->GetSpanKeys())
    {
        const Vec3f boundsMin = (spanKey.bounds.min - Vec3f(ChangeMargin)) / ChunkSize;
        const Vec3f boundsMax = (spanKey.bounds.max + Vec3f(ChangeMargin)) / ChunkSize;

        Vec3i chunkMin;
        Vec3i chunkMax;

        for (int axis = 0; axis < 3; axis++)
        {
            chunkMin[axis] = MathUtil::Max(int32(MathUtil::Floor(boundsMin[axis])), m_originChunk[axis]);
            chunkMax[axis] = MathUtil::Min(int32(MathUtil::Floor(boundsMax[axis])), windowMax[axis]);
        }

        for (int32 z = chunkMin.z; z <= chunkMax.z; z++)
        {
            for (int32 y = chunkMin.y; y <= chunkMax.y; y++)
            {
                for (int32 x = chunkMin.x; x <= chunkMax.x; x++)
                {
                    m_chunks[ChunkSlot(Vec3i(x, y, z))].hasGeometry = true;
                }
            }
        }
    }
}

void GlimmerBrickWorld::BuildChunks(Frame* frame, Chunk* const* chunks, uint32 numChunks, const GlimmerTechnique* technique)
{
    m_numBatchChunks = numChunks;

    for (uint32 slot = 0; slot < numChunks; slot++)
    {
        m_batchChunkMins[slot] = Vec4i(chunks[slot]->coord * ChunkCells, 0);
    }

    {
        ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassClear);

        Dispatch(frame, BuildPass::Begin, Box {}, numChunks * BrickChunkBricks);

        for (uint32 slot = 0; slot < numChunks; slot++)
        {
            Dispatch(frame, BuildPass::Clear, Box { m_batchChunkMins[slot].GetXYZ(), Vec3i(ChunkCells) }, BrickChunkBricks);
        }
    }

    const uint32 numSpanChunks = m_sceneInstances->GetNumSpanChunks();

    if (numSpanChunks != 0)
    {
        CommandRecorder& cr = frame->cr;

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassBin);

            // a group per span chunk
            Dispatch(frame, BuildPass::Bin, Box {}, numSpanChunks * BrickGroupSize);
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassAlloc);

            Dispatch(frame, BuildPass::Alloc, Box {}, numChunks * BrickChunkBricks);

            cr << InsertBarrier(m_dispatchArgsBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

            Dispatch(frame, BuildPass::Args, Box {}, 1);

            cr << InsertBarrier(m_dispatchArgsBuffer, ResourceState::IndirectArg);
        }

        // only as many groups as the bricks taken need
        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassCount);

            DispatchIndirect(frame, BuildPass::Count, BrickArgsColumns);
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassPrefix);

            DispatchIndirect(frame, BuildPass::Prefix, BrickArgsPrefix);
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassGather);

            DispatchIndirect(frame, BuildPass::Gather, BrickArgsColumns);
        }

        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassTiles);

            DispatchIndirect(frame, BuildPass::Tiles, BrickArgsTiles);
        }

        if (m_isLit)
        {
            ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickPassLight);

            LightBricks(frame, technique, 0, 0, true);
        }
    }

    for (uint32 slot = 0; slot < numChunks; slot++)
    {
        Chunk& chunk = *chunks[slot];

        chunk.isBuilt = true;
        chunk.isDirty = false;
        chunk.hasBricks = true;
        chunk.buildSerial++;
    }

    CopyBuildCounters(frame, chunks, numChunks);

    m_numBatchChunks = 0;
}

void GlimmerBrickWorld::ClearChunk(Frame* frame, Chunk& chunk)
{
    Dispatch(frame, BuildPass::Clear, Box { chunk.coord * ChunkCells, Vec3i(ChunkCells) }, BrickChunkBricks);

    chunk.Empty();
}

void GlimmerBrickWorld::CopyBuildCounters(Frame* frame, Chunk* const* chunks, uint32 numChunks)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;

    if (!m_readbackBuffers[slot].IsValid())
    {
        m_readbackBuffers[slot] = RI.MakeGpuBuffer(GpuBufferType::ReadbackBuffer, BrickReadbackWords * sizeof(uint32));
        m_readbackBuffers[slot]->SetIsCpuAccessible(true);
        Check(m_readbackBuffers[slot]->Create());
    }

    CommandRecorder& cr = frame->cr;

    // the counters, then each slot's
    const uint32 numBytes = (BrickNumCounters + numChunks * BrickSlotCounters) * sizeof(uint32);

    cr << InsertBarrier(m_countersBuffer, ResourceState::CopySrc, ShaderModuleType::Compute);
    cr << InsertBarrier(m_readbackBuffers[slot].Get(), ResourceState::CopyDst, ShaderModuleType::Compute);
    cr << CopyBuffer(m_countersBuffer.gpuBuffer, m_readbackBuffers[slot].Get(), 0, 0, numBytes);
    cr << InsertBarrier(m_countersBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

    for (uint32 buildIndex = 0; buildIndex < numChunks; buildIndex++)
    {
        m_pendingBuilds[slot][buildIndex] = ChunkBuild { chunks[buildIndex]->coord, chunks[buildIndex]->buildSerial };
    }

    m_numPendingBuilds[slot] = numChunks;
}

bool GlimmerBrickWorld::ReadBuildCounters(Frame* frame, const Vec3f& viewerPosition)
{
    const uint32 slot = frame->GetFrameIndex() % NumFramesInFlight;
    const uint32 numPending = m_numPendingBuilds[slot];

    m_numPendingBuilds[slot] = 0;

    // this frame slot's last submission has finished by now, so what it copied can be read
    if (numPending == 0)
    {
        return false;
    }

    FixedArray<uint32, BrickReadbackWords> counters;

    m_readbackBuffers[slot]->Invalidate();
    m_readbackBuffers[slot]->Read((BrickNumCounters + numPending * BrickSlotCounters) * sizeof(uint32), counters.Data());

    // the batch ran out of list nodes: its chunks are built again, one at a time
    const bool isOverflowed = counters[BrickCounterBatchDroppedNodes] != 0 && numPending > 1;

    if (isOverflowed)
    {
        m_numSoloBuilds = MathUtil::Max(m_numSoloBuilds, numPending);
    }

    const auto isCurrent = [&](uint32 buildIndex, const Chunk& chunk)
    {
        const ChunkBuild& build = m_pendingBuilds[slot][buildIndex];

        return chunk.coord == build.coord
            && chunk.buildSerial == build.buildSerial
            && !chunk.isDirty;
    };

    Vec3u batchNeeded = Vec3u::Zero();

    for (uint32 buildIndex = 0; buildIndex < numPending && !isOverflowed; buildIndex++)
    {
        const Chunk& chunk = m_chunks[ChunkSlot(m_pendingBuilds[slot][buildIndex].coord)];

        if (isCurrent(buildIndex, chunk))
        {
            const uint32* record = counters.Data() + BrickNumCounters + size_t(buildIndex) * BrickSlotCounters;

            batchNeeded += Vec3u(record[BrickSlotFailedBricks], record[BrickSlotFailedPages], record[BrickSlotFailedCards]);
        }
    }

    const bool isGrowing = batchNeeded != Vec3u::Zero() && GrowPools(batchNeeded);

    bool hasWritten = false;

    for (uint32 buildIndex = 0; buildIndex < numPending; buildIndex++)
    {
        Chunk& chunk = m_chunks[ChunkSlot(m_pendingBuilds[slot][buildIndex].coord)];

        if (!isCurrent(buildIndex, chunk))
        {
            continue;
        }

        const uint32* record = counters.Data() + BrickNumCounters + size_t(buildIndex) * BrickSlotCounters;

        chunk.numBricks = record[BrickSlotBricks];
        chunk.numPages = record[BrickSlotPages];
        chunk.numCards = record[BrickSlotCards];

        if (isOverflowed)
        {
            chunk.isDirty = true;

            continue;
        }

        const Vec3u needed = Vec3u(record[BrickSlotFailedBricks], record[BrickSlotFailedPages], record[BrickSlotFailedCards]);

        chunk.isShort = needed != Vec3u::Zero();

        if (needed.x == 0 && needed.y == 0 && needed.z == 0)
        {
            continue;
        }

        // it keeps what it got until it's built again
        chunk.isDirty = true;

        if (isGrowing)
        {
            continue;
        }

        if (MakeRoom(frame, chunk, viewerPosition, needed))
        {
            hasWritten = true;
        }
        else
        {
            chunk.isStarved = true;
            m_numStarved++;
        }
    }

    return hasWritten;
}

static Vec3f ChunkCenter(const Vec3i& coord)
{
    return (Vec3f(coord) + Vec3f(0.5f)) * ChunkSize;
}

bool GlimmerBrickWorld::MakeRoom(Frame* frame, const Chunk& requester, const Vec3f& viewerPosition, const Vec3u& needed)
{
    struct Candidate
    {
        Chunk* chunk;
        float distance;
    };

    // only from chunks a chunk farther, so two at about the same distance don't take turns
    const float minDistance = ChunkCenter(requester.coord).Distance(viewerPosition) + ChunkSize;

    Array<Candidate, GlimmerTempAllocator> candidates;
    candidates.Reserve(m_chunks.Size());

    for (Chunk& chunk : m_chunks)
    {
        if (!chunk.hasBricks || (chunk.numBricks == 0 && chunk.numPages == 0 && chunk.numCards == 0))
        {
            continue;
        }

        const float distance = ChunkCenter(chunk.coord).Distance(viewerPosition);

        if (distance > minDistance)
        {
            candidates.PushBack(Candidate { &chunk, distance });
        }
    }

    std::sort(candidates.Begin(), candidates.End(), [](const Candidate& lhs, const Candidate& rhs)
        {
            return lhs.distance > rhs.distance;
        });

    Vec3u freed = Vec3u::Zero();
    size_t numVictims = 0;

    const auto isEnough = [&]()
    {
        return freed.x >= needed.x && freed.y >= needed.y && freed.z >= needed.z;
    };

    while (numVictims < candidates.Size() && !isEnough())
    {
        const Chunk& victim = *candidates[numVictims++].chunk;

        freed += Vec3u(victim.numBricks, victim.numPages, victim.numCards);
    }

    if (!isEnough())
    {
        return false;
    }

    // given up until the window moves, rather than built again to push out the next one along
    for (size_t victimIndex = 0; victimIndex < numVictims; victimIndex++)
    {
        Chunk& victim = *candidates[victimIndex].chunk;

        ClearChunk(frame, victim);

        victim.isDirty = true;
        victim.isStarved = true;
    }

    m_numEvictions += uint32(numVictims);

    return true;
}

void GlimmerBrickWorld::Update(Frame* frame, const Vec3f& viewerPosition, const Vec3f& lookAhead, float budgetScale, const GlimmerSceneInstances& sceneInstances, const GlimmerTriangleCache& triangleCache, const GlimmerTechnique* technique, const GlimmerLightShaderData& light)
{
    HYP_SCOPE;

    if (!sceneInstances.IsReady() || !triangleCache.IsReady() || !sceneInstances.GetSpanInstancesBuffer().IsInitialized() || !sceneInstances.GetSpanChunksBuffer().IsInitialized())
    {
        return;
    }

    m_sceneInstances = &sceneInstances;
    m_triangleCache = &triangleCache;

    if (!m_tableBuffer.IsInitialized())
    {
        CreateResources();
    }

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickBuild);

    const Vec3i viewerChunk = Vec3i(
        int32(MathUtil::Floor(viewerPosition.x / ChunkSize)),
        int32(MathUtil::Floor(viewerPosition.y / ChunkSize)),
        int32(MathUtil::Floor(viewerPosition.z / ChunkSize)));

    const Vec3i originChunk = viewerChunk - Vec3i(ChunkDims.x / 2, ChunkDims.y / 2, ChunkDims.z / 2);

    bool hasWritten = false;

    if (m_grownPools != Vec3u::Zero())
    {
        GrowPoolResources(frame, m_grownPools);

        m_grownPools = Vec3u::Zero();

        hasWritten = true;
    }

    if (!m_hasWindow
        || MathUtil::Abs(originChunk.x - m_originChunk.x) >= WindowRecenterChunks
        || MathUtil::Abs(originChunk.y - m_originChunk.y) >= WindowRecenterChunks
        || MathUtil::Abs(originChunk.z - m_originChunk.z) >= WindowRecenterChunks)
    {
        MoveWindow(frame, originChunk);

        m_occupancyGeneration = ~0u;

        hasWritten = true;
    }

    if (m_isTextured != g_cvGlimmerBricksTextured.Get())
    {
        m_isTextured = g_cvGlimmerBricksTextured.Get();

        MarkAllDirty();
    }

    // only the reflections and the light debug views read the light
    const bool wantsLight = g_cvGlimmerBricksReflections.Get() || m_framesSinceLightView < LightViewFrames;

    if (wantsLight != m_isLit)
    {
        m_isLit = wantsLight;

        if (m_isLit)
        {
            CreatePoolBuffers(true);

            // bricks only take cards while lit
            MarkAllDirty();

            m_hasRelitLight = false;
        }
        else
        {
            ReleaseLitResources();
        }
    }

    MarkSceneChanges();

    if (m_occupancyGeneration != sceneInstances.GetGeneration())
    {
        MarkOccupiedChunks();

        m_occupancyGeneration = sceneInstances.GetGeneration();
    }

    // a chunk that ran out of room gets it from chunks farther away, or waits
    if (ReadBuildCounters(frame, viewerPosition))
    {
        hasWritten = true;
    }

    // a chunk no span instance reaches is done without a build, and only cleared when it had bricks
    uint32 numClears = 0;

    for (Chunk& chunk : m_chunks)
    {
        if (!chunk.isDirty || chunk.hasGeometry)
        {
            continue;
        }

        if (chunk.hasBricks)
        {
            if (numClears == MaxEmptyChunkClearsPerFrame)
            {
                continue;
            }

            ClearChunk(frame, chunk);

            numClears++;
            hasWritten = true;
        }

        chunk.isBuilt = true;
        chunk.isDirty = false;
        chunk.isStarved = false;
    }

    // counted per frame, so a low frame rate gets more a frame to keep the same pace
    const uint32 maxBuilds = m_numSoloBuilds != 0
        ? 1u
        : uint32(MathUtil::Clamp(
            int32(float(g_cvGlimmerBricksChunksPerFrame.Get()) * MathUtil::Clamp(budgetScale, 1.0f, 8.0f)),
            1,
            int32(GlimmerBrickMaxChunkBuildsPerFrame)));

    const Vec3f buildFocus = GlimmerHelpers::GetGlimmerWindowCenter(
        viewerPosition,
        lookAhead,
        Vec3f(ChunkDims) * ChunkSize);

    FixedArray<Chunk*, GlimmerBrickMaxChunkBuildsPerFrame> batch;
    uint32 numBatched = 0;

    while (numBatched < maxBuilds)
    {
        Chunk* nearestChunk = nullptr;
        float nearestDistance = MathUtil::MaxSafeValue<float>();

        for (Chunk& chunk : m_chunks)
        {
            if (!chunk.isDirty || !chunk.hasGeometry || chunk.isStarved)
            {
                continue;
            }

            const float distance = ChunkCenter(chunk.coord).DistanceSquared(buildFocus);

            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestChunk = &chunk;
            }
        }

        if (!nearestChunk)
        {
            break;
        }

        // taken for this batch
        nearestChunk->isDirty = false;

        batch[numBatched++] = nearestChunk;
    }

    if (numBatched != 0)
    {
        BuildChunks(frame, batch.Data(), numBatched, technique);

        m_numSoloBuilds -= MathUtil::Min(m_numSoloBuilds, numBatched);

        hasWritten = true;
    }

    const bool wasSettled = IsSettled();

    m_numDirtyChunks = 0;

    for (const Chunk& chunk : m_chunks)
    {
        m_numDirtyChunks += chunk.isDirty && !chunk.isStarved ? 1 : 0;
    }

    if (IsSettled() && !wasSettled)
    {
        HYP_LOG(Rendering, Debug, "Glimmer: bricks settled after {} frames", m_framesUnsettled);
    }

    m_framesUnsettled = IsSettled() ? 0 : m_framesUnsettled + 1;

    m_framesSincePoolLog++;

    if ((m_numEvictions != 0 || m_numStarved != 0) && m_framesSincePoolLog >= PoolLogIntervalFrames)
    {
        HYP_LOG(Rendering, Debug, "Glimmer: brick pools full; {} chunks given up for nearer ones, {} left without room", m_numEvictions, m_numStarved);

        m_numEvictions = 0;
        m_numStarved = 0;
        m_framesSincePoolLog = 0;
    }

    if (hasWritten)
    {
        ListLiveBricks(frame);

        InsertBarriers(frame, false);
    }

    m_framesSinceLightView = MathUtil::Min(m_framesSinceLightView + 1, LightViewFrames);

    if (!m_isLit)
    {
        return;
    }

    // a change of sun or sky relights every brick in turn.
    // otherwise a trickle follows the bounce, and the sky's small drift, as they settle
    const float intensity = g_cvGlimmerIntensity.Get();

    if (!m_hasRelitLight || intensity != m_relitIntensity || LightChange(m_relitLight, light) > RelightBurstChange)
    {
        m_relitLight = light;
        m_relitIntensity = intensity;
        m_hasRelitLight = true;

        m_relightBurst = 0;

        for (const Chunk& chunk : m_chunks)
        {
            m_relightBurst += chunk.hasBricks ? chunk.numBricks : 0;
        }
    }

    const uint32 numRelightBricks = MathUtil::Min(m_relightBurst != 0 ? RelightBricksPerFrame : RelightTrickleBricks, m_pools.x);

    m_relightBurst -= MathUtil::Min(m_relightBurst, numRelightBricks);

    if (numRelightBricks != 0)
    {
        LightBricks(frame, technique, m_relightCursor, numRelightBricks, false);

        m_relightCursor += numRelightBricks;
    }
}

void GlimmerBrickWorld::ListLiveBricks(Frame* frame)
{
    Dispatch(frame, BuildPass::Live, Box {}, 1, 0);
    Dispatch(frame, BuildPass::Live, Box {}, m_pools.x, 1);
}

void GlimmerBrickWorld::LightBricks(Frame* frame, const GlimmerTechnique* technique, uint32 firstBrick, uint32 numBricks, bool isBuildList)
{
    HYP_SCOPE;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickLight);

    CommandRecorder& cr = frame->cr;

    InsertBarriers(frame, false);

    const bool hasIndirect = technique && technique->IsReady();

    // a row of groups per brick: the build list's sized by the args pass, the live list's here
    const auto dispatchPass = [&](const StaticShaderPropertyId& modeProperty, uint32 level, uint32 threadsPerBrick, uint32 argsOffset)
    {
        GlimmerApplyShaderData applyShaderData {};
        applyShaderData.params = Vec4u(0u, hasIndirect ? 1u : 0u, 0u, 0u);
        applyShaderData.settings = Vec4f(MathUtil::Max(g_cvGlimmerIntensity.Get(), 0.0f), 0.0f, 0.0f, 0.0f);

        GlimmerBrickLightConstants constants {};
        constants.bricks = m_shaderData;
        constants.range = Vec4u(firstBrick, numBricks, isBuildList ? 1u : 0u, level);
        constants.params = Vec4f(ShadowBiasColumns, ShadowDistance, 0.0f, 0.0f);

        GpuBuffer* cbuffer = nullptr;
        size_t cbufferOffset = 0;
        size_t cbufferSize = 0;

        RI.cbufferAllocator->Write(&applyShaderData);
        GlimmerTechnique::WriteApplyShaderData(*RI.cbufferAllocator, hasIndirect ? technique : nullptr);
        RI.cbufferAllocator->Write(&constants);
        RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

        ShaderPropertySet shaderProperties;
        shaderProperties.Add(modeProperty);

        cr << InsertBarrier(m_radianceBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);
        cr << InsertBarrier(m_crossingLightBuffer, ResourceState::UnorderedAccess, ShaderModuleType::Compute);

        cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickLight"), shaderProperties));

        uint32 uniformIndex = 0;

        cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
        cr << SetShaderUniform(uniformIndex++, "WorldsBuffer"_sh, RI.namedBuffers[NamedBuffer::Worlds]);
        cr << SetShaderUniform(uniformIndex++, "MaterialsBuffer"_sh, RI.namedBuffers[NamedBuffer::Materials]);
        cr << SetShaderUniform(uniformIndex++, "SamplerLinearMipmap"_sh, RI.placeholderData->GetSamplerLinearMipmap());
        cr << SetShaderUniform(uniformIndex++, "GlimmerBrickLiveListBuffer"_sh, m_liveListBuffer);

        uniformIndex = BindBuffers(cr, uniformIndex, GlimmerBrickBindings::Counters | GlimmerBrickBindings::Light);
        uniformIndex = GlimmerTechnique::BindApplyResources(cr, uniformIndex, hasIndirect ? technique : nullptr);

        if (isBuildList)
        {
            cr << DispatchComputeIndirect(m_dispatchArgsBuffer, argsOffset * sizeof(uint32));
        }
        else
        {
            cr << DispatchCompute(Vec3u((threadsPerBrick + BrickGroupSize - 1) / BrickGroupSize, numBricks, 1));
        }
    };

    dispatchPass(s_propBrickLightModeLight, 0, BrickColumnsPerBrick, BrickArgsColumns);

    for (uint32 level = GlimmerBrickCardFirstMip; level < GlimmerBrickMips; level++)
    {
        const uint32 side = GlimmerBrickSide >> level;

        dispatchPass(s_propBrickLightModeMips, level, GlimmerBrickCards * side * side, BrickArgsMips + 3 * (level - GlimmerBrickCardFirstMip));
    }

    cr << InsertBarrier(m_radianceBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(m_crossingLightBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
}

void GlimmerBrickWorld::GetBuildingChunks(Vec4i& outOrigin, Vec4u (&outBits)[16]) const
{
    for (Vec4u& bits : outBits)
    {
        bits = Vec4u::Zero();
    }

    if (!IsReady())
    {
        outOrigin = Vec4i(0, 0, 0, 2);

        return;
    }

    outOrigin = Vec4i(m_originChunk.x, m_originChunk.y, m_originChunk.z, 1);

    for (const Chunk& chunk : m_chunks)
    {
        if ((chunk.isBuilt && !chunk.isDirty) || chunk.isStarved)
        {
            continue;
        }

        const Vec3i local = chunk.coord - m_originChunk;

        if (local.x < 0 || local.y < 0 || local.z < 0 || local.x >= ChunkDims.x || local.y >= ChunkDims.y || local.z >= ChunkDims.z)
        {
            continue;
        }

        const uint32 bit = uint32((local.z * ChunkDims.y + local.y) * ChunkDims.x + local.x);

        outBits[bit >> 7][(bit >> 5) & 3] |= 1u << (bit & 31);
    }
}

bool GlimmerBrickWorld::RenderDebugView(const GlimmerDebugViewContext& context, GlimmerBrickDebugView debugView)
{
    HYP_SCOPE;

    if (debugView == GlimmerBrickDebugView::Light || debugView == GlimmerBrickDebugView::LightCoarse)
    {
        m_framesSinceLightView = 0;
    }

    if (!IsReady() || !context.sceneInstances || !context.sceneInstances->IsReady() || !context.triangleCache || !context.triangleCache->IsReady())
    {
        return false;
    }

    // the light views wait for the light, which they ask for above
    if ((debugView == GlimmerBrickDebugView::Light || debugView == GlimmerBrickDebugView::LightCoarse) && !m_isLit)
    {
        return false;
    }


    Framebuffer* gbufferFramebuffer = context.gbufferFramebuffer;
    const Vec2u extent = context.extent;

    ENGINE_STAT_GPU_SCOPE(&s_statGlimmerBrickDebug);

    GlimmerBrickDebugConstants constants {};
    constants.dimensionsMode = Vec4u(extent.x, extent.y, uint32(debugView), 0);
    constants.params = Vec4f(10000.0f, 0.0f, 0.0f, 0.0f);
    constants.info = Vec4u(m_pools, 0u);
    constants.bricks = m_shaderData;

    GpuBuffer* cbuffer = nullptr;
    size_t cbufferOffset = 0;
    size_t cbufferSize = 0;

    RI.cbufferAllocator->Write(&constants);
    RI.cbufferAllocator->Commit(cbuffer, cbufferOffset, cbufferSize);

    CommandRecorder& cr = context.frame->cr;

    cr << InsertBarrier(gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier(gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetGpuImage(), ResourceState::ShaderResource, ShaderModuleType::Compute);

    InsertBarriers(context.frame, false);
    cr << InsertBarrier((m_isLit ? m_radianceBuffer : m_placeholderBuffer), ResourceState::ShaderResource, ShaderModuleType::Compute);
    cr << InsertBarrier((m_isLit ? m_crossingLightBuffer : m_placeholderBuffer), ResourceState::ShaderResource, ShaderModuleType::Compute);

    cr << SetCurrentShader(ShaderDesc(NAME("GlimmerBrickDebug")));

    uint32 uniformIndex = 0;

    cr << SetShaderUniform(uniformIndex++, "CBuffer"_sh, cbuffer, ShaderDataOffset(cbufferOffset, cbufferSize));
    cr << SetShaderUniform(uniformIndex++, "OutImage"_sh, context.outputImageView);
    cr << SetShaderUniform(uniformIndex++, "GBufferDepthTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Depth)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "GBufferNormalsTexture"_sh, gbufferFramebuffer->GetAttachment(GBufferTarget::Normals)->GetImageView());
    cr << SetShaderUniform(uniformIndex++, "CamerasBuffer"_sh, RI.namedBuffers[NamedBuffer::Cameras], Resources::GetBinding(context.view->GetCamera()));

    uniformIndex = BindBuffers(cr, uniformIndex, GlimmerBrickBindings::Counters | GlimmerBrickBindings::Light);

    cr << DispatchCompute(Vec3u { (extent.x + 7) / 8, (extent.y + 7) / 8, 1 });

    return true;
}

#pragma endregion GlimmerBrickWorld

} // namespace Hyperion
