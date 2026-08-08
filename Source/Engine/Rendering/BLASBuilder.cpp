/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <RenderingPch.hpp>

#include <Rendering/BLASBuilder.hpp>
#include <Rendering/AccelerationStructure.hpp>

#include <Rendering/Frame.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Asset/AssetRegistry.hpp>

#include <Core/Reflection/Handle.hpp>

#include <Core/Memory/Allocator/ThreadAllocator.hpp>

#include <Core/Resource/Resource.hpp>

namespace Hyperion {

struct BuildBLASCmd
{
    BottomLevelASRef blas;
    Array<float> packedVertices;
    Array<ubyte> packedIndices;
    Handle<Material> material;

    GpuBufferRef packedVerticesBuffer;
    GpuBufferRef packedIndicesBuffer;

    BuildBLASCmd(BottomLevelASRef& blas, Array<float>&& packedVertices, Array<ubyte>&& packedIndices, Mesh* mesh, Material* material)
        : packedVertices(std::move(packedVertices)),
          packedIndices(std::move(packedIndices)),
          material(MakeStrongRef(material))
    {
        Assert(mesh && material);

        const size_t packedVerticesSize = this->packedVertices.ByteSize();
        const size_t packedIndicesSize = this->packedIndices.ByteSize();

        packedVerticesBuffer = RI.MakeGpuBuffer(GpuBufferType::RTMeshVertexBuffer, packedVerticesSize);
        packedIndicesBuffer = RI.MakeGpuBuffer(GpuBufferType::RTMeshIndexBuffer, packedIndicesSize);

        blas = RI.MakeBottomLevelAS(
            packedVerticesBuffer,
            packedIndicesBuffer,
            uint32(packedVerticesSize),
            uint32(packedIndicesSize),
            this->material,
            Mat4f::identity);

#ifdef HYP_RHI_DEBUG_NAMES
        packedVerticesBuffer->SetDebugName(NAME_FMT("BLAS_VB_{}", mesh->GetName()));
        packedIndicesBuffer->SetDebugName(NAME_FMT("BLAS_IB_{}", mesh->GetName()));

        blas->SetDebugName(NAME_FMT("MeshBlas_{}", mesh->GetName()));
#endif

        this->blas = blas;
    }

    ~BuildBLASCmd()
    {
        EnqueueDeletion(std::move(packedVerticesBuffer));
        EnqueueDeletion(std::move(packedIndicesBuffer));
    }

    RendererResult operator()()
    {
        const size_t packedVerticesSize = this->packedVertices.ByteSize();
        const size_t packedIndicesSize = this->packedIndices.ByteSize();

        CheckResultOrReturn(packedVerticesBuffer->Create());
        CheckResultOrReturn(packedIndicesBuffer->Create());

        GpuBuffer* verticesStagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(packedVerticesSize);
        verticesStagingBuffer->Copy(packedVerticesSize, packedVertices.Data());
        verticesStagingBuffer->Flush(0, packedVerticesSize);

        GpuBuffer* indicesStagingBuffer = RI.stagingBufferPool->AcquireStagingBuffer(packedIndicesSize);
        indicesStagingBuffer->Copy(packedIndicesSize, packedIndices.Data());
        indicesStagingBuffer->Flush(0, packedIndicesSize);

        CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

        cr << InsertBarrier(verticesStagingBuffer, RS_COPY_SRC);
        cr << InsertBarrier(indicesStagingBuffer, RS_COPY_SRC);

        cr << InsertBarrier(packedVerticesBuffer.Get(), RS_COPY_DST);
        cr << InsertBarrier(packedIndicesBuffer.Get(), RS_COPY_DST);

        cr << CopyBuffer(verticesStagingBuffer, packedVerticesBuffer, packedVerticesSize);
        cr << CopyBuffer(indicesStagingBuffer, packedIndicesBuffer, packedIndicesSize);

        cr << InsertBarrier(packedVerticesBuffer.Get(), RS_SHADER_RESOURCE);
        cr << InsertBarrier(packedIndicesBuffer.Get(), RS_SHADER_RESOURCE);

        cr.Done();

        return {};
    }
};

BottomLevelASRef BLASBuilder::Build(Mesh* mesh, Material* material)
{
    if (!mesh)
    {
        return BottomLevelASRef::Null();
    }

    auto resGuard = mesh->GetReadScope();

    // Currently RT shaders are all expecting VT_Simple layout
    Array<float> packedVertices;

    // @TODO Build BLAS for lower lod index, if possible.
    mesh->BuildVertexBuffer(StaticVertexInputLayout<VT_Simple>, 0, packedVertices);

    Array<ubyte> packedIndices = mesh->GetIndexData(0);

    AssertDebug(packedVertices.Size() > 0 && packedIndices.Size() > 0);

    if (packedVertices.Size() == 0 || packedIndices.Size() == 0)
    {
        return BottomLevelASRef::Null();
    }

    // some assertions to prevent gpu faults down the line
    const uint32* packedIndicesU32 = reinterpret_cast<const uint32*>(packedIndices.Data());
    for (size_t i = 0; i < mesh->GetMeshDesc().lods[0].numIndices; i++)
    {
        Assert(packedIndicesU32[i] < mesh->GetMeshDesc().lods[0].numVertices);
    }

    BottomLevelASRef blas;
    BuildBLASCmd command { blas, std::move(packedVertices), std::move(packedIndices), mesh, material };

    command();

    return blas;
}

} // namespace Hyperion
