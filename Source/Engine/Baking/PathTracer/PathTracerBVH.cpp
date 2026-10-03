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

#pragma region PathTracerBVH

// VT_Simple: position, normal, texcoord0
static constexpr uint32 PackedVertexSizeInFloats = 8;

static uint32 PackNormalOctahedral(const Vec3f& normal)
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

        // instance transforms aren't traced yet; the entity's own transform isn't where its copies are drawn
        if (meshProxy->numInstances != 0)
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

    Array<BoundingBox> triangleBounds;
    triangleBounds.Resize(unorderedTriangles.Size());

    for (size_t triangleIndex = 0; triangleIndex < unorderedTriangles.Size(); triangleIndex++)
    {
        const PathTracerTriangle& triangle = unorderedTriangles[triangleIndex];

        const Vec3f position0 = Vec3f(triangle.position0[0], triangle.position0[1], triangle.position0[2]);
        const Vec3f position1 = position0 + Vec3f(triangle.edge1[0], triangle.edge1[1], triangle.edge1[2]);
        const Vec3f position2 = position0 + Vec3f(triangle.edge2[0], triangle.edge2[1], triangle.edge2[2]);

        triangleBounds[triangleIndex] = BoundingBox(position0, position0).Union(position1).Union(position2);
    }

    Array<uint32> triangleOrder;

    GlimmerBVHBuildParams buildParams;
    buildParams.maxDepth = 64;

    GlimmerBVHBuilder::Build(triangleBounds.ToSpan(), buildParams, buildResult.nodes, triangleOrder);

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
