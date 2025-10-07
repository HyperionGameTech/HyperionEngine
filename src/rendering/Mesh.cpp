/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/Mesh.hpp>

#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderCommand.hpp>

#include <rendering/util/SafeDeleter.hpp>

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

static const Name s_nameMeshDefault = NAME("<unnamed mesh>");

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
      m_flags(MF_NONE)
{
}

Mesh::Mesh(const Handle<MeshAsset>& asset, Topology topology, const VertexAttributeSet& vertexAttributes)
    : AssetObject(),
      m_assetReference(asset),
      m_aabb(BoundingBox::Empty()),
      m_flags(MF_NONE)
{
    if (asset)
    {
        ResourceHandle resourceHandle(*asset->GetResource());

        m_aabb = asset->GetMeshData()->CalculateAABB();
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
    : AssetObject(),
      m_aabb(BoundingBox::Empty()),
      m_flags(MF_NONE)
{
    const MeshDesc meshDesc {
        .meshAttributes = { .vertexAttributes = vertexAttributes, .topology = topology },
        .numVertices = uint32(vertexData.Size()),
        .numIndices = uint32(indexData.Size() / sizeof(uint32))
    };

    const MeshData meshData {
        .vertexData = vertexData,
        .indexData = indexData
    };

    m_aabb = meshData.CalculateAABB();

    m_assetReference = TAssetReference<MeshAsset>(CreateObject<MeshAsset>(s_nameMeshDefault, meshDesc, meshData));
}

Mesh::~Mesh()
{
    SafeDelete(std::move(m_vertexBuffer));
    SafeDelete(std::move(m_indexBuffer));
}

void Mesh::Init()
{
    HYP_SCOPE;

    if (const Handle<MeshAsset>& asset = GetAsset())
    {
        if (!asset->IsRegistered())
        {
            if (Result renameResult = asset->Rename(m_name); renameResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to rename mesh asset!", renameResult.GetError().GetMessage());
            }

            // all assets must be registered before uploading to gpu - if our asset isn't part of a package,
            // register it with transient Memory package
            g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", asset);
        }

        if (m_flags[MF_VIEW_INDEPENDENT])
        {
            UploadGpuData();
        }
    }
    else
    {
        if (m_flags[MF_VIEW_INDEPENDENT])
        {
            HYP_LOG(Mesh, Warning, "Mesh {} (name: {}) has no asset, cannot upload GPU data!", Id(), GetName());
        }
    }

    AssetObject::Init();

    SetReady(true);
}

void Mesh::SetAssetReference(const AssetReference& assetReference)
{
    HYP_SCOPE;

    if (assetReference == m_assetReference)
    {
        return;
    }

    m_assetReference = TAssetReference<MeshAsset>(assetReference);

    if (m_assetReference.IsValid() && IsInitCalled())
    {
        const Handle<MeshAsset>& asset = m_assetReference.Resolve();
        if (!asset)
        {
            HYP_LOG(Mesh, Error, "Failed to resolve mesh asset from asset reference with path '{}'", assetReference.GetAssetPath());
            return;
        }

        if (!asset->IsRegistered())
        {
            if (Result renameResult = asset->Rename(m_name); renameResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to rename mesh asset!", renameResult.GetError().GetMessage());
            }

            g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", asset);
        }

        if (m_flags[MF_VIEW_INDEPENDENT])
        {
            UploadGpuData();
        }
    }
}

void Mesh::UploadGpuData()
{
    const Handle<MeshAsset>& asset = GetAsset();

    if (!asset)
    {
        HYP_LOG(Mesh, Error, "Mesh asset is not set, cannot create GPU buffers");

        return;
    }

    ResourceHandle resourceHandle(*asset->GetResource());
    AssertDebug(asset->IsLoaded());

    const VertexAttributeSet& vertexAttributes = asset->GetMeshDesc().meshAttributes.vertexAttributes;

    Array<float> vertices = asset->GetMeshData()->BuildVertexBuffer(vertexAttributes);

    Array<uint32> indices;
    indices.Resize(asset->GetMeshData()->indexData.Size() / sizeof(uint32));
    Memory::MemCpy(indices.Data(), asset->GetMeshData()->indexData.Data(), asset->GetMeshData()->indexData.Size());

    AssertDebug(vertices.Size() == asset->GetMeshDesc().numVertices * asset->GetMeshDesc().meshAttributes.vertexAttributes.CalculateVertexSize());
    AssertDebug(indices.Size() == asset->GetMeshDesc().numIndices);

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
    if (IsReady() && !Threads::IsOnThread(g_renderThread))
    {
        HYP_BREAKPOINT;

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

            if (mesh->m_vertexBuffer != vertexBuffer || mesh->m_indexBuffer != indexBuffer)
            {
                SafeDelete(std::move(mesh->m_vertexBuffer));
                SafeDelete(std::move(mesh->m_indexBuffer));

                mesh->m_vertexBuffer = std::move(vertexBuffer);
                mesh->m_indexBuffer = std::move(indexBuffer);
            }

            mesh->gpuUploadFence.Signal();

            return {};
        }
    };

    PUSH_RENDER_COMMAND(CopyMeshGpuData, WeakHandleFromThis(), std::move(vertices), std::move(indices), std::move(vertexBuffer), std::move(indexBuffer));
}

Result Mesh::Rename(Name name)
{
    if (m_name == name)
    {
        return {};
    }

    if (Result result = AssetObject::Rename(name); result.HasError())
    {
        return result;
    }

    const Handle<MeshAsset>& asset = GetAsset();

    if (asset)
    {
        if (!asset->IsRegistered())
        {
            if (Result renameResult = asset->Rename(m_name); renameResult.HasError())
            {
                HYP_LOG(Assets, Error, "Failed to rename mesh asset!", renameResult.GetError().GetMessage());
            }

            if (IsInitCalled())
            {
                g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", asset);
            }
        }
    }

    return {};
}

void Mesh::SetMeshData(const MeshDesc& meshDesc, const MeshData& meshData)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    m_aabb = meshData.CalculateAABB();

    Handle<MeshAsset> asset = CreateObject<MeshAsset>(GetName(), meshDesc, meshData);
    m_assetReference = TAssetReference<MeshAsset>(asset);

    if (IsInitCalled())
    {
        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", asset);

        // CreateGpuBuffers();
    }
}

void Mesh::SetFlags(EnumFlags<MeshFlags> flags)
{
    HYP_SCOPE;
    HYP_MT_CHECK_RW(m_dataRaceDetector);

    if (m_flags == flags)
    {
        return;
    }

    const bool wasViewIndependent = m_flags[MF_VIEW_INDEPENDENT];
    m_flags = flags;

    if (m_flags[MF_VIEW_INDEPENDENT] && !wasViewIndependent && IsInitCalled())
    {
        UploadGpuData();
    }
}

uint32 Mesh::NumIndices() const
{
    HYP_SCOPE;
    HYP_MT_CHECK_READ(m_dataRaceDetector, "Streamed mesh data");

    ///! FIXME: Use meshProxy instead of reading from asset desc, as it may change on another thread
    const Handle<MeshAsset>& asset = GetAsset();
    return asset ? asset->GetMeshDesc().numIndices : 0;
}

bool Mesh::BuildBVH(int maxDepth)
{
    const Handle<MeshAsset>& asset = GetAsset();

    if (!asset)
    {
        return false;
    }

    ResourceHandle resourceHandle(*asset->GetResource());

    const MeshData& meshData = *asset->GetMeshData();

    return meshData.BuildBVH(m_bvh, maxDepth);
}

#pragma endregion Mesh

} // namespace hyperion
