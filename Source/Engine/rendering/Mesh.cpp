/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Frame.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <Core/containers/SparsePagedArray.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/BVH.hpp>

#include <engine/EngineDriver.hpp>

#include <cstring>

#include <Mesh.generated.inl>

namespace Hyperion {

#if HYP_EDITOR
HYP_DECLARE_LOG_CHANNEL(Editor);
#endif

static const Name s_nameMeshDefault = NAME("<unnamed mesh>");

const VertexAttribute* VertexAttribute::Attrs[] = {
    &Position,
    &Normal,
    &TexCoord0,
    &TexCoord1,
    &Tangent,
    &Bitangent,
    &BoneIndices,
    &BoneWeights,
    nullptr
};

#pragma region VertexAttributeSet

const VertexAttributeSet VertexAttributeSet::StaticMeshVertexAttributes =
    VertexAttribute::Position | VertexAttribute::Normal | VertexAttribute::TexCoord0;

const VertexAttributeSet VertexAttributeSet::SkeletalMeshVertexAttributes =
    VertexAttribute::BoneWeights | VertexAttribute::BoneIndices;

Array<const VertexAttribute*> VertexAttributeSet::BuildAttributes() const
{
    Array<const VertexAttribute*> attributes;
    FOR_EACH_BIT(flagMask, i)
    {
        attributes.PushBack(VertexAttribute::Attrs[i]);
    }

    return attributes;
}

SizeType VertexAttributeSet::CalculateVertexSize() const
{
    SizeType size = 0;

    FOR_EACH_BIT(flagMask, i)
    {
        size += VertexAttribute::Attrs[i]->size;
    }

    return size;
}

String VertexAttributeSet::ToString() const
{
    String result = "";
    bool first = true;

    FOR_EACH_BIT(flagMask, i)
    {
        if (!first)
        {
            result += ", ";
        }

        result += *VertexAttribute::Attrs[i]->name;
        first = false;
    }

    return result;
}

#pragma endregion VertexAttributeSet

#pragma region Mesh

Pair<Array<Vertex>, Array<uint32>> Mesh::CalculateIndices(const Array<Vertex>& vertices)
{
    HashMap<Vertex, uint32> indexMap;

    Array<uint32> indices;
    indices.Reserve(vertices.Size());

    /* This will be our resulting buffer with only the vertices we need. */
    Array<Vertex> newVertices;
    newVertices.Reserve(vertices.Size());

    for (const auto& vertex : vertices)
    {
        /* Check if the vertex already exists in our map */
        auto it = indexMap.Find(vertex);

        /* If it does, push to our indices */
        if (it != indexMap.End())
        {
            indices.PushBack(it->second);

            continue;
        }

        const uint32 meshIndex = uint32(newVertices.Size());

        /* The vertex is unique, so we push it. */
        newVertices.PushBack(vertex);
        indices.PushBack(meshIndex);

        indexMap[vertex] = meshIndex;
    }

    return { std::move(newVertices), std::move(indices) };
}

Mesh::Mesh()
    : AssetObject(),
      m_aabb(BoundingBox::Empty()),
      m_flags(MeshFlags::None)
{
}

Mesh::Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology)
    : Mesh(
          vertexData,
          indexData,
          topology,
          VertexAttributeSet::StaticMeshVertexAttributes | VertexAttributeSet::SkeletalMeshVertexAttributes)
{
}

Mesh::Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology, const VertexAttributeSet& vertexAttributes)
    : AssetObject(),
      m_aabb(BoundingBox::Empty()),
      m_flags(MeshFlags::None)
{
    m_meshDesc = MeshDesc {};
    m_meshDesc.meshAttributes.vertexAttributes = vertexAttributes;
    m_meshDesc.meshAttributes.topology = topology;
    m_meshDesc.numVertices = uint32(vertexData.Size());
    m_meshDesc.numIndices = uint32(indexData.Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));

    AllocateBlobData(m_vertexData, vertexData.Data(), sizeof(Vertex) * vertexData.Size(), alignof(Vertex));
    AllocateBlobData(m_indexData, indexData.Data(), indexData.Size(), alignof(uint32));

    m_aabb = CalculateAABB();
}

Mesh::~Mesh()
{
    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();

    FreeBlobData(m_vertexData);
    FreeBlobData(m_indexData);
}

void Mesh::Init()
{
    HYP_SCOPE;

    if (m_flags[MeshFlags::ViewIndependent])
    {
        SetPersistentRequested(true, /* setFlag */ true);

        UploadGpuData();
    }

    AssetObject::Init();

    SetReady(true);
}

void Mesh::SetVertexData(Span<const Vertex> vertexData)
{
    Assert(AtomicAdd(&m_rwState, 0) & 0x1);

    FreeBlobData(m_vertexData);
    AllocateBlobData(m_vertexData, vertexData.Data(), sizeof(Vertex) * vertexData.Size(), alignof(Vertex));

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
    if (m_vertexData.raw == nullptr
        && m_vertexData.key
        && m_vertexData.size != 0)
    {
        bool needsSaveBlobData = false;

        BlobStorage& blobStorage = g_assetManager->GetAssetRegistry()->GetBlobStorage();

        if (!blobStorage.GetData(m_vertexData.key, m_vertexData.size, m_vertexData.raw))
        {
            ([&]()
                {
#if HYP_EDITOR
                    // check if failed; if so, try to import from raw data blob in project directory
                    Handle<AssetPackage> package = GetPackage();
                    Assert(package.IsValid());
                    Assert(package->IsSaved());

                    FileByteReader stream { package->GetSavedDirectory() / (String(*GetName()) + ".VB.raw.blob") };
                    if (!stream.Eof())
                    {
                        ByteBuffer buffer = stream.Read(stream.Max());

                        AllocateBlobData(m_vertexData, buffer.Data(), buffer.Size(), alignof(Vertex));

                        needsSaveBlobData = true;

                        MarkDirty();

                        return;
                    }
#endif

                    HYP_FAIL("Blob data missing! Data corruption detected.");
                })();
        }
        else
        {
            m_vertexData.readOnly = true;
        }

        if (!blobStorage.GetData(m_indexData.key, m_indexData.size, m_indexData.raw))
        {
            ([&]()
                {
#if HYP_EDITOR
                    // check if failed; if so, try to import from raw data blob in project directory
                    Handle<AssetPackage> package = GetPackage();
                    Assert(package.IsValid());
                    Assert(package->IsSaved());

                    FileByteReader stream { package->GetSavedDirectory() / (String(*GetName()) + ".IB.raw.blob") };
                    if (!stream.Eof())
                    {
                        ByteBuffer buffer = stream.Read(stream.Max());

                        AllocateBlobData(m_indexData, buffer.Data(), buffer.Size(), alignof(uint32));

                        needsSaveBlobData = true;

                        MarkDirty();

                        return;
                    }
#endif

                    HYP_FAIL("Blob data missing! Data corruption detected.");
                })();

#if HYP_EDITOR
            if (needsSaveBlobData)
            {
                Result saveBlobDataResult = SaveBlobData(blobStorage);
                if (saveBlobDataResult.HasError())
                {
                    HYP_LOG(Editor, Error, "Failed to save local blob data: {}", saveBlobDataResult.GetError().GetMessage());
                }
            }
#endif
        }
        else
        {
            m_indexData.readOnly = true;
        }
    }
}

void Mesh::UnpageBlobData()
{
    if (m_vertexData.readOnly)
    {
        m_vertexData.raw = nullptr;
        m_indexData.raw = nullptr;
    }
}

void Mesh::UploadGpuData()
{
    gpuUploadSemaphore.Reset();

    auto resGuard = GetReadScope();

    const VertexAttributeSet& vertexAttributes = m_meshDesc.meshAttributes.vertexAttributes;
    // @TODO fix for non-uint32 indices
    Assert(GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType) == 4);

    Array<float> vertices = BuildVertexBuffer(vertexAttributes);

    const Span<const ubyte> indexData = GetIndexData();

    Array<uint32> indices;
    indices.Resize(indexData.Size() / sizeof(uint32));
    Memory::Copy(indices.Data(), indexData.Data(), indexData.Size());

    AssertDebug(vertices.Size() == m_meshDesc.numVertices * m_meshDesc.meshAttributes.vertexAttributes.CalculateVertexSize());
    AssertDebug(indices.Size() == m_meshDesc.numIndices);

    // Ensure vertex buffer is not empty
    if (vertices.Empty())
    {
        vertices.Resize(1);
    }

    // Ensure indices exist and are a multiple of 3
    if (indices.Empty())
    {
        indices.Resize(3);
    }
    else if (indices.Size() % 3 != 0)
    {
        indices.Resize(indices.Size() + (3 - (indices.Size() % 3)));
    }

    const SizeType packedBufferSize = vertices.ByteSize();
    const SizeType packedIndicesSize = indices.ByteSize();

    GpuBufferRef vertexBuffer;
    GpuBufferRef indexBuffer;

    // don't assign m_vertexBuffer and m_indexBuffer when render thread could be reading it.
    if (IsReady() && !IsOnThread(g_renderThread))
    {
        vertexBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::MESH_VERTEX_BUFFER, packedBufferSize);
        indexBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::MESH_INDEX_BUFFER, packedIndicesSize);

#if HYP_DEBUG_MODE
        vertexBuffer->SetDebugName(NAME_FMT("{}_VBO", GetName()));
        indexBuffer->SetDebugName(NAME_FMT("{}_IBO", GetName()));
#endif

        DeferCreate(vertexBuffer);
        DeferCreate(indexBuffer);
    }
    else
    {
        if (!m_vertexBuffer.IsValid() || m_vertexBuffer->Size() != packedBufferSize)
        {
            m_vertexBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::MESH_VERTEX_BUFFER, packedBufferSize);

#if HYP_DEBUG_MODE
            m_vertexBuffer->SetDebugName(NAME_FMT("{}_VBO", GetName()));
#endif
        }

        DeferCreate(m_vertexBuffer);

        if (!m_indexBuffer.IsValid() || m_indexBuffer->Size() != packedIndicesSize)
        {
            m_indexBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::MESH_INDEX_BUFFER, packedIndicesSize);

#if HYP_DEBUG_MODE
            m_indexBuffer->SetDebugName(NAME_FMT("{}_IBO", GetName()));
#endif
        }

        DeferCreate(m_indexBuffer);

        vertexBuffer = m_vertexBuffer;
        indexBuffer = m_indexBuffer;
    }

    struct CopyMeshGpuData : public RenderCommand
    {
        WeakHandle<Mesh> weakMesh;

        Array<float> vertices;
        Array<uint32> indices;

        GpuBufferRef vertexBuffer;
        GpuBufferRef indexBuffer;

        CopyMeshGpuData(const WeakHandle<Mesh>& weakMesh, Array<float>&& vertices, Array<uint32>&& indices, GpuBufferRef&& vertexBuffer, GpuBufferRef&& indexBuffer)
            : weakMesh(weakMesh),
              vertices(std::move(vertices)),
              indices(std::move(indices)),
              vertexBuffer(std::move(vertexBuffer)),
              indexBuffer(std::move(indexBuffer))
        {
        }

        virtual ~CopyMeshGpuData() override = default;

        virtual RendererResult operator()() override
        {
            AssertDebug(vertexBuffer.IsValid() && indexBuffer.IsValid());

            Handle<Mesh> mesh = weakMesh.Lock();
            AssertDebug(mesh.IsValid());

            if (!mesh.IsValid())
            {
                return {};
            }

            constexpr SizeType StagingBufferAlignment = 16;

            const SizeType packedVerticesSize = vertices.ByteSize();
            const SizeType packedIndicesSize = indices.ByteSize();

            const SizeType bufferSizeCombined = ByteUtil::AlignAs(packedVerticesSize, StagingBufferAlignment) + packedIndicesSize;

            Frame* frame = g_renderInterface->GetCurrentFrame();

            GpuBuffer* stagingBuffer = g_renderInterface->stagingBufferPool->AcquireStagingBuffer(frame->GetFrameIndex(), 0, bufferSizeCombined);
            stagingBuffer->Copy(packedVerticesSize, vertices.Data());
            stagingBuffer->Copy(ByteUtil::AlignAs(packedVerticesSize, StagingBufferAlignment), packedIndicesSize, indices.Data());

            // use prerender queue to copy from staging buffers to gpu buffers
            RenderQueue& renderQueue = frame->preRenderQueue;

            renderQueue << CopyBuffer(stagingBuffer, vertexBuffer, packedVerticesSize);
            renderQueue << CopyBuffer(stagingBuffer, indexBuffer, ByteUtil::AlignAs(packedVerticesSize, StagingBufferAlignment), 0, packedIndicesSize);

            if (mesh->m_vertexBuffer != vertexBuffer)
            {
                mesh->m_vertexBuffer = std::move(vertexBuffer);
            }

            if (mesh->m_indexBuffer != indexBuffer)
            {
                mesh->m_indexBuffer = std::move(indexBuffer);
            }

            mesh->gpuUploadSemaphore.Signal();

            return {};
        }
    };

    PUSH_RENDER_COMMAND(CopyMeshGpuData, WeakHandleFromThis(), std::move(vertices), std::move(indices), std::move(vertexBuffer), std::move(indexBuffer));
}

void Mesh::ReleaseGpuData()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    m_vertexBuffer.Reset();
    m_indexBuffer.Reset();

    gpuUploadSemaphore.Reset();
}

Result Mesh::Rename(Name name)
{
    return AssetObject::Rename(name);
}

void Mesh::SetMeshData(
    const MeshDesc& meshDesc,
    Span<const Vertex> vertices,
    Span<const ubyte> indices)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    FreeBlobData(m_vertexData);
    FreeBlobData(m_indexData);

    AllocateBlobData(m_vertexData, vertices.Data(), sizeof(Vertex) * vertices.Size(), alignof(Vertex));
    AllocateBlobData(m_indexData, indices.Data(), indices.Size(), alignof(uint32));

    m_meshDesc = meshDesc;

    AssertDebug(m_meshDesc.numVertices == vertices.Size());
    AssertDebug(m_meshDesc.numIndices == indices.Size() / GpuElemTypeSize(m_meshDesc.meshAttributes.indexBufferElemType));

    // recalc aabb
    m_aabb = CalculateAABB();

    if (IsInitCalled())
    {
        // needs reupload!
        if (m_flags[MeshFlags::ViewIndependent] || gpuUploadSemaphore.IsSignaled())
        {
            UploadGpuData();
        }
    }

    MarkDirty();
}

void Mesh::SetFlags(EnumFlags<MeshFlags> flags)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    if (m_flags == flags)
    {
        return;
    }

    const bool wasViewIndependent = m_flags[MeshFlags::ViewIndependent];

    m_flags = flags;

    if (IsInitCalled() && m_flags[MeshFlags::ViewIndependent] != wasViewIndependent)
    {
        SetPersistentRequested(m_flags[MeshFlags::ViewIndependent], /* setFlag */ true, /* markDirty */ false);

        if (m_flags[MeshFlags::ViewIndependent])
        {
            UploadGpuData();
        }
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

    AssertDebug(GetVertexData().Size() > 0);
    AssertDebug(GetIndexData().Size() > 0);

    return BuildBVH(m_bvh, maxDepth);
}

bool Mesh::BuildBVH(BVHNode& bvhNode, int maxDepth) const
{
    const Span<const Vertex> vertexData = GetVertexData();
    const Span<const ubyte> indexData = GetIndexData();
    const uint32 numVertices = uint32(vertexData.Size());
    const uint32 numIndices = uint32(indexData.Size() / sizeof(uint32));

    const BoundingBox meshAabb = CalculateAABB();

    const SizeType numTriangles = numIndices / 3;

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
    HYP_SCOPE;

    const Span<const Vertex> vertices = GetVertexData();

    BoundingBox aabb = BoundingBox::Empty();

    for (uint32 vertexIndex = 0; vertexIndex < vertices.Size(); vertexIndex++)
    {
        const Vertex& vertex = vertices[vertexIndex];

        aabb = aabb.Union(vertex.GetPosition());
    }

    return aabb;
}

#define PACKED_SET_ATTR(rawValues, argSize)                                                         \
    do                                                                                              \
    {                                                                                               \
        Memory::Copy((void*)(floatBuffer + currentOffset), (rawValues), (argSize) * sizeof(float)); \
        currentOffset += (argSize);                                                                 \
    }                                                                                               \
    while (0)

Array<float> Mesh::BuildVertexBuffer(const VertexAttributeSet& vertexAttributes) const
{
    const Span<const Vertex> vertices = GetVertexData();
    const SizeType vertexSize = vertexAttributes.CalculateVertexSize();

    Array<float> packedBuffer;
    packedBuffer.Resize(vertexSize * vertices.Size());

    float* floatBuffer = packedBuffer.Data();
    SizeType currentOffset = 0;

    for (SizeType i = 0; i < vertices.Size(); i++)
    {
        const Vertex& vertex = vertices[i];
        /* Offset aligned to the current vertex */
        // currentOffset = i * vertexSize;

        /* Position and normals */
        if (vertexAttributes.Has(VertexAttribute::Position))
            PACKED_SET_ATTR(vertex.GetPosition().values, 3);
        if (vertexAttributes.Has(VertexAttribute::Normal))
            PACKED_SET_ATTR(vertex.GetNormal().values, 3);
        /* Texture coordinates */
        if (vertexAttributes.Has(VertexAttribute::TexCoord0))
            PACKED_SET_ATTR(vertex.GetTexCoord0().values, 2);
        if (vertexAttributes.Has(VertexAttribute::TexCoord1))
            PACKED_SET_ATTR(vertex.GetTexCoord1().values, 2);
        /* Tangents and Bitangents */
        if (vertexAttributes.Has(VertexAttribute::Tangent))
            PACKED_SET_ATTR(vertex.GetTangent().values, 3);
        if (vertexAttributes.Has(VertexAttribute::Bitangent))
            PACKED_SET_ATTR(vertex.GetBitangent().values, 3);

        if (vertexAttributes.Has(VertexAttribute::BoneWeights))
        {
            float weights[4] = {
                vertex.GetBoneWeight(0), vertex.GetBoneWeight(1),
                vertex.GetBoneWeight(2), vertex.GetBoneWeight(3)
            };
            PACKED_SET_ATTR(weights, HYP_ARRAY_SIZE(weights));
        }

        if (vertexAttributes.Has(VertexAttribute::BoneIndices))
        {
            float indices[4] = {
                (float)vertex.GetBoneIndex(0), (float)vertex.GetBoneIndex(1),
                (float)vertex.GetBoneIndex(2), (float)vertex.GetBoneIndex(3)
            };
            PACKED_SET_ATTR(indices, HYP_ARRAY_SIZE(indices));
        }
    }

    return packedBuffer;
}

#undef PACKED_SET_ATTR

Array<PackedVertex> Mesh::BuildPackedVertices() const
{
    HYP_SCOPE;

    const Span<const Vertex> vertices = GetVertexData();

    Array<PackedVertex> packedVertices;
    packedVertices.Resize(vertices.Size());

    for (SizeType i = 0; i < vertices.Size(); i++)
    {
        PackedVertex& packed = packedVertices[i];
        packed.position[0] = vertices[i].position.x;
        packed.position[1] = vertices[i].position.y;
        packed.position[2] = vertices[i].position.z;
        packed.normal[0] = vertices[i].normal.x;
        packed.normal[1] = vertices[i].normal.y;
        packed.normal[2] = vertices[i].normal.z;
        packed.uv[0] = vertices[i].texcoord0.x;
        packed.uv[1] = vertices[i].texcoord0.y;
    }

    return packedVertices;
}

Array<uint32> Mesh::BuildPackedIndices() const
{
    HYP_SCOPE;

    const Span<const ubyte> indices = GetIndexData();
    const uint32 numIndices = GetMeshDesc().numIndices;

    Assert(numIndices % 3 == 0);

    // @TODO Fix for non-uint32 index size

    Array<uint32> packedIndices;
    packedIndices.Resize(numIndices);

    Memory::Copy(packedIndices.Data(), indices.Data(), numIndices * sizeof(uint32));

    // Ensure indices are a multiple of 3
    if (packedIndices.Size() % 3 != 0)
    {
        packedIndices.Resize(packedIndices.Size() + (3 - (packedIndices.Size() % 3)));
    }

    // Ensure indices are not empty
    if (packedIndices.Empty())
    {
        packedIndices.Resize(3);
        packedIndices[0] = 0;
        packedIndices[1] = 1;
        packedIndices[2] = 2;
    }

#if HYP_DEBUG_MODE
    for (SizeType i = 0; i < packedIndices.Size(); i++)
    {
        uint32 idx = packedIndices[i];
        AssertDebug(idx < GetMeshDesc().numVertices);
    }
#endif

    return packedIndices;
}

void Mesh::InvertNormals()
{
    HYP_SCOPE;

    Span<Vertex> vertices = GetVertexData();

    for (SizeType i = 0; i < vertices.Size(); i++)
    {
        vertices[i].SetNormal(vertices[i].GetNormal() * -1.0f);
    }
}

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
    HYP_SCOPE;

    Span<Vertex> vertexData = GetVertexData();
    Span<ubyte> indexData = GetIndexData();
    const uint32 numVertices = uint32(vertexData.Size());
    const uint32 numIndices = uint32(indexData.Size() / sizeof(uint32));

    // @TODO fix for non-uint32 indices

    uint32* uIndexData = reinterpret_cast<uint32*>(&indexData[0]);

    SparsePagedArray<Array<Vec3f, InlineAllocator<3>>, 1 << 6> normals;

    // compute per-face normals (facet normals)
    for (SizeType i = 0; i < numIndices; i += 3)
    {
        const uint32 i0 = uIndexData[i];
        const uint32 i1 = uIndexData[i + 1];
        const uint32 i2 = uIndexData[i + 2];

        const Vec3f& p0 = vertexData[i0].GetPosition();
        const Vec3f& p1 = vertexData[i1].GetPosition();
        const Vec3f& p2 = vertexData[i2].GetPosition();

        const Vec3f u = p2 - p0;
        const Vec3f v = p1 - p0;
        const Vec3f n = v.Cross(u).Normalize();

        ADD_NORMAL(normals, i0, n);
        ADD_NORMAL(normals, i1, n);
        ADD_NORMAL(normals, i2, n);
    }

    for (SizeType i = 0; i < numVertices; i++)
    {
        AssertDebug(normals.HasIndex(uint32(i)));

        if (weighted)
        {
            vertexData[i].SetNormal(normals.Get(uint32(i)).Sum());
        }
        else
        {
            vertexData[i].SetNormal(normals.Get(uint32(i)).Sum().Normalize());
        }
    }

    if (!weighted)
    {
        return;
    }

    normals.Clear();

    // weighted (smooth) normals

    for (SizeType i = 0; i < numIndices; i += 3)
    {
        const uint32 i0 = uIndexData[i];
        const uint32 i1 = uIndexData[i + 1];
        const uint32 i2 = uIndexData[i + 2];

        const Vec3f& p0 = vertexData[i0].GetPosition();
        const Vec3f& p1 = vertexData[i1].GetPosition();
        const Vec3f& p2 = vertexData[i2].GetPosition();

        const Vec3f& n0 = vertexData[i0].GetNormal();
        const Vec3f& n1 = vertexData[i1].GetNormal();
        const Vec3f& n2 = vertexData[i2].GetNormal();

        // Vector3 n = FixedArray { n0, n1, n2 }.Avg();

        FixedArray<Vec3f, 3> weightedNormals { n0, n1, n2 };

        // nested loop through faces to get weighted neighbours
        // any code that uses this really should bake the normals in
        // especially for any production code. this is an expensive process
        for (SizeType j = 0; j < numIndices; j += 3)
        {
            if (j == i)
            {
                continue;
            }

            const uint32 j0 = uIndexData[j];
            const uint32 j1 = uIndexData[j + 1];
            const uint32 j2 = uIndexData[j + 2];

            const FixedArray<Vec3f, 3> facePositions {
                vertexData[j0].GetPosition(),
                vertexData[j1].GetPosition(),
                vertexData[j2].GetPosition()
            };

            const FixedArray<Vec3f, 3> faceNormals {
                vertexData[j0].GetNormal(),
                vertexData[j1].GetNormal(),
                vertexData[j2].GetNormal()
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

    for (SizeType i = 0; i < numVertices; i++)
    {
        AssertDebug(normals.HasIndex(i));

        vertexData[i].SetNormal(normals.Get(i).Sum().Normalized());
    }

    normals.Clear();
}

#undef ADD_NORMAL

#define ADD_TANGENTS(ary, idx, tangents) \
    do                                   \
    {                                    \
        auto* idx_it = ary.TryGet(idx);  \
        if (!idx_it)                     \
        {                                \
            idx_it = &*ary.Emplace(idx); \
        }                                \
        idx_it->PushBack(tangents);      \
    }                                    \
    while (0)

void Mesh::CalculateTangents()
{
    HYP_SCOPE;

    Span<Vertex> vertexData = GetVertexData();
    Span<ubyte> indexData = GetIndexData();
    const uint32 numVertices = uint32(vertexData.Size());
    const uint32 numIndices = uint32(indexData.Size() / sizeof(uint32));

    // @TODO fix for non uint32 indices

    uint32* uIndexData = reinterpret_cast<uint32*>(&indexData[0]);

    struct TangentBitangentPair
    {
        Vec3f tangent;
        Vec3f bitangent;
    };

    static const Array<TangentBitangentPair, InlineAllocator<1>> placeholderTangentBitangents {};

    SparsePagedArray<Array<TangentBitangentPair, InlineAllocator<1>>, 1 << 6> data;

    for (SizeType i = 0; i < numIndices;)
    {
        const SizeType count = MathUtil::Min(3, numIndices - i);

        Vertex v[3];
        Vec2f uv[3];

        for (uint32 j = 0; j < count; j++)
        {
            v[j] = vertexData[uIndexData[i + j]];
            uv[j] = v[j].GetTexCoord0();
        }

        uint32 i0 = uIndexData[i];
        uint32 i1 = uIndexData[i + 1];
        uint32 i2 = uIndexData[i + 2];

        const Vec3f edge1 = v[1].GetPosition() - v[0].GetPosition();
        const Vec3f edge2 = v[2].GetPosition() - v[0].GetPosition();
        const Vec2f edge1uv = uv[1] - uv[0];
        const Vec2f edge2uv = uv[2] - uv[0];

        const float cp = edge1uv.x * edge2uv.y - edge1uv.y * edge2uv.x;

        if (cp != 0.0f)
        {
            const float mul = 1.0f / cp;

            const TangentBitangentPair tangentBitangent {
                .tangent = ((edge1 * edge2uv.y - edge2 * edge1uv.y) * mul).Normalize(),
                .bitangent = ((edge1 * edge2uv.x - edge2 * edge1uv.x) * mul).Normalize()
            };

            ADD_TANGENTS(data, i0, tangentBitangent);
            ADD_TANGENTS(data, i1, tangentBitangent);
            ADD_TANGENTS(data, i2, tangentBitangent);
        }

        i += count;
    }

    for (SizeType i = 0; i < numVertices; i++)
    {
        const Array<TangentBitangentPair, InlineAllocator<1>>* tangentBitangents = data.TryGet(i);

        if (!tangentBitangents)
        {
            tangentBitangents = &placeholderTangentBitangents;
        }

        // find average
        Vec3f averageTangent, averageBitangent;

        for (const auto& item : *tangentBitangents)
        {
            averageTangent += item.tangent * (1.0f / tangentBitangents->Size());
            averageBitangent += item.bitangent * (1.0f / tangentBitangents->Size());
        }

        averageTangent.Normalize();
        averageBitangent.Normalize();

        vertexData[i].SetTangent(averageTangent);
        vertexData[i].SetBitangent(averageBitangent);
    }
}

#undef ADD_TANGENTS

#pragma endregion Mesh

} // namespace Hyperion
