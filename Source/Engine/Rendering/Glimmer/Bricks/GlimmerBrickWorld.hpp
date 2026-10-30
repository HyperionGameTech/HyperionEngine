/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>

#include <Core/Containers/FixedArray.hpp>

#include <Core/Utilities/EnumFlags.hpp>

#include <Core/Math/Vector3.hpp>
#include <Core/Math/Vector4.hpp>

namespace Hyperion {

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Reference Docs ////////////////////////////////////////////////////////////////////////////////////////////////////////////
///     https://www.cs.princeton.edu/courses/archive/spring2001/cs598b/papers/shade98.pdf     /////////////////////////////////
///     https://tugraz.elsevierpure.com/ws/portalfiles/portal/92345598/3675382.pdf            /////////////////////////////////
///     https://research.nvidia.com/sites/default/files/pubs/2013-12_Lighting-Deep-G-Buffers/Mara2013DeepGBuffer-small.pdf  ///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class GlimmerSceneInstances;
class GlimmerTriangleCache;
class GlimmerTechnique;
struct GlimmerDebugViewContext;

static constexpr uint32 GlimmerBrickGridXZ = 128;
static constexpr uint32 GlimmerBrickGridY = 64;
static constexpr uint32 GlimmerBrickChunkCells = 8;
static constexpr uint32 GlimmerBrickSide = 32;
static constexpr uint32 GlimmerBrickTile = 4;
static constexpr uint32 GlimmerBrickPageCrossings = 256;
static constexpr uint32 GlimmerBrickMaxPages = 31;
static constexpr uint32 GlimmerBrickCards = 8;
static constexpr uint32 GlimmerBrickMips = 6;
static constexpr uint32 GlimmerBrickCardFirstMip = 2;
static constexpr uint32 GlimmerBrickCardSide = GlimmerBrickSide >> GlimmerBrickCardFirstMip;
static constexpr uint32 GlimmerBrickCardTexels = (4 * GlimmerBrickCardSide * GlimmerBrickCardSide - 1) / 3;
static constexpr uint32 GlimmerBrickIndexStride = 1 + GlimmerBrickMaxPages + GlimmerBrickCards + 6 + (GlimmerBrickSide / GlimmerBrickTile) * (GlimmerBrickSide / GlimmerBrickTile) + GlimmerBrickSide * GlimmerBrickSide;

static constexpr uint32 GlimmerBrickMaxPoolBricks = 32768;
static constexpr uint32 GlimmerBrickMaxPoolPages = 65535;
static constexpr uint32 GlimmerBrickMaxPoolCards = 65535;

// NOTE must match GlimmerBricks.hlsli: a reflection slot keeps its card in this many bits and the brick's serial above them
static constexpr uint32 GlimmerBrickCardBits = 18;

static_assert(GlimmerBrickMaxPoolBricks * GlimmerBrickCards <= (1u << GlimmerBrickCardBits));

static constexpr float GlimmerBrickCellSize = 2.0f;

// NOTE must match MAX_BATCH_CHUNKS in GlimmerBricks.hlsli
static constexpr uint32 GlimmerBrickMaxChunkBuildsPerFrame = 32;

static constexpr uint32 GlimmerBrickChunksXZ = GlimmerBrickGridXZ / GlimmerBrickChunkCells;
static constexpr uint32 GlimmerBrickChunksY = GlimmerBrickGridY / GlimmerBrickChunkCells;
static constexpr uint32 GlimmerBrickChunks = GlimmerBrickChunksXZ * GlimmerBrickChunksY * GlimmerBrickChunksXZ;

struct GlimmerBrickShaderData
{
    Vec4i origin; // xyz = absolute cell of the window's first cell, w = 1 once the window is valid
    Vec4f params; // x = cell size, y = 1 / cell size, z = column width, w = 1 / column width
};

static_assert(sizeof(GlimmerBrickShaderData) == 32);

enum class GlimmerBrickDebugView : int
{
    None = 0,
    Shaded,
    Steps,
    Receiver,
    Light,
    LightCoarse,

    Max
};

enum class GlimmerBrickBindings : uint8
{
    None = 0x0,
    Counters = 0x1,
    Light = 0x2
};

HYP_MAKE_ENUM_FLAGS(GlimmerBrickBindings);

class GlimmerBrickWorld final
{
public:
    GlimmerBrickWorld();

    GlimmerBrickWorld(const GlimmerBrickWorld& other) = delete;
    GlimmerBrickWorld& operator=(const GlimmerBrickWorld& other) = delete;

    ~GlimmerBrickWorld();

    /*! \param frame            the frame
     *  \param viewerPosition   where the window is centered
     *  \param lookAhead        where the viewer is headed, to build ahead of them
     *  \param budgetScale      how much of the frame's build budget to use
     *  \param sceneInstances   the scene's instances
     *  \param triangleCache    the scene's triangles
     *  \param technique        where the cards indirect light comes from
     *  \param light             */
    void Update(
        Frame* frame,
        const Vec3f& viewerPosition,
        const Vec3f& lookAhead,
        float budgetScale,
        const GlimmerSceneInstances& sceneInstances,
        const GlimmerTriangleCache& triangleCache,
        const GlimmerTechnique* technique,
        const GlimmerLightShaderData& light);

    bool RenderDebugView(const GlimmerDebugViewContext& context, GlimmerBrickDebugView debugView);

    // a bit per chunk of the window, by its place in it, set while it waits to be built (not while starved); origin.w is 0 without bricks, 1 with, 2 before any are built
    void GetBuildingChunks(Vec4i& outOrigin, Vec4u (&outBits)[16]) const;

    uint32 BindBuffers(CommandRecorder& cr, uint32 uniformIndex, EnumFlags<GlimmerBrickBindings> bindings = GlimmerBrickBindings::None) const;

    HYP_FORCE_INLINE bool IsReady() const
    {
        return m_tableBuffer.IsInitialized() && m_shaderData.origin.w != 0;
    }

    // every chunk of the window built for the scene as it is
    HYP_FORCE_INLINE bool IsSettled() const
    {
        return IsReady() && m_numDirtyChunks == 0;
    }

    HYP_FORCE_INLINE bool IsLit() const
    {
        return m_isLit;
    }

    HYP_FORCE_INLINE const GlimmerBrickShaderData& GetShaderData() const
    {
        return m_shaderData;
    }

    HYP_FORCE_INLINE const RWStructuredBuffer& GetTableBuffer() const
    {
        return m_tableBuffer;
    }

    HYP_FORCE_INLINE const RWStructuredBuffer& GetIndexBuffer() const
    {
        return m_indexBuffer;
    }

    HYP_FORCE_INLINE const RWStructuredBuffer& GetCrossingsBuffer() const
    {
        return m_crossingsBuffer;
    }

    HYP_FORCE_INLINE uint32 GetNumCrossings() const
    {
        return m_pools.y * GlimmerBrickPageCrossings;
    }

    HYP_FORCE_INLINE const RWStructuredBuffer& GetMaterialBuffer() const
    {
        return m_materialBuffer;
    }

private:
    struct Chunk
    {
        Vec3i coord;
        bool isBuilt = false;
        bool isDirty = false;
        bool hasGeometry = false;
        bool hasBricks = false;
        bool isStarved = false; // failed for want of room nothing farther could give up; tried again once the window moves
        bool isShort = false;
        uint32 buildSerial = 0;
        uint32 numBricks = 0;
        uint32 numPages = 0;
        uint32 numCards = 0;

        void Empty()
        {
            hasBricks = false;
            isShort = false;
            numBricks = 0;
            numPages = 0;
            numCards = 0;
        }
    };

    struct ChunkBuild
    {
        Vec3i coord;
        uint32 buildSerial;
    };

    struct Box
    {
        Vec3i min;
        Vec3i extent;
    };

    enum class BuildPass : uint8
    {
        Init,
        Begin,
        Clear,
        Bin,
        Alloc,
        Args,
        Count,
        Prefix,
        Gather,
        Tiles,
        Live,
        Grow,

        Max
    };

    struct PoolBuffer
    {
        RWStructuredBuffer GlimmerBrickWorld::* buffer;
        uint32 pool;
        uint32 wordsPerItem;
        bool isLitOnly;
        Name name;
    };

    static const PoolBuffer s_poolBuffers[];
    static Vec3u s_largestPools;

    static uint64 PoolBytes(const Vec3u& pools, bool isLit);

    Vec3u GetInitialPools() const;

    void CreateResources();
    void CreatePoolBuffers(bool isLitOnly);
    void ReleaseLitResources();
    void GrowPoolResources(Frame* frame, const Vec3u& grown);
    bool GrowPools(const Vec3u& needed);
    void ListLiveBricks(Frame* frame);
    void MoveWindow(Frame* frame, const Vec3i& originChunk);
    void MarkAllDirty();
    void MarkSceneChanges();
    void MarkOccupiedChunks();
    void BuildChunks(Frame* frame, Chunk* const* chunks, uint32 numChunks, const GlimmerTechnique* technique);
    void LightBricks(Frame* frame, const GlimmerTechnique* technique, uint32 firstBrick, uint32 numBricks, bool isBuildList);
    void Dispatch(Frame* frame, BuildPass pass, const Box& box, uint32 numThreads, uint32 step = 0);
    void DispatchIndirect(Frame* frame, BuildPass pass, uint32 argsOffset);
    void SetBuildPass(Frame* frame, BuildPass pass, const Box& box, uint32 groupsX, uint32 step);
    void InsertBarriers(Frame* frame, bool isForWriting);
    void CopyBuildCounters(Frame* frame, Chunk* const* chunks, uint32 numChunks);
    bool ReadBuildCounters(Frame* frame, const Vec3f& viewerPosition);
    bool MakeRoom(Frame* frame, const Chunk& requester, const Vec3f& viewerPosition, const Vec3u& needed);
    void ClearChunk(Frame* frame, Chunk& chunk);

    static uint32 ChunkSlot(const Vec3i& coord);

    RWStructuredBuffer m_tableBuffer;
    RWStructuredBuffer m_indexBuffer;
    RWStructuredBuffer m_crossingsBuffer;
    RWStructuredBuffer m_materialBuffer;
    RWStructuredBuffer m_countersBuffer;
    RWStructuredBuffer m_freeListBuffer;
    RWStructuredBuffer m_pageFreeListBuffer;
    RWStructuredBuffer m_cardFreeListBuffer;
    RWStructuredBuffer m_radianceBuffer;
    RWStructuredBuffer m_crossingLightBuffer;
    RWStructuredBuffer m_listNodesBuffer;
    RWStructuredBuffer m_liveListBuffer;
    IndirectArgsBuffer m_dispatchArgsBuffer;
    RWStructuredBuffer m_placeholderBuffer;

    FixedArray<GpuBufferRef, NumFramesInFlight> m_readbackBuffers;
    FixedArray<FixedArray<ChunkBuild, GlimmerBrickMaxChunkBuildsPerFrame>, NumFramesInFlight> m_pendingBuilds;
    FixedArray<uint32, NumFramesInFlight> m_numPendingBuilds {};

    FixedArray<Chunk, GlimmerBrickChunks> m_chunks;

    FixedArray<Vec4i, GlimmerBrickMaxChunkBuildsPerFrame> m_batchChunkMins;
    uint32 m_numBatchChunks = 0;
    uint32 m_numSoloBuilds = 0;

    const GlimmerSceneInstances* m_sceneInstances = nullptr;
    const GlimmerTriangleCache* m_triangleCache = nullptr;

    GlimmerLightShaderData m_relitLight;
    float m_relitIntensity = 0.0f;
    uint32 m_relightBurst = 0;
    bool m_hasRelitLight = false;
    bool m_isLit = false;

    Vec3i m_originChunk;
    Vec3u m_pools = Vec3u::Zero();
    uint32 m_relightCursor = 0;
    uint32 m_seenGeneration = ~0u;
    uint32 m_occupancyGeneration = ~0u;
    uint32 m_numDirtyChunks = 0;
    uint32 m_framesUnsettled = 0;
    uint32 m_numEvictions = 0;
    uint32 m_numStarved = 0;
    uint32 m_framesSincePoolLog = 0;
    uint32 m_framesSinceLightView;
    Vec3u m_grownPools = Vec3u::Zero();
    bool m_hasWindow = false;
    bool m_isTextured;

    GlimmerBrickShaderData m_shaderData {};
};

} // namespace Hyperion
