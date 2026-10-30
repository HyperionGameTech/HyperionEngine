/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/Glimmer/GlimmerTriangleCache.hpp>
#include <Rendering/Glimmer/GlimmerHelpers.hpp>
#include <Rendering/Glimmer/GlimmerCVars.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/GpuBuffer.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/SAHBVHBuilder.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>
#include <Core/Threading/ThreadPool.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Math/MathUtil.hpp>

#include <Core/Utilities/Float16.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <Core/Logging/Logger.hpp>

#include <algorithm>
#include <cmath>
#include <thread>

namespace Hyperion {

HYP_DECLARE_LOG_CHANNEL(Rendering);

static constexpr uint32 FreedRangeReuseDelayFrames = 4;
static constexpr uint32 MinFramesBeforeEviction = 8;
static constexpr uint32 FailedRetryDelayFrames = 120;

static constexpr uint32 TexcoordOffsetInFloats = 6;
static constexpr uint32 LightmapUVOffsetInFloats = sizeof(TVertex<VT_Simple>) / sizeof(float);

static constexpr uint64 PoolBytes = 160ull * 1024ull * 1024ull;
static constexpr uint64 LightmapUVPoolBytes = 24ull * 1024ull * 1024ull;
static constexpr uint64 TexcoordPoolBytes = 32ull * 1024ull * 1024ull;

static constexpr uint32 BLASMinLeafSize = 4;
static constexpr uint32 BLASMaxLeafSize = 16;

// builds queued a thread, so each has the next to hand
static constexpr uint32 BuildsInFlightPerThread = 2;
static constexpr size_t UploadBudgetBytes = 8u * 1024u * 1024u;

// a full pool only gives up a mesh's triangles for one this much nearer, so two meshes at about the same distance don't take turns
static constexpr float DemotionDistanceRatio = 1.25f;
static constexpr float DemotionDistanceMargin = 8.0f;
// a mesh waiting for a stretch of the pool to be cleared for it asks again no sooner than this, giving the scene instances time to let go
static constexpr uint32 RoomRequestIntervalFrames = 30;

// a mesh that didn't fit is built again no sooner than this (doubling each time it doesn't fit again), and only when there's room for it
static constexpr uint32 DeferredRetryDelayFrames = 30;
static constexpr uint32 MaxDeferredRetryShift = 5;
static constexpr uint32 PoolFullLogIntervalFrames = 600;

static constexpr uint32 LodErrorScaleIntervalFrames = 120;
static constexpr float LodErrorScaleStepUp = 1.5f;
static constexpr float LodErrorScaleStepDown = 1.25f;
static constexpr float MaxLodErrorScale = 4.0f;
static constexpr float LodErrorScaleMinUsageDrop = 0.005f;
static constexpr float LodErrorScaleRelaxUsage = 0.8f;

namespace /* Helpers */ {

// a quarter of the cores to prevent oversubscription of the frame's threads on startup.
uint32 GetNumBuildThreads()
{
    if (g_cvGlimmerBuildThreads.Get() > 0)
    {
        return uint32(g_cvGlimmerBuildThreads.Get());
    }

    return MathUtil::Clamp(std::thread::hardware_concurrency() / 4u, 1u, 4u);
}

GlimmerTriangleTexcoords PackTexCoords(const Vec2f* corners)
{
    GlimmerTriangleTexcoords packed {};

    Vec2f uvs[3];

    for (uint32 corner = 0; corner < 3; corner++)
    {
        uvs[corner] = MathUtil::IsFinite(corners[corner]) ? corners[corner] : Vec2f::Zero();
    }

    for (int axis = 0; axis < 2; axis++)
    {
        const float lowest = MathUtil::Min(uvs[0][axis], MathUtil::Min(uvs[1][axis], uvs[2][axis]));
        const float base = std::floor(MathUtil::Clamp(lowest, -32768.0f, 32767.0f));

        packed.base[axis] = int16(base);

        for (uint32 corner = 0; corner < 3; corner++)
        {
            packed.uvs[corner * 2 + axis] = Float16(uvs[corner][axis] - base).Raw();
        }
    }

    return packed;
}

/// Quantizes nodes for the BLAS pool.
/// \param outLeafEnds gets the index of the last primitive of every leaf.
/// \return false if an index doesn't fit a child ref.
bool PackBLAS(
    Span<const SAHBVHNode> nodes,
    Array<GlimmerBLASNode, GlimmerAllocator>& outNodes,
    Array<uint32, GlimmerAllocator>& outLeafEnds)
{
    constexpr int MinExponent = -32;
    constexpr int MaxExponent = 31;

    outNodes.Resize(nodes.Size());
    outLeafEnds.Clear();

    for (size_t nodeIndex = 0; nodeIndex < nodes.Size(); nodeIndex++)
    {
        const SAHBVHNode& node = nodes[nodeIndex];
        GlimmerBLASNode& packed = outNodes[nodeIndex];

        const float* const childMin[2] = { node.leftMin, node.rightMin };
        const float* const childMax[2] = { node.leftMax, node.rightMax };

        float origin[3];
        double scale[3];
        uint32 exponents = 0;

        for (int axis = 0; axis < 3; axis++)
        {
            origin[axis] = MathUtil::Min(node.leftMin[axis], node.rightMin[axis]);

            const double extent = double(MathUtil::Max(node.leftMax[axis], node.rightMax[axis])) - double(origin[axis]);

            int exponent = extent > 0.0 ? int(std::ceil(std::log2(extent / 255.0))) : MinExponent;
            exponent = MathUtil::Clamp(exponent, MinExponent, MaxExponent);

            while (exponent < MaxExponent && 255.0 * std::ldexp(1.0, exponent) < extent)
            {
                exponent++;
            }

            scale[axis] = std::ldexp(1.0, exponent);
            exponents |= uint32(exponent - MinExponent) << (axis * 6);
        }

        // rounding outward keeps the quantized bounds around the real ones
        ubyte childBytes[12];

        for (uint32 child = 0; child < 2; child++)
        {
            for (int axis = 0; axis < 3; axis++)
            {
                const double quantizedMin = std::floor((double(childMin[child][axis]) - double(origin[axis])) / scale[axis]);
                const double quantizedMax = std::ceil((double(childMax[child][axis]) - double(origin[axis])) / scale[axis]);

                childBytes[child * 6 + axis] = ubyte(MathUtil::Clamp(quantizedMin, 0.0, 255.0));
                childBytes[child * 6 + 3 + axis] = ubyte(MathUtil::Clamp(quantizedMax, 0.0, 255.0));
            }
        }

        const uint32 childIndices[2] = { node.leftIndex, node.rightIndex };
        const uint32 childCounts[2] = { node.leftCount, node.rightCount };

        uint32 refs[2];

        for (uint32 child = 0; child < 2; child++)
        {
            if (childIndices[child] > GlimmerBLASNode::MaxIndex)
            {
                return false;
            }

            refs[child] = childIndices[child];

            if (childCounts[child] != 0)
            {
                if (childIndices[child] + childCounts[child] - 1 > GlimmerBLASNode::MaxIndex)
                {
                    return false;
                }

                refs[child] |= GlimmerBLASNode::RefLeafBit;
                outLeafEnds.PushBack(childIndices[child] + childCounts[child] - 1);
            }
        }

        Memory::Copy(&packed.words[0], origin, sizeof(origin));
        Memory::Copy(&packed.words[4], childBytes, sizeof(childBytes));

        packed.words[3] = exponents | ((refs[1] >> 9) << 18);
        packed.words[7] = refs[0] | ((refs[1] & 0x1FFu) << 23);
    }

    return true;
}

template <class T>
void Reorder(Array<T, GlimmerAllocator>& values, Span<const uint32> order)
{
    if (values.Size() != order.Size())
    {
        return;
    }

    Array<T, GlimmerAllocator> reordered;
    reordered.Resize(order.Size());

    for (size_t orderIndex = 0; orderIndex < order.Size(); orderIndex++)
    {
        reordered[orderIndex] = values[order[orderIndex]];
    }

    values = std::move(reordered);
}

void UpdateEntryPriority(float& entryPriority, uint32& entryPriorityFrame, float priority)
{
    const uint32 frameCounter = GetFrameCounter();

    if (entryPriorityFrame != frameCounter)
    {
        entryPriority = priority;
        entryPriorityFrame = frameCounter;
    }
    else
    {
        entryPriority = MathUtil::Min(entryPriority, priority);
    }
}

} // namespace

#pragma region GlimmerPoolAllocator

void GlimmerPoolAllocator::Reset(uint32 capacity)
{
    m_capacity = capacity;
    m_numUsed = 0;

    m_freeRanges.Clear();

    if (capacity != 0)
    {
        m_freeRanges.PushBack(Range { 0, capacity });
    }
}

uint32 GlimmerPoolAllocator::Allocate(uint32 count)
{
    if (count == 0)
    {
        return InvalidOffset;
    }

    for (size_t rangeIndex = 0; rangeIndex < m_freeRanges.Size(); rangeIndex++)
    {
        Range& range = m_freeRanges[rangeIndex];

        if (range.count < count)
        {
            continue;
        }

        const uint32 offset = range.offset;

        range.offset += count;
        range.count -= count;

        if (range.count == 0)
        {
            m_freeRanges.EraseAt(rangeIndex);
        }

        m_numUsed += count;

        return offset;
    }

    return InvalidOffset;
}

uint32 GlimmerPoolAllocator::GetLargestFreeRange() const
{
    uint32 largest = 0;

    for (const Range& range : m_freeRanges)
    {
        largest = MathUtil::Max(largest, range.count);
    }

    return largest;
}

void GlimmerPoolAllocator::Free(uint32 offset, uint32 count)
{
    if (count == 0 || offset == InvalidOffset)
    {
        return;
    }

    AssertDebug(m_numUsed >= count);
    m_numUsed -= count;

    size_t insertIndex = 0;

    while (insertIndex < m_freeRanges.Size() && m_freeRanges[insertIndex].offset < offset)
    {
        insertIndex++;
    }

    m_freeRanges.Insert(m_freeRanges.Begin() + insertIndex, Range { offset, count });

    // merge with the following range, then the preceding one
    if (insertIndex + 1 < m_freeRanges.Size()
        && m_freeRanges[insertIndex].offset + m_freeRanges[insertIndex].count == m_freeRanges[insertIndex + 1].offset)
    {
        m_freeRanges[insertIndex].count += m_freeRanges[insertIndex + 1].count;
        m_freeRanges.EraseAt(insertIndex + 1);
    }

    if (insertIndex > 0
        && m_freeRanges[insertIndex - 1].offset + m_freeRanges[insertIndex - 1].count == m_freeRanges[insertIndex].offset)
    {
        m_freeRanges[insertIndex - 1].count += m_freeRanges[insertIndex].count;
        m_freeRanges.EraseAt(insertIndex);
    }
}

#pragma endregion GlimmerPoolAllocator

#pragma region GlimmerTriangleCache

class GlimmerBuildThread final : public TaskThread
{
public:
    GlimmerBuildThread(const ThreadId& id)
        : TaskThread(id, ThreadPriorityValue::LOW)
    {
    }

    virtual ~GlimmerBuildThread() override = default;
};

GlimmerTriangleCache::GlimmerTriangleCache()
    : m_numBuildThreads(GetNumBuildThreads()),
      m_buildPool(MakeUnique<TaskThreadPool>(TypeWrapper<GlimmerBuildThread>(), "GlimmerBuild", m_numBuildThreads)),
      m_residentGeneration(0),
      m_evictionGeneration(0),
      m_numBuildsInFlight(0),
      m_lastUpdateFrame(~0u),
      m_lastPoolFullLogFrame(~0u),
      m_lodErrorScale(1.0f),
      m_lastLodErrorScaleFrame(0),
      m_poolUsageAtLodErrorScaleStep(1.0f)
{
    m_buildPool->Start();
}

// NOTE: Cleared in ~GlimmerTriangleCache.
// Because the control block lives in g_glimmerPool, a weak ref left here
// until static destruction would free into the pool after ShutdownGlimmerMemory() deleted it.
static WeakPtr<GlimmerTriangleCache> s_triangleCache;

SharedPtr<GlimmerTriangleCache> GlimmerTriangleCache::AcquireShared()
{
    AssertOnThread(g_renderThread);

    SharedPtr<GlimmerTriangleCache> triangleCache = s_triangleCache.Lock();

    if (!triangleCache)
    {
        triangleCache = MakeSharedWithAllocator<GlimmerTriangleCache, GlimmerAllocator>();
        s_triangleCache = triangleCache;
    }

    return triangleCache;
}

void GlimmerTriangleCache::UpdateOncePerFrame(Frame* frame)
{
    const uint32 frameCounter = GetFrameCounter();

    if (m_lastUpdateFrame == frameCounter)
    {
        return;
    }

    m_lastUpdateFrame = frameCounter;

    Update(frame);
}

GlimmerTriangleCache::~GlimmerTriangleCache()
{
    if (s_triangleCache.GetUnsafe() == this)
    {
        s_triangleCache = WeakPtr<GlimmerTriangleCache>();
    }

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        if (entry.buildTask.IsValid() && !entry.buildTask.IsCompleted())
        {
            entry.buildTask.Await();
        }
    }

    m_entries.Clear();

    if (m_buildPool->IsRunning())
    {
        m_buildPool->Stop();

        for (const UniquePtr<ThreadBase>& thread : m_buildPool->GetThreads())
        {
            thread->GetScheduler().WakeUpOwnerThread();
        }

        while (m_buildPool->IsRunning())
        {
            ThreadSleep(1);
        }
    }

    m_nodesBuffer.Shutdown();
    m_trianglesBuffer.Shutdown();
    m_lightmapUVsBuffer.Shutdown();
    m_texcoordsBuffer.Shutdown();
}

uint64 GlimmerTriangleCache::MakeKey(const Mesh* mesh, uint8 lodIndex)
{
    AssertDebug(mesh != nullptr);

    return (uint64(mesh->Id().Value()) << 8) | uint64(lodIndex);
}

// the shaders read these pools as StructuredBuffer<uint>, so they're sized in words
template <class T>
static StructuredBuffer CreateWordPoolBuffer(uint32 capacity)
{
    static_assert(sizeof(T) % sizeof(uint32) == 0);

    return GlimmerHelpers::CreateGlimmerStructuredBuffer(sizeof(uint32), size_t(capacity) * (sizeof(T) / sizeof(uint32)));
}

void GlimmerTriangleCache::CreatePoolBuffers()
{
    const uint32 triangleCapacity = uint32(PoolBytes / sizeof(GlimmerTriangle));
    const uint32 lightmapUVCapacity = uint32(LightmapUVPoolBytes / sizeof(GlimmerTriangleLightmapUVs));

    m_trianglesBuffer = CreateWordPoolBuffer<GlimmerTriangle>(triangleCapacity);
    m_lightmapUVsBuffer = CreateWordPoolBuffer<GlimmerTriangleLightmapUVs>(lightmapUVCapacity);

    // only bindless-supported devices sample the textures for the bricks.
    // others will use material's average
    const uint32 texcoordCapacity = RI.GetRenderConfig().bindlessTextures
        ? uint32(TexcoordPoolBytes / sizeof(GlimmerTriangleTexcoords))
        : 1u;

    m_texcoordsBuffer = CreateWordPoolBuffer<GlimmerTriangleTexcoords>(texcoordCapacity);

#ifdef HYP_RHI_DEBUG_NAMES
    m_trianglesBuffer.gpuBuffer->SetDebugName(NAME("GlimmerTriangles"));
    m_lightmapUVsBuffer.gpuBuffer->SetDebugName(NAME("GlimmerTriangleLightmapUVs"));
    m_texcoordsBuffer.gpuBuffer->SetDebugName(NAME("GlimmerTriangleTexcoords"));
#endif

    m_triangleAllocator.Reset(triangleCapacity);
    m_lightmapUVAllocator.Reset(lightmapUVCapacity);
    m_texcoordAllocator.Reset(texcoordCapacity);

    const uint64 nodePoolBytes = uint64(MathUtil::Max(g_cvGlimmerBVHNodePoolMB.Get(), 0)) * 1024ull * 1024ull;
    const uint32 nodeCapacity = uint32(nodePoolBytes / sizeof(GlimmerBLASNode));

    if (nodeCapacity != 0)
    {
        m_nodesBuffer = GlimmerHelpers::CreateGlimmerStructuredBuffer(sizeof(GlimmerBLASNode), nodeCapacity);

#ifdef HYP_RHI_DEBUG_NAMES
        m_nodesBuffer.gpuBuffer->SetDebugName(NAME("GlimmerBLASNodes"));
#endif
    }

    m_nodeAllocator.Reset(nodeCapacity);

    HYP_LOG(Rendering, Info, "Glimmer: created triangle pool ({} MB, {} triangles) and BLAS node pool ({} MB, {} nodes)",
        PoolBytes / (1024ull * 1024ull), triangleCapacity, nodePoolBytes / (1024ull * 1024ull), nodeCapacity);
}

GlimmerTriangleRequestResult GlimmerTriangleCache::Request(Mesh* mesh, uint8 lodIndex, float priority, GlimmerTriangleRef& outRef)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!mesh)
    {
        return GlimmerTriangleRequestResult::Failed;
    }

    const uint32 numLods = mesh->GetMeshDesc().GetNumLods();

    if (lodIndex >= numLods)
    {
        lodIndex = 0;
    }

    const uint64 key = MakeKey(mesh, lodIndex);

    if (KeyValuePair<uint64, UniquePtr<Entry, GlimmerAllocator>>* it = m_entries.TryGet(key))
    {
        Entry& entry = *it->second;

        // ids get reused, so make sure this is still the mesh the triangles were built from
        const bool isSameMesh = !entry.meshWeak.Expired() && entry.meshWeak.GetUnsafe() == mesh;

        if (isSameMesh && entry.state != EntryState::Evicted)
        {
            entry.lastUsedFrame = GetFrameCounter();

            UpdateEntryPriority(entry.priority, entry.priorityFrame, priority);

            if (entry.isDeferred && entry.numDefers != 0 && entry.priority < entry.deferredPriority * 0.5f)
            {
                entry.numDefers = 0;
                entry.failedFrame = GetFrameCounter() - DeferredRetryDelayFrames;
            }

            if (entry.state == EntryState::Failed && entry.dead)
            {
                return GlimmerTriangleRequestResult::Failed;
            }

            // on its way out: once the scene instances have been gathered without it, it's freed
            if (entry.isDemoted)
            {
                return GlimmerTriangleRequestResult::Pending;
            }

            if (entry.state != EntryState::Resident)
            {
                return GlimmerTriangleRequestResult::Pending;
            }

            outRef = entry.ref;

            return GlimmerTriangleRequestResult::Resident;
        }

        if (entry.state == EntryState::Building || entry.numReferences != 0)
        {
            // let the stale build finish, or wait for the scene instances still using the old one to let go, then requeue for this mesh
            entry.replacementMesh = MakeWeakRef(mesh);

            return GlimmerTriangleRequestResult::Pending;
        }

        Requeue(entry, mesh);
        UpdateEntryPriority(entry.priority, entry.priorityFrame, priority);

        return GlimmerTriangleRequestResult::Pending;
    }

    auto entry = MakeUniqueWithAllocator<Entry, GlimmerAllocator>();
    entry->state = EntryState::Queued;
    entry->mesh = MakeStrongRef(mesh);
    entry->meshWeak = MakeWeakRef(mesh);
    entry->lodIndex = lodIndex;
    entry->ref.key = key;
    entry->lastUsedFrame = GetFrameCounter();

    UpdateEntryPriority(entry->priority, entry->priorityFrame, priority);

    m_entries.Set(key, std::move(entry));

    return GlimmerTriangleRequestResult::Pending;
}

bool GlimmerTriangleCache::TryGetResident(
    Mesh* mesh,
    uint8 lodIndex,
    float priority,
    GlimmerTriangleRef& outRef)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!mesh || lodIndex >= MathUtil::Max<uint8>(mesh->GetMeshDesc().GetNumLods(), 1))
    {
        return false;
    }

    KeyValuePair<uint64, UniquePtr<Entry, GlimmerAllocator>>* it = m_entries.TryGet(MakeKey(mesh, lodIndex));

    if (!it)
    {
        return false;
    }

    Entry& entry = *it->second;

    if (entry.state != EntryState::Resident || entry.isDemoted || entry.meshWeak.Expired() || entry.meshWeak.GetUnsafe() != mesh)
    {
        return false;
    }

    entry.lastUsedFrame = GetFrameCounter();

    UpdateEntryPriority(entry.priority, entry.priorityFrame, priority);

    outRef = entry.ref;

    return true;
}

void GlimmerTriangleCache::AddReferences(Span<const uint64> keys)
{
    for (const uint64 key : keys)
    {
        if (KeyValuePair<uint64, UniquePtr<Entry, GlimmerAllocator>>* it = m_entries.TryGet(key))
        {
            it->second->numReferences++;
        }
    }
}

void GlimmerTriangleCache::RemoveReferences(Span<const uint64> keys)
{
    for (const uint64 key : keys)
    {
        if (KeyValuePair<uint64, UniquePtr<Entry, GlimmerAllocator>>* it = m_entries.TryGet(key))
        {
            Entry& entry = *it->second;

            AssertDebug(entry.numReferences != 0);

            if (entry.numReferences != 0)
            {
                entry.numReferences--;
            }

            entry.lastUsedFrame = GetFrameCounter();

            if (entry.numReferences == 0 && entry.state != EntryState::Building)
            {
                RequeueForReplacement(entry);
            }
        }
    }
}

void GlimmerTriangleCache::Requeue(Entry& entry, Mesh* mesh)
{
    AssertDebug(entry.numReferences == 0 && !entry.buildTask.IsValid());

    if (entry.state == EntryState::Resident)
    {
        FreeEntryRanges(entry);
    }

    const uint64 key = entry.ref.key;
    const uint8 lodIndex = entry.lodIndex;

    entry = {};
    entry.state = EntryState::Queued;
    entry.mesh = MakeStrongRef(mesh);
    entry.meshWeak = MakeWeakRef(mesh);
    entry.lodIndex = lodIndex;
    entry.ref.key = key;
    entry.lastUsedFrame = GetFrameCounter();
}

bool GlimmerTriangleCache::RequeueForReplacement(Entry& entry)
{
    if (!entry.replacementMesh.IsValid())
    {
        return false;
    }

    const Handle<Mesh> replacementMesh = entry.replacementMesh.Expired() ? Handle<Mesh>() : entry.replacementMesh.Lock();
    entry.replacementMesh.Reset();

    if (!replacementMesh.IsValid())
    {
        return false;
    }

    Requeue(entry, replacementMesh.Get());

    return true;
}

GlimmerTriangleCache::BuildResult GlimmerTriangleCache::BuildMeshTriangles(const Handle<Mesh>& mesh, uint8 lodIndex, bool withBLAS)
{
    HYP_SCOPE;

    BuildResult result;

    if (!mesh.IsValid())
    {
        return result;
    }

    Array<float, GlimmerAllocator> packedVertices;
    Array<uint32, GlimmerAllocator> indices;

    VertexInputLayoutDesc inputLayout = StaticVertexInputLayout<VT_Simple>;
    bool hasLightmapUVs = false;

    {
        auto resourceGuard = mesh->GetReadScope();

        if (!resourceGuard)
        {
            return result;
        }

        if (lodIndex >= mesh->GetMeshDesc().GetNumLods())
        {
            lodIndex = 0;
        }

        if (mesh->GetMeshDesc().meshAttributes.inputLayout.mask & VT_UV1)
        {
            inputLayout = StaticVertexInputLayout<VT_Simple | VT_UV1>;
            hasLightmapUVs = true;
        }

        mesh->BuildVertexBuffer(inputLayout, lodIndex, packedVertices);

        const Span<const ubyte> indexData = mesh->GetIndexData(lodIndex);
        const size_t indexSize = GpuElemTypeSize(mesh->GetMeshDesc().meshAttributes.indexBufferElemType);

        if (indexSize != sizeof(uint16) && indexSize != sizeof(uint32))
        {
            result.isFail = true;

            return result;
        }

        const size_t numIndices = indexData.Size() / indexSize;
        indices.Resize(numIndices);

        for (size_t index = 0; index < numIndices; index++)
        {
            if (indexSize == sizeof(uint32))
            {
                Memory::Copy(&indices[index], indexData.Data() + index * sizeof(uint32), sizeof(uint32));
            }
            else
            {
                uint16 index16;
                Memory::Copy(&index16, indexData.Data() + index * sizeof(uint16), sizeof(uint16));

                indices[index] = index16;
            }
        }
    }

    const uint32 packedVertexSizeInFloats = uint32(inputLayout.VertexSize() / sizeof(float));
    const uint32 numVertices = uint32(packedVertices.Size() / packedVertexSizeInFloats);

    Array<Vec3f, GlimmerAllocator> trianglePositions; // object space, three per triangle
    trianglePositions.Reserve(indices.Size());

    Array<Vec2f, GlimmerAllocator> triangleLightmapUVs; // three per triangle, when the mesh has them
    Array<Vec2f, GlimmerAllocator> triangleTexcoords; // three per triangle, on bindless devices

    const bool hasTexcoords = RI.GetRenderConfig().bindlessTextures;

    BoundingBox objectBounds;

    for (size_t firstIndex = 0; firstIndex + 2 < indices.Size(); firstIndex += 3)
    {
        Vec3f positions[3];
        Vec2f lightmapUVs[3];
        Vec2f texcoords[3];

        Vec3f normalSum = Vec3f::Zero();
        
        bool isValid = true;

        for (uint32 corner = 0; corner < 3; corner++)
        {
            const uint32 vertexIndex = indices[firstIndex + corner];

            if (vertexIndex >= numVertices)
            {
                isValid = false;

                break;
            }

            const float* vertex = packedVertices.Data() + size_t(vertexIndex) * packedVertexSizeInFloats;
            
            positions[corner] = Vec3f(vertex[0], vertex[1], vertex[2]);
            normalSum += Vec3f(vertex[3], vertex[4], vertex[5]);
            texcoords[corner] = Vec2f(vertex[TexcoordOffsetInFloats], vertex[TexcoordOffsetInFloats + 1]);

            if (hasLightmapUVs)
            {
                lightmapUVs[corner] = Vec2f(vertex[LightmapUVOffsetInFloats], vertex[LightmapUVOffsetInFloats + 1]);
            }
        }

        if (!isValid || !MathUtil::IsFinite(positions[0]) || !MathUtil::IsFinite(positions[1]) || !MathUtil::IsFinite(positions[2]))
        {
            continue;
        }

        // wind every triangle so cross(edge1, edge2) faces the way its vertex normals do, so front and back faces
        // mean the same thing for every asset no matter which winding it was authored with
        if ((positions[1] - positions[0]).Cross(positions[2] - positions[0]).Dot(normalSum) < 0.0f)
        {
            std::swap(positions[1], positions[2]);
            std::swap(lightmapUVs[1], lightmapUVs[2]);
            std::swap(texcoords[1], texcoords[2]);
        }

        const Vec3f edge1 = positions[1] - positions[0];
        const Vec3f edge2 = positions[2] - positions[0];

        // Zero area triangles can never be hit
        if (!(edge1.Cross(edge2).LengthSquared() > 0.0f))
        {
            continue;
        }

        for (uint32 corner = 0; corner < 3; corner++)
        {
            trianglePositions.PushBack(positions[corner]);

            if (hasTexcoords)
            {
                triangleTexcoords.PushBack(texcoords[corner]);
            }

            if (hasLightmapUVs)
            {
                triangleLightmapUVs.PushBack(lightmapUVs[corner]);
            }
        }

        objectBounds = objectBounds.Union(positions[0]).Union(positions[1]).Union(positions[2]);
    }

    if (trianglePositions.Empty())
    {
        result.isFail = !indices.Empty();

        return result;
    }

    result.localBounds = objectBounds;
    result.gridOrigin = objectBounds.min;

    for (int axis = 0; axis < 3; axis++)
    {
        const float extent = objectBounds.max[axis] - objectBounds.min[axis];

        result.gridScale[axis] = extent > 0.0f ? extent / float(GlimmerTriangle::GridMax) : 1.0f;
    }

    result.triangles.Reserve(trianglePositions.Size() / 3);

    for (size_t firstPosition = 0; firstPosition + 2 < trianglePositions.Size(); firstPosition += 3)
    {
        Vec3f gridPositions[3];

        for (uint32 corner = 0; corner < 3; corner++)
        {
            for (int axis = 0; axis < 3; axis++)
            {
                const float grid = std::round((trianglePositions[firstPosition + corner][axis] - result.gridOrigin[axis]) / result.gridScale[axis]);

                gridPositions[corner][axis] = MathUtil::Clamp(grid, 0.0f, float(GlimmerTriangle::GridMax));
            }
        }

        // triangles that collapse on the grid can't be hit either
        if (!((gridPositions[1] - gridPositions[0]).Cross(gridPositions[2] - gridPositions[0]).LengthSquared() > 0.0f))
        {
            continue;
        }

        GlimmerTriangle& triangle = result.triangles.EmplaceBack();

        for (uint32 corner = 0; corner < 3; corner++)
        {
            for (int axis = 0; axis < 3; axis++)
            {
                triangle.grid[corner * 3 + axis] = uint16(gridPositions[corner][axis]);
            }
        }

        triangle.leafFlags = 0;

        if (hasLightmapUVs)
        {
            GlimmerTriangleLightmapUVs& packedUVs = result.lightmapUVs.EmplaceBack();

            for (uint32 corner = 0; corner < 3; corner++)
            {
                const Vec2f uv = triangleLightmapUVs[firstPosition + corner];

                for (int axis = 0; axis < 2; axis++)
                {
                    const float value = std::isfinite(uv[axis]) ? MathUtil::Clamp(uv[axis], 0.0f, 1.0f) : 0.0f;

                    packedUVs.uvs[corner * 2 + axis] = uint16(value * 65535.0f + 0.5f);
                }
            }
        }

        if (hasTexcoords)
        {
            result.texcoords.PushBack(PackTexCoords(&triangleTexcoords[firstPosition]));
        }
    }

    if (result.triangles.Empty())
    {
        result.isFail = true;

        return result;
    }

    if (!withBLAS)
    {
        return result;
    }

    Array<BoundingBox, DynamicAllocator> triangleBounds;
    triangleBounds.Reserve(result.triangles.Size());

    for (const GlimmerTriangle& triangle : result.triangles)
    {
        BoundingBox bounds;

        for (uint32 corner = 0; corner < 3; corner++)
        {
            bounds = bounds.Union(Vec3f(float(triangle.grid[corner * 3]), float(triangle.grid[corner * 3 + 1]), float(triangle.grid[corner * 3 + 2])));
        }

        triangleBounds.PushBack(bounds);
    }

    SAHBVHBuildParams buildParams;
    buildParams.minLeafSize = BLASMinLeafSize;
    buildParams.maxLeafSize = BLASMaxLeafSize;

    Array<SAHBVHNode, DynamicAllocator> bvhNodes;
    Array<uint32, DynamicAllocator> triangleOrder;

    SAHBVHBuilder::Build(triangleBounds.ToSpan(), buildParams, bvhNodes, triangleOrder);

    Reorder(result.triangles, triangleOrder);
    Reorder(result.lightmapUVs, triangleOrder);
    Reorder(result.texcoords, triangleOrder);

    result.depth = SAHBVHBuilder::CalculateDepth(bvhNodes.ToSpan());

    Array<uint32, GlimmerAllocator> leafEnds;

    if (!PackBLAS(bvhNodes.ToSpan(), result.nodes, leafEnds))
    {
        // more triangles or nodes than a child ref can address, which is more than the pool holds anyway
        result = BuildResult {};
        result.isFail = true;

        return result;
    }

    for (const uint32 leafEnd : leafEnds)
    {
        result.triangles[leafEnd].leafFlags |= uint16(GlimmerBLASNode::LeafEndFlag);
    }

    return result;
}

bool GlimmerTriangleCache::AllocateEntry(Entry& entry)
{
    const uint32 triangleCount = uint32(entry.result.triangles.Size());
    const uint32 nodeCount = uint32(entry.result.nodes.Size());

    const uint32 nodeBase = nodeCount != 0 ? m_nodeAllocator.Allocate(nodeCount) : 0u;
    const uint32 triangleBase = nodeBase != GlimmerPoolAllocator::InvalidOffset
        ? m_triangleAllocator.Allocate(triangleCount)
        : GlimmerPoolAllocator::InvalidOffset;

    if (triangleBase == GlimmerPoolAllocator::InvalidOffset
        && nodeCount != 0
        && nodeBase != GlimmerPoolAllocator::InvalidOffset)
    {
        m_nodeAllocator.Free(nodeBase, nodeCount);
    }

    if (triangleBase != GlimmerPoolAllocator::InvalidOffset)
    {
        entry.ref.nodeBase = nodeBase;
        entry.ref.nodeCount = nodeCount;
        entry.ref.triangleBase = triangleBase;
        entry.ref.triangleCount = triangleCount;

        // a mesh the pool has no room for goes without, and is lit as if it had no lightmap
        entry.ref.lightmapUVBase = entry.result.lightmapUVs.Size() == triangleCount
            ? m_lightmapUVAllocator.Allocate(triangleCount)
            : GlimmerPoolAllocator::InvalidOffset;

        // and without these, its bricks take its material's average color
        entry.ref.texcoordBase = entry.result.texcoords.Size() == triangleCount
            ? m_texcoordAllocator.Allocate(triangleCount)
            : GlimmerPoolAllocator::InvalidOffset;

        return true;
    }

    return false;
}

bool GlimmerTriangleCache::EvictOne()
{
    const uint32 frameCounter = GetFrameCounter();

    uint64 evictKey = 0;
    Entry* evictEntry = nullptr;

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        if (entry.state != EntryState::Resident || entry.numReferences != 0 || entry.isDemoted)
        {
            continue;
        }

        if (frameCounter - entry.lastUsedFrame < MinFramesBeforeEviction)
        {
            continue;
        }

        if (!evictEntry || entry.lastUsedFrame < evictEntry->lastUsedFrame)
        {
            evictKey = it.first;
            evictEntry = &entry;
        }
    }

    if (!evictEntry)
    {
        return false;
    }

    FreeEntryRanges(*evictEntry);

    // erased after the Update() loop, which may be iterating the entries
    evictEntry->state = EntryState::Evicted;
    m_keysToErase.PushBack(evictKey);

    return true;
}

bool GlimmerTriangleCache::IsWorthBuilding(const Entry& entry, uint32 frameCounter, float farthestDemotablePriority) const
{
    if (!entry.isDeferred)
    {
        return true;
    }

    const uint32 retryDelay = DeferredRetryDelayFrames << MathUtil::Min(entry.numDefers, MaxDeferredRetryShift);

    if (frameCounter - entry.failedFrame < retryDelay)
    {
        return false;
    }

    const bool fits = m_triangleAllocator.GetLargestFreeRange() >= entry.lastTriangleCount
        && (entry.lastNodeCount == 0 || m_nodeAllocator.GetLargestFreeRange() >= entry.lastNodeCount);

    return fits || farthestDemotablePriority > entry.priority * DemotionDistanceRatio + DemotionDistanceMargin;
}

bool GlimmerTriangleCache::MakeContiguousRoom(const Entry& requester, uint32 nodeCount, uint32 triangleCount, uint32& numDemotions)
{
    const float threshold = requester.priority * DemotionDistanceRatio + DemotionDistanceMargin;

    Array<Entry*, GlimmerTempAllocator> residents;

    for (auto& it : m_entries)
    {
        if (it.second->state == EntryState::Resident)
        {
            residents.PushBack(it.second.Get());
        }
    }

    const auto findStretch = [&](
        bool isNodes,
        uint32 count,
        uint32 capacity,
        Array<Entry*, GlimmerTempAllocator>& outVictims) -> bool
    {
        const auto getBaseOf = [isNodes](const Entry* entry)
        {
            return isNodes ? entry->ref.nodeBase : entry->ref.triangleBase;
        };

        const auto getCountOf = [isNodes](const Entry* entry)
        {
            return isNodes ? entry->ref.nodeCount : entry->ref.triangleCount;
        };

        std::sort(residents.Begin(), residents.End(), [&getBaseOf](const Entry* lhs, const Entry* rhs)
            {
                return getBaseOf(lhs) < getBaseOf(rhs);
            });

        uint64 bestCost = ~0ull;
        uint32 bestStart = 0;

        // the cheapest stretch starts at the start of the pool or right after a mesh's triangles
        for (size_t first = 0; first <= residents.Size(); first++)
        {
            const uint32 start = first == 0 ? 0u : getBaseOf(residents[first - 1]) + getCountOf(residents[first - 1]);

            if (uint64(start) + count > capacity)
            {
                break;
            }

            uint64 cost = 0;
            bool isBlocked = false;

            for (size_t index = first; index < residents.Size() && getBaseOf(residents[index]) < start + count; index++)
            {
                const Entry* occupant = residents[index];

                if (occupant->isDemoted)
                {
                    continue;
                }

                if (occupant->priority <= threshold)
                {
                    isBlocked = true;

                    break;
                }

                cost += uint64(occupant->ref.triangleCount) + 1;
            }

            if (!isBlocked && cost < bestCost)
            {
                bestCost = cost;
                bestStart = start;

                if (cost == 0)
                {
                    break;
                }
            }
        }

        if (bestCost == ~0ull)
        {
            return false;
        }

        for (Entry* occupant : residents)
        {
            if (!occupant->isDemoted
                && getBaseOf(occupant) < bestStart + count
                && getBaseOf(occupant) + getCountOf(occupant) > bestStart)
            {
                outVictims.PushBack(occupant);
            }
        }

        return true;
    };

    Array<Entry*, GlimmerTempAllocator> victims;

    if ((nodeCount != 0 && !findStretch(true, nodeCount, m_nodeAllocator.GetCapacity(), victims))
        || !findStretch(false, triangleCount, m_triangleAllocator.GetCapacity(), victims))
    {
        return false;
    }

    for (Entry* victim : victims)
    {
        if (!victim->isDemoted)
        {
            victim->isDemoted = true;
            numDemotions++;
        }
    }

    return true;
}

void GlimmerTriangleCache::Defer(Entry& entry, uint32 frameCounter)
{
    entry.lastNodeCount = uint32(entry.result.nodes.Size());
    entry.lastTriangleCount = uint32(entry.result.triangles.Size());
    entry.result = BuildResult {};
    entry.mesh = entry.meshWeak.Lock();
    entry.failedFrame = frameCounter;

    if (!entry.mesh.IsValid())
    {
        entry.state = EntryState::Evicted;
        m_keysToErase.PushBack(entry.ref.key);

        return;
    }

    entry.state = EntryState::Queued;
    entry.isDeferred = true;
    entry.deferredPriority = entry.priority;
    entry.numDefers++;
}

void GlimmerTriangleCache::FreeEntryRanges(Entry& entry)
{
    m_deferredFrees.PushBack(DeferredFree {
        entry.ref.nodeBase,
        entry.ref.nodeCount,
        entry.ref.triangleBase,
        entry.ref.triangleCount,
        GetFrameCounter(),
        entry.ref.lightmapUVBase,
        entry.ref.lightmapUVBase != GlimmerPoolAllocator::InvalidOffset ? entry.ref.triangleCount : 0u,
        entry.ref.texcoordBase,
        entry.ref.texcoordBase != GlimmerPoolAllocator::InvalidOffset ? entry.ref.triangleCount : 0u });

    entry.ref.nodeCount = 0;
    entry.ref.triangleCount = 0;
    entry.ref.lightmapUVBase = GlimmerPoolAllocator::InvalidOffset;
    entry.ref.texcoordBase = GlimmerPoolAllocator::InvalidOffset;
}

void GlimmerTriangleCache::UploadEntry(Frame* frame, Entry& entry)
{
    CommandRecorder& cr = frame->cr;

    const size_t nodesByteSize = entry.ref.nodeCount != 0 ? entry.result.nodes.ByteSize() : 0;
    const size_t trianglesByteSize = entry.result.triangles.ByteSize();
    const size_t lightmapUVsByteSize = entry.ref.lightmapUVBase != GlimmerPoolAllocator::InvalidOffset ? entry.result.lightmapUVs.ByteSize() : 0;
    const size_t texcoordsByteSize = entry.ref.texcoordBase != GlimmerPoolAllocator::InvalidOffset ? entry.result.texcoords.ByteSize() : 0;
    const size_t texcoordsOffset = trianglesByteSize + lightmapUVsByteSize;
    const size_t nodesOffset = texcoordsOffset + texcoordsByteSize;

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(nodesOffset + nodesByteSize);
    Assert(stagingBuffer != nullptr);

    stagingBuffer->Copy(0, trianglesByteSize, entry.result.triangles.Data());

    if (lightmapUVsByteSize != 0)
    {
        stagingBuffer->Copy(trianglesByteSize, lightmapUVsByteSize, entry.result.lightmapUVs.Data());
    }

    if (texcoordsByteSize != 0)
    {
        stagingBuffer->Copy(texcoordsOffset, texcoordsByteSize, entry.result.texcoords.Data());
    }

    if (nodesByteSize != 0)
    {
        stagingBuffer->Copy(nodesOffset, nodesByteSize, entry.result.nodes.Data());
    }

    stagingBuffer->Flush(0, nodesOffset + nodesByteSize);

    cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
    cr << InsertBarrier(m_trianglesBuffer, ResourceState::CopyDst);

    cr << CopyBuffer(stagingBuffer, m_trianglesBuffer.gpuBuffer,
        0,
        uint32(size_t(entry.ref.triangleBase) * sizeof(GlimmerTriangle)),
        uint32(trianglesByteSize));

    if (lightmapUVsByteSize != 0)
    {
        cr << InsertBarrier(m_lightmapUVsBuffer, ResourceState::CopyDst);

        cr << CopyBuffer(stagingBuffer, m_lightmapUVsBuffer.gpuBuffer,
            uint32(trianglesByteSize),
            uint32(size_t(entry.ref.lightmapUVBase) * sizeof(GlimmerTriangleLightmapUVs)),
            uint32(lightmapUVsByteSize));
    }

    if (texcoordsByteSize != 0)
    {
        cr << InsertBarrier(m_texcoordsBuffer, ResourceState::CopyDst);

        cr << CopyBuffer(stagingBuffer, m_texcoordsBuffer.gpuBuffer,
            uint32(texcoordsOffset),
            uint32(size_t(entry.ref.texcoordBase) * sizeof(GlimmerTriangleTexcoords)),
            uint32(texcoordsByteSize));
    }

    if (nodesByteSize != 0)
    {
        cr << InsertBarrier(m_nodesBuffer, ResourceState::CopyDst);

        cr << CopyBuffer(stagingBuffer, m_nodesBuffer.gpuBuffer,
            uint32(nodesOffset),
            uint32(size_t(entry.ref.nodeBase) * sizeof(GlimmerBLASNode)),
            uint32(nodesByteSize));
    }

    entry.ref.depth = entry.result.depth;
    entry.ref.localBounds = entry.result.localBounds;
    entry.ref.gridOrigin = entry.result.gridOrigin;
    entry.ref.gridScale = entry.result.gridScale;

    entry.result = BuildResult {};
    entry.state = EntryState::Resident;
}

void GlimmerTriangleCache::Update(Frame* frame)
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    if (!IsReady())
    {
        CreatePoolBuffers();

        frame->cr << InsertBarrier(m_lightmapUVsBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
        frame->cr << InsertBarrier(m_texcoordsBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);

        if (m_nodesBuffer.IsInitialized())
        {
            frame->cr << InsertBarrier(m_nodesBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
        }
    }

    const uint32 frameCounter = GetFrameCounter();

    for (size_t freeIndex = 0; freeIndex < m_deferredFrees.Size();)
    {
        const DeferredFree& deferredFree = m_deferredFrees[freeIndex];

        if (frameCounter - deferredFree.frameFreed < FreedRangeReuseDelayFrames)
        {
            ++freeIndex;

            continue;
        }

        if (deferredFree.nodeCount != 0)
        {
            m_nodeAllocator.Free(deferredFree.nodeOffset, deferredFree.nodeCount);
        }

        m_triangleAllocator.Free(deferredFree.triangleOffset, deferredFree.triangleCount);
        m_lightmapUVAllocator.Free(deferredFree.lightmapUVOffset, deferredFree.lightmapUVCount);
        m_texcoordAllocator.Free(deferredFree.texcoordOffset, deferredFree.texcoordCount);

        m_deferredFrees.EraseAt(freeIndex);
    }

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        if (entry.isDemoted && entry.state == EntryState::Resident && entry.numReferences == 0)
        {
            FreeEntryRanges(entry);

            entry.state = EntryState::Evicted;
            m_keysToErase.PushBack(it.first);
        }
    }

    float farthestDemotablePriority = 0.0f;

    uint32 pendingDemotedTriangles = 0;

    Array<Entry*, GlimmerTempAllocator> queued;
    Array<Entry*, GlimmerTempAllocator> pendingUploads;

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        if (entry.state == EntryState::Resident)
        {
            if (entry.isDemoted)
            {
                pendingDemotedTriangles += entry.ref.triangleCount;
            }
            else if (entry.numReferences != 0)
            {
                farthestDemotablePriority = MathUtil::Max(farthestDemotablePriority, entry.priority);
            }
        }
        else if (entry.state == EntryState::Queued)
        {
            queued.PushBack(&entry);
        }
    }

    const auto byPriority = [](const Entry* lhs, const Entry* rhs)
    {
        return lhs->priority < rhs->priority;
    };

    // nearest first
    std::sort(queued.Begin(), queued.End(), byPriority);

    for (Entry* entryPtr : queued)
    {
        if (m_numBuildsInFlight >= m_numBuildThreads * BuildsInFlightPerThread)
        {
            break;
        }

        Entry& entry = *entryPtr;

        if (!IsWorthBuilding(entry, frameCounter, farthestDemotablePriority))
        {
            continue;
        }

        entry.isDeferred = false;

        entry.buildTask = m_buildPool->Enqueue(
            [mesh = entry.mesh, lodIndex = entry.lodIndex, withBLAS = HasBLAS()]() -> BuildResult
            {
                return BuildMeshTriangles(mesh, lodIndex, withBLAS);
            });

        entry.state = EntryState::Building;
        m_numBuildsInFlight++;
    }

    for (auto& it : m_entries)
    {
        Entry& entry = *it.second;

        switch (entry.state)
        {
        case EntryState::Building:
        {
            if (!entry.buildTask.IsCompleted())
            {
                break;
            }

            entry.result = std::move(entry.buildTask).Await();
            entry.buildTask = Task<BuildResult>();
            entry.mesh = Handle<Mesh>();

            AssertDebug(m_numBuildsInFlight != 0);
            m_numBuildsInFlight--;

            // built from a mesh whose id has since been reused
            if (RequeueForReplacement(entry))
            {
                break;
            }

            if (entry.result.triangles.Empty())
            {
                entry.state = EntryState::Failed;
                entry.failedFrame = frameCounter;

                if ((entry.dead = entry.result.isFail))
                {
                    HYP_LOG(Rendering, Warning, "Glimmer: mesh {} (LOD {}) has no traceable triangles", it.first >> 8, entry.lodIndex);
                }

                break;
            }

            if (entry.result.triangles.Size() > m_triangleAllocator.GetCapacity())
            {
                HYP_LOG(Rendering, Warning, "Glimmer: mesh {} (LOD {}) has {} triangles, more than the triangle pool holds",
                    it.first >> 8, entry.lodIndex, entry.result.triangles.Size());

                entry.result = BuildResult {};
                entry.state = EntryState::Failed;
                entry.failedFrame = frameCounter;
                entry.dead = true;

                break;
            }

            entry.state = EntryState::PendingUpload;

            break;
        }
        case EntryState::Failed:
        {
            if (entry.dead)
            {
                if (entry.meshWeak.Expired())
                {
                    entry.state = EntryState::Evicted;
                    m_keysToErase.PushBack(it.first);
                }

                break;
            }

            if (frameCounter - entry.failedFrame < FailedRetryDelayFrames)
            {
                break;
            }

            entry.mesh = entry.meshWeak.Lock();

            if (entry.mesh.IsValid())
            {
                entry.state = EntryState::Queued;
                entry.lastUsedFrame = frameCounter;
            }
            else
            {
                entry.state = EntryState::Evicted;
                m_keysToErase.PushBack(it.first);
            }

            break;
        }
        default:
            break;
        }

        if (entry.state == EntryState::PendingUpload)
        {
            pendingUploads.PushBack(&entry);
        }
    }

    std::sort(pendingUploads.Begin(), pendingUploads.End(), byPriority);

    uint32 availableTriangles = m_triangleAllocator.GetCapacity() - m_triangleAllocator.GetNumUsed() + pendingDemotedTriangles;

    for (const DeferredFree& deferredFree : m_deferredFrees)
    {
        availableTriangles += deferredFree.triangleCount;
    }

    size_t uploadedBytes = 0;
    bool uploadedAny = false;

    uint32 numDemotions = 0;
    uint32 numDeferred = 0;
    uint32 numWaiting = 0;

    const Entry* nearestWaiting = nullptr;

    bool isNearerWaiting = false;

    for (const Entry* entryPtr : queued)
    {
        if (entryPtr->isDeferred && entryPtr->state == EntryState::Queued)
        {
            nearestWaiting = entryPtr;

            break;
        }
    }

    for (Entry* entryPtr : pendingUploads)
    {
        Entry& entry = *entryPtr;

        const uint32 triangleCount = uint32(entry.result.triangles.Size());
        const size_t entryBytes = entry.result.triangles.ByteSize();

        const bool isWithinBudget = !uploadedAny || uploadedBytes + entryBytes <= UploadBudgetBytes;

        if (!isWithinBudget || isNearerWaiting)
        {
            availableTriangles -= MathUtil::Min(availableTriangles, triangleCount);

            continue;
        }

        if (AllocateEntry(entry))
        {
            entry.numDefers = 0;

            UploadEntry(frame, entry);

            uploadedBytes += entryBytes;
            uploadedAny = true;

            availableTriangles -= MathUtil::Min(availableTriangles, triangleCount);

            m_residentGeneration++;

            continue;
        }

        if (!nearestWaiting || entry.priority < nearestWaiting->priority)
        {
            nearestWaiting = &entry;
        }

        // unused meshes go first
        while (availableTriangles < triangleCount)
        {
            const size_t numDeferredFrees = m_deferredFrees.Size();

            if (!EvictOne())
            {
                break;
            }

            for (size_t freeIndex = numDeferredFrees; freeIndex < m_deferredFrees.Size(); freeIndex++)
            {
                availableTriangles += m_deferredFrees[freeIndex].triangleCount;
            }
        }

        if (frameCounter - entry.roomRequestFrame >= RoomRequestIntervalFrames)
        {
            entry.roomRequestFrame = frameCounter;

            if (!MakeContiguousRoom(entry, uint32(entry.result.nodes.Size()), triangleCount, numDemotions))
            {
                Defer(entry, frameCounter);
                numDeferred++;

                continue;
            }
        }

        availableTriangles -= MathUtil::Min(availableTriangles, triangleCount);

        isNearerWaiting = true;
        numWaiting++;
    }

    if (numDemotions != 0)
    {
        m_evictionGeneration++;
    }

    if (frameCounter - m_lastLodErrorScaleFrame >= LodErrorScaleIntervalFrames)
    {
        m_lastLodErrorScaleFrame = frameCounter;

        const float poolUsage = float(m_triangleAllocator.GetNumUsed()) / float(MathUtil::Max(m_triangleAllocator.GetCapacity(), 1u));

        const float previousScale = m_lodErrorScale;

        if (nearestWaiting != nullptr)
        {
            if (m_lodErrorScale == 1.0f || poolUsage < m_poolUsageAtLodErrorScaleStep - LodErrorScaleMinUsageDrop)
            {
                m_lodErrorScale = MathUtil::Min(m_lodErrorScale * LodErrorScaleStepUp, MaxLodErrorScale);
                m_poolUsageAtLodErrorScaleStep = poolUsage;
            }
        }
        else if (poolUsage < LodErrorScaleRelaxUsage)
        {
            m_lodErrorScale = MathUtil::Max(m_lodErrorScale / LodErrorScaleStepDown, 1.0f);
            m_poolUsageAtLodErrorScaleStep = poolUsage;
        }

        if (m_lodErrorScale != previousScale)
        {
            m_evictionGeneration++;
        }
    }

    if (nearestWaiting && (m_lastPoolFullLogFrame == ~0u || frameCounter - m_lastPoolFullLogFrame >= PoolFullLogIntervalFrames))
    {
        m_lastPoolFullLogFrame = frameCounter;

        const bool isBuilt = nearestWaiting->state == EntryState::PendingUpload;

        HYP_LOG(Rendering, Warning, "Glimmer triangle pool full (triangles {}/{}, largest free {}); {} built and {} more waiting for room, nearest {} m away ({} triangles)",
            m_triangleAllocator.GetNumUsed(), m_triangleAllocator.GetCapacity(), m_triangleAllocator.GetLargestFreeRange(),
            numWaiting, numDeferred, nearestWaiting->priority,
            isBuilt ? uint32(nearestWaiting->result.triangles.Size()) : nearestWaiting->lastTriangleCount);
    }

    for (const uint64 keyToErase : m_keysToErase)
    {
        if (KeyValuePair<uint64, UniquePtr<Entry, GlimmerAllocator>>* it = m_entries.TryGet(keyToErase))
        {
            if (it->second->state == EntryState::Evicted)
            {
                m_entries.Erase(keyToErase);
            }
        }
    }

    m_keysToErase.Clear();

    if (uploadedAny)
    {
        frame->cr << InsertBarrier(m_trianglesBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
        frame->cr << InsertBarrier(m_lightmapUVsBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);

        if (m_nodesBuffer.IsInitialized())
        {
            frame->cr << InsertBarrier(m_nodesBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
        }
        frame->cr << InsertBarrier(m_texcoordsBuffer, ResourceState::ShaderResource, ShaderModuleType::Compute);
    }
}

GlimmerTriangleCacheStats GlimmerTriangleCache::GetStats() const
{
    GlimmerTriangleCacheStats stats;

    for (const auto& it : m_entries)
    {
        const Entry& entry = *it.second;

        switch (entry.state)
        {
        case EntryState::Queued:
            if (entry.isDeferred)
            {
                stats.numWaitingForRoom++;
            }
            else
            {
                stats.numBuilding++;
            }
            break;
        case EntryState::Building:
            stats.numBuilding++;
            break;
        case EntryState::PendingUpload:
            stats.numPendingUpload++;
            break;
        case EntryState::Resident:
            stats.numResident++;
            stats.numResidentTriangles += entry.ref.triangleCount;
            stats.numDemoted += entry.isDemoted ? 1u : 0u;
            break;
        case EntryState::Failed:
            stats.numFailed++;
            break;
        }
    }

    stats.nodesUsed = m_nodeAllocator.GetNumUsed();
    stats.nodesCapacity = m_nodeAllocator.GetCapacity();
    stats.trianglesUsed = m_triangleAllocator.GetNumUsed();
    stats.trianglesCapacity = m_triangleAllocator.GetCapacity();
    stats.lightmapUVsUsed = m_lightmapUVAllocator.GetNumUsed();
    stats.lightmapUVsCapacity = m_lightmapUVAllocator.GetCapacity();
    stats.texcoordsUsed = m_texcoordAllocator.GetNumUsed();
    stats.texcoordsCapacity = m_texcoordAllocator.GetCapacity();

    return stats;
}

#pragma endregion GlimmerTriangleCache

} // namespace Hyperion
