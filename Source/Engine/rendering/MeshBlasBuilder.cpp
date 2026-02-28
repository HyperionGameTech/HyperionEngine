/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <RenderingPch.hpp>

#include <rendering/MeshBlasBuilder.hpp>
#include <rendering/AccelerationStructure.hpp>

#include <rendering/RenderCommand.hpp>
#include <rendering/Frame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <asset/AssetRegistry.hpp>

#include <Core/reflection/Handle.hpp>

#include <Core/memory/resource/Resource.hpp>

namespace Hyperion {

HYP_DISABLE_OPTIMIZATION;

struct BuildMeshBlas : public RenderCommand
{
    GpuBlasRef blas;
    Array<PackedVertex> packedVertices;
    Array<uint32> packedIndices;
    Handle<Material> material;

    GpuBufferRef packedVerticesBuffer;
    GpuBufferRef packedIndicesBuffer;
    GpuBufferRef verticesStagingBuffer;
    GpuBufferRef indicesStagingBuffer;

    BuildMeshBlas(GpuBlasRef& blas, Array<PackedVertex>&& packedVertices, Array<uint32>&& packedIndices, const Handle<Material>& material)
        : packedVertices(std::move(packedVertices)),
          packedIndices(std::move(packedIndices)),
          material(material)
    {
        const SizeType packedVerticesSize = this->packedVertices.Size() * sizeof(PackedVertex);
        const SizeType packedIndicesSize = this->packedIndices.Size() * sizeof(uint32);

        packedVerticesBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::RT_MESH_VERTEX_BUFFER, packedVerticesSize);
        packedIndicesBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::RT_MESH_INDEX_BUFFER, packedIndicesSize);

        blas = g_renderInterface->MakeGpuBlas(
            packedVerticesBuffer,
            packedIndicesBuffer,
            uint32(this->packedVertices.Size()),
            uint32(this->packedIndices.Size()),
            material,
            Mat4f::identity);

#if HYP_DEBUG_MODE
        packedVerticesBuffer->SetDebugName(NAME_FMT("PackedVertexBuffer_GpuBlas_{}", blas->GetDebugName()));
        packedIndicesBuffer->SetDebugName(NAME_FMT("PackedIndexBuffer_GpuBlas_{}", blas->GetDebugName()));
#endif

        this->blas = blas;
    }

    virtual ~BuildMeshBlas() override
    {
        EnqueueDeletion(std::move(packedVerticesBuffer));
        EnqueueDeletion(std::move(packedIndicesBuffer));
        EnqueueDeletion(std::move(verticesStagingBuffer));
        EnqueueDeletion(std::move(indicesStagingBuffer));
    }

    virtual RendererResult operator()() override
    {
        const SizeType packedVerticesSize = packedVertices.Size() * sizeof(PackedVertex);
        const SizeType packedIndicesSize = packedIndices.Size() * sizeof(uint32);

        CheckResultOrReturn(packedVerticesBuffer->Create());
        CheckResultOrReturn(packedIndicesBuffer->Create());

        verticesStagingBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedVerticesSize);
#if HYP_DEBUG_MODE
        verticesStagingBuffer->SetDebugName(NAME_FMT("StagingBuffer_VB_GpuBlas_{}", blas->GetDebugName()));
#endif
        CheckResultOrReturn(verticesStagingBuffer->Create());
        verticesStagingBuffer->Memset(packedVerticesSize, 0); // zero out
        verticesStagingBuffer->Copy(packedVertices.Size() * sizeof(PackedVertex), packedVertices.Data());

        indicesStagingBuffer = g_renderInterface->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, packedIndicesSize);
#if HYP_DEBUG_MODE
        indicesStagingBuffer->SetDebugName(NAME_FMT("StagingBuffer_IB_GpuBlas_{}", blas->GetDebugName()));
#endif
        CheckResultOrReturn(indicesStagingBuffer->Create());
        indicesStagingBuffer->Memset(packedIndicesSize, 0); // zero out
        indicesStagingBuffer->Copy(packedIndices.Size() * sizeof(uint32), packedIndices.Data());

        UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderInterface->GetSingleTimeCommands();

        singleTimeCommands->Push([this, packedVerticesSize, packedIndicesSize](RenderQueue& renderQueue)
            {
                renderQueue << CopyBuffer(verticesStagingBuffer, packedVerticesBuffer, packedVerticesSize);
                renderQueue << CopyBuffer(indicesStagingBuffer, packedIndicesBuffer, packedIndicesSize);
            });

        CheckResultOrReturn(singleTimeCommands->Execute());

        /*Frame* frame = g_renderInterface->GetCurrentFrame();
        RenderQueue& renderQueue = frame->renderQueue;

        renderQueue << CopyBuffer(verticesStagingBuffer, packedVerticesBuffer, packedVerticesSize);
        renderQueue << CopyBuffer(indicesStagingBuffer, packedIndicesBuffer, packedIndicesSize);*/

        return {};
    }
};

GpuBlasRef MeshBlasBuilder::Build(Mesh* mesh, Material* material)
{
    if (!mesh)
    {
        return nullptr;
    }

    auto resGuard = mesh->GetReadScope();

    Array<PackedVertex> packedVertices = mesh->BuildPackedVertices();
    Array<uint32> packedIndices = mesh->BuildPackedIndices();

    if (packedVertices.Empty() || packedIndices.Empty())
    {
        return nullptr;
    }

    // some assertions to prevent gpu faults down the line
    for (SizeType i = 0; i < packedIndices.Size(); i++)
    {
        Assert(packedIndices[i] < packedVertices.Size());
    }

    GpuBlasRef blas;
    PUSH_RENDER_COMMAND(BuildMeshBlas, blas, std::move(packedVertices), std::move(packedIndices), MakeStrongRef(material));

#if HYP_DEBUG_MODE
    blas->SetDebugName(NAME_FMT("MeshBlas_{}", mesh->GetName()));
#endif

    return blas;
}

} // namespace Hyperion
