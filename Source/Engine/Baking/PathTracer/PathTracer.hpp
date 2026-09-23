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
#include <Core/Threading/Mutex.hpp>
#include <Core/Containers/Array.hpp>

namespace Hyperion {

struct RenderSetup;

class RenderProxyList;
struct GpuLightmapperReadyNotification;

namespace Baking {

class PathTracerBVH;

enum class PathTraceResult : uint8
{
    Dispatched = 0, //!< Read back pending
    Deferred,       //!< Can be retried
    Failed          //!< RIP
};

class PathTracerTLAS final
{
public:
    PathTracerTLAS() = default;

    PathTracerTLAS(const PathTracerTLAS& other) = delete;
    PathTracerTLAS& operator=(const PathTracerTLAS& other) = delete;

    PathTracerTLAS(PathTracerTLAS&& other) noexcept = delete;
    PathTracerTLAS& operator=(PathTracerTLAS&& other) noexcept = delete;

    ~PathTracerTLAS();

    HYP_FORCE_INLINE const TopLevelASRef& GetTLAS() const
    {
        return m_tlas;
    }

    bool IsCreated() const;

    bool Create(RenderProxyList& rpl);

private:
    // Only ever released through EnqueueDeletion, its destructor frees bindless storage on the render thread
    TopLevelASRef m_tlas;
};

class PathTracer final
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_bakerPool);

    static constexpr uint32 ComputeThreadGroupSize = 64;
    static constexpr uint32 MaxComputeRaysPerBatch = 64 * 1024;

    /*! \brief Traces rays against \p tlas with hardware ray tracing */
    PathTracer(
        BakerBase* baker,
        const Handle<Scene>& scene,
        PathTraceType shadingType,
        uint32 maxTexelsPerFrame,
        const SharedPtr<PathTracerTLAS>& tlas);

    /*! \brief Traces rays against \p computeBVH with a compute shader */
    PathTracer(
        BakerBase* baker,
        const Handle<Scene>& scene,
        PathTraceType shadingType,
        uint32 maxTexelsPerFrame,
        const SharedPtr<PathTracerBVH>& computeBVH);
    
    PathTracer(const PathTracer& other) = delete;
    PathTracer& operator=(const PathTracer& other) = delete;
    
    PathTracer(PathTracer&& other) noexcept = delete;
    PathTracer& operator=(PathTracer&& other) noexcept = delete;

    ~PathTracer();

    uint32 MaxTexelsPerFrame() const
    {
        return m_computeBVH ? MaxComputeRaysPerBatch : UINT32_MAX;
    }

    PathTraceType GetShadingType() const
    {
        return m_shadingType;
    }

    bool CanRender() const;

    void Create();
    void CleanJobData(BakeJobBase* job);
    void ReadHitsBuffer(Frame* frame, BakeJobBase* job, size_t count, Proc<void(Span<LightmapHit> hits)>&& callback);

    PathTraceResult Render(Frame* frame, const RenderSetup& renderSetup, BakeJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset);

private:
    struct JobData
    {
        GpuBufferRef cbuffers[NumFramesInFlight];
        GpuBufferRef raysBuffers[NumFramesInFlight];
        RWStructuredBuffer hitsBufferGpu;
        bool isCreated = false;
    };

    PathTracer(
        BakerBase* baker,
        const Handle<Scene>& scene,
        PathTraceType shadingType,
        uint32 maxTexelsPerFrame,
        const SharedPtr<PathTracerTLAS>& tlas,
        const SharedPtr<PathTracerBVH>& computeBVH);

    void UpdatePipelineState(Frame* frame, BakeJobBase* job);
    void CreateBuffers(BakeJobBase* job);
    void ProcessPendingJobDataCleanup();

    static void ReleaseJobData(JobData& jd);

    BakerBase* m_baker;

    Handle<Scene> m_scene;
    PathTraceType m_shadingType;
    uint32 m_maxTexelsPerFrame;

    // only touched on the render thread
    Map<BakeJobBase*, JobData> m_jobData;

    // completed jobs queued from the sim thread, erased from m_jobData on the render thread
    Mutex m_pendingCleanupMutex;
    Array<BakeJobBase*> m_pendingCleanupJobs;

    SharedPtr<GpuLightmapperReadyNotification> m_readyNotification;

    // Exactly one of these is set, shared with the baker's other PathTracers
    SharedPtr<PathTracerTLAS> m_tlas;
    SharedPtr<PathTracerBVH> m_computeBVH;
};

} // namespace Baking

} // namespace Hyperion
