/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <HyperionPch.hpp>

#include <Baking/PathTracer/PathTracerBVH.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/CommandRecorder.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/RenderProxy.hpp>
#include <Rendering/Buffers.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Scene/Scene.hpp>
#include <Scene/Entity.hpp>

#include <Framework/EngineGlobals.hpp>

#include <Core/Math/BoundingBox.hpp>
#include <Core/Math/MathUtil.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/Threads.hpp>

#include <Core/Profiling/ProfileScope.hpp>

#include <algorithm>

namespace Hyperion {

namespace Baking {

#pragma region PathTracerBVHBuilder

static constexpr uint32 NumSplitBins = 16;

// Ranges this small are always leaves, ranges larger than MaxLeafTriangles are always split
static constexpr uint32 MinLeafTriangles = 2;
static constexpr uint32 MaxLeafTriangles = 8;

// Past this depth splits are made at the median, which keeps the tree within the traversal stack (BVH_STACK_SIZE = 64)
static constexpr uint32 MaxSahSplitDepth = 32;

static constexpr float SplitTraversalCost = 1.0f;

static inline float GetSurfaceArea(const BoundingBox& bounds)
{
    const Vec3f extent = Vec3f::Max(bounds.max - bounds.min, Vec3f::Zero());

    return 2.0f * (extent.x * extent.y + extent.y * extent.z + extent.z * extent.x);
}

static HYP_FORCE_INLINE uint32 GetSplitBinIndex(float centroid, float binMin, float binScale)
{
    return MathUtil::Min(uint32((centroid - binMin) * binScale), NumSplitBins - 1);
}

class PathTracerBVHBuildContext
{
public:
    explicit PathTracerBVHBuildContext(Span<const PathTracerTriangle> triangles)
    {
        const uint32 numTriangles = uint32(triangles.Size());

        m_triangleBounds.Resize(numTriangles);
        m_triangleCentroids.Resize(numTriangles);
        m_triangleOrder.Resize(numTriangles);

        BoundingBox sceneBounds;

        for (uint32 triangleIndex = 0; triangleIndex < numTriangles; triangleIndex++)
        {
            const PathTracerTriangle& triangle = triangles[triangleIndex];

            const Vec3f position0 = Vec3f(triangle.position0[0], triangle.position0[1], triangle.position0[2]);
            const Vec3f position1 = position0 + Vec3f(triangle.edge1[0], triangle.edge1[1], triangle.edge1[2]);
            const Vec3f position2 = position0 + Vec3f(triangle.edge2[0], triangle.edge2[1], triangle.edge2[2]);

            const BoundingBox triangleBounds = BoundingBox(position0, position0).Union(position1).Union(position2);

            m_triangleBounds[triangleIndex] = triangleBounds;
            m_triangleCentroids[triangleIndex] = (triangleBounds.min + triangleBounds.max) * 0.5f;
            m_triangleOrder[triangleIndex] = triangleIndex;

            sceneBounds = sceneBounds.Union(triangleBounds);
        }

        // Flat bounds (e.g around an axis aligned floor) can be missed by rounding in the slab test without some padding
        const float sceneMagnitude = MathUtil::Max(
            MathUtil::Max(MathUtil::Abs(sceneBounds.min.x), MathUtil::Abs(sceneBounds.min.y), MathUtil::Abs(sceneBounds.min.z)),
            MathUtil::Max(MathUtil::Abs(sceneBounds.max.x), MathUtil::Abs(sceneBounds.max.y), MathUtil::Abs(sceneBounds.max.z)),
            1.0f);

        m_boundsPadding = sceneMagnitude * 1e-5f;
    }

    void Build(Array<PathTracerBVHNode>& outNodes, Array<uint32>& outTriangleOrder)
    {
        const uint32 numTriangles = uint32(m_triangleOrder.Size());

        m_nodes.Clear();

        if (numTriangles == 1)
        {
            // Traversal always starts at an interior node, so a single triangle becomes both children of the root
            const ChildReference leaf { m_triangleBounds[0], 0, 1 };

            PathTracerBVHNode& root = m_nodes.EmplaceBack();
            WriteChild(leaf, root.leftMin, root.leftMax, root.leftIndex, root.leftCount);
            WriteChild(leaf, root.rightMin, root.rightMax, root.rightIndex, root.rightCount);
        }
        else if (numTriangles > 1)
        {
            uint32 middle = 0;

            if (!FindSplit(0, numTriangles, ComputeBounds(0, numTriangles), 0, middle))
            {
                middle = SplitAtMedian(0, numTriangles, ComputeCentroidBounds(0, numTriangles));
            }

            BuildNode(0, middle, numTriangles, 0);
        }

        outNodes = std::move(m_nodes);
        outTriangleOrder = std::move(m_triangleOrder);
    }

private:
    struct ChildReference
    {
        BoundingBox bounds;
        uint32 index;
        uint32 count;
    };

    struct SplitBin
    {
        BoundingBox bounds;
        uint32 count = 0;
    };

    BoundingBox ComputeBounds(uint32 begin, uint32 end) const
    {
        BoundingBox bounds;

        for (uint32 orderIndex = begin; orderIndex < end; orderIndex++)
        {
            bounds = bounds.Union(m_triangleBounds[m_triangleOrder[orderIndex]]);
        }

        return bounds;
    }

    BoundingBox ComputeCentroidBounds(uint32 begin, uint32 end) const
    {
        BoundingBox centroidBounds;

        for (uint32 orderIndex = begin; orderIndex < end; orderIndex++)
        {
            centroidBounds = centroidBounds.Union(m_triangleCentroids[m_triangleOrder[orderIndex]]);
        }

        return centroidBounds;
    }

    /*! \brief Partitions [begin, end) in two using binned SAH, falling back to a median split.
     *  \return false if the range should be a leaf instead. */
    bool FindSplit(uint32 begin, uint32 end, const BoundingBox& bounds, uint32 depth, uint32& outMiddle)
    {
        const uint32 count = end - begin;
        const BoundingBox centroidBounds = ComputeCentroidBounds(begin, end);

        if (depth < MaxSahSplitDepth)
        {
            const float parentArea = MathUtil::Max(GetSurfaceArea(bounds), 1e-20f);

            uint32 bestAxis = ~0u;
            uint32 bestBin = 0;
            float bestCost = MathUtil::MaxSafeValue<float>();

            for (uint32 axis = 0; axis < 3; axis++)
            {
                const float centroidExtent = centroidBounds.max[axis] - centroidBounds.min[axis];

                if (!(centroidExtent > 0.0f))
                {
                    continue;
                }

                const float binScale = float(NumSplitBins) / centroidExtent;

                for (SplitBin& bin : m_bins)
                {
                    bin = SplitBin {};
                }

                for (uint32 orderIndex = begin; orderIndex < end; orderIndex++)
                {
                    const uint32 triangleIndex = m_triangleOrder[orderIndex];

                    SplitBin& bin = m_bins[GetSplitBinIndex(m_triangleCentroids[triangleIndex][axis], centroidBounds.min[axis], binScale)];
                    bin.bounds = bin.bounds.Union(m_triangleBounds[triangleIndex]);
                    bin.count++;
                }

                // Split i puts bins [0, i] on the left and [i + 1, NumSplitBins) on the right
                float rightAreas[NumSplitBins - 1];
                uint32 rightCounts[NumSplitBins - 1];

                BoundingBox rightBounds;
                uint32 rightCount = 0;

                for (uint32 binIndex = NumSplitBins - 1; binIndex > 0; binIndex--)
                {
                    rightBounds = rightBounds.Union(m_bins[binIndex].bounds);
                    rightCount += m_bins[binIndex].count;

                    rightAreas[binIndex - 1] = GetSurfaceArea(rightBounds);
                    rightCounts[binIndex - 1] = rightCount;
                }

                BoundingBox leftBounds;
                uint32 leftCount = 0;

                for (uint32 splitIndex = 0; splitIndex < NumSplitBins - 1; splitIndex++)
                {
                    leftBounds = leftBounds.Union(m_bins[splitIndex].bounds);
                    leftCount += m_bins[splitIndex].count;

                    if (leftCount == 0 || rightCounts[splitIndex] == 0)
                    {
                        continue;
                    }

                    const float cost = SplitTraversalCost
                        + (GetSurfaceArea(leftBounds) * float(leftCount) + rightAreas[splitIndex] * float(rightCounts[splitIndex])) / parentArea;

                    if (cost < bestCost)
                    {
                        bestCost = cost;
                        bestAxis = axis;
                        bestBin = splitIndex;
                    }
                }
            }

            if (bestAxis != ~0u)
            {
                if (count <= MaxLeafTriangles && bestCost >= float(count))
                {
                    return false;
                }

                const float binMin = centroidBounds.min[bestAxis];
                const float binScale = float(NumSplitBins) / (centroidBounds.max[bestAxis] - binMin);

                uint32* partitionPoint = std::partition(
                    m_triangleOrder.Data() + begin,
                    m_triangleOrder.Data() + end,
                    [this, bestAxis, bestBin, binMin, binScale](uint32 triangleIndex)
                    {
                        return GetSplitBinIndex(m_triangleCentroids[triangleIndex][bestAxis], binMin, binScale) <= bestBin;
                    });

                outMiddle = uint32(partitionPoint - m_triangleOrder.Data());

                if (outMiddle > begin && outMiddle < end)
                {
                    return true;
                }
            }
        }

        if (count <= MaxLeafTriangles)
        {
            return false;
        }

        outMiddle = SplitAtMedian(begin, end, centroidBounds);

        return true;
    }

    uint32 SplitAtMedian(uint32 begin, uint32 end, const BoundingBox& centroidBounds)
    {
        const uint32 middle = begin + (end - begin) / 2;

        const Vec3f centroidExtent = centroidBounds.max - centroidBounds.min;

        const uint32 axis = (centroidExtent.x >= centroidExtent.y && centroidExtent.x >= centroidExtent.z)
            ? 0
            : (centroidExtent.y >= centroidExtent.z ? 1 : 2);

        // All centroids coincide - any halving is as good as another
        if (centroidExtent[axis] > 0.0f)
        {
            std::nth_element(
                m_triangleOrder.Data() + begin,
                m_triangleOrder.Data() + middle,
                m_triangleOrder.Data() + end,
                [this, axis](uint32 lhs, uint32 rhs)
                {
                    return m_triangleCentroids[lhs][axis] < m_triangleCentroids[rhs][axis];
                });
        }

        return middle;
    }

    ChildReference BuildChild(uint32 begin, uint32 end, uint32 depth)
    {
        ChildReference child;
        child.bounds = ComputeBounds(begin, end);

        uint32 middle = 0;

        if (end - begin <= MinLeafTriangles || !FindSplit(begin, end, child.bounds, depth, middle))
        {
            child.index = begin;
            child.count = end - begin;

            return child;
        }

        child.index = BuildNode(begin, middle, end, depth);
        child.count = 0;

        return child;
    }

    uint32 BuildNode(uint32 begin, uint32 middle, uint32 end, uint32 depth)
    {
        AssertDebug(begin < middle && middle < end);

        const uint32 nodeIndex = uint32(m_nodes.Size());
        m_nodes.EmplaceBack();

        const ChildReference left = BuildChild(begin, middle, depth + 1);
        const ChildReference right = BuildChild(middle, end, depth + 1);

        // m_nodes may have grown while building the children
        PathTracerBVHNode& node = m_nodes[nodeIndex];
        WriteChild(left, node.leftMin, node.leftMax, node.leftIndex, node.leftCount);
        WriteChild(right, node.rightMin, node.rightMax, node.rightIndex, node.rightCount);

        return nodeIndex;
    }

    void WriteChild(const ChildReference& child, float (&outMin)[3], float (&outMax)[3], uint32& outIndex, uint32& outCount) const
    {
        for (int axis = 0; axis < 3; axis++)
        {
            outMin[axis] = child.bounds.min[axis] - m_boundsPadding;
            outMax[axis] = child.bounds.max[axis] + m_boundsPadding;
        }

        outIndex = child.index;
        outCount = child.count;
    }

    Array<BoundingBox> m_triangleBounds;
    Array<Vec3f> m_triangleCentroids;
    Array<uint32> m_triangleOrder;

    Array<PathTracerBVHNode> m_nodes;

    SplitBin m_bins[NumSplitBins];

    float m_boundsPadding = 0.0f;
};

void PathTracerBVHBuilder::Build(Span<const PathTracerTriangle> triangles, Array<PathTracerBVHNode>& outNodes, Array<uint32>& outTriangleOrder)
{
    HYP_SCOPE;

    PathTracerBVHBuildContext context { triangles };
    context.Build(outNodes, outTriangleOrder);
}

#pragma endregion PathTracerBVHBuilder

#pragma region PathTracerBVH

// VT_Simple: position, normal, texcoord0
static constexpr uint32 PackedVertexSizeInFloats = 8;

uint32 PackNormalOctahedral(const Vec3f& normal)
{
    const Vec2f octahedralCoord = MathUtil::EncodeOctahedralCoord(normal);

    const int32 x = int32(MathUtil::Round(MathUtil::Clamp(octahedralCoord.x, -1.0f, 1.0f) * 32767.0f));
    const int32 y = int32(MathUtil::Round(MathUtil::Clamp(octahedralCoord.y, -1.0f, 1.0f) * 32767.0f));

    return (uint32(x) & 0xFFFFu) | ((uint32(y) & 0xFFFFu) << 16);
}

static bool ReadMeshGeometry(const Mesh& mesh, Array<float>& outPackedVertices, Array<uint32>& outIndices)
{
    outPackedVertices.Clear();
    outIndices.Clear();

    auto resourceGuard = mesh.GetReadScope();

    if (!resourceGuard)
    {
        return false;
    }

    mesh.BuildVertexBuffer(StaticVertexInputLayout<VT_Simple>, 0, outPackedVertices);

    const Span<const ubyte> indexData = mesh.GetIndexData(0);
    const size_t indexSize = GpuElemTypeSize(mesh.GetMeshDesc().meshAttributes.indexBufferElemType);

    if (indexSize != sizeof(uint16) && indexSize != sizeof(uint32))
    {
        HYP_LOG(Lightmap, Warning, "Mesh '{}' has an unsupported index size ({} bytes), skipping it for compute path tracing", mesh.GetName(), indexSize);

        return false;
    }

    const size_t numIndices = indexData.Size() / indexSize;
    outIndices.Resize(numIndices);

    for (size_t index = 0; index < numIndices; index++)
    {
        if (indexSize == sizeof(uint32))
        {
            Memory::Copy(&outIndices[index], indexData.Data() + index * sizeof(uint32), sizeof(uint32));
        }
        else
        {
            uint16 index16;
            Memory::Copy(&index16, indexData.Data() + index * sizeof(uint16), sizeof(uint16));

            outIndices[index] = index16;
        }
    }

    return outPackedVertices.Any() && outIndices.Any();
}

PathTracerBVH::PathTracerBVH()
    : m_state(PathTracerBVHState::NotBuilt)
{
}

PathTracerBVH::~PathTracerBVH()
{
    if (m_buildTask.IsValid() && !m_buildTask.IsCompleted())
    {
        m_buildTask.Await();
    }

    EnqueueDeletion(std::move(m_nodesBuffer));
    EnqueueDeletion(std::move(m_trianglesBuffer));
    EnqueueDeletion(std::move(m_triangleAttributesBuffer));
}

PathTracerBVHState PathTracerBVH::Prepare(RenderProxyList& rpl)
{
    AssertOnThread(g_renderThread);

    if (m_state == PathTracerBVHState::NotBuilt)
    {
        GatherInstances(rpl);

        if (m_instances.Empty())
        {
            HYP_LOG(Lightmap, Warning, "No meshes to build the compute path tracing BVH from");

            m_state = PathTracerBVHState::Failed;

            return m_state;
        }

        m_buildTask = TaskSystem::GetInstance().Enqueue(
            [instances = m_instances.ToSpan()]() -> BuildResult
            {
                return Build(instances);
            },
            TaskThreadPoolName::THREAD_POOL_BACKGROUND);

        m_state = PathTracerBVHState::Building;
    }

    if (m_state == PathTracerBVHState::Building && m_buildTask.IsCompleted())
    {
        const BuildResult buildResult = std::move(m_buildTask).Await();
        m_buildTask = Task<BuildResult>();

        if (buildResult.nodes.Empty())
        {
            HYP_LOG(Lightmap, Error, "Compute path tracing BVH has no triangles, cannot bake");

            m_state = PathTracerBVHState::Failed;

            return m_state;
        }

        Upload(buildResult);

        HYP_LOG(Lightmap, Info, "Built compute path tracing BVH: {} instances, {} triangles, {} nodes",
            m_instances.Size(), buildResult.triangles.Size(), buildResult.nodes.Size());

        m_state = PathTracerBVHState::Ready;
    }

    return m_state;
}

void PathTracerBVH::GatherInstances(RenderProxyList& rpl)
{
    m_instances.Clear();

    for (Entity* entity : rpl.GetMeshEntities())
    {
        AssertDebug(entity != nullptr);

        // Matches the hardware ray tracing path, backdrop scenes are covered by the sky probes
        if (entity->GetScene()->GetSceneFlags() & SceneFlags::BACKDROP)
        {
            continue;
        }

        RenderProxyMesh* meshProxy = rpl.GetMeshEntities().GetProxy(entity->Id());
        AssertDebug(meshProxy != nullptr);

        if (!meshProxy || !meshProxy->mesh)
        {
            continue;
        }

        Instance& instance = m_instances.EmplaceBack();
        instance.mesh = MakeStrongRef(meshProxy->mesh);
        instance.transform = meshProxy->bufferData.modelMatrix;
        instance.materialIndex = meshProxy->material != nullptr
            ? Resources::GetBinding(meshProxy->material)
            : ~0u;
    }

    // Grouped by mesh so shared meshes only have their data read once
    std::sort(m_instances.Begin(), m_instances.End(),
        [](const Instance& lhs, const Instance& rhs)
        {
            return lhs.mesh.Get() < rhs.mesh.Get();
        });
}

PathTracerBVH::BuildResult PathTracerBVH::Build(Span<const Instance> instances)
{
    HYP_SCOPE;

    Array<PathTracerTriangle> unorderedTriangles;
    Array<PathTracerTriangleAttributes> unorderedTriangleAttributes;

    Array<float> packedVertices;
    Array<uint32> indices;

    const Mesh* loadedMesh = nullptr;
    bool loadedMeshIsValid = false;

    for (const Instance& instance : instances)
    {
        if (instance.mesh.Get() != loadedMesh)
        {
            loadedMesh = instance.mesh.Get();
            loadedMeshIsValid = ReadMeshGeometry(*loadedMesh, packedVertices, indices);
        }

        if (!loadedMeshIsValid)
        {
            continue;
        }

        const Mat4f& transform = instance.transform;
        const Mat4f normalMatrix = transform.Inverse().Transpose();

        const uint32 numVertices = uint32(packedVertices.Size() / PackedVertexSizeInFloats);

        unorderedTriangles.Reserve(unorderedTriangles.Size() + indices.Size() / 3);
        unorderedTriangleAttributes.Reserve(unorderedTriangleAttributes.Size() + indices.Size() / 3);

        for (size_t firstIndex = 0; firstIndex + 2 < indices.Size(); firstIndex += 3)
        {
            Vec3f positions[3];
            Vec3f normals[3];
            Vec2f texcoords[3];

            bool isValid = true;

            for (uint32 corner = 0; corner < 3; corner++)
            {
                const uint32 vertexIndex = indices[firstIndex + corner];

                if (vertexIndex >= numVertices)
                {
                    isValid = false;

                    break;
                }

                const float* vertex = packedVertices.Data() + vertexIndex * PackedVertexSizeInFloats;

                positions[corner] = transform.TransformVector(Vec3f(vertex[0], vertex[1], vertex[2]));
                normals[corner] = normalMatrix.TransformVector(Vec4f(vertex[3], vertex[4], vertex[5], 0.0f)).GetXYZ();
                texcoords[corner] = Vec2f(vertex[6], vertex[7]);
            }

            if (!isValid || !MathUtil::IsFinite(positions[0]) || !MathUtil::IsFinite(positions[1]) || !MathUtil::IsFinite(positions[2]))
            {
                continue;
            }

            const Vec3f edge1 = positions[1] - positions[0];
            const Vec3f edge2 = positions[2] - positions[0];
            const Vec3f faceNormal = edge1.Cross(edge2);

            // Zero area triangles can never be hit
            if (!(faceNormal.LengthSquared() > 0.0f))
            {
                continue;
            }

            PathTracerTriangle& triangle = unorderedTriangles.EmplaceBack();
            PathTracerTriangleAttributes& attributes = unorderedTriangleAttributes.EmplaceBack();

            for (int axis = 0; axis < 3; axis++)
            {
                triangle.position0[axis] = positions[0][axis];
                triangle.edge1[axis] = edge1[axis];
                triangle.edge2[axis] = edge2[axis];
            }

            triangle.padding0 = 0;
            triangle.padding1 = 0;
            triangle.padding2 = 0;

            for (uint32 corner = 0; corner < 3; corner++)
            {
                const float normalLengthSquared = normals[corner].LengthSquared();

                const Vec3f normal = (normalLengthSquared > 0.0f && MathUtil::IsFinite(normals[corner]))
                    ? normals[corner] / MathUtil::Sqrt(normalLengthSquared)
                    : faceNormal.Normalized();

                attributes.packedNormals[corner] = PackNormalOctahedral(normal);

                attributes.texcoords[corner * 2] = texcoords[corner].x;
                attributes.texcoords[corner * 2 + 1] = texcoords[corner].y;
            }

            attributes.materialIndex = instance.materialIndex;
            attributes.padding[0] = 0;
            attributes.padding[1] = 0;
        }
    }

    BuildResult buildResult;

    if (unorderedTriangles.Empty())
    {
        return buildResult;
    }

    Array<uint32> triangleOrder;
    PathTracerBVHBuilder::Build(unorderedTriangles.ToSpan(), buildResult.nodes, triangleOrder);

    buildResult.triangles.Resize(triangleOrder.Size());
    buildResult.triangleAttributes.Resize(triangleOrder.Size());

    for (size_t orderIndex = 0; orderIndex < triangleOrder.Size(); orderIndex++)
    {
        buildResult.triangles[orderIndex] = unorderedTriangles[triangleOrder[orderIndex]];
        buildResult.triangleAttributes[orderIndex] = unorderedTriangleAttributes[triangleOrder[orderIndex]];
    }

    return buildResult;
}

void PathTracerBVH::Upload(const BuildResult& buildResult)
{
    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

    const auto uploadBuffer = [&cr](GpuBufferRef& outBuffer, const void* data, size_t size)
    {
        outBuffer = RI.MakeGpuBuffer(GpuBufferType::StructuredBuffer, size, alignof(Vec4f));
        Check(outBuffer->Create());

        GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(size);
        stagingBuffer->Copy(size, data);
        stagingBuffer->Flush(0, size);

        cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
        cr << InsertBarrier(outBuffer.Get(), ResourceState::CopyDst);

        cr << CopyBuffer(stagingBuffer, outBuffer, uint32(size));

        cr << InsertBarrier(outBuffer.Get(), ResourceState::ShaderResource);
    };

    uploadBuffer(m_nodesBuffer, buildResult.nodes.Data(), buildResult.nodes.ByteSize());
    uploadBuffer(m_trianglesBuffer, buildResult.triangles.Data(), buildResult.triangles.ByteSize());
    uploadBuffer(m_triangleAttributesBuffer, buildResult.triangleAttributes.Data(), buildResult.triangleAttributes.ByteSize());

    cr.Done();
}

#pragma endregion PathTracerBVH

} // namespace Baking

} // namespace Hyperion
