/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <rendering/rt/MeshBlasBuilder.hpp>
#include <rendering/rt/RenderAccelerationStructure.hpp>
#include <rendering/RenderCommand.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderHelpers.hpp>

#include <asset/MeshAsset.hpp>
#include <asset/AssetRegistry.hpp>

#include <core/Handle.hpp>

#include <core/containers/Array.hpp>

#include <core/memory/resource/Resource.hpp>

#include <EngineGlobals.hpp>

namespace hyperion {

struct RENDER_COMMAND(BuildMeshBlas) : public RenderCommand
{
    BLASRef blas;
    Array<PackedVertex> packedVertices;
    Array<uint32> packedIndices;
    Handle<Material> material;

    GpuBufferRef packedVerticesBuffer;
    GpuBufferRef packedIndicesBuffer;
    GpuBufferRef verticesStagingBuffer;
    GpuBufferRef indicesStagingBuffer;

    RENDER_COMMAND(BuildMeshBlas)(BLASRef& blas, Array<PackedVertex>&& packedVertices, Array<uint32>&& packedIndices, const Handle<Material>& material)
        : packedVertices(std::move(packedVertices)),
          packedIndices(std::move(packedIndices)),
          material(material)
    {
        const SizeType packedVerticesSize = ByteUtil::AlignAs(this->packedVertices.Size() * sizeof(PackedVertex), 16);
        const SizeType packedIndicesSize = ByteUtil::AlignAs(this->packedIndices.Size() * sizeof(uint32), 16);

        packedVerticesBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::RT_MESH_VERTEX_BUFFER, packedVerticesSize);
        packedIndicesBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::RT_MESH_INDEX_BUFFER, packedIndicesSize);

        blas = g_renderBackend->MakeBLAS(
            packedVerticesBuffer,
            packedIndicesBuffer,
            uint32(this->packedVertices.Size()),
            uint32(this->packedIndices.Size()),
            material,
            Matrix4::identity);

        this->blas = blas;
    }

    virtual ~RENDER_COMMAND(BuildMeshBlas)() override
    {
        SafeRelease(std::move(packedVerticesBuffer));
        SafeRelease(std::move(packedIndicesBuffer));
        SafeRelease(std::move(verticesStagingBuffer));
        SafeRelease(std::move(indicesStagingBuffer));
    }

    virtual RendererResult operator()() override
    {
        const SizeType packedVerticesSize = ByteUtil::AlignAs(packedVertices.Size() * sizeof(PackedVertex), 16);
        const SizeType packedIndicesSize = ByteUtil::AlignAs(packedIndices.Size() * sizeof(uint32), 16);

        HYP_GFX_CHECK(packedVerticesBuffer->Create());
        HYP_GFX_CHECK(packedIndicesBuffer->Create());

        verticesStagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedVerticesSize);
        verticesStagingBuffer->SetDebugName(NAME_FMT("StagingBuffer_V_BLAS_{}", blas->GetDebugName()));
        HYP_GFX_CHECK(verticesStagingBuffer->Create());
        verticesStagingBuffer->Memset(packedVerticesSize, 0); // zero out
        verticesStagingBuffer->Copy(packedVertices.Size() * sizeof(PackedVertex), packedVertices.Data());

        indicesStagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedIndicesSize);
        verticesStagingBuffer->SetDebugName(NAME_FMT("StagingBuffer_I_BLAS_{}", blas->GetDebugName()));
        HYP_GFX_CHECK(indicesStagingBuffer->Create());
        indicesStagingBuffer->Memset(packedIndicesSize, 0); // zero out
        indicesStagingBuffer->Copy(packedIndices.Size() * sizeof(uint32), packedIndices.Data());

        UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderBackend->GetSingleTimeCommands();

        singleTimeCommands->Push([this, packedVerticesSize, packedIndicesSize](RenderQueue& renderQueue)
            {
                renderQueue << CopyBuffer(verticesStagingBuffer, packedVerticesBuffer, packedVerticesSize);
                renderQueue << CopyBuffer(indicesStagingBuffer, packedIndicesBuffer, packedIndicesSize);
            });

        HYP_GFX_CHECK(singleTimeCommands->Execute());

        /*FrameBase* frame = g_renderBackend->GetCurrentFrame();
        RenderQueue& renderQueue = frame->renderQueue;

        renderQueue << CopyBuffer(verticesStagingBuffer, packedVerticesBuffer, packedVerticesSize);
        renderQueue << CopyBuffer(indicesStagingBuffer, packedIndicesBuffer, packedIndicesSize);*/

        return {};
    }
};

BLASRef MeshBlasBuilder::Build(Mesh* mesh, Material* material)
{
    if (!mesh)
    {
        return nullptr;
    }

    Handle<MeshAsset> asset = mesh->GetAsset();

    if (!asset)
    {
        return nullptr;
    }

    ResourceHandle resourceHandle(*asset->GetResource());

    Array<PackedVertex> packedVertices = asset->GetMeshData()->BuildPackedVertices();
    Array<uint32> packedIndices = asset->GetMeshData()->BuildPackedIndices();

    if (packedVertices.Empty() || packedIndices.Empty())
    {
        return nullptr;
    }

    // some assertions to prevent gpu faults down the line
    for (SizeType i = 0; i < packedIndices.Size(); i++)
    {
        Assert(packedIndices[i] < packedVertices.Size());
    }

    BLASRef blas;
    PUSH_RENDER_COMMAND(BuildMeshBlas, blas, std::move(packedVertices), std::move(packedIndices), material ? material->HandleFromThis() : nullptr);
    blas->SetDebugName(NAME_FMT("MeshBlas_{}", mesh->GetName()));

    return blas;
}

} // namespace hyperion
