/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */
#include <rendering/GaussianSplatting.hpp>

#include <rendering/Buffers.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderableAttributes.hpp>
#include <rendering/GBuffer.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/GraphicsPipelineCache.hpp>

#include <rendering/RenderQueue.hpp>

#include <rendering/RenderFrame.hpp>
#include <rendering/RenderComputePipeline.hpp>
#include <rendering/RenderGraphicsPipeline.hpp>

#include <rendering/Mesh.hpp>

#include <scene/View.hpp>
#include <scene/camera/OrthoCamera.hpp>

#include <core/object/HypClassUtils.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <core/math/MathUtil.hpp>
#include <core/math/Color.hpp>

#include <core/filesystem/FsUtil.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <util/NoiseFactory.hpp>
#include <util/MeshBuilder.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#ifdef HYP_VULKAN
#include <rendering/vulkan/VulkanStructs.hpp>
#endif

// #define HYP_GAUSSIAN_SPLATTING_CPU_SORT

namespace hyperion {

enum BitonicSortStage : uint32
{
    STAGE_LOCAL_BMS,
    STAGE_LOCAL_DISPERSE,
    STAGE_BIG_FLIP,
    STAGE_BIG_DISPERSE
};

struct alignas(8) GaussianSplatIndex
{
    uint32 index;
    float32 distance;
};

struct RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)
    : RenderCommand
{
    GpuBufferRef splatBuffer;
    GpuBufferRef splatIndicesBuffer;
    GpuBufferRef sceneBuffer;
    GpuBufferRef indirectBuffer;
    RC<GaussianSplattingModelData> model;

    RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)(
        GpuBufferRef splatBuffer,
        GpuBufferRef splatIndicesBuffer,
        GpuBufferRef sceneBuffer,
        GpuBufferRef indirectBuffer,
        RC<GaussianSplattingModelData> model)
        : splatBuffer(std::move(splatBuffer)),
          splatIndicesBuffer(std::move(splatIndicesBuffer)),
          sceneBuffer(std::move(sceneBuffer)),
          indirectBuffer(std::move(indirectBuffer)),
          model(std::move(model))
    {
    }

    virtual ~RENDER_COMMAND(CreateGaussianSplattingInstanceBuffers)() override = default;

    virtual RendererResult operator()() override
    {
        static_assert(sizeof(GaussianSplattingInstanceShaderData) == sizeof(model->points[0]));

        const SizeType numPoints = model->points.Size();

        HYP_GFX_CHECK(splatBuffer->Create());

        splatBuffer->Copy(splatBuffer->Size(), model->points.Data());

        HYP_GFX_CHECK(splatIndicesBuffer->Create());

        // Set default indices
        GaussianSplatIndex* indicesBufferData = new GaussianSplatIndex[splatIndicesBuffer->Size() / sizeof(GaussianSplatIndex)];

        for (SizeType index = 0; index < numPoints; index++)
        {
            if (index >= UINT32_MAX)
            {
                break;
            }

            indicesBufferData[index] = GaussianSplatIndex {
                uint32(index),
                -1000.0f
            };
        }

        for (SizeType index = numPoints; index < splatIndicesBuffer->Size() / sizeof(GaussianSplatIndex); index++)
        {
            indicesBufferData[index] = GaussianSplatIndex {
                uint32(-1),
                -1000.0f
            };
        }

        splatIndicesBuffer->Copy(splatIndicesBuffer->Size(), indicesBufferData);

        // Discard the data we used for initially setting up the buffers
        delete[] indicesBufferData;

        const GaussianSplattingSceneShaderData gaussianSplattingSceneShaderData = {
            model->transform.GetMatrix()
        };

        HYP_GFX_CHECK(sceneBuffer->Create());
        sceneBuffer->Copy(sizeof(GaussianSplattingSceneShaderData), &gaussianSplattingSceneShaderData);

        HYP_GFX_CHECK(indirectBuffer->Create());

        HYPERION_RETURN_OK;
    }
};

struct RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)
    : RenderCommand
{
    GpuBufferRef stagingBuffer;
    Handle<Mesh> quadMesh;

    RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)(
        GpuBufferRef stagingBuffer,
        Handle<Mesh> quadMesh)
        : stagingBuffer(std::move(stagingBuffer)),
          quadMesh(std::move(quadMesh))
    {
    }

    virtual ~RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers)() override = default;

    virtual RendererResult operator()() override
    {
        ByteBuffer byteBuffer;
        g_renderBackend->PopulateIndirectDrawCommandsBuffer(quadMesh->GetVertexBuffer(), quadMesh->GetIndexBuffer(), 0, byteBuffer);

        if (!stagingBuffer->IsCreated())
        {
            HYP_GFX_CHECK(stagingBuffer->Create());
        }

        HYP_GFX_CHECK(stagingBuffer->EnsureCapacity(byteBuffer.Size()));

        stagingBuffer->Copy(byteBuffer.Size(), byteBuffer.Data());

        HYPERION_RETURN_OK;
    }
};

GaussianSplattingInstance::GaussianSplattingInstance()
{
}

GaussianSplattingInstance::GaussianSplattingInstance(RC<GaussianSplattingModelData> model)
    : m_model(std::move(model))
{
}

GaussianSplattingInstance::~GaussianSplattingInstance()
{
    if (IsInitCalled())
    {
        SafeRelease(std::move(m_splatBuffer));
        SafeRelease(std::move(m_splatIndicesBuffer));
        SafeRelease(std::move(m_sceneBuffer));
        SafeRelease(std::move(m_indirectBuffer));
        SafeRelease(std::move(m_sortStageDescriptorTables));
    }
}

void GaussianSplattingInstance::Init()
{
    CreateBuffers();
    CreateShader();
    CreateGraphicsPipeline();
    CreateComputePipelines();

#ifdef HYP_GAUSSIAN_SPLATTING_CPU_SORT
    // Temporary
    m_cpuSortedIndices.Resize(m_model->points.Size());
    m_cpuDistances.Resize(m_model->points.Size());

    for (SizeType index = 0; index < m_model->points.Size(); index++)
    {
        m_cpuSortedIndices[index] = index;
        m_cpuDistances[index] = -1000.0f;
    }
#endif

    SetReady(true);
}

void GaussianSplattingInstance::Record(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);

    Assert(IsReady());

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    const uint32 numPoints = static_cast<uint32>(m_model->points.Size());

    Assert(m_splatBuffer->Size() == sizeof(GaussianSplattingInstanceShaderData) * numPoints);

    { // Update splat distances from camera before we sort

        struct
        {
            uint32 numPoints;
        } updateSplatsDistancesPushConstants;

        updateSplatsDistancesPushConstants.numPoints = numPoints;

        m_updateSplatDistances->SetPushConstants(
            &updateSplatsDistancesPushConstants,
            sizeof(updateSplatsDistancesPushConstants));

        frame->renderQueue << BindComputePipeline(m_updateSplatDistances);

        frame->renderQueue << BindDescriptorTable(
            m_updateSplatDistances->GetDescriptorTable(),
            m_updateSplatDistances,
            { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
            frame->GetFrameIndex());

        frame->renderQueue << DispatchCompute(
            m_updateSplatDistances,
            Vec3u { uint32((numPoints + 255) / 256), 1, 1 });

        frame->renderQueue << InsertBarrier(
            m_splatIndicesBuffer,
            RS_UNORDERED_ACCESS);
    }

#ifdef HYP_GAUSSIAN_SPLATTING_CPU_SORT
    { // Temporary CPU sorting -- inefficient but useful for testing
        for (SizeType index = 0; index < m_model->points.Size(); index++)
        {
            m_cpuDistances[index] = ((activeCamera ? activeCamera->GetBufferData().view : Matrix4()) * m_model->points[index].position).z;
        }

        std::sort(m_cpuSortedIndices.Begin(), m_cpuSortedIndices.End(), [&distances = m_cpuDistances](uint32 a, uint32 b)
            {
                return distances[a] < distances[b];
            });

        Assert(m_splatIndicesBuffer->Size() >= m_cpuSortedIndices.Size() * sizeof(m_cpuSortedIndices[0]),
            "Expected buffer size to be at least %llu -- got %llu.",
            m_cpuSortedIndices.Size() * sizeof(m_cpuSortedIndices[0]),
            m_splatIndicesBuffer->Size());

        // Copy the cpu sorted indices over
        m_splatIndicesBuffer->Copy(MathUtil::Min(m_splatIndicesBuffer->Size(), m_cpuSortedIndices.Size() * sizeof(m_cpuSortedIndices[0])), m_cpuSortedIndices.Data());
    }
#else
    { // Sort splats
        constexpr uint32 blockSize = 512;
        constexpr uint32 transposeBlockSize = 16;

        struct
        {
            uint32 numPoints;
            uint32 stage;
            uint32 h;
        } sortSplatsPushConstants;

        Memory::MemSet(&sortSplatsPushConstants, 0x0, sizeof(sortSplatsPushConstants));

        const uint32 numSortableElements = uint32(MathUtil::NextPowerOf2(numPoints)); // Values are stored in components of uvec4

        const uint32 width = blockSize;
        const uint32 height = numSortableElements / blockSize;

        sortSplatsPushConstants.numPoints = numPoints;

        frame->renderQueue << InsertBarrier(
            m_splatIndicesBuffer,
            RS_UNORDERED_ACCESS);

        static constexpr uint32 maxWorkgroupSize = 512;
        uint32 workgroupSizeX = 1;

        if (numSortableElements < maxWorkgroupSize * 2)
        {
            workgroupSizeX = numSortableElements / 2;
        }
        else
        {
            workgroupSizeX = maxWorkgroupSize;
        }

        Assert(workgroupSizeX == maxWorkgroupSize, "Not implemented for workgroup size < max_workgroup_size");

        uint32 h = workgroupSizeX * 2;
        const uint32 workgroupCount = numSortableElements / (workgroupSizeX * 2);

        Assert(h < numSortableElements);
        Assert(h % 2 == 0);

        auto doPass = [this, frame, &renderSetup, pc = sortSplatsPushConstants, workgroupCount](BitonicSortStage stage, uint32 h) mutable
        {
            pc.stage = uint32(stage);
            pc.h = h;

            m_sortSplats->SetPushConstants(&pc, sizeof(pc));

            frame->renderQueue << BindComputePipeline(m_sortSplats);

            frame->renderQueue << BindDescriptorTable(
                m_sortSplats->GetDescriptorTable(),
                m_sortSplats,
                { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
                frame->GetFrameIndex());

            frame->renderQueue << DispatchCompute(
                m_sortSplats,
                Vec3u { workgroupCount, 1, 1 });

            frame->renderQueue << InsertBarrier(
                m_splatIndicesBuffer,
                RS_UNORDERED_ACCESS);
        };

        doPass(STAGE_LOCAL_BMS, h);

        h <<= 1;

        for (; h <= numSortableElements; h <<= 1)
        {
            doPass(STAGE_BIG_FLIP, h);

            for (uint32 hh = h >> 1; hh > 1; hh >>= 1)
            {
                if (hh <= workgroupSizeX * 2)
                {
                    doPass(STAGE_LOCAL_DISPERSE, hh);

                    break;
                }
                else
                {
                    doPass(STAGE_BIG_DISPERSE, hh);
                }
            }
        }
    }
#endif
    { // Update splats

        struct
        {
            uint32 numPoints;
        } updateSplatsPushConstants;

        updateSplatsPushConstants.numPoints = numPoints;

        m_updateSplats->SetPushConstants(
            &updateSplatsPushConstants,
            sizeof(updateSplatsPushConstants));

        frame->renderQueue << BindComputePipeline(m_updateSplats);

        frame->renderQueue << BindDescriptorTable(
            m_updateSplats->GetDescriptorTable(),
            m_updateSplats,
            { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
            frame->GetFrameIndex());

        frame->renderQueue << DispatchCompute(
            m_updateSplats,
            Vec3u { uint32((numPoints + 255) / 256), 1, 1 });

        frame->renderQueue << InsertBarrier(
            m_indirectBuffer,
            RS_INDIRECT_ARG);
    }
}

void GaussianSplattingInstance::CreateBuffers()
{
    const SizeType numPoints = m_model->points.Size();

    m_splatBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, numPoints * sizeof(GaussianSplattingInstanceShaderData));
    m_splatIndicesBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, MathUtil::NextPowerOf2(numPoints) * sizeof(GaussianSplatIndex));
    m_sceneBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(GaussianSplattingSceneShaderData));
    m_indirectBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::INDIRECT_ARGS_BUFFER, 0);

    PUSH_RENDER_COMMAND(
        CreateGaussianSplattingInstanceBuffers,
        m_splatBuffer,
        m_splatIndicesBuffer,
        m_sceneBuffer,
        m_indirectBuffer,
        m_model);
}

void GaussianSplattingInstance::CreateShader()
{
    m_shader = g_shaderManager->GetOrCreate(NAME("GaussianSplatting"));
}

void GaussianSplattingInstance::CreateGraphicsPipeline()
{
    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&m_shader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = descriptorTable->GetDescriptorSet("GaussianSplattingDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("SplatIndicesBuffer", m_splatIndicesBuffer);
        descriptorSet->SetElement("SplatInstancesBuffer", m_splatBuffer);
        descriptorSet->SetElement("GaussianSplattingSceneShaderData", m_sceneBuffer);
    }

    DeferCreate(descriptorTable);

#if 0
    // FIXME
    m_graphicsPipeline = g_renderGlobalState->graphicsPipelineCache->GetOrCreate(
        m_shader,
        descriptorTable,
        { &m_framebuffer, 1 },
        RenderableAttributeSet(
            MeshAttributes {
                .vertexAttributes = staticMeshVertexAttributes // VertexAttribute::MESH_INPUT_ATTRIBUTE_POSITION
            },
            MaterialAttributes {
                .bucket = RB_TRANSLUCENT,
                .blendFunction = BlendFunction::Additive(),
                .cullFaces = FCM_NONE,
                .flags = MAF_NONE }));
#endif
}

void GaussianSplattingInstance::CreateComputePipelines()
{
    ShaderProperties baseProperties;

    // UpdateSplats

    ShaderRef updateSplatsShader = g_shaderManager->GetOrCreate(
        NAME("GaussianSplatting_UpdateSplats"),
        baseProperties);

    DescriptorTableRef updateSplatsDescriptorTable = g_renderBackend->MakeDescriptorTable(&updateSplatsShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = updateSplatsDescriptorTable->GetDescriptorSet("UpdateSplatsDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("SplatIndicesBuffer", m_splatIndicesBuffer);
        descriptorSet->SetElement("SplatInstancesBuffer", m_splatBuffer);
        descriptorSet->SetElement("IndirectDrawCommandsBuffer", m_indirectBuffer);
        descriptorSet->SetElement("GaussianSplattingSceneShaderData", m_sceneBuffer);
    }

    DeferCreate(updateSplatsDescriptorTable);

    m_updateSplats = g_renderBackend->MakeComputePipeline(
        updateSplatsShader,
        updateSplatsDescriptorTable);

    DeferCreate(m_updateSplats);

    // UpdateDistances

    ShaderRef updateSplatDistancesShader = g_shaderManager->GetOrCreate(
        NAME("GaussianSplatting_UpdateDistances"),
        baseProperties);

    DescriptorTableRef updateSplatDistancesDescriptorTable = g_renderBackend->MakeDescriptorTable(&updateSplatDistancesShader->GetCompiledShader()->GetDescriptorTableDeclaration());

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        const DescriptorSetRef& descriptorSet = updateSplatDistancesDescriptorTable->GetDescriptorSet("UpdateDistancesDescriptorSet", frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement("SplatIndicesBuffer", m_splatIndicesBuffer);
        descriptorSet->SetElement("SplatInstancesBuffer", m_splatBuffer);
        descriptorSet->SetElement("GaussianSplattingSceneShaderData", m_sceneBuffer);
    }

    DeferCreate(updateSplatDistancesDescriptorTable);

    m_updateSplatDistances = g_renderBackend->MakeComputePipeline(
        updateSplatDistancesShader,
        updateSplatDistancesDescriptorTable);

    DeferCreate(m_updateSplatDistances);

    // SortSplats

    ShaderRef sortSplatsShader = g_shaderManager->GetOrCreate(
        NAME("GaussianSplatting_SortSplats"),
        baseProperties);

    m_sortStageDescriptorTables.Resize(SortStage::SORT_STAGE_MAX);

    for (uint32 sortStageIndex = 0; sortStageIndex < SortStage::SORT_STAGE_MAX; sortStageIndex++)
    {
        DescriptorTableRef sortSplatsDescriptorTable = g_renderBackend->MakeDescriptorTable(&sortSplatsShader->GetCompiledShader()->GetDescriptorTableDeclaration());

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            const DescriptorSetRef& descriptorSet = sortSplatsDescriptorTable->GetDescriptorSet("SortSplatsDescriptorSet", frameIndex);
            Assert(descriptorSet != nullptr);

            descriptorSet->SetElement("SplatIndicesBuffer", m_splatIndicesBuffer);
            descriptorSet->SetElement("SplatInstancesBuffer", m_splatBuffer);
            descriptorSet->SetElement("GaussianSplattingSceneShaderData", m_sceneBuffer);
        }

        DeferCreate(sortSplatsDescriptorTable);

        m_sortStageDescriptorTables[sortStageIndex] = std::move(sortSplatsDescriptorTable);
    }

    m_sortSplats = g_renderBackend->MakeComputePipeline(
        sortSplatsShader,
        m_sortStageDescriptorTables[0]);

    DeferCreate(m_sortSplats);
}

GaussianSplatting::GaussianSplatting()
    : HypObjectBase()
{
}

GaussianSplatting::~GaussianSplatting()
{
    if (IsInitCalled())
    {
        m_quadMesh.Reset();
        m_gaussianSplattingInstance.Reset();

        SafeRelease(std::move(m_stagingBuffer));
    }
}

void GaussianSplatting::Init()
{
    AddDelegateHandler(g_engineDriver->GetDelegates().OnShutdown.Bind([this]()
        {
            m_quadMesh.Reset();
            m_gaussianSplattingInstance.Reset();

            SafeRelease(std::move(m_stagingBuffer));
        }));

    static const Array<Vertex> vertices = {
        Vertex { { -1.0f, -1.0f, 0.0f }, { 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        Vertex { { 1.0f, -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },
        Vertex { { 1.0f, 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } },
        Vertex { { -1.0f, 1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f, -1.0f } }
    };

    static const Array<uint32> indices = {
        0, 3, 1,
        2, 3, 1
    };

    MeshData meshData;
    meshData.desc.numIndices = uint32(indices.Size());
    meshData.desc.numVertices = uint32(vertices.Size());
    meshData.vertexData = vertices;
    meshData.indexData.SetSize(indices.Size() * sizeof(uint32));
    meshData.indexData.Write(indices.Size() * sizeof(uint32), 0, indices.Data());

    m_quadMesh = CreateObject<Mesh>();
    m_quadMesh->SetMeshData(meshData);

    InitObject(m_quadMesh);

    InitObject(m_gaussianSplattingInstance);

    CreateBuffers();

    SetReady(true);
}

void GaussianSplatting::SetGaussianSplattingInstance(Handle<GaussianSplattingInstance> gaussianSplattingInstance)
{
    m_gaussianSplattingInstance = std::move(gaussianSplattingInstance);

    if (IsInitCalled())
    {
        InitObject(m_gaussianSplattingInstance);
    }
}

void GaussianSplatting::CreateBuffers()
{
    m_stagingBuffer = g_renderBackend->MakeGpuBuffer(GpuBufferType::STAGING_BUFFER, sizeof(IndirectDrawCommand));

    PUSH_RENDER_COMMAND(CreateGaussianSplattingIndirectBuffers,
        m_stagingBuffer,
        m_quadMesh);
}

void GaussianSplatting::UpdateSplats(FrameBase* frame, const RenderSetup& renderSetup)
{
    Threads::AssertOnThread(g_renderThread);
    AssertReady();

    if (!m_gaussianSplattingInstance)
    {
        return;
    }

    frame->renderQueue << InsertBarrier(
        m_stagingBuffer,
        RS_COPY_SRC);

    frame->renderQueue << InsertBarrier(
        m_gaussianSplattingInstance->GetIndirectBuffer(),
        RS_COPY_DST);

    frame->renderQueue << CopyBuffer(
        m_stagingBuffer,
        m_gaussianSplattingInstance->GetIndirectBuffer(),
        m_gaussianSplattingInstance->GetIndirectBuffer()->Size());

    frame->renderQueue << InsertBarrier(
        m_gaussianSplattingInstance->GetIndirectBuffer(),
        RS_INDIRECT_ARG);

    m_gaussianSplattingInstance->Record(frame, renderSetup);
}

void GaussianSplatting::Render(FrameBase* frame, const RenderSetup& renderSetup)
{
    AssertReady();
    Threads::AssertOnThread(g_renderThread);

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    if (!m_gaussianSplattingInstance)
    {
        return;
    }

    const uint32 frameIndex = frame->GetFrameIndex();

    const GraphicsPipelineRef& graphicsPipeline = m_gaussianSplattingInstance->GetGraphicsPipeline();

    frame->renderQueue << BindGraphicsPipeline(graphicsPipeline);

    frame->renderQueue << BindDescriptorTable(
        graphicsPipeline->GetDescriptorTable(),
        graphicsPipeline,
        { { "Global", { { "CamerasBuffer", ShaderDataOffset<CameraShaderData>(renderSetup.view->GetCamera()) } } } },
        frameIndex);

    frame->renderQueue << BindVertexBuffer(m_quadMesh->GetVertexBuffer());
    frame->renderQueue << BindIndexBuffer(m_quadMesh->GetIndexBuffer());
    frame->renderQueue << DrawIndexedIndirect(m_gaussianSplattingInstance->GetIndirectBuffer(), 0);
}

} // namespace hyperion
