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
#include <Rendering/Util/MeshLodGenerator.hpp>

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
      m_flags(MeshFlags::None)
{
}

Mesh::Mesh(const MeshDataView& meshData, Topology topology)
    : Mesh(meshData, topology, StaticVertexInputLayout<VT_Simple>)
{
}

Mesh::Mesh(const MeshDataView& meshData, Topology topology, const VertexInputLayoutDesc& inputLayout)
    : AssetObject(),
      m_aabb(BoundingBox::Empty()),
      m_flags(MeshFlags::None)
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

#ifdef HYP_EDITOR
    if (lodIndex == 0)
    {
        ++m_lod0DataRevision;
    }
#endif // HYP_EDITOR

    MarkDirty();
}

void Mesh::SetIndexData(uint8 lodIndex, Span<const ubyte> indexData)
{
    FreeBlobData(m_lodData[lodIndex].indexData);
    AllocateBlobData(m_lodData[lodIndex].indexData, indexData.Data(), indexData.Size(), alignof(uint32));

    m_meshDesc.lods[lodIndex].numIndices = uint32(indexData.Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));

#ifdef HYP_EDITOR
    if (lodIndex == 0)
    {
        ++m_lod0DataRevision;
    }
#endif // HYP_EDITOR

    MarkDirty();
}

void Mesh::SetIsDynamicMesh(bool isDynamic)
{
    if (isDynamic)
    {
        m_flags |= MeshFlags::DynamicMesh;
    }
    else
    {
        m_flags &= ~MeshFlags::DynamicMesh;
    }

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

            if (layoutMask & VT_Tree)
            {
                for (size_t treeFloatIndex = 0; treeFloatIndex < sizeof(TVertexPacket<VT_Tree>) / sizeof(float); ++treeFloatIndex)
                {
                    bs.vertexData.PushBack(0.0f);
                }
            }

            if (layoutMask & VT_Foliage)
            {
                for (size_t foliageFloatIndex = 0; foliageFloatIndex < sizeof(TVertexPacket<VT_Foliage>) / sizeof(float); ++foliageFloatIndex)
                {
                    bs.vertexData.PushBack(0.0f);
                }
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

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        BlobDataReference& vertexData = m_lodData[lodIndex].vertexData;
        BlobDataReference& indexData = m_lodData[lodIndex].indexData;

        // load all LODs together for now
        // (the read scope acquires all blob data references at once)
        if (vertexData.raw != nullptr
            || !vertexData.key
            || vertexData.size == 0)
        {
            continue;
        }

        const bool vertexLoaded = PageBlobDataFromStorage(vertexData)
            || PageBlobDataFromLocalFile(vertexData, GetVertexBufferBlobMagic(lodIndex), 16);

        const bool indexLoaded = PageBlobDataFromStorage(indexData)
            || PageBlobDataFromLocalFile(indexData, GetIndexBufferBlobMagic(lodIndex), alignof(uint32));

        if (vertexLoaded && indexLoaded)
        {
            continue;
        }

        // Both replaced together so we never end up with mismatched vertex/index counts.
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

        if (lodIndex != 0)
        {
            // Cut the chain here - GetNumLods() shrinks and nothing tries to draw an empty LOD.
            HYP_LOG(Assets, Warning, "Dropping LODs {} and above for {} because their vertex/index data could not be loaded",
                lodIndex, GetName());

            ClearLodData(lodIndex);

            break;
        }

        HYP_LOG(Assets, Warning, "Using placeholder cube mesh for {} (LOD {}) because its real vertex/index data could not be loaded",
            GetName(), lodIndex);

        const uint8 layoutMask = m_meshDesc.meshAttributes.inputLayout.mask;
        const PlaceholderVertexIndexCache::Buffers* placeholder = PlaceholderVertexIndexCache::GetInstance().GetForLayout(layoutMask);

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

    // Keep BVH data separate from vertex and index data because it is mutually exclusive from them
    if (m_bvhData.raw == nullptr
        && m_bvhData.key
        && m_bvhData.size != 0)
    {
        if (!PageBlobDataFromStorage(m_bvhData))
        {
            (void)PageBlobDataFromLocalFile(m_bvhData, "BVH", alignof(uint32));
        }
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

    const uint8 numLods = MathUtil::Max<uint8>(m_meshDesc.GetNumLods(), 1);

    uint8 uploadedLods = 0;

    for (uint8 lodIndex = 0; lodIndex < numLods; lodIndex++)
    {
        const bool uploaded = UploadLod(lodIndex);

        uploadedLods |= (1u << lodIndex) * uploaded;
    }
    
    // LOD 0 must upload successfully for the mesh to be considered usable
    if (uploadedLods & 0x1)
    {
        isUploaded.Store(true);
    }
}

bool Mesh::UploadLod(uint8 lodIndex)
{
    AssertDebug(lodIndex < MaxMeshLods);

    if (EngineGlobals::IsHeadless())
    {
        return false;
    }

    auto readScope = GetReadScope();

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
        return false;
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

    const bool reuseBuffers = m_vertexBuffers[lodIndex].IsValid()
        && m_indexBuffers[lodIndex].IsValid()
        && m_vertexBuffers[lodIndex]->Size() >= packedVerticesSize
        && m_indexBuffers[lodIndex]->Size() >= packedIndicesSize;

    GpuBufferRef vertexBuffer;
    GpuBufferRef indexBuffer;

    if (reuseBuffers)
    {
        vertexBuffer = m_vertexBuffers[lodIndex];
        indexBuffer = m_indexBuffers[lodIndex];
    }
    else
    {
        vertexBuffer = RI.MakeGpuBuffer(GpuBufferType::VertexBuffer, packedVerticesSize);
        indexBuffer = RI.MakeGpuBuffer(GpuBufferType::IndexBuffer, packedIndicesSize);

#ifdef HYP_RHI_DEBUG_NAMES
        vertexBuffer->SetDebugName(NAME_FMT("{}_VBO_LOD{}", GetName(), lodIndex));
        indexBuffer->SetDebugName(NAME_FMT("{}_IBO_LOD{}", GetName(), lodIndex));
#endif

        Check(vertexBuffer->Create());
        Check(indexBuffer->Create());

        AssertDebug(vertexBuffer.IsValid() && indexBuffer.IsValid());
    }

    const size_t bufferSizeCombined = packedVerticesSize + packedIndicesSize;

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(bufferSizeCombined);
    stagingBuffer->Copy(packedVerticesSize, vertices.Data());
    stagingBuffer->Copy(packedVerticesSize, packedIndicesSize, indices.Data());
    stagingBuffer->Flush(0, bufferSizeCombined);

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

    cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);

    cr << InsertBarrier(vertexBuffer, ResourceState::CopyDst);
    cr << InsertBarrier(indexBuffer, ResourceState::CopyDst);

    cr << CopyBuffer(stagingBuffer, vertexBuffer, packedVerticesSize);
    cr << CopyBuffer(stagingBuffer, indexBuffer, packedVerticesSize, 0, packedIndicesSize);

    cr << InsertBarrier(vertexBuffer, ResourceState::VertexBuffer);
    cr << InsertBarrier(indexBuffer, ResourceState::IndexBuffer);

    if (!reuseBuffers)
    {
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
    }

    cr.Submit();

    return true;
}

void Mesh::UpdateDynamicVertexData(uint8 lodIndex, uint32 firstVertex, const VertexArrayView& vertexRange)
{
    Assert(IsDynamicMesh());

    if (EngineGlobals::IsHeadless())
    {
        return;
    }

    if (!vertexRange.floatData || vertexRange.vertexCount == 0)
    {
        return;
    }

    const bool validRange = (firstVertex + vertexRange.vertexCount <= m_meshDesc.lods[lodIndex].numVertices);
    Assert(validRange);

    if (!validRange)
    {
        return;
    }

    auto writeScope = GetWriteScope();

    const size_t srcVertexSize = m_meshDesc.meshAttributes.inputLayout.VertexSize();
    Assert(srcVertexSize != 0);

    BlobDataReference& vertexBlob = m_lodData[lodIndex].vertexData;

    if (!vertexBlob.raw || vertexBlob.readOnly || vertexBlob.size < (firstVertex + vertexRange.vertexCount) * srcVertexSize)
    {
        // needs a full upload of just this LOD

        writeScope.Reset();

        const bool uploaded = UploadLod(lodIndex);

        // as above, lodIndex 0 needs to be uploaded successfully in order to say it's usable.
        if (lodIndex == 0 && uploaded)
        {
            isUploaded.Store(true);
        }

        return;
    }

    Memory::Copy(
        (char*)vertexBlob.raw + firstVertex * srcVertexSize,
        vertexRange.floatData,
        vertexRange.vertexCount * srcVertexSize);

    // sync bounds
    m_aabb = CalculateAABB();

    GpuBufferRef& vertexBuffer = m_vertexBuffers[lodIndex];

    const size_t rangeOffset = firstVertex * srcVertexSize;
    const size_t rangeSize = vertexRange.vertexCount * srcVertexSize;

    if (!vertexBuffer.IsValid() || vertexBuffer->Size() < rangeOffset + rangeSize)
    {
        // not yet created

        return;
    }

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(rangeSize);
    stagingBuffer->Copy(rangeSize, vertexRange.floatData);
    stagingBuffer->Flush(0, rangeSize);

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder(CommandRecorderQueue::PreRender);

    cr << InsertBarrier(stagingBuffer, ResourceState::CopySrc);
    cr << InsertBarrier(vertexBuffer, ResourceState::CopyDst);

    cr << CopyBuffer(stagingBuffer, vertexBuffer, 0, uint32(rangeOffset), uint32(rangeSize));

    cr << InsertBarrier(vertexBuffer, ResourceState::VertexBuffer);

    // Gets submitted along with the frame - not transient cmd buffer
    // @FIXME: This could be changed in the future, but right now we're getting some validation errors with it - needs an eye on it
    cr.Done();
}

void Mesh::UpdateDynamicBVH()
{
    Assert(IsDynamicMesh());

    BVHNode bvh;

    {
        auto readScope = GetReadScope();
        BuildBVH(bvh);
    }

    {
        auto writeScope = GetWriteScope();
        m_bvh = std::move(bvh);
    }

    // No mark dirty - intentional
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
        else
        {
            // a LOD that is going away must not keep its old key or size, or it gets saved and paged back in
            m_lodData[lodIndex].vertexData = BlobDataReference {};
        }

        if (indices.Size() != 0)
        {
            AllocateBlobData(m_lodData[lodIndex].indexData, indices.Data(), indices.Size(), alignof(uint32));
        }
        else
        {
            m_lodData[lodIndex].indexData = BlobDataReference {};
        }
    }

    m_meshDesc = meshDesc;
    m_lodDataVersion.Increment(1, MemoryOrder::RELEASE);

#ifdef HYP_EDITOR
    ++m_lod0DataRevision;
#endif // HYP_EDITOR

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

void Mesh::SetLodGenerationSettings(const MeshLodGenerationSettings& settings)
{
    m_lodGenerationSettings = settings;

    MarkDirty();
}

uint64 Mesh::ComputeLod0DataHash() const
{
    const VertexArrayView vertices = GetVertexData(0);
    const Span<const ubyte> indices = GetIndexData(0);

    if (!vertices.floatData || !indices.Data())
    {
        return 0;
    }

    const ubyte* vertexBytes = reinterpret_cast<const ubyte*>(vertices.floatData);
    const size_t vertexDataSize = vertices.layoutDesc.VertexSize() * vertices.vertexCount;

    HashCode hashCode;
    hashCode.Add(FNV1::DoHashBytes(vertexBytes, vertexBytes + vertexDataSize));
    hashCode.Add(FNV1::DoHashBytes(indices.Data(), indices.Data() + indices.Size()));

    return uint64(hashCode.Value());
}

void Mesh::SetLightmapUVDataHash(uint64 lightmapUvDataHash)
{
    if (m_lightmapUvDataHash == lightmapUvDataHash)
    {
        return;
    }

    m_lightmapUvDataHash = lightmapUvDataHash;

    MarkDirty();
}

bool Mesh::HasValidLightmapUVs() const
{
    if (m_lightmapUvDataHash == 0 || !(m_meshDesc.meshAttributes.inputLayout.mask & VT_UV1))
    {
        return false;
    }

    return ComputeLod0DataHash() == m_lightmapUvDataHash;
}

bool Mesh::AreLodsOutOfDate() const
{
    if (m_meshDesc.GetNumLods() <= 1 || m_lodGenerationSettings.sourceDataHash == 0)
    {
        return false;
    }

    return ComputeLod0DataHash() != m_lodGenerationSettings.sourceDataHash;
}

void Mesh::SetLodData(
    uint8 lodIndex,
    const MeshLodDesc& lodDesc,
    const VertexArrayView& vertices,
    ConstByteView indices)
{
    Assert(lodIndex > 0 && lodIndex < MaxMeshLods, "LOD 0 must be set through SetMeshData()");
    Assert(vertices.layoutDesc.mask == m_meshDesc.meshAttributes.inputLayout.mask,
        "LOD {} has input layout mask {} but the mesh uses {}", lodIndex, vertices.layoutDesc.mask, m_meshDesc.meshAttributes.inputLayout.mask);

    auto writeScope = GetWriteScope();

    FreeBlobData(m_lodData[lodIndex].vertexData);
    FreeBlobData(m_lodData[lodIndex].indexData);

    m_lodData[lodIndex].vertexData = BlobDataReference {};
    m_lodData[lodIndex].indexData = BlobDataReference {};

    AllocateBlobData(m_lodData[lodIndex].vertexData, vertices.floatData, vertices.layoutDesc.VertexSize() * vertices.vertexCount, 16);
    AllocateBlobData(m_lodData[lodIndex].indexData, indices.Data(), indices.Size(), alignof(uint32));

    m_meshDesc.lods[lodIndex] = lodDesc;
    m_meshDesc.lods[lodIndex].numVertices = uint32(vertices.vertexCount);
    m_meshDesc.lods[lodIndex].numIndices = uint32(indices.Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));

    m_lodDataVersion.Increment(1, MemoryOrder::RELEASE);

    MarkDirty();

    writeScope.Reset();
}

void Mesh::ClearLodData(uint8 firstLodIndex)
{
    for (uint8 lodIndex = firstLodIndex; lodIndex < MaxMeshLods; lodIndex++)
    {
        for (BlobDataReference* reference : { &m_lodData[lodIndex].vertexData, &m_lodData[lodIndex].indexData })
        {
            if (reference->readOnly)
            {
                // Mapped from blob storage; the storage owns the memory
                reference->raw = nullptr;
            }
            else
            {
                FreeBlobData(*reference);
            }

            *reference = BlobDataReference {};
        }

        m_meshDesc.lods[lodIndex] = MeshLodDesc {};

        if (m_vertexBuffers[lodIndex].IsValid())
        {
            EnqueueDeletion(std::move(m_vertexBuffers[lodIndex]));
        }

        if (m_indexBuffers[lodIndex].IsValid())
        {
            EnqueueDeletion(std::move(m_indexBuffers[lodIndex]));
        }
    }
}

void Mesh::ClearLods(uint8 firstLodIndex)
{
    Assert(firstLodIndex > 0 && firstLodIndex < MaxMeshLods, "LOD 0 cannot be cleared");

    if (m_meshDesc.GetNumLods() <= firstLodIndex)
    {
        return;
    }

    auto writeScope = GetWriteScope();

    ClearLodData(firstLodIndex);

    m_lodDataVersion.Increment(1, MemoryOrder::RELEASE);

    MarkDirty();

    writeScope.Reset();
}

Handle<Mesh> Mesh::Clone() const
{
    auto readScope = GetReadScope();

    MeshDesc meshDesc = m_meshDesc;
    MeshDataView meshData {};

    for (uint8 lodIndex = 0; lodIndex < MaxMeshLods; lodIndex++)
    {
        const Span<const ubyte> indexData = GetIndexData(lodIndex);
        const VertexArrayView vertexData = GetVertexData(lodIndex);

        if (indexData.Size() == 0 || vertexData.vertexCount == 0)
        {
            // a LOD without data can't be cloned; make the desc agree so the copy stays consistent
            meshDesc.lods[lodIndex] = MeshLodDesc {};

            continue;
        }

        meshData.vertices[lodIndex] = vertexData;
        meshData.indices[lodIndex] = ConstByteView(indexData.Data(), indexData.Data() + indexData.Size());
    }

    Handle<Mesh> mesh = MakeHandle<Mesh>();
    mesh->SetName(GetName()); // will be unique'd
    mesh->SetMeshData(meshDesc, meshData);
    mesh->SetFlags(m_flags);
    mesh->SetAABB(m_aabb);
    mesh->SetLodGenerationSettings(m_lodGenerationSettings);
    mesh->SetLightmapUVDataHash(m_lightmapUvDataHash);

    if (m_bvh.IsValid())
    {
        auto writeScope = mesh->GetWriteScope();

        BVHNode bvh = m_bvh;
        mesh->SetBVH(std::move(bvh));
    }

    return mesh;
}

Handle<AssetObject> Mesh::CloneAsset() const
{
    return Clone();
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

void Mesh::BuildBVH(BVHNode& bvhNode, int maxDepth, uint8 lodIndex) const
{
    AssertDebug(lodIndex < MaxMeshLods);

    const VertexArrayView vertexData = GetVertexData(lodIndex);
    const Span<const ubyte> indexData = GetIndexData(lodIndex);
    const uint32 numVertices = uint32(vertexData.vertexCount);
    const uint32 numIndices = uint32(indexData.Size() / sizeof(uint32));

    const BoundingBox meshAabb = CalculateAABB(lodIndex);

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

BoundingBox Mesh::CalculateAABB(uint8 lodIndex) const
{
    // Simplified LODs never leave LOD 0's bounds, so the mesh AABB stays LOD 0's.
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
        else if (dstMask & VT_Skeletal)
        {
            dstFloatBuffer += sizeof(TVertexPacket<VT_Skeletal>) / sizeof(float);
        }
        if (srcMask & VT_Skeletal)
        {
            srcFloatBuffer += sizeof(TVertexPacket<VT_Skeletal>) / sizeof(float);
        }

        // Zeroed tree and foliage data hold the mesh still
        if (combinedMask & VT_Tree)
        {
            Memory::Copy(dstFloatBuffer, srcFloatBuffer, sizeof(TVertexPacket<VT_Tree>));
        }
        else if (dstMask & VT_Tree)
        {
            Memory::Zero(dstFloatBuffer, sizeof(TVertexPacket<VT_Tree>));
        }
        if (srcMask & VT_Tree)
        {
            srcFloatBuffer += sizeof(TVertexPacket<VT_Tree>) / sizeof(float);
        }
        if (dstMask & VT_Tree)
        {
            dstFloatBuffer += sizeof(TVertexPacket<VT_Tree>) / sizeof(float);
        }

        if (combinedMask & VT_Foliage)
        {
            Memory::Copy(dstFloatBuffer, srcFloatBuffer, sizeof(TVertexPacket<VT_Foliage>));
        }
        else if (dstMask & VT_Foliage)
        {
            Memory::Zero(dstFloatBuffer, sizeof(TVertexPacket<VT_Foliage>));
        }
        if (srcMask & VT_Foliage)
        {
            srcFloatBuffer += sizeof(TVertexPacket<VT_Foliage>) / sizeof(float);
        }
        if (dstMask & VT_Foliage)
        {
            dstFloatBuffer += sizeof(TVertexPacket<VT_Foliage>) / sizeof(float);
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

#ifdef HYP_EDITOR
    ++m_lod0DataRevision;
#endif // HYP_EDITOR
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

bool Mesh::CanGenerateLods() const
{
    return MeshLodGenerator::CanGenerate(this);
}

void Mesh::GenerateLods()
{
    TResult<MeshLodGenerationResult> result = MeshLodGenerator::Generate(this, m_lodGenerationSettings);

    if (result.HasError())
    {
        HYP_LOG(Rendering, Error, "Failed to generate LODs for mesh {}: {}", GetName(), result.GetError().GetMessage());

        return;
    }

    if (Result applyResult = MeshLodGenerator::Apply(this, result.GetValue()); applyResult.HasError())
    {
        HYP_LOG(Rendering, Error, "Failed to apply generated LODs to mesh {}: {}", GetName(), applyResult.GetError().GetMessage());

        return;
    }

    HYP_LOG(Rendering, Info, "Generated {} LOD(s) for mesh {}", result.GetValue().lods.Size(), GetName());
}

void Mesh::ClearGeneratedLods()
{
    ClearLods(1);

    MeshLodGenerationSettings settings = m_lodGenerationSettings;
    settings.sourceDataHash = 0;
    SetLodGenerationSettings(settings);
}

#endif // HYP_EDITOR

#pragma endregion Mesh

} // namespace Hyperion
