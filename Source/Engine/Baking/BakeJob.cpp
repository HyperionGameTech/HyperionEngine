/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <HyperionPch.hpp>

#include <Baking/BakeJob.hpp>
#include <Baking/Baker.hpp>

#include <Baking/Lightmaps/LightmapPathTraceGpu.hpp>

#include <Rendering/RenderInterface.hpp>
#include <Rendering/RenderHelpers.hpp>
#include <Rendering/RendererMain.hpp>
#include <Rendering/RenderTypes.hpp>
#include <Rendering/RenderConfig.hpp>
#include <Rendering/Device.hpp>
#include <Rendering/Frame.hpp>
#include <Rendering/RenderProxyList.hpp>
#include <Rendering/Mesh.hpp>
#include <Rendering/Material.hpp>
#include <Rendering/Texture.hpp>
#include <Rendering/Pass.hpp>

#include <Scene/World.hpp>
#include <Scene/View.hpp>
#include <Scene/EnvProbe.hpp>
#include <Scene/FogVolume.hpp>
#include <Scene/LightmapVolume.hpp>

#include <Scene/Util/VoxelOctree.hpp>

#include <Core/Debug/Debug.hpp>

#include <Core/Threading/TaskSystem.hpp>
#include <Core/Threading/TaskThread.hpp>

#include <Framework/EngineGlobals.hpp>
#include <Framework/EngineDriver.hpp>
#include <Rendering/DebugDrawer.hpp>

namespace Hyperion {

namespace Baking {

#pragma region Render command

struct TraceLightmapRaysPayload
{
    BakeJobBase* job;
    Handle<World> world;
    Handle<View> view;
    Array<LightmapRay> rays;
    uint32 rayOffset;
};

class TraceLightmapRaysCmd : public CmdBase
{
public:
    TraceLightmapRaysPayload* payload;

    explicit TraceLightmapRaysCmd(TraceLightmapRaysPayload* payload)
        : payload(payload)
    {
        payload->job->tracingCompleteSignal.Reset();
    }

    static void InvokeStatic(CmdBase* cmd, CommandBuffer* commandBuffer)
    {
        TraceLightmapRaysCmd* cmdCasted = static_cast<TraceLightmapRaysCmd*>(cmd);

        TraceLightmapRaysPayload& payload = *cmdCasted->payload;

        BakeJobBase* job = payload.job;
        Handle<World>& world = payload.world;
        Handle<View>& view = payload.view;
        Array<LightmapRay>& rays = payload.rays;
        uint32 rayOffset = payload.rayOffset;

        bool needsSignal = true;

        HYP_DEFER({ delete &payload; });

        HYP_DEFER({
            if (needsSignal)
            {
                job->tracingCompleteSignal.Signal(uint32(job->GetParams().renderers->Size()));
            }
        });

        Frame* frame = RI.GetCurrentFrame();

        RenderSetup renderSetup { world, view };

        RenderProxyList* rpl = nullptr;

        if (view)
        {
            rpl = &GetConsumerProxyList(view);
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

        if (rays.Any())
        {
            const size_t numRays = rays.Size();
            job->readbackData.SetSize(numRays * sizeof(LightmapHit) * job->GetParams().renderers->Size());

            size_t readbackDataOffset = 0;

            SharedPtr<Array<LightmapRay>> raysRc = MakeShared<Array<LightmapRay>>(std::move(rays));

            for (const UniquePtr<ILightmapRenderer>& lightmapRenderer : *job->GetParams().renderers)
            {
                AssertDebug(lightmapRenderer != nullptr);

                if (!lightmapRenderer->Render(frame, renderSetup, job, *raysRc, rayOffset))
                {
                    HYP_LOG(Lightmap, Error, "Failed to process lightmap!");

                    continue;
                }

                Proc<void(Span<LightmapHit>)> cb = [job, raysRc, readbackDataOffset, shadingType = lightmapRenderer->GetShadingType()](Span<LightmapHit> hits)
                {
                    Memory::Copy(job->readbackData.Data() + readbackDataOffset, hits.Data(), hits.Size() * sizeof(LightmapHit));

                    job->IntegrateRayHits(*raysRc, hits, shadingType);

                    job->tracingCompleteSignal.Signal();
                };

                lightmapRenderer->ReadHitsBuffer(frame, job, numRays, std::move(cb));
                needsSignal = false;

                readbackDataOffset += numRays * sizeof(LightmapHit);
            }
        }
    }
};

#pragma endregion Render command

#pragma region BakeJobBase

BakeJobBase::BakeJobBase(BakeJobParams&& params)
    : tracingCompleteSignal(0),
      m_baker(nullptr),
      m_params(std::move(params)),
      m_texelIndex(0),
      m_lastLoggedPercentage(0),
      m_wasStarted(false)
{
}

BakeJobBase::~BakeJobBase()
{
    if (m_wasStarted)
    {
        tracingCompleteSignal.Wait(int32(m_params.renderers->Size()));
    }

    for (TaskBatch* taskBatch : m_currentTasks)
    {
        taskBatch->AwaitCompletion();

        delete taskBatch;
    }
}

void BakeJobBase::Start()
{
    m_wasStarted = true;

    auto Functor = [this](bool)
    {
        Start_Internal();

        tracingCompleteSignal.Reset(int32(m_params.renderers->Size()));
    };

    m_runningSemaphore.Produce(1, Functor);
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
    TUniqueLock lock(m_currentTasksMutex);

    m_currentTasks.PushBack(taskBatch);
}

bool BakeJobBase::HasRemainingTexels() const
{
    return m_texelIndex < m_texelIndices.Size() * m_baker->NumTexelSamples();
}

void BakeJobBase::GatherTexels(uint32 maxTexels, Array<LightmapTexel*>& outTexels)
{
    const bool hasRays = m_baker->PerformsRayTracing();

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
    if (!m_baker->PerformsRayTracing())
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

    CommandRecorder& cr = RI.commandRecorderAllocator.GetCommandRecorder();

    TraceLightmapRaysPayload* payload = new TraceLightmapRaysPayload;
    payload->job = this;
    payload->world = MakeStrongRef(world);
    payload->view = m_params.view;
    payload->rays = std::move(rays);
    payload->rayOffset = texelOffset;

    cr << TraceLightmapRaysCmd(payload);

    cr.Done();

    return numTexels;
}

void BakeJobBase::IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType)
{
    Assert(rays.Size() == hits.Size());

    BakeDataBase& bakeData = GetBakeData();

    for (size_t i = 0; i < hits.Size(); i++)
    {
        const LightmapRay& ray = rays[i];
        const LightmapHit& hit = hits[i];

        LightmapTexel& texel = bakeData.texels[ray.texelIndex];

        switch (shadingType)
        {
        case LightmapShadingType::RADIANCE:
            texel.color1 += hit.color;
            break;
        default:
            texel.color0 += hit.color;
            break;
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

    const int32 expectedSignalValue = int32(m_params.renderers->Size());

    if (!tracingCompleteSignal.IsSignalled(expectedSignalValue))
    {
        // Wait for current rendering tasks to complete before enqueueing new ones.

        return 0;
    }

    {
        TSharedLock lock(m_currentTasksMutex);

        if (m_currentTasks.Any())
        {
            for (size_t taskIndex = 0; taskIndex < m_currentTasks.Size(); taskIndex++)
            {
                TaskBatch* taskBatch = m_currentTasks[taskIndex];

                if (!taskBatch->IsCompleted())
                {
                    // Skip this call

                    return 0;
                }
            }

            lock.Reset();

            TUniqueLock uniqueLock(m_currentTasksMutex);

            for (size_t taskIndex = 0; taskIndex < m_currentTasks.Size(); taskIndex++)
            {
                TaskBatch* taskBatch = m_currentTasks[taskIndex];

                taskBatch->AwaitCompletion();

                delete taskBatch;
            }

            m_currentTasks.Clear();
        }
    }

    if (m_texelIndex >= m_texelIndices.Size() * m_baker->NumTexelSamples()
        && tracingCompleteSignal.IsSignalled(expectedSignalValue))
    {
        HYP_LOG(Lightmap, Verbose, "Lightmap job {}: All texels processed ({} / {}), stopping", m_uuid, m_texelIndex, m_texelIndices.Size() * m_baker->NumTexelSamples());

        Stop();

        return 0;
    }

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

    const uint32 totalNumTexels = uint32(m_texelIndices.Size()) * m_baker->NumTexelSamples();
    AssertDebug(totalNumTexels > 0);

    maxTexels = MathUtil::Min(maxTexels, totalNumTexels);
    maxTexels = MathUtil::Min(maxTexels, m_baker->MaxTexelsPerFrame());

    if (m_baker->PerformsRayTracing())
    {
        if (m_params.renderers->Empty())
        {
            return 0;
        }

        maxTexels = MathUtil::Min(maxTexels, (*m_params.renderers)[0]->MaxTexelsPerFrame());
    }

    if (totalNumTexels == 0 || maxTexels == 0)
    {
        return 0;
    }

    const uint32 texelOffset = uint32(m_texelIndex % totalNumTexels);

    Array<LightmapTexel*> texels;
    texels.Reserve(maxTexels);

    GatherTexels(maxTexels, texels);
    AssertDebug(texels.Size() <= maxTexels);

    return ProcessTexels(texels, texelOffset);
}

#pragma endregion BakeJobBase

} // namespace Baking

} // namespace Hyperion
