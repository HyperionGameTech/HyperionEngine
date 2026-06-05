/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <RenderingPch.hpp>

#include <Rendering/MeshBlasBuilder.hpp>
#include <Rendering/AccelerationStructure.hpp>

#include <Rendering/RenderCommand.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/MaterialInstance.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Resource/Resource.hpp>

namespace Hyperion {

struct BuildMeshBlas : public RenderCommand
{
    GpuBlasRef blas;
    Array<float> packedVertices;
    Array<ubyte> packedIndices;
    Handle<MaterialInstance> material;

    GpuBufferRef packedVerticesBuffer;
    GpuBufferRef packedIndicesBuffer;
    GpuBufferRef verticesStagingBuffer;
    GpuBufferRef indicesStagingBuffer;

    BuildMeshBlas(GpuBlasRef& blas, Array<float>&& packedVertices, Array<ubyte>&& packedIndices, const Handle<MaterialInstance>& material)
        : packedVertices(std::move(packedVertices)),
          packedIndices(std::move(packedIndices)),
          material(material)
    {
        const size_t packedVerticesSize = this->packedVertices.ByteSize();
        const size_t packedIndicesSize = this->packedIndices.ByteSize();

        packedVerticesBuffer = RI.MakeGpuBuffer(GpuBufferType::RTMeshVertexBuffer, packedVerticesSize);
        packedIndicesBuffer = RI.MakeGpuBuffer(GpuBufferType::RTMeshIndexBuffer, packedIndicesSize);

        blas = RI.MakeGpuBlas(
            packedVerticesBuffer,
            packedIndicesBuffer,
            uint32(packedVerticesSize),
            uint32(packedIndicesSize),
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
        const size_t packedVerticesSize = this->packedVertices.ByteSize();
        const size_t packedIndicesSize = this->packedIndices.ByteSize();

        CheckResultOrReturn(packedVerticesBuffer->Create());
        CheckResultOrReturn(packedIndicesBuffer->Create());

        verticesStagingBuffer = RI.MakeGpuBuffer(GpuBufferType::StagingBuffer, packedVerticesSize);
#if HYP_DEBUG_MODE
        verticesStagingBuffer->SetDebugName(NAME_FMT("StagingBuffer_VB_GpuBlas_{}", blas->GetDebugName()));
#endif
        CheckResultOrReturn(verticesStagingBuffer->Create());
        verticesStagingBuffer->Memset(packedVerticesSize, 0); // zero out
        verticesStagingBuffer->Copy(packedVerticesSize, packedVertices.Data());
        verticesStagingBuffer->Flush(0, packedVerticesSize);

        indicesStagingBuffer = RI.MakeGpuBuffer(GpuBufferType::StagingBuffer, packedIndicesSize);
#if HYP_DEBUG_MODE
        indicesStagingBuffer->SetDebugName(NAME_FMT("StagingBuffer_IB_GpuBlas_{}", blas->GetDebugName()));
#endif
        CheckResultOrReturn(indicesStagingBuffer->Create());
        indicesStagingBuffer->Memset(packedIndicesSize, 0); // zero out
        indicesStagingBuffer->Copy(packedIndicesSize, packedIndices.Data());
        indicesStagingBuffer->Flush(0, packedIndicesSize);

        CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

        cr << InsertBarrier(verticesStagingBuffer.Get(), RS_COPY_SRC);
        cr << InsertBarrier(indicesStagingBuffer.Get(), RS_COPY_SRC);

        cr << InsertBarrier(packedVerticesBuffer.Get(), RS_COPY_DST);
        cr << InsertBarrier(packedIndicesBuffer.Get(), RS_COPY_DST);

        cr << CopyBuffer(verticesStagingBuffer, packedVerticesBuffer, packedVerticesSize);
        cr << CopyBuffer(indicesStagingBuffer, packedIndicesBuffer, packedIndicesSize);

        cr << InsertBarrier(packedVerticesBuffer.Get(), RS_SHADER_RESOURCE);
        cr << InsertBarrier(packedIndicesBuffer.Get(), RS_SHADER_RESOURCE);

        cr.Submit();

        return {};
    }
};

GpuBlasRef MeshBlasBuilder::Build(Mesh* mesh, MaterialInstance* material)
{
    if (!mesh)
    {
        return nullptr;
    }

    auto resGuard = mesh->GetReadScope();

    // Currently RT shaders are all expecting VT_Simple layout
    Array<float> packedVertices = mesh->BuildVertexBuffer(StaticVertexInputLayout<VT_Simple>);
    Array<ubyte> packedIndices = mesh->GetIndexData();

    AssertDebug(packedVertices.Size() > 0 && packedIndices.Size() > 0);

    if (packedVertices.Size() == 0 || packedIndices.Size() == 0)
    {
        return nullptr;
    }

    // some assertions to prevent gpu faults down the line
    const uint32* packedIndicesU32 = reinterpret_cast<const uint32*>(packedIndices.Data());
    for (size_t i = 0; i <  mesh->GetMeshDesc().numIndices; i++)
    {
        Assert(packedIndicesU32[i] < mesh->GetMeshDesc().numVertices);
    }

    GpuBlasRef blas;
    PUSH_RENDER_COMMAND(BuildMeshBlas, blas, std::move(packedVertices), std::move(packedIndices), MakeStrongRef(material));

#if HYP_DEBUG_MODE
    blas->SetDebugName(NAME_FMT("MeshBlas_{}", mesh->GetName()));
#endif

    return blas;
}

} // namespace Hyperion
