/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderCommand.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Frame.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

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

Array<VertexType> VertexTypeMask::GetAllTypes() const
{
    Array<VertexType> attributes;
    FOR_EACH_BIT(flagMask, i)
    {
        attributes.PushBack(VertexType(1u << i));
    }

    return attributes;
}

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

Mesh::Mesh(const VertexArrayView& vertexData, const ByteBuffer& indexData, Topology topology)
    : Mesh(vertexData, indexData, topology, StaticVertexInputLayout<VT_Simple>)
{
}

Mesh::Mesh(const VertexArrayView& vertexData, const ByteBuffer& indexData, Topology topology, const VertexInputLayoutDesc& inputLayout)
    : AssetObject(),
      m_aabb(BoundingBox::Empty()),
      m_flags(MeshFlags::None)
{
    m_meshDesc = MeshDesc {};
    m_meshDesc.meshAttributes.inputLayout = inputLayout;
    m_meshDesc.meshAttributes.topology = topology;
    m_meshDesc.numVertices = uint32(vertexData.vertexCount);
    m_meshDesc.numIndices = uint32(indexData.Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));

    AllocateBlobData(m_vertexData, vertexData.floatData, inputLayout.VertexSize() * vertexData.vertexCount, 16);
    AllocateBlobData(m_indexData, indexData.Data(), indexData.Size(), alignof(uint32));

    m_aabb = CalculateAABB();
}

Mesh::~Mesh()
{
    if (m_vertexBuffer.IsValid())
    {
        EnqueueDeletion(std::move(m_vertexBuffer));
    }

    if (m_indexBuffer.IsValid())
    {
        EnqueueDeletion(std::move(m_indexBuffer));
    }

    FreeBlobData(m_vertexData);
    FreeBlobData(m_indexData);
    FreeBlobData(m_bvhData);
}

VertexArrayView Mesh::GetVertexData() const
{
    Assert(m_vertexData.raw != nullptr, "Vertex data not loaded!");

    const float* floatData = reinterpret_cast<const float*>(m_vertexData.raw);

    const size_t vertexSize = m_meshDesc.meshAttributes.inputLayout.VertexSize();
    Assert(vertexSize != 0); // bad vertex size in layout desc - corrupt?

    VertexArrayView view {};
    view.floatData = floatData;
    view.vertexCount = m_meshDesc.numVertices;
    view.layoutDesc = m_meshDesc.meshAttributes.inputLayout;

    return view;
}

void Mesh::SetVertexData(const VertexArrayView& view)
{
    Assert(AtomicAdd(&m_rwState, 0) & 0x1);

    size_t vertexSize = view.layoutDesc.VertexSize();
    Assert(vertexSize != 0);

    FreeBlobData(m_vertexData);
    AllocateBlobData(m_vertexData, view.floatData, view.vertexCount * vertexSize, 16);

    MarkDirty();
}

void Mesh::SetIndexData(Span<const ubyte> indexData)
{
    Assert(AtomicAdd(&m_rwState, 0) & 0x1);

    FreeBlobData(m_indexData);
    AllocateBlobData(m_indexData, indexData.Data(), indexData.Size(), alignof(uint32));

    MarkDirty();
}

void Mesh::PageBlobData()
{
    if (IsTransient() || !IsRegistered())
    {
        return;
    }

    Handle<AssetRegistry> registry = GetAssetRegistry();
    AssertDebug(registry.IsValid());

    if (!registry.IsValid())
    {
        return;
    }

    bool needsSaveBlobData = false;

    BlobStorage* blobStorage = registry->HasBlobStorage() ? &registry->GetBlobStorage() : nullptr;

    if (m_vertexData.raw == nullptr
        && m_vertexData.key
        && m_vertexData.size != 0)
    {
        if (!blobStorage || !blobStorage->GetData(m_vertexData.key, m_vertexData.size, m_vertexData.raw))
        {
            ([&]()
             {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
                 // check if failed; if so, try to import from raw data blob in project directory
                 FileByteReader stream { registry->GetRootPath() / AssetBuckets::Meshes.GetName() / (String(*GetName()) + ".VB.raw.blob") };
                 if (!stream.Eof())
                 {
                     ByteBuffer buffer = stream.Read(stream.Max());

                     AllocateBlobData(m_vertexData, buffer.Data(), buffer.Size(), 16);

                     needsSaveBlobData = true;

                     return;
                 }
#endif

                 HYP_LOG(Assets, Error, "Blob data missing or corrupted for {} vertex buffer", GetName());
             })();
        }
        else
        {
            m_vertexData.readOnly = true;
        }

        if (!blobStorage || !blobStorage->GetData(m_indexData.key, m_indexData.size, m_indexData.raw))
        {
            ([&]()
             {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
                 // check if failed; if so, try to import from raw data blob in project directory
                 FileByteReader stream { registry->GetRootPath() / AssetBuckets::Meshes.GetName() / (String(*GetName()) + ".IB.raw.blob") };
                 if (!stream.Eof())
                 {
                     ByteBuffer buffer = stream.Read(stream.Max());
                     AssertDebug(buffer.Size() == stream.Max());

                     AllocateBlobData(m_indexData, buffer.Data(), buffer.Size(), alignof(uint32));

                     needsSaveBlobData = true;

                     return;
                 }
#endif

                 HYP_LOG(Assets, Error, "Blob data missing or corrupted for {} index buffer", GetName());
             })();
        }
        else
        {
            m_indexData.readOnly = true;
        }
    }

    // Keep BVH data separate from vertex and index data because it is mutually exclusive from them
    if (m_bvhData.raw == nullptr
        && m_bvhData.key
        && m_bvhData.size != 0)
    {
        if (!blobStorage || !blobStorage->GetData(m_bvhData.key, m_bvhData.size, m_bvhData.raw))
        {
            ([&]()
             {
#if HYP_EDITOR || HYP_ALLOW_INLINE_BLOBS
                 FileByteReader stream { registry->GetRootPath() / AssetBuckets::Meshes.GetName() / (String(*GetName()) + ".BVH.raw.blob") };
                 if (!stream.Eof())
                 {
                     ByteBuffer buffer = stream.Read(stream.Max());

                     AllocateBlobData(m_bvhData, buffer.Data(), buffer.Size(), alignof(uint32));

                     needsSaveBlobData = true;

                     return;
                 }
#endif

                 HYP_FAIL("Blob data missing! Data corruption detected.");
             })();
        }
        else
        {
            m_bvhData.readOnly = true;
        }

        if (m_bvhData.raw != nullptr)
        {
            BVHNode::Deserialize(m_bvh, m_bvhData.raw, m_bvhData.size);
        }
    }

#if HYP_EDITOR
    // Update to use blob cache rather than inline
    if (needsSaveBlobData && blobStorage != nullptr)
    {
        Result saveBlobDataResult = SaveBlobData(blobStorage);
        if (saveBlobDataResult.HasError())
        {
            HYP_LOG(Assets, Error, "Failed to save local blob data: {}", saveBlobDataResult.GetError().GetMessage());
        }

        MarkDirty();
    }
#endif
}

void Mesh::UnpageBlobData()
{
    if (m_vertexData.readOnly)
    {
        m_vertexData.raw = nullptr;
        m_indexData.raw = nullptr;
    }

    if (m_bvhData.readOnly)
    {
        m_bvhData.raw = nullptr;
    }
}

void Mesh::UploadGpuData()
{
    auto readScope = GetReadScope();

    // @TODO fix for non-uint32 indices
    Assert(GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType) == 4);

    Array<float, ThreadAllocator> vertices;
    BuildVertexBuffer(m_meshDesc.meshAttributes.inputLayout, vertices);

    const Span<const ubyte> indexData = GetIndexData();

    if (vertices.Size() == 0 || indexData.Size() == 0)
    {
        // No data
        return;
    }

    TByteBuffer<ThreadAllocator> indices;
    indices.SetSize(indexData.Size());
    Memory::Copy(indices.Data(), indexData.Data(), indexData.Size());

    const size_t vertexSize = m_meshDesc.meshAttributes.inputLayout.VertexSize();
    const size_t vertexSizeInFloats = vertexSize / sizeof(float);

    AssertDebug(vertices.Size() == m_meshDesc.numVertices * vertexSizeInFloats);
    AssertDebug(indices.Size() == m_meshDesc.numIndices * sizeof(uint32));

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
    if (m_meshDesc.numIndices == 0)
    {
        indices.SetSize(3 * sizeof(uint32));
    }
    else if (m_meshDesc.numIndices % 3 != 0)
    {
        indices.SetSize((m_meshDesc.numIndices + (3 - (m_meshDesc.numIndices % 3))) * sizeof(uint32));
    }

    const size_t packedVerticesSize = vertices.ByteSize();
    const size_t packedIndicesSize = indices.Size();

    GpuBufferRef vertexBuffer = RI.MakeGpuBuffer(GpuBufferType::VertexBuffer, packedVerticesSize);
    GpuBufferRef indexBuffer = RI.MakeGpuBuffer(GpuBufferType::IndexBuffer, packedIndicesSize);

#if HYP_DEBUG_MODE
    vertexBuffer->SetDebugName(NAME_FMT("{}_VBO", GetName()));
    indexBuffer->SetDebugName(NAME_FMT("{}_IBO", GetName()));
#endif

    CheckResult(vertexBuffer->Create());
    CheckResult(indexBuffer->Create());

    AssertDebug(vertexBuffer.IsValid() && indexBuffer.IsValid());

    const size_t bufferSizeCombined = packedVerticesSize + packedIndicesSize;

    GpuBuffer* stagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(bufferSizeCombined);
    stagingBuffer->Copy(packedVerticesSize, vertices.Data());
    stagingBuffer->Copy(packedVerticesSize, packedIndicesSize, indices.Data());
    stagingBuffer->Flush(0, bufferSizeCombined);

    Frame* currentFrame = nullptr;

    CommandRecorder& cr = IsOnThread(g_renderThread) && (currentFrame = RI.GetCurrentFrame()) != nullptr
        ? currentFrame->preRenderCommands
        : RI.commandRecorderAllocator.GetCommandRecorder();

    cr << InsertBarrier(stagingBuffer, RS_COPY_SRC);

    cr << InsertBarrier(vertexBuffer, RS_COPY_DST);
    cr << InsertBarrier(indexBuffer, RS_COPY_DST);

    cr << CopyBuffer(stagingBuffer, vertexBuffer, packedVerticesSize);
    cr << CopyBuffer(stagingBuffer, indexBuffer, packedVerticesSize, 0, packedIndicesSize);

    cr << InsertBarrier(vertexBuffer, RS_VERTEX_BUFFER);
    cr << InsertBarrier(indexBuffer, RS_INDEX_BUFFER);

    if (m_vertexBuffer.IsValid())
    {
        EnqueueDeletion(std::move(m_vertexBuffer));
    }

    m_vertexBuffer = std::move(vertexBuffer);

    if (m_indexBuffer.IsValid())
    {
        EnqueueDeletion(std::move(m_indexBuffer));
    }

    m_indexBuffer = std::move(indexBuffer);

    isUploaded.Store(true);

    if (!currentFrame || &cr != &currentFrame->preRenderCommands)
    {
        cr.Done();
    }
}

void Mesh::ReleaseGpuData()
{
    auto writeScope = GetWriteScope();

    EnqueueDeletion(std::move(m_vertexBuffer));
    EnqueueDeletion(std::move(m_indexBuffer));

    isUploaded.Store(false);
}

Result Mesh::Rename(Name name)
{
    return AssetObject::Rename(name);
}

void Mesh::SetMeshData(
    const MeshDesc& meshDesc,
    const VertexArrayView& vertices,
    Span<const ubyte> indices)
{
    auto writeScope = GetWriteScope();

    FreeBlobData(m_vertexData);
    FreeBlobData(m_indexData);

    AllocateBlobData(m_vertexData, vertices.floatData, vertices.layoutDesc.VertexSize() * vertices.vertexCount, 16);
    AllocateBlobData(m_indexData, indices.Data(), indices.Size(), alignof(uint32));

    m_meshDesc = meshDesc;

    AssertDebug(m_meshDesc.numVertices == vertices.vertexCount);
    AssertDebug(m_meshDesc.numIndices == indices.Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));

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
        SetPersistentRequested(m_flags[MeshFlags::ViewIndependent], /* setFlag */ true, /* markDirty */ false);
    }

    MarkDirty();
}

bool Mesh::BuildBVH(int maxDepth)
{
    auto resGuard = GetReadScope();

    if (m_meshDesc.numIndices == 0)
    {
        // no data to build from
        return false;
    }

    AssertDebug(GetVertexData().vertexCount > 0);
    AssertDebug(GetIndexData().Size() > 0);

    if (!BuildBVH(m_bvh, maxDepth))
    {
        return false;
    }

    // Serialize the BVH into blob data so it can be saved/loaded without a rebuild
    ByteBuffer bvhBuffer = BVHNode::Serialize(m_bvh);
    FreeBlobData(m_bvhData);
    AllocateBlobData(m_bvhData, bvhBuffer.Data(), bvhBuffer.Size(), alignof(uint32));

    return true;
}

bool Mesh::BuildBVH(BVHNode& bvhNode, int maxDepth) const
{
    const VertexArrayView vertexData = GetVertexData();
    const Span<const ubyte> indexData = GetIndexData();
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

    return true;
}

BoundingBox Mesh::CalculateAABB() const
{
    const VertexArrayView vertexArrayView = GetVertexData();

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
    Array<float, AllocatorType>& outData) const
{
    const VertexArrayView vertices = GetVertexData();
    AssertDebug(uintptr_t(vertices.floatData) > 0x1000000);

    const uint8 srcMask = m_meshDesc.meshAttributes.inputLayout.mask;
    const uint8 dstMask = inputLayout.mask;
    const uint8 combinedMask = srcMask & dstMask;

    const size_t srcVertexSize = m_meshDesc.meshAttributes.inputLayout.VertexSize();
    const size_t srcVertexSizeInFloats = srcVertexSize / sizeof(float);

    const size_t dstVertexSize = inputLayout.VertexSize();
    const size_t dstVertexSizeInFloats = dstVertexSize / sizeof(float);

    outData.Resize(dstVertexSizeInFloats * vertices.vertexCount);

    size_t currentOffset = 0;

    for (size_t i = 0; i < vertices.vertexCount; i++)
    {
        const float* srcFloatBuffer = vertices.floatData + (i * srcVertexSizeInFloats);
        AssertDebug((uintptr_t(srcFloatBuffer + srcVertexSizeInFloats) - uintptr_t(vertices.floatData)) <= m_vertexData.size);

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

template void Mesh::BuildVertexBuffer<ThreadAllocator>(const VertexInputLayoutDesc& inputLayout, Array<float, ThreadAllocator>& outData) const;
template void Mesh::BuildVertexBuffer<DynamicAllocator>(const VertexInputLayoutDesc& inputLayout, Array<float, DynamicAllocator>& outData) const;

#define ADD_NORMAL(ary, idx, normal)     \
    do                                   \
    {                                    \
        auto* idx_it = ary.TryGet(idx);  \
        if (!idx_it)                     \
        {                                \
            idx_it = &*ary.Emplace(idx); \
        }                                \
        idx_it->PushBack(normal);        \
    }                                    \
    while (0)

void Mesh::CalculateNormals(bool weighted)
{
    VertexArrayView vertexData = GetVertexData();
    AssertDebug(((VT_Position | VT_Normal) & vertexData.layoutDesc.mask) == (VT_Position | VT_Normal),
                "Vertex data must have VT_Position and VT_Normal at least in order to calculate normals");

    Span<ubyte> indexData = GetIndexData();

    const uint32 numVertices = uint32(vertexData.vertexCount);
    const uint32 numIndices = uint32(indexData.Size() / sizeof(uint32));

    // @TODO fix for non-uint32 indices

    uint32* uIndexData = reinterpret_cast<uint32*>(&indexData[0]);

    SparsePagedArray<TFatArray<Vec3f, InlineAllocator<3>>, (1 << 6)> normals;

    const size_t vertexSizeInFloats = vertexData.layoutDesc.VertexSize() / sizeof(float);

    // compute per-face normals (facet normals)
    for (size_t i = 0; i < numIndices; i += 3)
    {
        const uint32 i0 = uIndexData[i];
        const uint32 i1 = uIndexData[i + 1];
        const uint32 i2 = uIndexData[i + 2];

        const float* floatDataOffset0 = vertexData.floatData + (i0 * vertexSizeInFloats);
        const float* floatDataOffset1 = vertexData.floatData + (i1 * vertexSizeInFloats);
        const float* floatDataOffset2 = vertexData.floatData + (i2 * vertexSizeInFloats);

        const TVertexPacket<VT_Position>* packet0 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset0);
        const TVertexPacket<VT_Position>* packet1 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset1);
        const TVertexPacket<VT_Position>* packet2 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset2);

        const Vec3f p0 = packet0->GetPosition();
        const Vec3f p1 = packet1->GetPosition();
        const Vec3f p2 = packet2->GetPosition();

        const Vec3f u = p2 - p0;
        const Vec3f v = p1 - p0;
        const Vec3f n = u.Cross(v).Normalize();

        ADD_NORMAL(normals, i0, n);
        ADD_NORMAL(normals, i1, n);
        ADD_NORMAL(normals, i2, n);
    }

    for (size_t i = 0; i < numVertices; i++)
    {
        AssertDebug(normals.HasIndex(uint32(i)));

        float* floatDataOffset = const_cast<float*>(vertexData.floatData + (i * vertexSizeInFloats));
        TVertexPacket<VT_Normal>* packet = reinterpret_cast<TVertexPacket<VT_Normal>*>(floatDataOffset + (sizeof(TVertexPacket<VT_Position>) / sizeof(float)));

        if (weighted)
        {
            packet->SetNormal(normals.Get(uint32(i)).Sum());
        }
        else
        {
            packet->SetNormal(normals.Get(uint32(i)).Sum().Normalize());
        }
    }

    if (!weighted)
    {
        return;
    }

    normals.Clear();

    // weighted (smooth) normals

    for (size_t i = 0; i < numIndices; i += 3)
    {
        const uint32 i0 = uIndexData[i];
        const uint32 i1 = uIndexData[i + 1];
        const uint32 i2 = uIndexData[i + 2];

        const float* floatDataOffset0 = vertexData.floatData + (i0 * vertexSizeInFloats);
        const float* floatDataOffset1 = vertexData.floatData + (i1 * vertexSizeInFloats);
        const float* floatDataOffset2 = vertexData.floatData + (i2 * vertexSizeInFloats);

        const TVertexPacket<VT_Position>* posPacket0 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset0);
        const TVertexPacket<VT_Position>* posPacket1 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset1);
        const TVertexPacket<VT_Position>* posPacket2 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset2);

        const Vec3f p0 = posPacket0->GetPosition();
        const Vec3f p1 = posPacket1->GetPosition();
        const Vec3f p2 = posPacket2->GetPosition();

        const TVertexPacket<VT_Normal>* normPacket0 = reinterpret_cast<const TVertexPacket<VT_Normal>*>(posPacket0 + 1);
        const TVertexPacket<VT_Normal>* normPacket1 = reinterpret_cast<const TVertexPacket<VT_Normal>*>(posPacket1 + 1);
        const TVertexPacket<VT_Normal>* normPacket2 = reinterpret_cast<const TVertexPacket<VT_Normal>*>(posPacket2 + 1);

        const Vec3f n0 = normPacket0->GetNormal();
        const Vec3f n1 = normPacket1->GetNormal();
        const Vec3f n2 = normPacket2->GetNormal();

        // Vector3 n = FixedArray { n0, n1, n2 }.Avg();

        FixedArray<Vec3f, 3> weightedNormals { n0, n1, n2 };

        // nested loop through faces to get weighted neighbours
        // any code that uses this really should bake the normals in;
        // especially for any production code. this is an expensive process
        for (size_t j = 0; j < numIndices; j += 3)
        {
            if (j == i)
            {
                continue;
            }

            const uint32 j0 = uIndexData[j];
            const uint32 j1 = uIndexData[j + 1];
            const uint32 j2 = uIndexData[j + 2];

            floatDataOffset0 = vertexData.floatData + (j0 * vertexSizeInFloats);
            floatDataOffset1 = vertexData.floatData + (j1 * vertexSizeInFloats);
            floatDataOffset2 = vertexData.floatData + (j2 * vertexSizeInFloats);

            posPacket0 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset0);
            posPacket1 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset1);
            posPacket2 = reinterpret_cast<const TVertexPacket<VT_Position>*>(floatDataOffset2);

            normPacket0 = reinterpret_cast<const TVertexPacket<VT_Normal>*>(posPacket0 + 1);
            normPacket1 = reinterpret_cast<const TVertexPacket<VT_Normal>*>(posPacket1 + 1);
            normPacket2 = reinterpret_cast<const TVertexPacket<VT_Normal>*>(posPacket2 + 1);

            const FixedArray<Vec3f, 3> facePositions {
                posPacket0->GetPosition(),
                posPacket1->GetPosition(),
                posPacket2->GetPosition()
            };

            const FixedArray<Vec3f, 3> faceNormals {
                normPacket0->GetNormal(),
                normPacket1->GetNormal(),
                normPacket2->GetNormal()
            };

            const Vec3f a = p1 - p0;
            const Vec3f b = p2 - p0;
            const Vec3f c = a.Cross(b);

            const float area = 0.5f * MathUtil::Sqrt(c.Dot(c));

            if (facePositions.Contains(p0))
            {
                const float angle = (p0 - p1).AngleBetween(p0 - p2);
                weightedNormals[0] += faceNormals.Avg() * area * angle;
            }

            if (facePositions.Contains(p1))
            {
                const float angle = (p1 - p0).AngleBetween(p1 - p2);
                weightedNormals[1] += faceNormals.Avg() * area * angle;
            }

            if (facePositions.Contains(p2))
            {
                const float angle = (p2 - p0).AngleBetween(p2 - p1);
                weightedNormals[2] += faceNormals.Avg() * area * angle;
            }

            // if (facePositions.Contains(p0)) {
            //     weightedNormals[0] += faceNormals.Avg();
            // }

            // if (facePositions.Contains(p1)) {
            //     weightedNormals[1] += faceNormals.Avg();
            // }

            // if (facePositions.Contains(p2)) {
            //     weightedNormals[2] += faceNormals.Avg();
            // }
        }

        ADD_NORMAL(normals, i0, weightedNormals[0].Normalized());
        ADD_NORMAL(normals, i1, weightedNormals[1].Normalized());
        ADD_NORMAL(normals, i2, weightedNormals[2].Normalized());
    }

    for (size_t i = 0; i < numVertices; i++)
    {
        AssertDebug(normals.HasIndex(i));

        float* floatDataOffset = const_cast<float*>(vertexData.floatData + (i * vertexSizeInFloats));
        TVertexPacket<VT_Normal>* packet = reinterpret_cast<TVertexPacket<VT_Normal>*>(floatDataOffset + (sizeof(TVertexPacket<VT_Position>) / sizeof(float)));

        packet->SetNormal(normals.Get(i).Sum().Normalized());
    }

    normals.Clear();
}

#undef ADD_NORMAL

#pragma endregion Mesh

} // namespace Hyperion
