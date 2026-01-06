/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Frame.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <asset/Assets.hpp>
#include <asset/AssetRegistry.hpp>

#include <scene/BVH.hpp>

#include <engine/EngineDriver.hpp>

#include <cstring>

#include <Mesh.generated.inl>

namespace Hyperion {

static const Name s_nameMeshDefault = NAME("<unnamed mesh>");

const VertexAttributeSet VertexAttributeSet::StaticMeshVertexAttributes =
    VertexAttribute::Position | VertexAttribute::Normal
        | VertexAttribute::TexCoord0 | VertexAttribute::TexCoord1
        | VertexAttribute::Tangent | VertexAttribute::Bitangent;

const VertexAttributeSet VertexAttributeSet::SkeletalMeshVertexAttributes =
    StaticMeshVertexAttributes | VertexAttribute::BoneWeights | VertexAttribute::BoneIndices;

#pragma region VertexAttribute

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

        result += VertexAttribute::Attrs[i]->name;
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
      m_flags(MF_NONE)
{
}

Mesh::Mesh(const Handle<MeshAsset>& asset, Topology topology, const VertexAttributeSet& vertexAttributes)
    : AssetObject(),
      m_meshAsset(asset),
      m_aabb(BoundingBox::Empty()),
      m_flags(MF_NONE)
{
    if (asset)
    {
        ResourceGuard resGuard(*asset->GetResource());

        m_aabb = asset->GetMeshData()->CalculateAABB();
    }
}

Mesh::Mesh(const Handle<MeshAsset>& asset, Topology topology)
    : Mesh(
          asset,
          topology,
          VertexAttributeSet::StaticMeshVertexAttributes | VertexAttributeSet::SkeletalMeshVertexAttributes)
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

    m_meshAsset = TAssetReference<MeshAsset>(CreateObject<MeshAsset>(s_nameMeshDefault, meshDesc, meshData));
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

void Mesh::SetMeshAsset(const AssetReference& assetReference)
{
    HYP_SCOPE;

    if (assetReference == m_meshAsset)
    {
        return;
    }

    m_meshAsset = TAssetReference<MeshAsset>(assetReference);

    if (m_meshAsset.IsValid() && IsInitCalled())
    {
        const Handle<MeshAsset>& asset = m_meshAsset.Resolve();
        if (!asset)
        {
            HYP_LOG(Mesh, Error, "Failed to resolve mesh asset from asset reference with path '{}'", assetReference.GetAssetPath());
            return;
        }

        if (!asset->IsRegistered())
        {
            if (Result renameResult = asset->Rename(NAME_FMT("{}_MeshAsset", m_name)); renameResult.HasError())
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

    gpuUploadFence.Reset();

    ResourceGuard resGuard(*asset->GetResource());
    AssertDebug(asset->IsLoaded());

    if (!asset->IsLoaded())
    {
        HYP_LOG(Mesh, Error, "Mesh asset for {} is not loaded in memory, cannot create GPU buffers", GetName());

        return;
    }

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
    if (IsReady() && !IsOnThread(g_renderThread))
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
                SafeDelete(std::move(vertexBuffer));
                SafeDelete(std::move(indexBuffer));

                return {};
            }

            const SizeType packedBufferSize = vertices.ByteSize();
            const SizeType packedIndicesSize = indices.ByteSize();

            GpuBufferRef stagingBufferVertices = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedBufferSize);
            CheckResult(stagingBufferVertices->Create());
            stagingBufferVertices->Copy(packedBufferSize, vertices.Data());

            GpuBufferRef stagingBufferIndices = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedIndicesSize);
            CheckResult(stagingBufferIndices->Create());
            stagingBufferIndices->Copy(packedIndicesSize, indices.Data());

            Frame* frame = g_renderBackend->GetCurrentFrame();

            // use prerender queue to copy from staging buffers to gpu buffers
            RenderQueue& renderQueue = frame->preRenderQueue;

            renderQueue << CopyBuffer(stagingBufferVertices, vertexBuffer, packedBufferSize);
            renderQueue << CopyBuffer(stagingBufferIndices, indexBuffer, packedIndicesSize);

            SafeDelete(std::move(stagingBufferVertices));
            SafeDelete(std::move(stagingBufferIndices));

            if (mesh->m_vertexBuffer != vertexBuffer)
            {
                SafeDelete(std::move(mesh->m_vertexBuffer));
                mesh->m_vertexBuffer = std::move(vertexBuffer);
            }

            if (mesh->m_indexBuffer != indexBuffer)
            {
                SafeDelete(std::move(mesh->m_indexBuffer));
                mesh->m_indexBuffer = std::move(indexBuffer);
            }

            mesh->gpuUploadFence.Signal();

            return {};
        }
    };

    PUSH_RENDER_COMMAND(CopyMeshGpuData, WeakHandleFromThis(), std::move(vertices), std::move(indices), std::move(vertexBuffer), std::move(indexBuffer));
}

void Mesh::ReleaseGpuData()
{
    HYP_SCOPE;
    AssertOnThread(g_renderThread);

    SafeDelete(std::move(m_vertexBuffer));
    SafeDelete(std::move(m_indexBuffer));

    gpuUploadFence.Reset();
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
            if (Result renameResult = asset->Rename(NAME_FMT("{}_MeshAsset", m_name)); renameResult.HasError())
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

    Handle<MeshAsset> asset = CreateObject<MeshAsset>(NAME_FMT("{}_MeshAsset", m_name), meshDesc, meshData);
    m_meshAsset = TAssetReference<MeshAsset>(asset);

    if (IsInitCalled())
    {
        g_assetManager->GetAssetRegistry()->RegisterAsset("$Memory/Media/Meshes", asset);

        // needs reupload!
        if (m_flags[MF_VIEW_INDEPENDENT] || gpuUploadFence.IsSignaled())
        {
            UploadGpuData();
        }
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

    ResourceGuard resGuard(*asset->GetResource());
    AssertDebug(asset->GetMeshData() != nullptr);

    if (!asset->GetMeshData())
    {
        return false;
    }

    const MeshData& meshData = *asset->GetMeshData();

    return meshData.BuildBVH(m_bvh, maxDepth);
}

#pragma endregion Mesh

} // namespace Hyperion
