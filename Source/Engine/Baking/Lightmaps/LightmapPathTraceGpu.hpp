/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Baking/Baker.hpp>
#include <Baking/BakerMemory.hpp>

#include <Rendering/RenderTypes.hpp>
#include <Rendering/RawBuffer.hpp>

#include <Core/Memory/SharedPtr.hpp>

namespace Hyperion {

struct RenderSetup;

class RenderProxyList;
struct GpuLightmapperReadyNotification;

namespace Baking {

enum class PathTracerRenderResult : uint8
{
    /*! Rays were dispatched; a readback callback is pending and will signal the job. */
    Dispatched = 0,

    /*! Nothing was dispatched, but the same batch can be traced again on a later frame. */
    Deferred,

    /*! Nothing was dispatched and retrying will not help. */
    Failed
};

class PathTracer final
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_bakerPool);

    PathTracer(
        BakerBase* baker,
        const Handle<Scene>& scene,
        LightmapShadingType shadingType,
        uint32 maxTexelsPerFrame);
    
    PathTracer(const PathTracer& other) = delete;
    PathTracer& operator=(const PathTracer& other) = delete;
    
    PathTracer(PathTracer&& other) noexcept = delete;
    PathTracer& operator=(PathTracer&& other) noexcept = delete;

    ~PathTracer();

    uint32 MaxTexelsPerFrame() const
    {
        return UINT32_MAX;
    }

    LightmapShadingType GetShadingType() const
    {
        return m_shadingType;
    }

    bool CanRender() const;

    void Create();
    void CleanJobData(BakeJobBase* job);
    void ReadHitsBuffer(Frame* frame, BakeJobBase* job, size_t count, Proc<void(Span<LightmapHit> hits)>&& callback);

    PathTracerRenderResult Render(Frame* frame, const RenderSetup& renderSetup, BakeJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset);

private:
    struct JobData
    {
        GpuBufferRef cbuffers[NumFramesInFlight];
        GpuBufferRef raysBuffers[NumFramesInFlight];
        RWStructuredBuffer hitsBufferGpu;
        bool isCreated = false;
    };

    void UpdatePipelineState(Frame* frame, BakeJobBase* job);
    void CreateBuffers(BakeJobBase* job);

    /*! \brief Build the acceleration structures if they don't exist yet.
     *  \return true if they were built by this call, false if they already existed. */
    bool CreateAccelerationStructures();

    BakerBase* m_baker;

    Handle<Scene> m_scene;
    LightmapShadingType m_shadingType;
    uint32 m_maxTexelsPerFrame;

    Map<BakeJobBase*, JobData> m_jobData;

    SharedPtr<GpuLightmapperReadyNotification> m_readyNotification;

    TopLevelASRef m_tlas;
};

} // namespace Baking

} // namespace Hyperion
