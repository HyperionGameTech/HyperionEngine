/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>
#include <Rendering/Util/MeshBuilder.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Threading/SharedMutex.hpp>
#include <Core/Threading/LockGuard.hpp>

#include <Asset/Assets.hpp>
#include <Asset/AssetRegistry.hpp>
#include <Asset/BlobStorage.hpp>

#include <Scene/BVH.hpp>

#include <Framework/EngineDriver.hpp>

#include <cstring>

#include <Mesh.generated.inl>

namespace Hyperion {

static const Name s_nameMeshDefault = NAME("<unnamed mesh>");

#pragma region VertexTypeMask

String VertexTypeMask::ToString() const
{
    String result = "";
    bool first = true;

    FOR_EACH_BIT(flagMask, i)
    {
        if (!first)
        {
            result += ", ";
        }

        result += EnumToString(VertexType(1u << i));
        first = false;
    }

    return result;
}

#pragma endregion VertexTypeMask

#pragma region Mesh

Mesh::Mesh()
    : AssetObject(),
      m_aabb(BoundingBox::Empty()),
      m_flags(MeshFlags::None),
      m_currentLodIndex(0)
{
}

Mesh::Mesh(const MeshDataView& meshData, Topology topology)
    : Mesh(meshData, topology, StaticVertexInputLayout<VT_Simple>)
{
}

Mesh::Mesh(const MeshDataView& meshData, Topology topology, const VertexInputLayoutDesc& inputLayout)
    : AssetObject(),
      m_aabb(BoundingBox::Empty()),
      m_flags(MeshFlags::None),
      m_currentLodIndex(0)
{
    m_meshDesc = MeshDesc {};
    m_meshDesc.meshAttributes.inputLayout = inputLayout;
    m_meshDesc.meshAttributes.topology = topology;

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        const VertexArrayView& vertices = meshData.vertices[lodIndex];
        const ConstByteView& indices = meshData.indices[lodIndex];

        if (vertices.vertexCount == 0 && indices.Size() == 0)
        {
            continue;
        }

        m_meshDesc.lods[lodIndex].numVertices = uint32(vertices.vertexCount);
        m_meshDesc.lods[lodIndex].numIndices = uint32(indices.Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));

        if (vertices.vertexCount != 0)
        {
            AllocateBlobData(m_lodData[lodIndex].vertexData, vertices.floatData, inputLayout.VertexSize() * vertices.vertexCount, 16);
        }

        if (indices.Size() != 0)
        {
            AllocateBlobData(m_lodData[lodIndex].indexData, indices.Data(), indices.Size(), alignof(uint32));
        }
    }

    m_aabb = CalculateAABB();
}

Mesh::~Mesh()
{
    LockWriter();

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        if (m_vertexBuffers[lodIndex].IsValid())
        {
            EnqueueDeletion(std::move(m_vertexBuffers[lodIndex]));
        }

        if (m_indexBuffers[lodIndex].IsValid())
        {
            EnqueueDeletion(std::move(m_indexBuffers[lodIndex]));
        }
    }

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        FreeBlobData(m_lodData[lodIndex].vertexData);
        FreeBlobData(m_lodData[lodIndex].indexData);
    }

    FreeBlobData(m_bvhData);
}

VertexArrayView Mesh::GetVertexData(uint8 lodIndex) const
{
    VertexArrayView view {};
    view.layoutDesc = m_meshDesc.meshAttributes.inputLayout;

    // Data may be missing; PageBlobData() already logged why.
    if (!m_lodData[lodIndex].vertexData.raw)
    {
        return view;
    }

    const size_t vertexSize = m_meshDesc.meshAttributes.inputLayout.VertexSize();
    Assert(vertexSize != 0); // bad vertex size in layout desc - corrupt?

    view.floatData = reinterpret_cast<const float*>(m_lodData[lodIndex].vertexData.raw);
    view.vertexCount = m_meshDesc.lods[lodIndex].numVertices;

    return view;
}

void Mesh::SetVertexData(uint8 lodIndex, const VertexArrayView& view)
{
    size_t vertexSize = view.layoutDesc.VertexSize();
    Assert(vertexSize != 0);

    FreeBlobData(m_lodData[lodIndex].vertexData);
    AllocateBlobData(m_lodData[lodIndex].vertexData, view.floatData, view.vertexCount * vertexSize, 16);

    m_meshDesc.lods[lodIndex].numVertices = uint32(view.vertexCount);

    MarkDirty();
}

void Mesh::SetIndexData(uint8 lodIndex, Span<const ubyte> indexData)
{
    FreeBlobData(m_lodData[lodIndex].indexData);
    AllocateBlobData(m_lodData[lodIndex].indexData, indexData.Data(), indexData.Size(), alignof(uint32));

    m_meshDesc.lods[lodIndex].numIndices = uint32(indexData.Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));

    MarkDirty();
}

class PlaceholderVertexIndexCache
{
public:
    struct Buffers
    {
        Array<float> vertexData;
        Array<uint32> indexData;
    };

private:
    Map<uint8, UniquePtr<Buffers>> m_map;
    SharedMutex m_mutex;

    static UniquePtr<Buffers> BuildForLayout(uint8 layoutMask)
    {
        static const Vec3f s_corners[8] = {
            { -0.5f, -0.5f, -0.5f }, { 0.5f, -0.5f, -0.5f }, { 0.5f, 0.5f, -0.5f }, { -0.5f, 0.5f, -0.5f },
            { -0.5f, -0.5f, 0.5f }, { 0.5f, -0.5f, 0.5f }, { 0.5f, 0.5f, 0.5f }, { -0.5f, 0.5f, 0.5f }
        };

        static constexpr uint32 s_indices[36] = {
            0, 2, 1, 0, 3, 2, // back  (-Z)
            4, 5, 6, 4, 6, 7, // front (+Z)
            0, 1, 5, 0, 5, 4, // bottom(-Y)
            3, 6, 2, 3, 7, 6, // top   (+Y)
            1, 2, 6, 1, 6, 5, // right (+X)
            0, 4, 7, 0, 7, 3  // left  (-X)
        };

        Buffers bs;
        bs.vertexData.Reserve(GetArrayCount(s_corners) * (VertexInputLayoutDesc { layoutMask }.VertexSize() / sizeof(float)));

        for (const Vec3f& corner : s_corners)
        {
            const Vec3f normal = corner.Normalized();

            if (layoutMask & VT_Position)
            {
                bs.vertexData.PushBack(corner.x);
                bs.vertexData.PushBack(corner.y);
                bs.vertexData.PushBack(corner.z);
            }

            if (layoutMask & VT_Normal)
            {
                bs.vertexData.PushBack(normal.x);
                bs.vertexData.PushBack(normal.y);
                bs.vertexData.PushBack(normal.z);
            }

            if (layoutMask & VT_UV0)
            {
                bs.vertexData.PushBack(0.0f);
                bs.vertexData.PushBack(0.0f);
            }

            if (layoutMask & VT_UV1)
            {
                bs.vertexData.PushBack(0.0f);
                bs.vertexData.PushBack(0.0f);
            }

            if (layoutMask & VT_Skeletal)
            {
                static constexpr uint32 NoBoneIndices = UINT32_MAX;

                bs.vertexData.PushBack(BitCast<float>(NoBoneIndices));

                // Weights
                bs.vertexData.PushBack(0.0f);
                bs.vertexData.PushBack(0.0f);
                bs.vertexData.PushBack(0.0f);
                bs.vertexData.PushBack(0.0f);
            }
        }

        bs.indexData.Reserve(GetArrayCount(s_indices));

        for (uint32 index : s_indices)
        {
            bs.indexData.PushBack(index);
        }

        return MakeUnique<Buffers>(std::move(bs));
    }

public:
    static PlaceholderVertexIndexCache& GetInstance()
    {
        static PlaceholderVertexIndexCache instance;

        return instance;
    }

    Buffers* GetForLayout(uint8 layoutMask)
    {
        {
            TSharedLock lock(m_mutex);

            auto it = m_map.Find(layoutMask);

            if (it != m_map.End())
            {
                return it->second.Get();
            }
        }

        TUniqueLock lock(m_mutex);

        auto it = m_map.Find(layoutMask);

        if (it != m_map.End())
        {
            return it->second.Get();
        }

        return m_map.Insert(layoutMask, BuildForLayout(layoutMask)).first->second.Get();
    }
};

void Mesh::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    const String meshName = String(*GetName());

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        BlobDataReference& vertexData = m_lodData[lodIndex].vertexData;
        BlobDataReference& indexData = m_lodData[lodIndex].indexData;

        // load all LODs together for now
        // (the read scope acquires all blob data references at once)
        if (vertexData.raw == nullptr
            && vertexData.key
            && vertexData.size != 0)
        {
            bool vertexLoaded = false;
            bool indexLoaded = false;

            if (lodIndex == 0)
            {
                const Name blobKey = vertexData.key;
                const uint64 expectedSize = vertexData.size;

                vertexLoaded = ([&]() -> bool
                    {
                        if (PageBlobDataFromStorage(vertexData))
                        {
                            return true;
                        }

                        Handle<AssetRegistry> registry = GetAssetRegistry();
                        AssertDebug(registry.IsValid());

                        if (registry.IsValid())
                        {
                            // check if failed; if so, try to import from raw data blob in project directory
                            FileByteReader stream { registry->GetRootPath() / AssetBuckets::Meshes.GetName() / (meshName + ".VB.raw.blob") };
                            if (stream.Eof())
                            {
                                HYP_LOG(Assets, Error, "Blob data missing or corrupted for {} vertex buffer (LOD {}) at: {} ", GetName(), lodIndex, stream.GetFilepath());
                                return false;
                            }
                            else
                            {
                                if (stream.Max() != expectedSize)
                                {
                                    HYP_LOG(Engine, Error, "Local blob data for {} vertex buffer (LOD {}) is {} bytes but the manifest expects {}, ignoring it",
                                            GetName(), lodIndex, stream.Max(), expectedSize);

                                    return false;
                                }

                                ByteBuffer buffer = stream.Read(stream.Max());

                                AllocateBlobData(vertexData, buffer.Data(), buffer.Size(), 16);
                                vertexData.key = blobKey;

                                return true;
                            }
                        }

                        return false;
                    })();
            }
            else
            {
                HYP_LOG(Assets, Error, "Blob data missing or corrupted for {} vertex buffer (LOD {})", GetName(), lodIndex);
            }

            if (lodIndex == 0)
            {
                const Name blobKey = indexData.key;
                const uint64 expectedSize = indexData.size;

                indexLoaded = ([&]() -> bool
                    {
                        if (PageBlobDataFromStorage(indexData))
                        {
                            return true;
                        }

                        Handle<AssetRegistry> registry = GetAssetRegistry();
                        AssertDebug(registry.IsValid());

                        if (registry.IsValid())
                        {
                            // check if failed; if so, try to import from raw data blob in project directory
                            FileByteReader stream { registry->GetRootPath() / AssetBuckets::Meshes.GetName() / (meshName + ".IB.raw.blob") };
                            if (stream.Eof())
                            {
                                HYP_LOG(Assets, Error, "Blob data missing or corrupted for {} index buffer (LOD {}) at {}", GetName(), lodIndex, stream.GetFilepath());
                                return false;
                            }
                            else
                            {
                                if (stream.Max() != expectedSize)
                                {
                                    HYP_LOG(Engine, Error, "Local blob data for {} index buffer (LOD {}) is {} bytes but the manifest expects {}, ignoring it",
                                            GetName(), lodIndex, stream.Max(), expectedSize);

                                    return false;
                                }

                                ByteBuffer buffer = stream.Read(stream.Max());
                                AssertDebug(buffer.Size() == stream.Max());

                                AllocateBlobData(indexData, buffer.Data(), buffer.Size(), alignof(uint32));
                                indexData.key = blobKey;

                                return true;
                            }
                        }

                        return false;
                    })();
            }
            else
            {
                HYP_LOG(Assets, Error, "Blob data missing or corrupted for {} index buffer (LOD {})", GetName(), lodIndex);
            }

            // Both replaced together so we never end up with mismatched vertex/index counts.
            if (lodIndex == 0 && (!vertexLoaded || !indexLoaded))
            {
                HYP_LOG(Assets, Warning, "Using placeholder cube mesh for {} (LOD {}) because its real vertex/index data could not be loaded",
                    GetName(), lodIndex);

                const uint8 layoutMask = m_meshDesc.meshAttributes.inputLayout.mask;
                const PlaceholderVertexIndexCache::Buffers* placeholder = PlaceholderVertexIndexCache::GetInstance().GetForLayout(layoutMask);

                // Whichever of the real vertex/index buffers did load has to be released first,
                // so the placeholder data is always allocated as a matched pair
                // (AllocateBlobData asserts when allocating over non-readonly data).
                for (BlobDataReference* reference : { &vertexData, &indexData })
                {
                    if (reference->raw == nullptr)
                    {
                        continue;
                    }

                    if (reference->readOnly)
                    {
                        // Mapped from blob storage; the storage owns the memory
                        reference->raw = nullptr;
                    }
                    else
                    {
                        FreeBlobData(*reference);
                    }
                }

                const Name vertexBlobKey = vertexData.key;
                AllocateBlobData(vertexData, placeholder->vertexData.Data(), placeholder->vertexData.ByteSize(), 16);
                vertexData.key = vertexBlobKey;

                const Name indexBlobKey = indexData.key;
                AllocateBlobData(indexData, placeholder->indexData.Data(), placeholder->indexData.ByteSize(), alignof(uint32));
                indexData.key = indexBlobKey;

                const size_t vertexSize = MathUtil::Max(VertexInputLayoutDesc { layoutMask }.VertexSize(), size_t(1));
                m_meshDesc.lods[lodIndex].numVertices = uint32(placeholder->vertexData.ByteSize() / vertexSize);
                m_meshDesc.lods[lodIndex].numIndices = uint32(placeholder->indexData.Size());
            }
        }
    }

    // Keep BVH data separate from vertex and index data because it is mutually exclusive from them
    if (m_bvhData.raw == nullptr
        && m_bvhData.key
        && m_bvhData.size != 0)
    {
        const Name blobKey = m_bvhData.key;
        const uint64 expectedSize = m_bvhData.size;

        ([&]()
            {
                if (PageBlobDataFromStorage(m_bvhData))
                {
                    return;
                }

                Handle<AssetRegistry> registry = GetAssetRegistry();
                AssertDebug(registry.IsValid());

                if (registry.IsValid())
                {
                    FileByteReader stream { registry->GetRootPath() / AssetBuckets::Meshes.GetName() / (meshName + ".BVH.raw.blob") };
                    if (stream.Eof())
                    {
                        HYP_LOG(Engine, Error, "Data corruption detected for {} due to missing blob data at {} ", GetPath().ToString(), stream.GetFilepath());
                        return;
                    }
                    
                    if (stream.Max() != expectedSize)
                    {
                        HYP_LOG(Engine, Error, "Local BVH blob data for {} is {} bytes but the manifest expects {}, ignoring it",
                                GetName(), stream.Max(), expectedSize);

                        return;
                    }

                    ByteBuffer buffer = stream.Read(stream.Max());

                    AllocateBlobData(m_bvhData, buffer.Data(), buffer.Size(), alignof(uint32));
                    m_bvhData.key = blobKey;
                }
        })();
    }

    if (m_bvhData.raw != nullptr)
    {
        BVHNode::Deserialize(m_bvh, m_bvhData.raw, m_bvhData.size);
    }
}

void Mesh::UnpageBlobData()
{
    AssetObject::UnpageBlobData();

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        AssertBlobDataPersisted(m_lodData[lodIndex].vertexData);

        if (!m_lodData[lodIndex].vertexData.readOnly)
        {
            FreeBlobData(m_lodData[lodIndex].vertexData);
        }

        m_lodData[lodIndex].vertexData.raw = nullptr;

        AssertBlobDataPersisted(m_lodData[lodIndex].indexData);

        if (!m_lodData[lodIndex].indexData.readOnly)
        {
            FreeBlobData(m_lodData[lodIndex].indexData);
        }
        
        m_lodData[lodIndex].indexData.raw = nullptr;
    }

    AssertBlobDataPersisted(m_bvhData);

    if (!m_bvhData.readOnly)
    {
        FreeBlobData(m_bvhData);
    }

    m_bvhData.raw = nullptr;
}

void Mesh::UploadGpuData()
{
    if (EngineGlobals::IsHeadless())
    {
        return;
    }

    auto readScope = GetReadScope();

    // @TODO: Upload all LODs to GPU; for now LOD 0 is uploaded
    const uint8 lodIndex = m_currentLodIndex;

    // @TODO fix for non-uint32 indices
    Assert(GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType) == 4);

    Array<float> vertices;

    BuildVertexBuffer(
        m_meshDesc.meshAttributes.inputLayout,
        lodIndex,
        vertices);

    const Span<const ubyte> indexData = GetIndexData(lodIndex);

    if (vertices.Size() == 0 || indexData.Size() == 0)
    {
        // No data
        return;
    }

    ByteBuffer indices;
    indices.SetSize(indexData.Size());

    Memory::Copy(indices.Data(), indexData.Data(), indexData.Size());

    const size_t vertexSize = m_meshDesc.meshAttributes.inputLayout.VertexSize();
    const size_t vertexSizeInFloats = vertexSize / sizeof(float);

    AssertDebug(vertices.Size() == m_meshDesc.lods[lodIndex].numVertices * vertexSizeInFloats);
    AssertDebug(indices.Size() == m_meshDesc.lods[lodIndex].numIndices * sizeof(uint32));

    // Done reading data into buffers for upload
    readScope.Reset();

    auto writeScope = GetWriteScope();

    isUploaded.Store(false);

    // Ensure vertex buffer is not empty (at least one vertex)
    if (vertices.Empty())
    {
        vertices.Resize(vertexSizeInFloats);
    }

    // Ensure indices exist and are a multiple of 3
    if (m_meshDesc.lods[lodIndex].numIndices == 0)
    {
        indices.SetSize(3 * sizeof(uint32));
    }
    else if (m_meshDesc.lods[lodIndex].numIndices % 3 != 0)
    {
        indices.SetSize((m_meshDesc.lods[lodIndex].numIndices + (3 - (m_meshDesc.lods[lodIndex].numIndices % 3))) * sizeof(uint32));
    }

    const size_t packedVerticesSize = vertices.ByteSize();
    const size_t packedIndicesSize = indices.Size();

    GpuBufferRef vertexBuffer = RI.MakeGpuBuffer(GpuBufferType::VertexBuffer, packedVerticesSize);
    GpuBufferRef indexBuffer = RI.MakeGpuBuffer(GpuBufferType::IndexBuffer, packedIndicesSize);

#ifdef HYP_RHI_DEBUG_NAMES
    vertexBuffer->SetDebugName(NAME_FMT("{}_VBO", GetName()));
    indexBuffer->SetDebugName(NAME_FMT("{}_IBO", GetName()));
#endif

    Check(vertexBuffer->Create());
    Check(indexBuffer->Create());

    AssertDebug(vertexBuffer.IsValid() && indexBuffer.IsValid());

    const size_t bufferSizeCombined = packedVerticesSize + packedIndicesSize;

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(bufferSizeCombined);
    stagingBuffer->Copy(packedVerticesSize, vertices.Data());
    stagingBuffer->Copy(packedVerticesSize, packedIndicesSize, indices.Data());
    stagingBuffer->Flush(0, bufferSizeCombined);

    Frame* currentFrame = nullptr;

    // Must run before the frame's render commands: the mesh is marked uploaded as soon as we return here,
    // so the very same frame can bind these buffers for drawing.
    CommandRecorder& cr = IsOnThread(g_renderThread) && (currentFrame = RI.GetCurrentFrame()) != nullptr
        ? currentFrame->preRenderCommands
        : RI.commandRecorderAllocator.GetCommandRecorder(CommandRecorderQueue::PreRender);

    cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);

    cr << InsertBarrier(vertexBuffer, RS_COPY_DST);
    cr << InsertBarrier(indexBuffer, RS_COPY_DST);

    cr << CopyBuffer(stagingBuffer, vertexBuffer, packedVerticesSize);
    cr << CopyBuffer(stagingBuffer, indexBuffer, packedVerticesSize, 0, packedIndicesSize);

    cr << InsertBarrier(vertexBuffer, RS_VERTEX_BUFFER);
    cr << InsertBarrier(indexBuffer, RS_INDEX_BUFFER);

    if (m_vertexBuffers[lodIndex].IsValid())
    {
        EnqueueDeletion(std::move(m_vertexBuffers[lodIndex]));
    }

    if (m_indexBuffers[lodIndex].IsValid())
    {
        EnqueueDeletion(std::move(m_indexBuffers[lodIndex]));
    }

    m_vertexBuffers[lodIndex] = std::move(vertexBuffer);
    m_indexBuffers[lodIndex] = std::move(indexBuffer);

    isUploaded.Store(true);

    if (!currentFrame || &cr != &currentFrame->preRenderCommands)
    {
        cr.Done();
    }
}

void Mesh::ReleaseGpuData()
{
    auto writeScope = GetWriteScope();

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        if (m_vertexBuffers[lodIndex].IsValid())
        {
            EnqueueDeletion(std::move(m_vertexBuffers[lodIndex]));
        }

        if (m_indexBuffers[lodIndex].IsValid())
        {
            EnqueueDeletion(std::move(m_indexBuffers[lodIndex]));
        }
    }

    isUploaded.Store(false);
}

Result Mesh::Rename(Name name)
{
    return AssetObject::Rename(name);
}

void Mesh::SetMeshData(
    const MeshDesc& meshDesc,
    const MeshDataView& meshData)
{
    auto writeScope = GetWriteScope();

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        FreeBlobData(m_lodData[lodIndex].vertexData);
        FreeBlobData(m_lodData[lodIndex].indexData);

        const VertexArrayView& vertices = meshData.vertices[lodIndex];
        const ConstByteView& indices = meshData.indices[lodIndex];

        if (vertices.vertexCount != 0)
        {
            AllocateBlobData(m_lodData[lodIndex].vertexData, vertices.floatData, vertices.layoutDesc.VertexSize() * vertices.vertexCount, 16);
        }

        if (indices.Size() != 0)
        {
            AllocateBlobData(m_lodData[lodIndex].indexData, indices.Data(), indices.Size(), alignof(uint32));
        }
    }

    m_meshDesc = meshDesc;

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        AssertDebug(m_meshDesc.lods[lodIndex].numVertices == meshData.vertices[lodIndex].vertexCount);
        AssertDebug(m_meshDesc.lods[lodIndex].numIndices == meshData.indices[lodIndex].Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));
    }

    // recalc aabb
    m_aabb = CalculateAABB();

    MarkDirty();

    writeScope.Reset();
}

void Mesh::SetFlags(EnumFlags<MeshFlags> flags)
{
    if (m_flags == flags)
    {
        return;
    }

    const bool wasViewIndependent = m_flags[MeshFlags::ViewIndependent];

    m_flags = flags;

    if (m_flags[MeshFlags::ViewIndependent] != wasViewIndependent)
    {
        SetPersistentRequested(m_flags[MeshFlags::ViewIndependent], /* markDirty */ false);
    }

    MarkDirty();
}

void Mesh::BuildBVH(BVHNode& bvhNode, int maxDepth) const
{
    // @TODO: Support building BVH for arbitrary LOD; for now LOD 0
    constexpr uint8 lodIndex = 0;

    const VertexArrayView vertexData = GetVertexData(lodIndex);
    const Span<const ubyte> indexData = GetIndexData(lodIndex);
    const uint32 numVertices = uint32(vertexData.vertexCount);
    const uint32 numIndices = uint32(indexData.Size() / sizeof(uint32));

    const BoundingBox meshAabb = CalculateAABB();

    const size_t numTriangles = numIndices / 3;

    // @TODO Fix for non uint32 indices

    const uint32* indexDataU32 = reinterpret_cast<const uint32*>(indexData.Data());

    bvhNode = BVHNode(meshAabb);
    bvhNode.triangleIds.Reserve(numTriangles);

    for (uint32 triangleId = 0; triangleId < numTriangles; triangleId++)
    {
        bvhNode.AddTriangleId(triangleId);
    }

    // pass mesh spans so Split can do AABB/triangle overlap without copying triangles
    bvhNode.Split(
        maxDepth,
        vertexData,
        Span<const uint32>(indexDataU32, numIndices));

    bvhNode.Shake();
}

void Mesh::SetBVH(BVHNode&& bvh)
{
    // Assume caller has write scope

    ByteBuffer bvhBuffer = BVHNode::Serialize(bvh);
    FreeBlobData(m_bvhData);
    AllocateBlobData(m_bvhData, bvhBuffer.Data(), bvhBuffer.Size(), alignof(uint32));

    m_bvh = std::move(bvh);

    MarkDirty();
}

BoundingBox Mesh::CalculateAABB() const
{
    // @TODO: AABB may need to encompass all LODs; for now use LOD 0
    constexpr uint8 lodIndex = 0;

    const VertexArrayView vertexArrayView = GetVertexData(lodIndex);

    BoundingBox aabb = BoundingBox::Empty();

    for (uint32 vertexIndex = 0; vertexIndex < vertexArrayView.vertexCount; vertexIndex++)
    {
        const float* floatDataOffset = vertexArrayView.floatData + (vertexIndex * vertexArrayView.layoutDesc.VertexSize() / sizeof(float));

        aabb = aabb.Union(reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset)->GetPosition());
    }

    return aabb;
}

template <class AllocatorType>
void Mesh::BuildVertexBuffer(
    const VertexInputLayoutDesc& inputLayout,
    uint8 lodIndex,
    Array<float, AllocatorType>& outData) const
{
    const VertexArrayView vertices = GetVertexData(lodIndex);
    AssertDebug(vertices.floatData != nullptr);

    if (!vertices.floatData)
    {
        return;
    }

    const uint8 srcMask = m_meshDesc.meshAttributes.inputLayout.mask;
    const uint8 dstMask = inputLayout.mask;
    const uint8 combinedMask = srcMask & dstMask;

    const size_t srcVertexSize = m_meshDesc.meshAttributes.inputLayout.VertexSize();
    const size_t srcVertexSizeInFloats = srcVertexSize / sizeof(float);

    const size_t dstVertexSize = inputLayout.VertexSize();
    const size_t dstVertexSizeInFloats = dstVertexSize / sizeof(float);

    outData.Resize(dstVertexSizeInFloats * vertices.vertexCount);

    for (size_t i = 0; i < vertices.vertexCount; i++)
    {
        const float* srcFloatBuffer = vertices.floatData + (i * srcVertexSizeInFloats);
        AssertDebug((uintptr_t(srcFloatBuffer + srcVertexSizeInFloats) - uintptr_t(vertices.floatData)) <= m_lodData[lodIndex].vertexData.size);

        float* dstFloatBuffer = outData.Data() + (i * dstVertexSizeInFloats);

        if (combinedMask & VT_Position)
        {
            Memory::Copy(dstFloatBuffer, srcFloatBuffer, sizeof(TVertexPacket<VT_Position>));
        }
        if (srcMask & VT_Position)
        {
            srcFloatBuffer += sizeof(TVertexPacket<VT_Position>) / sizeof(float);
        }
        if (dstMask & VT_Position)
        {
            dstFloatBuffer += sizeof(TVertexPacket<VT_Position>) / sizeof(float);
        }

        if (combinedMask & VT_Normal)
        {
            Memory::Copy(dstFloatBuffer, srcFloatBuffer, sizeof(TVertexPacket<VT_Normal>));
        }
        if (srcMask & VT_Normal)
        {
            srcFloatBuffer += sizeof(TVertexPacket<VT_Normal>) / sizeof(float);
        }
        if (dstMask & VT_Normal)
        {
            dstFloatBuffer += sizeof(TVertexPacket<VT_Normal>) / sizeof(float);
        }

        if (combinedMask & VT_UV0)
        {
            Memory::Copy(dstFloatBuffer, srcFloatBuffer, sizeof(TVertexPacket<VT_UV0>));
        }
        if (srcMask & VT_UV0)
        {
            srcFloatBuffer += sizeof(TVertexPacket<VT_UV0>) / sizeof(float);
        }
        if (dstMask & VT_UV0)
        {
            dstFloatBuffer += sizeof(TVertexPacket<VT_UV0>) / sizeof(float);
        }

        if (combinedMask & VT_UV1)
        {
            Memory::Copy(dstFloatBuffer, srcFloatBuffer, sizeof(TVertexPacket<VT_UV1>));
        }
        if (srcMask & VT_UV1)
        {
            srcFloatBuffer += sizeof(TVertexPacket<VT_UV1>) / sizeof(float);
        }
        if (dstMask & VT_UV1)
        {
            dstFloatBuffer += sizeof(TVertexPacket<VT_UV1>) / sizeof(float);
        }

        uint8 bonesMask = 0;

        if (combinedMask & VT_Skeletal)
        {
            const TVertexPacket<VT_Skeletal>* packet = reinterpret_cast<const TVertexPacket<VT_Skeletal>*>(srcFloatBuffer);

            bonesMask = uint8((1 << packet->NumBoneIndices()) - 1);

            Memory::Copy(dstFloatBuffer, &packet->boneIndices, sizeof(packet->boneIndices));
            dstFloatBuffer += sizeof(packet->boneIndices) / sizeof(float);

            float weights[4] {};
            FOR_EACH_BIT(bonesMask, j)
            {
                weights[j] = packet->GetBoneWeight(j);
            }
            Memory::Copy(dstFloatBuffer, weights, sizeof(weights));
            dstFloatBuffer += sizeof(weights) / sizeof(float);
        }
    }
}

template void Mesh::BuildVertexBuffer<ThreadAllocator>(const VertexInputLayoutDesc& inputLayout, uint8 lodIndex, Array<float, ThreadAllocator>& outData) const;
template void Mesh::BuildVertexBuffer<DynamicAllocator>(const VertexInputLayoutDesc& inputLayout, uint8 lodIndex, Array<float, DynamicAllocator>& outData) const;

void Mesh::CalculateNormals(bool weighted)
{
    // @TODO: Support calculating normals for arbitrary LOD; for now LOD 0
    constexpr uint8 lodIndex = 0;

    VertexArrayView vertexData = GetVertexData(lodIndex);
    AssertDebug(((VT_Position | VT_Normal) & vertexData.layoutDesc.mask) == (VT_Position | VT_Normal),
                "Vertex data must have VT_Position and VT_Normal at least in order to calculate normals");

    const Span<ubyte> indexData = GetIndexData(lodIndex);

    const uint32 indexElemSize = GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType);
    AssertDebug(indexElemSize == 2 || indexElemSize == 4, "CalculateNormals only supports 16- or 32-bit indices");

    const uint32 numVertices = uint32(vertexData.vertexCount);
    const uint32 numIndices = uint32(indexData.Size() / indexElemSize);

    const size_t vertexSizeInFloats = vertexData.layoutDesc.VertexSize() / sizeof(float);

    const auto readIndex = [&](size_t i) -> uint32
    {
        return indexElemSize == 4
            ? reinterpret_cast<const uint32*>(indexData.Data())[i]
            : uint32(reinterpret_cast<const uint16*>(indexData.Data())[i]);
    };

    const auto getPosition = [&](uint32 idx) -> Vec3f
    {
        const float* floatDataOffset = vertexData.floatData + (idx * vertexSizeInFloats);
        return reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset)->GetPosition();
    };

    const auto setNormal = [&](uint32 idx, const Vec3f& normal)
    {
        float* floatDataOffset = const_cast<float*>(vertexData.floatData + (idx * vertexSizeInFloats));
        TVertexPacket<VT_Normal>* packet = reinterpret_cast<TVertexPacket<VT_Normal>*>(floatDataOffset + (sizeof(TVertexPacket<VT_Position>) / sizeof(float)));
        packet->SetNormal(normal);
    };

    Array<Vec3f> accumulated;
    accumulated.Resize(numVertices);

    for (size_t i = 0; i + 2 < numIndices; i += 3)
    {
        const uint32 i0 = readIndex(i);
        const uint32 i1 = readIndex(i + 1);
        const uint32 i2 = readIndex(i + 2);

        const Vec3f p0 = getPosition(i0);
        const Vec3f p1 = getPosition(i1);
        const Vec3f p2 = getPosition(i2);

        const Vec3f faceNormalScaled = (p2 - p0).Cross(p1 - p0);
        const float doubleArea = faceNormalScaled.Length();

        if (doubleArea <= MathUtil::epsilonF)
        {
            continue; // degenerate triangle.
        }

        const Vec3f faceNormal = faceNormalScaled / doubleArea;

        if (weighted)
        {
            const float area = 0.5f * doubleArea;

            const float angle0 = (p1 - p0).AngleBetween(p2 - p0);
            const float angle1 = (p0 - p1).AngleBetween(p2 - p1);
            const float angle2 = (p0 - p2).AngleBetween(p1 - p2);

            accumulated[i0] += faceNormal * (area * angle0);
            accumulated[i1] += faceNormal * (area * angle1);
            accumulated[i2] += faceNormal * (area * angle2);
        }
        else
        {
            accumulated[i0] += faceNormal;
            accumulated[i1] += faceNormal;
            accumulated[i2] += faceNormal;
        }
    }

    for (uint32 i = 0; i < numVertices; i++)
    {
        const float magnitude = accumulated[i].Length();

        const Vec3f normal = magnitude > MathUtil::epsilonF
            ? accumulated[i] / magnitude
            : Vec3f { 0.0f, 1.0f, 0.0f };

        setNormal(i, normal);
    }
}


#ifdef HYP_EDITOR

void Mesh::RegenerateNormals()
{
    {
        auto writeScope = GetWriteScope();
        CalculateNormals();
    }

    UploadGpuData();
}

void Mesh::RebuildBVH()
{
    BVHNode bvh;

    {
        auto readScope = GetReadScope();
        BuildBVH(bvh);
    }

    auto writeScope = GetWriteScope();

    // Serialize the BVH into blob data so it can be saved/loaded without a rebuild
    ByteBuffer bvhBuffer = BVHNode::Serialize(bvh);
    FreeBlobData(m_bvhData);
    AllocateBlobData(m_bvhData, bvhBuffer.Data(), bvhBuffer.Size(), alignof(uint32));

    m_bvh = std::move(bvh);

    MarkDirty();
}

void Mesh::RecalculateBounds()
{
    BoundingBox bounds;

    {
        auto readScope = GetReadScope();
        bounds = CalculateAABB();
    }

    auto writeScope = GetWriteScope();

    m_aabb = bounds;

    MarkDirty();
}

#endif // HYP_EDITOR

#pragma endregion Mesh

} // namespace Hyperion
