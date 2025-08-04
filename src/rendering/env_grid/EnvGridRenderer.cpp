/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/env_grid/EnvGridRenderer.hpp>
#include <rendering/Deferred.hpp>
#include <rendering/PlaceholderData.hpp>
#include <rendering/RenderGlobalState.hpp>

#include <rendering/debug/DebugDrawer.hpp>

#include <rendering/RenderBackend.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/RenderImage.hpp>
#include <rendering/RenderImageView.hpp>
#include <rendering/RenderGpuBuffer.hpp>
#include <rendering/AsyncCompute.hpp>
#include <rendering/Texture.hpp>

#include <scene/EnvGrid.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/View.hpp>
#include <scene/Light.hpp>

#include <core/math/MathUtil.hpp>

#include <core/utilities/DeferredScope.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <HyperionEngine.hpp>

#include <EngineGlobals.hpp>
#include <Engine.hpp>

namespace hyperion {

#pragma region EnvProbeGridIndex

struct EnvProbeGridIndex
{
    Vec3u position;
    Vec3u gridSize;

    // defaults such that GetProbeIndex() == ~0u
    // because (~0u * 0 * 0) + (~0u * 0) + ~0u == ~0u
    EnvProbeGridIndex()
        : position { ~0u, ~0u, ~0u },
          gridSize { 0, 0, 0 }
    {
    }

    EnvProbeGridIndex(const Vec3u& position, const Vec3u& gridSize)
        : position(position),
          gridSize(gridSize)
    {
    }

    EnvProbeGridIndex(const EnvProbeGridIndex& other) = default;
    EnvProbeGridIndex& operator=(const EnvProbeGridIndex& other) = default;
    EnvProbeGridIndex(EnvProbeGridIndex&& other) noexcept = default;
    EnvProbeGridIndex& operator=(EnvProbeGridIndex&& other) noexcept = default;
    ~EnvProbeGridIndex() = default;

    HYP_FORCE_INLINE uint32 GetProbeIndex() const
    {
        return (position.x * gridSize.y * gridSize.z)
            + (position.y * gridSize.z)
            + position.z;
    }

    HYP_FORCE_INLINE bool operator<(uint32 value) const
    {
        return GetProbeIndex() < value;
    }

    HYP_FORCE_INLINE bool operator==(uint32 value) const
    {
        return GetProbeIndex() == value;
    }

    HYP_FORCE_INLINE bool operator!=(uint32 value) const
    {
        return GetProbeIndex() != value;
    }

    HYP_FORCE_INLINE bool operator<(const EnvProbeGridIndex& other) const
    {
        return GetProbeIndex() < other.GetProbeIndex();
    }

    HYP_FORCE_INLINE bool operator==(const EnvProbeGridIndex& other) const
    {
        return GetProbeIndex() == other.GetProbeIndex();
    }

    HYP_FORCE_INLINE bool operator!=(const EnvProbeGridIndex& other) const
    {
        return GetProbeIndex() != other.GetProbeIndex();
    }

    HYP_FORCE_INLINE HashCode GetHashCode() const
    {
        HashCode hc;
        hc.Add(GetProbeIndex());

        return hc;
    }
};

#pragma endregion EnvProbeGridIndex

#pragma region Global Constants

static const Vec2u shNumSamples { 16, 16 };
static const Vec2u shNumTiles { 16, 16 };
static const uint32 shNumLevels = MathUtil::Max(1u, uint32(MathUtil::FastLog2(shNumSamples.Max()) + 1));
static const bool shParallelReduce = false;

static const uint32 maxQueuedProbesForRender = 1;

static const EnvProbeGridIndex invalidProbeIndex = EnvProbeGridIndex();

#pragma endregion Globals Constants

#pragma region Helpers

static EnvProbeGridIndex GetProbeBindingIndex(const Vec3f& probePosition, const BoundingBox& gridAabb, const Vec3u& gridDensity)
{
    const Vec3f diff = probePosition - gridAabb.GetCenter();
    const Vec3f posClamped = (diff / gridAabb.GetExtent()) + Vec3f(0.5f);
    const Vec3f diffUnits = MathUtil::Trunc(posClamped * Vec3f(gridDensity));

    const int probeIndexAtPoint = (int(diffUnits.x) * int(gridDensity.y) * int(gridDensity.z))
        + (int(diffUnits.y) * int(gridDensity.z))
        + int(diffUnits.z);

    EnvProbeGridIndex calculatedProbeIndex = invalidProbeIndex;

    if (probeIndexAtPoint >= 0 && uint32(probeIndexAtPoint) < g_maxBoundAmbientProbes)
    {
        calculatedProbeIndex = EnvProbeGridIndex(
            Vec3u { uint32(diffUnits.x), uint32(diffUnits.y), uint32(diffUnits.z) },
            gridDensity);
    }

    return calculatedProbeIndex;
}

#pragma endregion Helpers

#pragma region Uniform buffer structs

struct LightFieldUniforms
{
    Vec4u imageDimensions;
    Vec4u probeGridPosition;
    Vec4u dimensionPerProbe;
    Vec4u probeOffsetCoord;

    uint32 numBoundLights;
    uint32 _pad0;
    uint32 _pad1;
    uint32 _pad2;

    uint32 lightIndices[16];
};

#pragma endregion Uniform buffer structs

#pragma region EnvGridPassData

EnvGridPassData::~EnvGridPassData()
{
    SafeRelease(std::move(clearSh));
    SafeRelease(std::move(computeSh));
    SafeRelease(std::move(reduceSh));
    SafeRelease(std::move(finalizeSh));

    SafeRelease(std::move(computeIrradiance));
    SafeRelease(std::move(computeFilteredDepth));
    SafeRelease(std::move(copyBorderTexels));

    SafeRelease(std::move(voxelizeProbe));
    SafeRelease(std::move(offsetVoxelGrid));

    SafeRelease(std::move(shTilesBuffers));

    SafeRelease(std::move(computeShDescriptorTables));

    SafeRelease(std::move(voxelGridMips));

    SafeRelease(std::move(generateVoxelGridMipmaps));
    SafeRelease(std::move(generateVoxelGridMipmapsDescriptorTables));
}

#pragma endregion EnvGridPassData

#pragma region EnvGridRenderer

EnvGridRenderer::EnvGridRenderer()
{
}

EnvGridRenderer::~EnvGridRenderer()
{
}

void EnvGridRenderer::Initialize()
{
}

void EnvGridRenderer::Shutdown()
{
}

Handle<PassData> EnvGridRenderer::CreateViewPassData(View* view, PassDataExt& ext)
{
    EnvGridPassDataExt* extCasted = ext.AsType<EnvGridPassDataExt>();
    AssertDebug(extCasted != nullptr, "EnvGridPassDataExt must be provided for EnvGridRenderer");
    AssertDebug(extCasted->envGrid != nullptr);

    EnvGrid* envGrid = extCasted->envGrid;

    EnvProbeCollection& envProbeCollection = envGrid->GetEnvProbeCollection();

    Handle<EnvGridPassData> pd = CreateObject<EnvGridPassData>();
    pd->view = view->WeakHandleFromThis();
    pd->viewport = view->GetViewport();

    pd->currentProbeIndex = envProbeCollection.numProbes != 0 ? 0 : ~0u;

    if (envGrid->GetOptions().flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        CreateVoxelGridData(envGrid, *pd);
    }

    switch (envGrid->GetEnvGridType())
    {
    case ENV_GRID_TYPE_SH:
        CreateSphericalHarmonicsData(envGrid, *pd);

        break;
    case ENV_GRID_TYPE_LIGHT_FIELD:
        CreateLightFieldData(envGrid, *pd);

        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    return pd;
}

void EnvGridRenderer::CreateVoxelGridData(EnvGrid* envGrid, EnvGridPassData& pd)
{
    HYP_SCOPE;

    AssertDebug(envGrid != nullptr);

    const EnvGridOptions& options = envGrid->GetOptions();

    if (!(options.flags & EnvGridFlags::USE_VOXEL_GRID))
    {
        return;
    }

    const ViewOutputTarget& outputTarget = envGrid->GetView()->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid(), "Framebuffer must be created before voxelizing probes");

    // Create shader, descriptor sets for voxelizing probes

    ShaderRef voxelizeProbeShader = g_shaderManager->GetOrCreate(NAME("EnvProbe_VoxelizeProbe"), { { NAME("MODE_VOXELIZE") } });
    ShaderRef offsetVoxelGridShader = g_shaderManager->GetOrCreate(NAME("EnvProbe_VoxelizeProbe"), { { NAME("MODE_OFFSET") } });
    ShaderRef clearVoxelsShader = g_shaderManager->GetOrCreate(NAME("EnvProbe_ClearProbeVoxels"));

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* depthAttachment = framebuffer->GetAttachment(2);

    const DescriptorTableDeclaration& descriptorTableDecl = voxelizeProbeShader->GetCompiledShader()->GetDescriptorTableDeclaration();

    DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        // create descriptor sets for depth pyramid generation.
        DescriptorSetRef descriptorSet = descriptorTable->GetDescriptorSet(NAME("VoxelizeProbeDescriptorSet"), frameIndex);
        Assert(descriptorSet != nullptr);

        descriptorSet->SetElement(NAME("InColorImage"), colorAttachment ? colorAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
        descriptorSet->SetElement(NAME("InNormalsImage"), normalsAttachment ? normalsAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());
        descriptorSet->SetElement(NAME("InDepthImage"), depthAttachment ? depthAttachment->GetImageView() : g_renderGlobalState->placeholderData->GetImageViewCube1x1R8());

        descriptorSet->SetElement(NAME("SamplerLinear"), g_renderGlobalState->placeholderData->GetSamplerLinear());
        descriptorSet->SetElement(NAME("SamplerNearest"), g_renderGlobalState->placeholderData->GetSamplerNearest());

        descriptorSet->SetElement(NAME("EnvGridBuffer"), 0, sizeof(EnvGridShaderData), g_renderGlobalState->gpuBuffers[GRB_ENV_GRIDS]->GetBuffer(frameIndex));
        descriptorSet->SetElement(NAME("EnvProbesBuffer"), g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frameIndex));

        descriptorSet->SetElement(NAME("OutVoxelGridImage"), g_renderBackend->GetTextureImageView(envGrid->GetVoxelGridTexture()));
    }

    DeferCreate(descriptorTable);

    { // Compute shader to clear the voxel grid at a specific position
        pd.clearVoxels = g_renderBackend->MakeComputePipeline(clearVoxelsShader, descriptorTable);
        DeferCreate(pd.clearVoxels);
    }

    { // Compute shader to voxelize a probe into voxel grid
        pd.voxelizeProbe = g_renderBackend->MakeComputePipeline(voxelizeProbeShader, descriptorTable);
        DeferCreate(pd.voxelizeProbe);
    }

    { // Compute shader to 'offset' the voxel grid
        pd.offsetVoxelGrid = g_renderBackend->MakeComputePipeline(offsetVoxelGridShader, descriptorTable);
        DeferCreate(pd.offsetVoxelGrid);
    }

    { // Compute shader to generate mipmaps for voxel grid
        ShaderRef generateVoxelGridMipmapsShader = g_shaderManager->GetOrCreate(NAME("VCTGenerateMipmap"));
        Assert(generateVoxelGridMipmapsShader.IsValid());

        const DescriptorTableDeclaration& generateVoxelGridMipmapsDescriptorTableDecl = generateVoxelGridMipmapsShader->GetCompiledShader()->GetDescriptorTableDeclaration();

        const uint32 numVoxelGridMipLevels = envGrid->GetVoxelGridTexture()->GetTextureDesc().NumMipmaps();
        pd.voxelGridMips.Resize(numVoxelGridMipLevels);

        for (uint32 mipLevel = 0; mipLevel < numVoxelGridMipLevels; mipLevel++)
        {
            pd.voxelGridMips[mipLevel] = g_renderBackend->MakeImageView(
                envGrid->GetVoxelGridTexture()->GetGpuImage(),
                mipLevel, 1,
                0, envGrid->GetVoxelGridTexture()->NumFaces());

            DeferCreate(pd.voxelGridMips[mipLevel]);

            // create descriptor sets for mip generation.
            DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&generateVoxelGridMipmapsDescriptorTableDecl);

            for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
            {
                const DescriptorSetRef& mipDescriptorSet = descriptorTable->GetDescriptorSet(NAME("GenerateMipmapDescriptorSet"), frameIndex);
                Assert(mipDescriptorSet != nullptr);

                if (mipLevel == 0)
                {
                    // first mip level -- input is the actual image
                    mipDescriptorSet->SetElement(NAME("InputTexture"), g_renderBackend->GetTextureImageView(envGrid->GetVoxelGridTexture()));
                }
                else
                {
                    mipDescriptorSet->SetElement(NAME("InputTexture"), pd.voxelGridMips[mipLevel - 1]);
                }

                mipDescriptorSet->SetElement(NAME("OutputTexture"), pd.voxelGridMips[mipLevel]);
            }

            DeferCreate(descriptorTable);

            pd.generateVoxelGridMipmapsDescriptorTables.PushBack(std::move(descriptorTable));
        }

        pd.generateVoxelGridMipmaps = g_renderBackend->MakeComputePipeline(generateVoxelGridMipmapsShader, pd.generateVoxelGridMipmapsDescriptorTables[0]);
        DeferCreate(pd.generateVoxelGridMipmaps);
    }
}

void EnvGridRenderer::CreateSphericalHarmonicsData(EnvGrid* envGrid, EnvGridPassData& pd)
{
    HYP_SCOPE;

    AssertDebug(envGrid != nullptr);

    pd.shTilesBuffers.Resize(shNumLevels);

    for (uint32 i = 0; i < shNumLevels; i++)
    {
        const SizeType size = sizeof(SHTile) * (shNumTiles.x >> i) * (shNumTiles.y >> i);
        pd.shTilesBuffers[i] = g_renderBackend->MakeGpuBuffer(GpuBufferType::SSBO, size);

        DeferCreate(pd.shTilesBuffers[i]);
    }

    FixedArray<ShaderRef, 4> shaders = {
        g_shaderManager->GetOrCreate(NAME("ComputeSH"), { { NAME("MODE_CLEAR") } }),
        g_shaderManager->GetOrCreate(NAME("ComputeSH"), { { NAME("MODE_BUILD_COEFFICIENTS") } }),
        g_shaderManager->GetOrCreate(NAME("ComputeSH"), { { NAME("MODE_REDUCE") } }),
        g_shaderManager->GetOrCreate(NAME("ComputeSH"), { { NAME("MODE_FINALIZE") } })
    };

    for (const ShaderRef& shader : shaders)
    {
        Assert(shader.IsValid());
    }

    const DescriptorTableDeclaration& descriptorTableDecl = shaders[0]->GetCompiledShader()->GetDescriptorTableDeclaration();

    pd.computeShDescriptorTables.Resize(shNumLevels);

    for (uint32 i = 0; i < shNumLevels; i++)
    {
        pd.computeShDescriptorTables[i] = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            const DescriptorSetRef& computeShDescriptorSet = pd.computeShDescriptorTables[i]->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frameIndex);
            Assert(computeShDescriptorSet != nullptr);

            computeShDescriptorSet->SetElement(NAME("InColorCubemap"), g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultCubemap));
            computeShDescriptorSet->SetElement(NAME("InNormalsCubemap"), g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultCubemap));
            computeShDescriptorSet->SetElement(NAME("InDepthCubemap"), g_renderBackend->GetTextureImageView(g_renderGlobalState->placeholderData->defaultCubemap));
            computeShDescriptorSet->SetElement(NAME("InputSHTilesBuffer"), pd.shTilesBuffers[i]);

            if (i != shNumLevels - 1)
            {
                computeShDescriptorSet->SetElement(NAME("OutputSHTilesBuffer"), pd.shTilesBuffers[i + 1]);
            }
            else
            {
                computeShDescriptorSet->SetElement(NAME("OutputSHTilesBuffer"), pd.shTilesBuffers[i]);
            }
        }

        DeferCreate(pd.computeShDescriptorTables[i]);
    }

    pd.clearSh = g_renderBackend->MakeComputePipeline(shaders[0], pd.computeShDescriptorTables[0]);
    DeferCreate(pd.clearSh);

    pd.computeSh = g_renderBackend->MakeComputePipeline(shaders[1], pd.computeShDescriptorTables[0]);
    DeferCreate(pd.computeSh);

    pd.reduceSh = g_renderBackend->MakeComputePipeline(shaders[2], pd.computeShDescriptorTables[0]);
    DeferCreate(pd.reduceSh);

    pd.finalizeSh = g_renderBackend->MakeComputePipeline(shaders[3], pd.computeShDescriptorTables[0]);
    DeferCreate(pd.finalizeSh);
}

void EnvGridRenderer::CreateLightFieldData(EnvGrid* envGrid, EnvGridPassData& pd)
{
    HYP_SCOPE;

    AssertDebug(envGrid != nullptr);
    AssertDebug(envGrid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    const ViewOutputTarget& outputTarget = envGrid->GetView()->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    AssertDebug(framebuffer.IsValid());

    const EnvGridOptions& options = envGrid->GetOptions();

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        GpuBufferRef lightFieldUniforms = g_renderBackend->MakeGpuBuffer(GpuBufferType::CBUFF, sizeof(LightFieldUniforms));
        DeferCreate(lightFieldUniforms);

        pd.uniformBuffers.PushBack(std::move(lightFieldUniforms));
    }

    ShaderRef computeIrradianceShader = g_shaderManager->GetOrCreate(NAME("LightField_ComputeIrradiance"));
    ShaderRef computeFilteredDepthShader = g_shaderManager->GetOrCreate(NAME("LightField_ComputeFilteredDepth"));
    ShaderRef copyBorderTexelsShader = g_shaderManager->GetOrCreate(NAME("LightField_CopyBorderTexels"));

    Pair<ShaderRef, ComputePipelineRef&> shaders[] = {
        { computeIrradianceShader, pd.computeIrradiance },
        { computeFilteredDepthShader, pd.computeFilteredDepth },
        { copyBorderTexelsShader, pd.copyBorderTexels }
    };

    for (Pair<ShaderRef, ComputePipelineRef&>& pair : shaders)
    {
        ShaderRef& shader = pair.first;
        ComputePipelineRef& pipeline = pair.second;

        Assert(shader.IsValid());

        const DescriptorTableDeclaration& descriptorTableDecl = shader->GetCompiledShader()->GetDescriptorTableDeclaration();

        DescriptorTableRef descriptorTable = g_renderBackend->MakeDescriptorTable(&descriptorTableDecl);

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            DescriptorSetRef descriptorSet = descriptorTable->GetDescriptorSet(NAME("LightFieldProbeDescriptorSet"), frameIndex);
            Assert(descriptorSet != nullptr);

            descriptorSet->SetElement(NAME("UniformBuffer"), pd.uniformBuffers[frameIndex]);

            descriptorSet->SetElement(NAME("InColorImage"), framebuffer->GetAttachment(0)->GetImageView());
            descriptorSet->SetElement(NAME("InNormalsImage"), framebuffer->GetAttachment(1)->GetImageView());
            descriptorSet->SetElement(NAME("InDepthImage"), framebuffer->GetAttachment(2)->GetImageView());
            descriptorSet->SetElement(NAME("SamplerLinear"), g_renderGlobalState->placeholderData->GetSamplerLinear());
            descriptorSet->SetElement(NAME("SamplerNearest"), g_renderGlobalState->placeholderData->GetSamplerNearest());
            descriptorSet->SetElement(NAME("OutColorImage"), g_renderBackend->GetTextureImageView(envGrid->GetLightFieldIrradianceTexture()));
            descriptorSet->SetElement(NAME("OutDepthImage"), g_renderBackend->GetTextureImageView(envGrid->GetLightFieldDepthTexture()));
        }

        DeferCreate(descriptorTable);

        pipeline = g_renderBackend->MakeComputePipeline(shader, descriptorTable);
        DeferCreate(pipeline);
    }
}

void EnvGridRenderer::RenderFrame(FrameBase* frame, const RenderSetup& renderSetup)
{
    HYP_SCOPE;
    Threads::AssertOnThread(g_renderThread);
    
    static const ConfigurationValue& globalIlluminationEnabled = GetGlobalConfig().Get("rendering.envGrid.globalIllumination.enabled");

    if (!globalIlluminationEnabled.ToBool())
    {
        return;
    }

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.envGrid != nullptr);

    EnvGrid* envGrid = renderSetup.envGrid;
    AssertDebug(envGrid != nullptr);

    EnvGridPassDataExt ext;
    ext.envGrid = envGrid;

    const Handle<EnvGridPassData>& pd = ObjCast<EnvGridPassData>(FetchViewPassData(envGrid->GetView(), &ext));
    AssertDebug(pd != nullptr);

    RenderSetup rs = renderSetup;
    rs.view = envGrid->GetView().Get();
    rs.passData = pd;

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(envGrid->GetView());
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    /// FIXME: Not thread safe; use render proxy
    const BoundingBox gridAabb = envGrid->GetAABB();

    if (!gridAabb.IsValid() || !gridAabb.IsFinite())
    {
        HYP_LOG(EnvGrid, Warning, "EnvGrid AABB is invalid or not finite - skipping rendering");

        return;
    }

    const EnvGridOptions& options = envGrid->GetOptions();
    const EnvProbeCollection& envProbeCollection = envGrid->GetEnvProbeCollection();

    // Debug draw
    if (options.flags & EnvGridFlags::DEBUG_DISPLAY_PROBES)
    {
        DebugDrawCommandList& debugDrawer = g_engine->GetDebugDrawer()->CreateCommandList();

        for (uint32 index = 0; index < envProbeCollection.numProbes; index++)
        {
            const Handle<EnvProbe>& probe = envProbeCollection.GetEnvProbeDirect(index);

            if (!probe.IsValid())
            {
                continue;
            }

            RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(probe->Id()));
            Assert(envProbeProxy != nullptr);

            debugDrawer.ambientProbe(
                envProbeProxy->bufferData.worldPosition.GetXYZ(),
                0.25f,
                *probe);
        }
    }

    // render enqueued probes
    while (pd->nextRenderIndices.Any())
    {
        RenderProbe(frame, rs, pd->nextRenderIndices.Pop());
    }

    if (envProbeCollection.numProbes != 0)
    {
        // update probe positions in grid, choose next to render.
        Assert(pd->currentProbeIndex < envProbeCollection.numProbes);

        // const Vec3f &cameraPosition = cameraResourceHandle->GetBufferData().cameraPosition.GetXYZ();

        Array<Pair<uint32, float>> indicesDistances;
        indicesDistances.Reserve(16);

        for (uint32 i = 0; i < envProbeCollection.numProbes; i++)
        {
            const uint32 index = (pd->currentProbeIndex + i) % envProbeCollection.numProbes;
            const Handle<EnvProbe>& probe = envProbeCollection.GetEnvProbeOnRenderThread(index);

            if (probe.IsValid() && probe->NeedsRender())
            {
                indicesDistances.PushBack({
                    index,
                    0 // probe->GetRenderResource().GetBufferData().worldPosition.GetXYZ().Distance(cameraPosition)
                });
            }
        }

        // std::sort(indicesDistances.Begin(), indicesDistances.End(), [](const auto &lhs, const auto &rhs) {
        //     return lhs.second < rhs.second;
        // });

        if (indicesDistances.Any())
        {
            for (const auto& it : indicesDistances)
            {
                const uint32 foundIndex = it.first;
                const uint32 indirectIndex = envProbeCollection.GetIndexOnRenderThread(foundIndex);

                const Handle<EnvProbe>& probe = envProbeCollection.GetEnvProbeDirect(indirectIndex);
                Assert(probe.IsValid());

                RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(probe->Id()));
                Assert(envProbeProxy != nullptr);

                const Vec3f worldPosition = envProbeProxy->bufferData.worldPosition.GetXYZ();

                const EnvProbeGridIndex bindingIndex = GetProbeBindingIndex(worldPosition, gridAabb, options.density);

                if (bindingIndex != invalidProbeIndex)
                {
                    if (pd->nextRenderIndices.Size() < maxQueuedProbesForRender)
                    {
                        // render this probe in the next frame, since the data will have been updated on the gpu on start of the frame
                        pd->nextRenderIndices.Push(indirectIndex);

                        pd->currentProbeIndex = (foundIndex + 1) % envProbeCollection.numProbes;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    HYP_LOG(EnvGrid, Warning, "EnvProbe {} out of range of max bound env probes (position: {}, world position: {}",
                        probe->Id(), bindingIndex.position, worldPosition);
                }
            }
        }
    }
}

void EnvGridRenderer::RenderProbe(FrameBase* frame, const RenderSetup& renderSetup, uint32 probeIndex)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.IsValid());

    EnvGrid* envGrid = renderSetup.envGrid;
    AssertDebug(envGrid != nullptr);

    View* view = envGrid->GetView();
    AssertDebug(view != nullptr);

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    RenderCollector& renderCollector = RenderApi_GetRenderCollector(view);

    const EnvGridOptions& options = envGrid->GetOptions();
    const EnvProbeCollection& envProbeCollection = envGrid->GetEnvProbeCollection();

    const Handle<EnvProbe>& probe = envProbeCollection.GetEnvProbeDirect(probeIndex);
    Assert(probe.IsValid());

    const uint32 probeBoundIndex = RenderApi_RetrieveResourceBinding(probe.Id());
    AssertDebug(probeBoundIndex != ~0u, "EnvProbe {} is not bound when rendering EnvGrid!", probe.Id());

    {
        RenderSetup rs = renderSetup;
        rs.envProbe = probe;

        renderCollector.ExecuteDrawCalls(frame, rs, (1u << RB_OPAQUE));
    }

    switch (envGrid->GetEnvGridType())
    {
    case ENV_GRID_TYPE_SH:
        ComputeEnvProbeIrradiance_SphericalHarmonics(frame, renderSetup, probe);

        break;
    case ENV_GRID_TYPE_LIGHT_FIELD:
        ComputeEnvProbeIrradiance_LightField(frame, renderSetup, probe);

        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    if (options.flags & EnvGridFlags::USE_VOXEL_GRID)
    {
        VoxelizeProbe(frame, renderSetup, probeIndex);
    }

    probe->SetNeedsRender(false);
}

void EnvGridRenderer::ComputeEnvProbeIrradiance_SphericalHarmonics(FrameBase* frame, const RenderSetup& renderSetup, const Handle<EnvProbe>& probe)
{
    HYP_SCOPE;

    AssertDebug(probe.IsValid());

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    EnvGrid* envGrid = renderSetup.envGrid;
    AssertDebug(envGrid != nullptr);
    AssertDebug(envGrid->GetEnvGridType() == ENV_GRID_TYPE_SH);

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(probe.Id()));
    AssertDebug(envProbeProxy != nullptr);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    const Handle<EnvGridPassData>& pd = ObjCast<EnvGridPassData>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    Assert(framebuffer.IsValid());

    const EnvGridOptions& options = envGrid->GetOptions();
    const EnvProbeCollection& envProbeCollection = envGrid->GetEnvProbeCollection();

    const uint32 gridSlot = probe->m_gridSlot;
    Assert(gridSlot != ~0u);

    AttachmentBase* colorAttachment = framebuffer->GetAttachment(0);
    AttachmentBase* normalsAttachment = framebuffer->GetAttachment(1);
    AttachmentBase* depthAttachment = framebuffer->GetAttachment(2);

    const Vec2u cubemapDimensions = colorAttachment->GetImage()->GetExtent().GetXY();
    Assert(cubemapDimensions.Volume() > 0);

    struct
    {
        Vec4u probeGridPosition;
        Vec4u cubemapDimensions;
        Vec4u levelDimensions;
        Vec4f worldPosition;
        uint32 envProbeIndex;
    } pushConstants;

    pushConstants.envProbeIndex = RenderApi_RetrieveResourceBinding(probe.Id());

    pushConstants.probeGridPosition = {
        gridSlot % options.density.x,
        (gridSlot % (options.density.x * options.density.y)) / options.density.x,
        gridSlot / (options.density.x * options.density.y),
        gridSlot
    };

    pushConstants.cubemapDimensions = Vec4u { cubemapDimensions, 0, 0 };
    pushConstants.worldPosition = envProbeProxy->bufferData.worldPosition;

    for (const DescriptorTableRef& descriptorSetRef : pd->computeShDescriptorTables)
    {
        descriptorSetRef->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InColorCubemap"), framebuffer->GetAttachment(0)->GetImageView());

        descriptorSetRef->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InNormalsCubemap"), framebuffer->GetAttachment(1)->GetImageView());

        descriptorSetRef->GetDescriptorSet(NAME("ComputeSHDescriptorSet"), frame->GetFrameIndex())
            ->SetElement(NAME("InDepthCubemap"), framebuffer->GetAttachment(2)->GetImageView());

        descriptorSetRef->Update(frame->GetFrameIndex());
    }

    pd->clearSh->SetPushConstants(&pushConstants, sizeof(pushConstants));
    pd->computeSh->SetPushConstants(&pushConstants, sizeof(pushConstants));

    RenderQueue& asyncComputeCommandList = g_renderBackend->GetAsyncCompute()->renderQueue;

    asyncComputeCommandList << InsertBarrier(pd->shTilesBuffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    asyncComputeCommandList << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    asyncComputeCommandList << BindDescriptorTable(
        pd->computeShDescriptorTables[0],
        pd->clearSh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(renderSetup.light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncComputeCommandList << BindComputePipeline(pd->clearSh);
    asyncComputeCommandList << DispatchCompute(pd->clearSh, Vec3u { 1, 1, 1 });

    asyncComputeCommandList << InsertBarrier(pd->shTilesBuffers[0], RS_UNORDERED_ACCESS, SMT_COMPUTE);

    asyncComputeCommandList << BindDescriptorTable(
        pd->computeShDescriptorTables[0],
        pd->computeSh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(renderSetup.light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncComputeCommandList << BindComputePipeline(pd->computeSh);
    asyncComputeCommandList << DispatchCompute(pd->computeSh, Vec3u { 1, 1, 1 });

    // Parallel reduce
    if (shParallelReduce)
    {
        for (uint32 i = 1; i < shNumLevels; i++)
        {
            asyncComputeCommandList << InsertBarrier(pd->shTilesBuffers[i - 1], RS_UNORDERED_ACCESS, SMT_COMPUTE);

            const Vec2u prevDimensions {
                MathUtil::Max(1u, shNumSamples.x >> (i - 1)),
                MathUtil::Max(1u, shNumSamples.y >> (i - 1))
            };

            const Vec2u nextDimensions {
                MathUtil::Max(1u, shNumSamples.x >> i),
                MathUtil::Max(1u, shNumSamples.y >> i)
            };

            Assert(prevDimensions.x >= 2);
            Assert(prevDimensions.x > nextDimensions.x);
            Assert(prevDimensions.y > nextDimensions.y);

            pushConstants.levelDimensions = {
                prevDimensions.x,
                prevDimensions.y,
                nextDimensions.x,
                nextDimensions.y
            };

            pd->reduceSh->SetPushConstants(&pushConstants, sizeof(pushConstants));

            asyncComputeCommandList << BindDescriptorTable(
                pd->computeShDescriptorTables[i - 1],
                pd->reduceSh,
                ArrayMap<Name, ArrayMap<Name, uint32>> {
                    { NAME("Global"),
                        { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) },
                            { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(renderSetup.light, 0) },
                            { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
                frame->GetFrameIndex());

            asyncComputeCommandList << BindComputePipeline(pd->reduceSh);
            asyncComputeCommandList << DispatchCompute(pd->reduceSh, Vec3u { 1, (nextDimensions.x + 3) / 4, (nextDimensions.y + 3) / 4 });
        }
    }

    const uint32 finalizeShBufferIndex = shParallelReduce ? shNumLevels - 1 : 0;

    // Finalize - build into final buffer
    asyncComputeCommandList << InsertBarrier(pd->shTilesBuffers[finalizeShBufferIndex], RS_UNORDERED_ACCESS, SMT_COMPUTE);
    asyncComputeCommandList << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    pd->finalizeSh->SetPushConstants(&pushConstants, sizeof(pushConstants));

    asyncComputeCommandList << BindDescriptorTable(
        pd->computeShDescriptorTables[finalizeShBufferIndex],
        pd->finalizeSh,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) },
                    { NAME("CurrentLight"), ShaderDataOffset<LightShaderData>(renderSetup.light, 0) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    asyncComputeCommandList << BindComputePipeline(pd->finalizeSh);
    asyncComputeCommandList << DispatchCompute(pd->finalizeSh, Vec3u { 1, 1, 1 });

    asyncComputeCommandList << InsertBarrier(g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->GetBuffer(frame->GetFrameIndex()), RS_UNORDERED_ACCESS, SMT_COMPUTE);

    DelegateHandler* delegateHandle = new DelegateHandler();
    *delegateHandle = frame->OnFrameEnd.Bind(
        [probe = Handle<EnvProbe>(probe), delegateHandle](FrameBase* frame)
        {
            HYP_NAMED_SCOPE("EnvGridRenderer::ComputeEnvProbeIrradiance_SphericalHarmonics - Buffer readback");

            EnvProbeShaderData readbackBuffer;

            const uint32 boundIndex = RenderApi_RetrieveResourceBinding(probe.Id());
            Assert(boundIndex != ~0u);

            g_renderGlobalState->gpuBuffers[GRB_ENV_PROBES]->ReadbackElement(frame->GetFrameIndex(), boundIndex, &readbackBuffer);

            // Log out SH data
            HYP_LOG(EnvGrid, Info, "EnvProbe {} SH data:\n\t{}\n\t{}\n\t{}\n\t{}\n",
                probe->Id(),
                readbackBuffer.sh.values[0],
                readbackBuffer.sh.values[1],
                readbackBuffer.sh.values[2],
                readbackBuffer.sh.values[3]);

            // resourceHandle->SetSphericalHarmonics(readbackBuffer.sh);

            delete delegateHandle;
        });
}

void EnvGridRenderer::ComputeEnvProbeIrradiance_LightField(FrameBase* frame, const RenderSetup& renderSetup, const Handle<EnvProbe>& probe)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    EnvGrid* envGrid = renderSetup.envGrid;
    AssertDebug(envGrid != nullptr);
    AssertDebug(envGrid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    const Handle<EnvGridPassData>& pd = ObjCast<EnvGridPassData>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    Assert(framebuffer.IsValid());

    RenderProxyList& rpl = RenderApi_GetConsumerProxyList(view);
    rpl.BeginRead();
    HYP_DEFER({ rpl.EndRead(); });

    RenderProxyEnvGrid* proxy = static_cast<RenderProxyEnvGrid*>(RenderApi_GetRenderProxy(envGrid->Id()));
    Assert(proxy != nullptr, "EnvGrid render proxy not found!");

    const Vec2i irradianceOctahedronSize = proxy->bufferData.irradianceOctahedronSize;

    const EnvGridOptions& options = envGrid->GetOptions();
    const uint32 probeIndex = probe->m_gridSlot;

    { // Update uniform buffer

        LightFieldUniforms uniforms;
        Memory::MemSet(&uniforms, 0, sizeof(uniforms));

        uniforms.imageDimensions = Vec4u { envGrid->GetLightFieldIrradianceTexture()->GetExtent(), 0 };

        uniforms.probeGridPosition = {
            probeIndex % options.density.x,
            (probeIndex % (options.density.x * options.density.y)) / options.density.x,
            probeIndex / (options.density.x * options.density.y),
            RenderApi_RetrieveResourceBinding(probe.Id())
        };

        uniforms.dimensionPerProbe = {
            uint32(irradianceOctahedronSize.x),
            uint32(irradianceOctahedronSize.y),
            0, 0
        };

        uniforms.probeOffsetCoord = {
            (irradianceOctahedronSize.x + 2) * (uniforms.probeGridPosition.x * options.density.y + uniforms.probeGridPosition.y) + 1,
            (irradianceOctahedronSize.y + 2) * uniforms.probeGridPosition.z + 1,
            0, 0
        };

        const uint32 maxBoundLights = ArraySize(uniforms.lightIndices);
        uint32 numBoundLights = 0;

        for (Light* light : rpl.GetLights())
        {
            const LightType lightType = light->GetLightType();

            if (lightType != LT_DIRECTIONAL && lightType != LT_POINT)
            {
                continue;
            }

            if (numBoundLights >= maxBoundLights)
            {
                break;
            }

            uniforms.lightIndices[numBoundLights++] = RenderApi_RetrieveResourceBinding(light);
        }

        uniforms.numBoundLights = numBoundLights;

        pd->uniformBuffers[frame->GetFrameIndex()]->Copy(sizeof(uniforms), &uniforms);
    }

    frame->renderQueue << InsertBarrier(envGrid->GetLightFieldIrradianceTexture()->GetGpuImage(), RS_UNORDERED_ACCESS);
    frame->renderQueue << BindComputePipeline(pd->computeIrradiance);

    frame->renderQueue << BindDescriptorTable(
        pd->computeIrradiance->GetDescriptorTable(),
        pd->computeIrradiance,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(pd->computeIrradiance, Vec3u { uint32(irradianceOctahedronSize.x + 7) / 8, uint32(irradianceOctahedronSize.y + 7) / 8, 1 });

    frame->renderQueue << BindComputePipeline(pd->computeFilteredDepth);

    frame->renderQueue << BindDescriptorTable(
        pd->computeFilteredDepth->GetDescriptorTable(),
        pd->computeFilteredDepth,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(pd->computeFilteredDepth, Vec3u { uint32(irradianceOctahedronSize.x + 7) / 8, uint32(irradianceOctahedronSize.y + 7) / 8, 1 });

    frame->renderQueue << InsertBarrier(envGrid->GetLightFieldDepthTexture()->GetGpuImage(), RS_UNORDERED_ACCESS);

    frame->renderQueue << BindComputePipeline(pd->copyBorderTexels);
    frame->renderQueue << BindDescriptorTable(
        pd->copyBorderTexels->GetDescriptorTable(),
        pd->copyBorderTexels,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) },
                    { NAME("CurrentEnvProbe"), ShaderDataOffset<EnvProbeShaderData>(renderSetup.envProbe, 0) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(pd->copyBorderTexels, Vec3u { uint32((irradianceOctahedronSize.x * 4) + 255) / 256, 1, 1 });

    frame->renderQueue << InsertBarrier(envGrid->GetLightFieldIrradianceTexture()->GetGpuImage(), RS_SHADER_RESOURCE);
    frame->renderQueue << InsertBarrier(envGrid->GetLightFieldDepthTexture()->GetGpuImage(), RS_SHADER_RESOURCE);
}

void EnvGridRenderer::OffsetVoxelGrid(FrameBase* frame, const RenderSetup& renderSetup, Vec3i offset)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    EnvGrid* envGrid = renderSetup.envGrid;
    AssertDebug(envGrid != nullptr);
    AssertDebug(envGrid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    const Handle<EnvGridPassData>& pd = ObjCast<EnvGridPassData>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    Assert(envGrid->GetVoxelGridTexture().IsValid());

    struct
    {
        Vec4u probeGridPosition;
        Vec4u cubemapDimensions;
        Vec4i offset;
    } pushConstants;

    Memory::MemSet(&pushConstants, 0, sizeof(pushConstants));

    pushConstants.offset = { offset.x, offset.y, offset.z, 0 };

    pd->offsetVoxelGrid->SetPushConstants(&pushConstants, sizeof(pushConstants));

    frame->renderQueue << InsertBarrier(envGrid->GetVoxelGridTexture()->GetGpuImage(), RS_UNORDERED_ACCESS);
    frame->renderQueue << BindComputePipeline(pd->offsetVoxelGrid);

    frame->renderQueue << BindDescriptorTable(
        pd->offsetVoxelGrid->GetDescriptorTable(),
        pd->offsetVoxelGrid,
        ArrayMap<Name, ArrayMap<Name, uint32>> {
            { NAME("Global"),
                { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) } } } },
        frame->GetFrameIndex());

    frame->renderQueue << DispatchCompute(pd->offsetVoxelGrid, (envGrid->GetVoxelGridTexture()->GetExtent() + Vec3u(7)) / Vec3u(8));
    frame->renderQueue << InsertBarrier(envGrid->GetVoxelGridTexture()->GetGpuImage(), RS_SHADER_RESOURCE);
}

void EnvGridRenderer::VoxelizeProbe(FrameBase* frame, const RenderSetup& renderSetup, uint32 probeIndex)
{
    HYP_SCOPE;

    AssertDebug(renderSetup.IsValid());
    AssertDebug(renderSetup.HasView());

    EnvGrid* envGrid = renderSetup.envGrid;
    AssertDebug(envGrid != nullptr);
    AssertDebug(envGrid->GetEnvGridType() == ENV_GRID_TYPE_LIGHT_FIELD);

    View* view = renderSetup.view;
    AssertDebug(view != nullptr);

    const Handle<EnvGridPassData>& pd = ObjCast<EnvGridPassData>(FetchViewPassData(view));
    AssertDebug(pd != nullptr);

    const ViewOutputTarget& outputTarget = view->GetOutputTarget();
    AssertDebug(outputTarget.IsValid());

    const FramebufferRef& framebuffer = outputTarget.GetFramebuffer();
    Assert(framebuffer.IsValid());

    RenderProxyEnvProbe* envProbeProxy = static_cast<RenderProxyEnvProbe*>(RenderApi_GetRenderProxy(renderSetup.envProbe->Id()));
    Assert(envProbeProxy != nullptr);

    const EnvGridOptions& options = envGrid->GetOptions();
    const EnvProbeCollection& envProbeCollection = envGrid->GetEnvProbeCollection();

    Assert(envGrid->GetVoxelGridTexture().IsValid());

    const Vec3u& voxelGridTextureExtent = envGrid->GetVoxelGridTexture()->GetExtent();

    // size of a probe in the voxel grid
    const Vec3u probeVoxelExtent = voxelGridTextureExtent / options.density;

    const Handle<EnvProbe>& probe = envProbeCollection.GetEnvProbeDirect(probeIndex);
    Assert(probe.IsValid());
    Assert(probe->IsReady());

    const ImageRef& colorImage = framebuffer->GetAttachment(0)->GetImage();
    const Vec3u cubemapDimensions = colorImage->GetExtent();

    struct
    {
        Vec4u probeGridPosition;
        Vec4u voxelTextureDimensions;
        Vec4u cubemapDimensions;
        Vec4f worldPosition;
    } pushConstants;

    pushConstants.probeGridPosition = {
        probeIndex % options.density.x,
        (probeIndex % (options.density.x * options.density.y)) / options.density.x,
        probeIndex / (options.density.x * options.density.y),
        RenderApi_RetrieveResourceBinding(probe.Id())
    };

    pushConstants.voxelTextureDimensions = Vec4u(voxelGridTextureExtent, 0);
    pushConstants.cubemapDimensions = Vec4u(cubemapDimensions, 0);
    pushConstants.worldPosition = envProbeProxy->bufferData.worldPosition;

    frame->renderQueue << InsertBarrier(colorImage, RS_SHADER_RESOURCE);

    if (false)
    { // Clear our voxel grid at the start of each probe
        pd->clearVoxels->SetPushConstants(&pushConstants, sizeof(pushConstants));

        frame->renderQueue << InsertBarrier(envGrid->GetVoxelGridTexture()->GetGpuImage(), RS_UNORDERED_ACCESS);

        frame->renderQueue << BindComputePipeline(pd->clearVoxels);

        frame->renderQueue << BindDescriptorTable(
            pd->clearVoxels->GetDescriptorTable(),
            pd->clearVoxels,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) } } } },
            frame->GetFrameIndex());

        frame->renderQueue << DispatchCompute(pd->clearVoxels, (probeVoxelExtent + Vec3u(7)) / Vec3u(8));
    }

    { // Voxelize probe
        pd->voxelizeProbe->SetPushConstants(&pushConstants, sizeof(pushConstants));

        frame->renderQueue << InsertBarrier(envGrid->GetVoxelGridTexture()->GetGpuImage(), RS_UNORDERED_ACCESS);
        frame->renderQueue << BindComputePipeline(pd->voxelizeProbe);

        frame->renderQueue << BindDescriptorTable(
            pd->voxelizeProbe->GetDescriptorTable(),
            pd->voxelizeProbe,
            ArrayMap<Name, ArrayMap<Name, uint32>> {
                { NAME("Global"),
                    { { NAME("EnvGridsBuffer"), ShaderDataOffset<EnvGridShaderData>(envGrid) } } } },
            frame->GetFrameIndex());

        frame->renderQueue << DispatchCompute(
            pd->voxelizeProbe,
            Vec3u {
                (probeVoxelExtent.x + 31) / 32,
                (probeVoxelExtent.y + 31) / 32,
                (probeVoxelExtent.z + 31) / 32 });
    }

    { // Generate mipmaps for the voxel grid
        frame->renderQueue << InsertBarrier(envGrid->GetVoxelGridTexture()->GetGpuImage(), RS_SHADER_RESOURCE);

        const uint32 numMipLevels = envGrid->GetVoxelGridTexture()->GetTextureDesc().NumMipmaps();

        const Vec3u voxelImageExtent = envGrid->GetVoxelGridTexture()->GetExtent();
        Vec3u mipExtent = voxelImageExtent;

        struct
        {
            Vec4u mipDimensions;
            Vec4u prevMipDimensions;
            uint32 mipLevel;
        } pushConstantData;

        for (uint32 mipLevel = 0; mipLevel < numMipLevels; mipLevel++)
        {
            const Vec3u prevMipExtent = mipExtent;

            mipExtent.x = MathUtil::Max(1u, voxelImageExtent.x >> mipLevel);
            mipExtent.y = MathUtil::Max(1u, voxelImageExtent.y >> mipLevel);
            mipExtent.z = MathUtil::Max(1u, voxelImageExtent.z >> mipLevel);

            pushConstantData.mipDimensions = Vec4u { mipExtent.x, mipExtent.y, mipExtent.z, 0 };
            pushConstantData.prevMipDimensions = Vec4u { prevMipExtent.x, prevMipExtent.y, prevMipExtent.z, 0 };
            pushConstantData.mipLevel = mipLevel;

            if (mipLevel != 0)
            {
                // put the mip into writeable state
                frame->renderQueue << InsertBarrier(
                    envGrid->GetVoxelGridTexture()->GetGpuImage(),
                    RS_UNORDERED_ACCESS,
                    ImageSubResource { .baseMipLevel = mipLevel });
            }

            frame->renderQueue << BindDescriptorTable(
                pd->generateVoxelGridMipmapsDescriptorTables[mipLevel],
                pd->generateVoxelGridMipmaps,
                ArrayMap<Name, ArrayMap<Name, uint32>> {},
                frame->GetFrameIndex());

            pd->generateVoxelGridMipmaps->SetPushConstants(&pushConstantData, sizeof(pushConstantData));

            frame->renderQueue << BindComputePipeline(pd->generateVoxelGridMipmaps);

            frame->renderQueue << DispatchCompute(pd->generateVoxelGridMipmaps, (mipExtent + Vec3u(7)) / Vec3u(8));

            // put this mip into readable state
            frame->renderQueue << InsertBarrier(envGrid->GetVoxelGridTexture()->GetGpuImage(), RS_SHADER_RESOURCE, ImageSubResource { .baseMipLevel = mipLevel });
        }

        // all mip levels have been transitioned into this state
        frame->renderQueue << InsertBarrier(envGrid->GetVoxelGridTexture()->GetGpuImage(), RS_SHADER_RESOURCE);
    }
}

#pragma endregion EnvGridRenderer

} // namespace hyperion
