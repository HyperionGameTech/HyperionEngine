/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/Mesh.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderCommand.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/BVH.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <cstring>

namespace hyperion {

static const Name g_nameMeshDefault = NAME("<unnamed mesh>");

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
    : HypObjectBase(),
      m_aabb(BoundingBox::Empty())
{
}

Mesh::Mesh(const Handle<MeshAsset>& asset, Topology topology, const VertexAttributeSet& vertexAttributes)
    : HypObjectBase(),
      m_asset(asset),
      m_aabb(BoundingBox::Empty())
{
    if (m_asset.IsValid())
    {
        ResourceHandle resourceHandle(*m_asset->GetResource());

        m_aabb = m_asset->GetMeshData()->CalculateAABB();
    }
}

Mesh::Mesh(const Handle<MeshAsset>& asset, Topology topology)
    : Mesh(asset, topology, staticMeshVertexAttributes | skeletonVertexAttributes)
{
}

Mesh::Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology)
    : Mesh(
          vertexData,
          indexData,
          topology,
          staticMeshVertexAttributes | skeletonVertexAttributes)
{
}

Mesh::Mesh(const Array<Vertex>& vertexData, const ByteBuffer& indexData, Topology topology, const VertexAttributeSet& vertexAttributes)
    : HypObjectBase(),
      m_aabb(BoundingBox::Empty())
{
    const MeshDesc meshDesc {
        .meshAttributes = { .vertexAttributes = vertexAttributes, .topology = topology },
        .numVertices = uint32(vertexData.Size()),
        .numIndices = uint32(indexData.Size() / sizeof(uint32))
    };

    const MeshData meshData {
        .desc = meshDesc,
        .vertexData = vertexData,
        .indexData = indexData
    };

    m_aabb = m_asset->GetMeshData()->CalculateAABB();

    m_asset = CreateObject<MeshAsset>(g_nameMeshDefault, meshData);
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_asset(std::move(other.m_asset)),
      m_aabb(other.m_aabb)
{
    other.m_aabb = BoundingBox::Empty();
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (&other == this)
    {
        return *this;
    }

    m_asset = std::move(other.m_asset);
    m_aabb = other.m_aabb;

    other.m_aabb = BoundingBox::Empty();

    return *this;
}

Mesh::~Mesh()
{
    SafeDelete(std::move(m_vertexBuffer));
    SafeDelete(std::move(m_indexBuffer));
    SafeDelete(std::move(m_asset));
}

void Mesh::Init()
{
    if (m_asset.IsValid() && !m_asset->IsRegistered())
    {
        if (!m_asset->GetName().IsValid() && m_name.IsValid() && m_name != g_nameMeshDefault)
        {
            m_asset->Rename(m_name);
        }

        // all assets must be registered before uploading to gpu - if our asset isn't part of a package,
        // register it with transient Memory package
        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", m_asset);
    }

    CreateGpuBuffers();

    SetReady(true);
}

void Mesh::CreateGpuBuffers()
{
    if (!m_asset)
    {
        HYP_LOG(Mesh, Error, "Mesh asset is not set, cannot create GPU buffers");

        return;
    }

    ResourceHandle resourceHandle(*m_asset->GetResource());
    Assert(m_asset->IsLoaded());

    Array<float> vertices = m_asset->GetMeshData()->BuildVertexBuffer();

    Array<uint32> indices;
    indices.Resize(m_asset->GetMeshData()->indexData.Size() / sizeof(uint32));
    Memory::MemCpy(indices.Data(), m_asset->GetMeshData()->indexData.Data(), m_asset->GetMeshData()->indexData.Size());

    AssertDebug(m_asset->GetMeshData()->desc.meshAttributes.vertexAttributes != 0, "No vertex attributes set on mesh");
    Assert(vertices.Size() == m_asset->GetMeshDesc().numVertices * m_asset->GetMeshDesc().meshAttributes.vertexAttributes.CalculateVertexSize());
    Assert(indices.Size() == m_asset->GetMeshDesc().numIndices);

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

    if (IsInitCalled() && !Threads::IsOnThread(g_renderThread))
    {
        vertexBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::MESH_VERTEX_BUFFER, packedBufferSize);
        indexBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::MESH_INDEX_BUFFER, packedIndicesSize);

#ifdef HYP_DEBUG_MODE
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
            SafeDelete(std::move(m_vertexBuffer));

            m_vertexBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::MESH_VERTEX_BUFFER, packedBufferSize);

#ifdef HYP_DEBUG_MODE
            m_vertexBuffer->SetDebugName(NAME_FMT("{}_VBO", GetName()));
#endif
        }

        DeferCreate(m_vertexBuffer);

        if (!m_indexBuffer.IsValid() || m_indexBuffer->Size() != packedIndicesSize)
        {
            SafeDelete(std::move(m_indexBuffer));

            m_indexBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::MESH_INDEX_BUFFER, packedIndicesSize);

#ifdef HYP_DEBUG_MODE
            m_indexBuffer->SetDebugName(NAME_FMT("{}_IBO", GetName()));
#endif
        }

        DeferCreate(m_indexBuffer);

        vertexBuffer = m_vertexBuffer;
        indexBuffer = m_indexBuffer;
    }

    struct RENDER_COMMAND(CopyMeshGpuData)
        : public RenderCommand
    {
        WeakHandle<Mesh> weakMesh;

        Array<float> vertices;
        Array<uint32> indices;

        GpuBufferRef vertexBuffer;
        GpuBufferRef indexBuffer;

        RENDER_COMMAND(CopyMeshGpuData)(const WeakHandle<Mesh>& weakMesh, Array<float>&& vertices, Array<uint32>&& indices, GpuBufferRef&& vertexBuffer, GpuBufferRef&& indexBuffer)
            : weakMesh(weakMesh),
              vertices(std::move(vertices)),
              indices(std::move(indices)),
              vertexBuffer(std::move(vertexBuffer)),
              indexBuffer(std::move(indexBuffer))
        {
        }

        virtual ~RENDER_COMMAND(CopyMeshGpuData)() override = default;

        virtual RendererResult operator()() override
        {
            Handle<Mesh> mesh = weakMesh.Lock();

            if (!mesh.IsValid())
            {
                SafeDelete(std::move(vertexBuffer));
                SafeDelete(std::move(indexBuffer));

                return {};
            }

            const SizeType packedBufferSize = vertices.ByteSize();
            const SizeType packedIndicesSize = indices.ByteSize();

            GpuBufferRef stagingBufferVertices = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedBufferSize);
            HYP_GFX_ASSERT(stagingBufferVertices->Create());
            stagingBufferVertices->Copy(packedBufferSize, vertices.Data());

            GpuBufferRef stagingBufferIndices = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedIndicesSize);
            HYP_GFX_ASSERT(stagingBufferIndices->Create());
            stagingBufferIndices->Copy(packedIndicesSize, indices.Data());

            FrameBase* frame = g_renderBackend->GetCurrentFrame();
            RenderQueue& renderQueue = frame->renderQueue;

            renderQueue << CopyBuffer(stagingBufferVertices, vertexBuffer, packedBufferSize);
            renderQueue << CopyBuffer(stagingBufferIndices, indexBuffer, packedIndicesSize);

            SafeDelete(std::move(stagingBufferVertices));
            SafeDelete(std::move(stagingBufferIndices));

            SafeDelete(std::move(mesh->m_vertexBuffer));
            SafeDelete(std::move(mesh->m_indexBuffer));

            mesh->m_vertexBuffer = std::move(vertexBuffer);
            mesh->m_indexBuffer = std::move(indexBuffer);

            return {};
        }
    };

    PUSH_RENDER_COMMAND(CopyMeshGpuData, WeakHandleFromThis(), std::move(vertices), std::move(indices), std::move(vertexBuffer), std::move(indexBuffer));
}

void Mesh::SetName(Name name)
{
    if (m_name == name)
    {
        return;
    }

    m_name = name;

    if (m_asset.IsValid() && !m_asset->IsRegistered() && IsInitCalled())
    {
        if (!m_asset->GetName().IsValid() && m_name.IsValid() && m_name != g_nameMeshDefault)
        {
            m_asset->Rename(m_name);
        }

        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", m_asset);
    }
}

void Mesh::SetMeshData(const MeshData& meshData)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    m_aabb = meshData.CalculateAABB();

    Handle<MeshAsset> newAsset = CreateObject<MeshAsset>(GetName(), meshData);
    m_asset = std::move(newAsset);

    if (IsInitCalled())
    {
        if (!m_asset->IsRegistered())
        {
            g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", m_asset);
        }

        CreateGpuBuffers();
    }
}

uint32 Mesh::NumIndices() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_dataRaceDetector, "Streamed mesh data");

    ///! FIXME: Use meshProxy instead of reading from asset desc, as it may change on another thread
    return m_asset ? m_asset->GetMeshDesc().numIndices : 0;
}

bool Mesh::BuildBVH(int maxDepth)
{
    if (!m_asset)
    {
        return false;
    }

    ResourceHandle resourceHandle(*m_asset->GetResource());

    const MeshData& meshData = *m_asset->GetMeshData();

    return meshData.BuildBVH(m_bvh, maxDepth);
}

#pragma endregion Mesh

} // namespace hyperion