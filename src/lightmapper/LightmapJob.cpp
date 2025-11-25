/* Copyright (c) 2025 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <lightmapper/LightmapJob.hpp>
#include <lightmapper/Lightmapper.hpp>
#include <lightmapper/LightmapPathTraceCpu.hpp>
#include <lightmapper/LightmapPathTraceGpu.hpp>

#include <rendering/RenderEnvironment.hpp>
#include <rendering/RenderGlobalState.hpp>
#include <rendering/RenderHelpers.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderBackend.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderConfig.hpp>
#include <rendering/RenderDevice.hpp>
#include <rendering/RenderFrame.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Material.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Renderer.hpp>

#include <lightmapper/LightmapVolume.hpp>

#include <scene/World.hpp>
#include <scene/View.hpp>
#include <scene/EnvProbe.hpp>

#include <core/debug/Debug.hpp>

#include <core/threading/TaskSystem.hpp>
#include <core/threading/TaskThread.hpp>

#include <core/logging/LogChannels.hpp>
#include <core/logging/Logger.hpp>

#include <core/profiling/ProfileScope.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

namespace hyperion {

#pragma region Render command

struct LightmapRender : RenderCommand
{
    LightmapJobBase* job;
    Handle<World> world;
    Handle<View> view;
    Array<LightmapRay> rays;
    uint32 rayOffset;

    LightmapRender(LightmapJobBase* job, const Handle<World>& world, const Handle<View>& view, Array<LightmapRay>&& rays, uint32 rayOffset)
        : job(job),
          world(world),
          view(view),
          rays(std::move(rays)),
          rayOffset(rayOffset)
    {
        job->numConcurrentRenderingTasks.Increment(1, MemoryOrder::RELEASE);
    }

    virtual ~LightmapRender() override
    {
        job->numConcurrentRenderingTasks.Decrement(1, MemoryOrder::RELEASE);
    }

    virtual RendererResult operator()() override
    {
        FrameBase* frame = g_renderBackend->GetCurrentFrame();

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

                for (const UniquePtr<ILightmapRenderer>& lightmapRenderer : job->GetParams().renderers)
                {
                    AssertDebug(lightmapRenderer != nullptr);

                    lightmapRenderer->ReadHitsBuffer(frame, hitsBuffer);

                    job->IntegrateRayHits(previousRays, hitsBuffer, lightmapRenderer->GetShadingType());
                }
            }

            job->SetPreviousFrameRays(rays);
        }

        if (rays.Any())
        {
            for (const UniquePtr<ILightmapRenderer>& lightmapRenderer : job->GetParams().renderers)
            {
                AssertDebug(lightmapRenderer != nullptr);

                lightmapRenderer->Render(frame, renderSetup, job, rays, rayOffset);
            }
        }

        HYPERION_RETURN_OK;
    }
};

#pragma endregion Render command

static constexpr uint32 MaxConcurrentRenderingTasksPerJob = 1;

#pragma region LightmapJobBase

bool LightmapJobBase::HasRemainingTexels() const
{
    return m_texelIndex < m_texelIndices.Size() * m_params.config->numSamples;
}

LightmapJobBase::LightmapJobBase(LightmapJobParams&& params)
    : m_params(std::move(params)),
      m_texelIndex(0),
      m_lastLoggedPercentage(0),
      numConcurrentRenderingTasks(0)
{
}

LightmapJobBase::~LightmapJobBase()
{
    for (TaskBatch* taskBatch : m_currentTasks)
    {
        taskBatch->AwaitCompletion();

        delete taskBatch;
    }
}

void LightmapJobBase::Start()
{
    m_runningSemaphore.Produce(1, [this](bool)
        {
            Start_Internal();
        });
}

void LightmapJobBase::Stop()
{
    m_runningSemaphore.Release(1);
}

void LightmapJobBase::Stop(const Error& error)
{
    HYP_LOG(Lightmap, Error, "Lightmap job {} stopped with error: {}", m_uuid, error.GetMessage());

    m_result = error;

    Stop();
}

bool LightmapJobBase::IsCompleted() const
{
    return !m_runningSemaphore.IsInSignalState();
}

void LightmapJobBase::AddTask(TaskBatch* taskBatch)
{
    Mutex::Guard guard(m_currentTasksMutex);

    m_currentTasks.PushBack(taskBatch);
}

void LightmapJobBase::GatherRays(uint32 maxRayHits, Array<LightmapRay>& outRays)
{
    LightmapDataBase& lightmapData = GetLightmapData();

    for (uint32 rayIndex = 0; rayIndex < maxRayHits && HasRemainingTexels(); ++rayIndex)
    {
        const uint32 texelIndex = NextTexel();

        LightmapRay ray = lightmapData.texels[texelIndex].ray;
        ray.texelIndex = texelIndex;

        outRays.PushBack(ray);
    }
}

void LightmapJobBase::IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType)
{
    Assert(rays.Size() == hits.Size());

    LightmapDataBase& lightmapData = GetLightmapData();

    for (SizeType i = 0; i < hits.Size(); i++)
    {
        const LightmapRay& ray = rays[i];
        const LightmapHit& hit = hits[i];

        LightmapTexel& texel = lightmapData.texels[ray.texelIndex];

        switch (shadingType)
        {
        case LightmapShadingType::FULL: // fallthrough
        case LightmapShadingType::RADIANCE:
            texel.radiance += Vec4f(hit.color, 1.0f);
            break;
        case LightmapShadingType::IRRADIANCE:
            texel.irradiance += Vec4f(hit.color, 1.0f);
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
}

uint32 LightmapJobBase::Process(uint32 maxRays)
{
    Assert(IsRunning());
    Assert(!m_result.HasError(), "Unhandled error in lightmap job: {}", *m_result.GetError().GetMessage());

    bool isReadyToProcess = true;
    Process_Internal(&isReadyToProcess);

    if (!isReadyToProcess)
    {
        return 0;
    }

    if (numConcurrentRenderingTasks.Get(MemoryOrder::ACQUIRE) >= MaxConcurrentRenderingTasksPerJob)
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

    bool hasRemainingRays = false;

    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        if (m_previousFrameRays.Any())
        {
            hasRemainingRays = true;
        }
    }

    if (!hasRemainingRays
        && m_texelIndex >= m_texelIndices.Size() * m_params.config->numSamples
        && numConcurrentRenderingTasks.Get(MemoryOrder::ACQUIRE) == 0)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: All texels processed ({} / {}), stopping", m_uuid, m_texelIndex, m_texelIndices.Size() * m_params.config->numSamples);

        Stop();

        return 0;
    }

    Array<UniquePtr<ILightmapRenderer>>& lightmapRenderers = m_params.renderers;
    AssertDebug(lightmapRenderers[0] != nullptr);

    // @TODO: Radiance map won't need as many samples as irradiance due to having less variance in directions,
    // we should separate LightmapJob to be per- shading type, so the radiance one can finish earlier.

    for (UniquePtr<ILightmapRenderer>& lightmapRenderer : lightmapRenderers)
    {
        AssertDebug(lightmapRenderer != nullptr);

        if (!lightmapRenderer->CanRender())
        {
            HYP_LOG(Lightmap, Info, "Waiting for lightmap renderers to be ready...");

            return 0;
        }
    }

    const SizeType calculatedMaxRays = MathUtil::Min(maxRays, lightmapRenderers[0]->MaxRaysPerFrame(), m_texelIndices.Size() * m_params.config->numSamples);

    Array<LightmapRay> rays;
    rays.Reserve(calculatedMaxRays);

    GatherRays(calculatedMaxRays, rays);

    for (UniquePtr<ILightmapRenderer>& lightmapRenderer : lightmapRenderers)
    {
        lightmapRenderer->UpdateRays(rays);
    }

    const double percentage = double(m_texelIndex) / double(m_texelIndices.Size() * m_params.config->numSamples) * 100.0;

    if (MathUtil::Abs(MathUtil::Floor(percentage) - MathUtil::Floor(m_lastLoggedPercentage)) >= 1)
    {
        HYP_LOG(Lightmap, Debug, "Lightmap job {}: Texel {} / {} ({}%)",
            m_uuid.ToString(), m_texelIndex, m_texelIndices.Size() * m_params.config->numSamples, percentage);

        m_lastLoggedPercentage = percentage;
    }

    World* world = GetScene()->GetWorld();
    Assert(world != nullptr);

    const uint32 rayOffset = uint32(m_texelIndex % (m_texelIndices.Size() * m_params.config->numSamples));

    const uint32 numRays = uint32(rays.Size());

    PUSH_RENDER_COMMAND(LightmapRender, this, MakeStrongRef(world), m_params.view, std::move(rays), rayOffset);

    return numRays;
}

#pragma endregion LightmapJobBase

#pragma region LightmapJob < LightmapVolume>

LightmapJob<LightmapVolume>::LightmapJob(LightmapJobParams&& params, const Handle<LightmapVolume>& volume, LightmapData<LightmapVolume>* lightmapData)
    : LightmapJobBase(std::move(params)),
      m_volume(volume),
      m_lightmapData(lightmapData),
      m_lightmapElement(nullptr)
{
    Assert(m_volume != nullptr);
    Assert(m_lightmapData != nullptr);
}

LightmapJob<LightmapVolume>::~LightmapJob()
{
    // m_lightmapElement is now managed externally or not used in the same way
}

void LightmapJob<LightmapVolume>::Start_Internal()
{
    // Data is already built globally
}

void LightmapJob<LightmapVolume>::Process_Internal(bool* outIsReadyToProcess)
{
    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

#pragma endregion LightmapJob < LightmapVolume>

#pragma region LightmapJob < EnvProbe>

LightmapJob<EnvProbe>::~LightmapJob()
{
}

void LightmapJob<EnvProbe>::Start_Internal()
{
    m_lightmapData = LightmapData<EnvProbe>(m_params.subElementsView, m_envProbe.Get());

    if (Result result = m_lightmapData.Build(); result.HasError())
    {
        Stop(result.GetError());
        return;
    }

    // Flatten texel indices for processing
    m_texelIndices.Reserve(m_lightmapData.texels.Size());
    for (uint32 i = 0; i < m_lightmapData.texels.Size(); i++)
    {
        m_texelIndices.PushBack(i);
    }
}

void LightmapJob<EnvProbe>::Process_Internal(bool* outIsReadyToProcess)
{
    if (outIsReadyToProcess)
    {
        *outIsReadyToProcess = true;
    }
}

#pragma endregion LightmapJob < EnvProbe>

} // namespace hyperion
