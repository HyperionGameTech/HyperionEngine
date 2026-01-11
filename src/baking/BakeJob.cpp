/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <baking/BakeJob.hpp>
#include <baking/Baker.hpp>

#include <baking/lightmaps/LightmapPathTraceGpu.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/Device.hpp>
#include <rendering/Frame.hpp>
#include <rendering/RenderProxyList.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/RendererBase.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>
#include <scene/FogVolume.hpp>
#include <scene/LightmapVolume.hpp>

#include <scene/util/VoxelOctree.hpp>

#include <core/debug/Debug.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>
#include <engine/DebugDrawer.hpp>

namespace Hyperion {

namespace Baking {

#pragma region Render command

struct LightmapRender : RenderCommand
{
    BakeJobBase* job;
    Handle<World> world;
    Handle<View> view;
    Array<LightmapRay> rays;
    uint32 rayOffset;

    LightmapRender(BakeJobBase* job, const Handle<World>& world, const Handle<View>& view, Array<LightmapRay>&& rays, uint32 rayOffset)
        : job(job),
          world(world),
          view(view),
          rays(std::move(rays)),
          rayOffset(rayOffset)
    {
        job->NumConcurrentRenderingTasks.Increment(1, MemoryOrder::RELEASE);
    }

    virtual ~LightmapRender() override
    {
        job->NumConcurrentRenderingTasks.Decrement(1, MemoryOrder::RELEASE);
    }

    virtual RendererResult operator()() override
    {
        Frame* frame = g_renderBackend->GetCurrentFrame();

        RenderSetup renderSetup { world, view };

        RenderProxyList* rpl = nullptr;

        if (view)
        {
            rpl = &RenderApi::GetConsumerProxyList(view);
        }

        if (rpl)
        {
            rpl->BeginRead();
        }

        HYP_DEFER({ if (rpl) rpl->EndRead(); });

        if (rpl)
        {
            if (const auto& skyProbes = rpl->GetEnvProbes().GetElements<SkyProbe>(); skyProbes.Any())
            {
                renderSetup.envProbe = skyProbes.Front();
            }
        }

        {
            // Read ray hits from last time this frame was rendered
            Array<LightmapRay> previousRays;
            job->GetPreviousFrameRays(previousRays);

            // Read previous frame hits into CPU buffer
            if (previousRays.Size() != 0)
            {
                Array<LightmapHit> hitsBuffer;
                hitsBuffer.Resize(previousRays.Size());

                for (const UniquePtr<ILightmapRenderer>& lightmapRenderer : *job->GetParams().renderers)
                {
                    AssertDebug(lightmapRenderer != nullptr);

                    lightmapRenderer->ReadHitsBuffer(frame, job, hitsBuffer);

                    job->IntegrateRayHits(previousRays, hitsBuffer, lightmapRenderer->GetShadingType());
                }
            }

            job->SetPreviousFrameRays(rays);
        }

        if (rays.Any())
        {
            for (const UniquePtr<ILightmapRenderer>& lightmapRenderer : *job->GetParams().renderers)
            {
                AssertDebug(lightmapRenderer != nullptr);

                lightmapRenderer->Render(frame, renderSetup, job, rays, rayOffset);
            }
        }

        return {};
    }
};

#pragma endregion Render command

static constexpr uint32 MaxConcurrentRenderingTasksPerJob = 1;

#pragma region BakeJobBase

BakeJobBase::BakeJobBase(BakeJobParams&& params)
    : NumConcurrentRenderingTasks(0),
      m_lightmapper(nullptr),
      m_params(std::move(params)),
      m_texelIndex(0),
      m_lastLoggedPercentage(0),
      m_wasStarted(false)
{
}

BakeJobBase::~BakeJobBase()
{
    for (TaskBatch* taskBatch : m_currentTasks)
    {
        taskBatch->AwaitCompletion();

        delete taskBatch;
    }
}

void BakeJobBase::Start()
{
    m_wasStarted = true;
    m_runningSemaphore.Produce(1, [this](bool)
        {
            Start_Internal();
        });
}

void BakeJobBase::Stop()
{
    m_runningSemaphore.Release(1);
}

void BakeJobBase::Stop(const Error& error)
{
    HYP_LOG(Lightmap, Error, "Lightmap job {} stopped with error: {}", m_uuid, error.GetMessage());

    m_result = error;

    Stop();
}

bool BakeJobBase::IsCompleted() const
{
    return !m_runningSemaphore.IsInSignalState() && m_wasStarted;
}

void BakeJobBase::AddTask(TaskBatch* taskBatch)
{
    Mutex::Guard guard(m_currentTasksMutex);

    m_currentTasks.PushBack(taskBatch);
}

bool BakeJobBase::HasRemainingTexels() const
{
    return m_texelIndex < m_texelIndices.Size() * m_lightmapper->NumTexelSamples();
}

void BakeJobBase::GatherTexels(uint32 maxTexels, Array<LightmapTexel*>& outTexels)
{
    const bool hasRays = m_lightmapper->PerformsRayTracing();

    BakeDataBase& bakeData = GetBakeData();

    for (uint32 txlIdx = 0; txlIdx < maxTexels && HasRemainingTexels(); ++txlIdx)
    {
        const uint32 texelIndex = NextTexel();

        LightmapTexel& texel = bakeData.texels[texelIndex];

        if (texel.pRay != nullptr)
        {
            texel.pRay->texelIndex = texelIndex;
        }

        outTexels.PushBack(&texel);
    }
}

uint32 BakeJobBase::ProcessTexels(Span<LightmapTexel*> texels, uint32 texelOffset)
{
    if (!m_lightmapper->PerformsRayTracing())
    {
        return 0;
    }

    const uint32 numTexels = uint32(texels.Size());

    // @NOTE: Can't skip if numTexels == 0, because previous frame rays may still need to be integrated.

    Array<LightmapRay> rays;
    rays.Reserve(numTexels);

    for (uint32 i = 0; i < numTexels; i++)
    {
        if (texels[i]->pRay != nullptr)
        {
            rays.PushBack(*texels[i]->pRay);
        }
    }

    World* world = GetScene()->GetWorld();
    Assert(world != nullptr);

    PUSH_RENDER_COMMAND(LightmapRender, this, MakeStrongRef(world), m_params.view, std::move(rays), texelOffset);

    return numTexels;
}

void BakeJobBase::IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType)
{
    Assert(rays.Size() == hits.Size());

    BakeDataBase& bakeData = GetBakeData();

    for (SizeType i = 0; i < hits.Size(); i++)
    {
        const LightmapRay& ray = rays[i];
        const LightmapHit& hit = hits[i];

        LightmapTexel& texel = bakeData.texels[ray.texelIndex];

        switch (shadingType)
        {
        case LightmapShadingType::FULL: // fallthrough
        case LightmapShadingType::IRRADIANCE:
            texel.color0 += Vec4f(hit.color, 1.0f);
            break;
        case LightmapShadingType::RADIANCE:
            texel.color1 += Vec4f(hit.color, 1.0f);
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
}

uint32 BakeJobBase::Process(uint32 maxTexels)
{
    Assert(IsRunning());
    Assert(!m_result.HasError(), "Unhandled error in lightmap job: {}", *m_result.GetError().GetMessage());

    bool isReadyToProcess = true;
    Process_Internal(&isReadyToProcess);

    if (!isReadyToProcess)
    {
        return 0;
    }

    if (NumConcurrentRenderingTasks.Get(MemoryOrder::ACQUIRE) >= MaxConcurrentRenderingTasksPerJob)
    {
        // Wait for current rendering tasks to complete before enqueueing new ones.

        return 0;
    }

    { // cpu tracing only
        Mutex::Guard guard(m_currentTasksMutex);

        if (m_currentTasks.Any())
        {
            for (SizeType taskIndex = 0; taskIndex < m_currentTasks.Size(); taskIndex++)
            {
                TaskBatch* taskBatch = m_currentTasks[taskIndex];

                if (!taskBatch->IsCompleted())
                {
                    // Skip this call

                    return 0;
                }
            }

            for (SizeType taskIndex = 0; taskIndex < m_currentTasks.Size(); taskIndex++)
            {
                TaskBatch* taskBatch = m_currentTasks[taskIndex];

                taskBatch->AwaitCompletion();

                delete taskBatch;
            }

            m_currentTasks.Clear();
        }
    }

    bool isProcessingRemainingTexels = false;

    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        if (m_previousFrameRays.Any())
        {
            isProcessingRemainingTexels = true;
        }
    }

    if (!isProcessingRemainingTexels
        && m_texelIndex >= m_texelIndices.Size() * m_lightmapper->NumTexelSamples()
        && NumConcurrentRenderingTasks.Get(MemoryOrder::ACQUIRE) == 0)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: All texels processed ({} / {}), stopping", m_uuid, m_texelIndex, m_texelIndices.Size() * m_lightmapper->NumTexelSamples());

        Stop();

        return 0;
    }

    /// \todo : Radiance map won't need as many samples as irradiance due to having less variance in directions,
    // we should separate BakeJob to be per- shading type, so the radiance one can finish earlier.

    for (UniquePtr<ILightmapRenderer>& lightmapRenderer : *m_params.renderers)
    {
        AssertDebug(lightmapRenderer != nullptr);
        
        if (!lightmapRenderer)
        {
            return 0;
        }

        if (!lightmapRenderer->CanRender())
        {
            HYP_LOG(Lightmap, Info, "Waiting for lightmap renderers to be ready...");

            return 0;
        }
    }

    const uint32 totalNumTexels = uint32(m_texelIndices.Size()) * m_lightmapper->NumTexelSamples();
    AssertDebug(totalNumTexels > 0);

    maxTexels = MathUtil::Min(maxTexels, totalNumTexels);
    maxTexels = MathUtil::Min(maxTexels, m_lightmapper->MaxTexelsPerFrame());

    if (m_lightmapper->PerformsRayTracing())
    {
        Assert(m_params.renderers->Size() > 0);

        maxTexels = MathUtil::Min(maxTexels, (*m_params.renderers)[0]->MaxTexelsPerFrame());
    }

    const uint32 texelOffset = uint32(m_texelIndex % totalNumTexels);

    Array<LightmapTexel*> texels;
    texels.Reserve(maxTexels);

    GatherTexels(maxTexels, texels);
    AssertDebug(texels.Size() <= maxTexels);

    return ProcessTexels(Span<LightmapTexel*>(texels.Data(), texels.Size()), texelOffset);
}

#pragma endregion BakeJobBase

} // namespace Baking

} // namespace Hyperion
