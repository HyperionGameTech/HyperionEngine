/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAPPER_HPP
#define HYPERION_LIGHTMAPPER_HPP

#include <core/Base.hpp>

#include <core/containers/Queue.hpp>
#include <core/containers/LinkedList.hpp>
#include <core/containers/HeapArray.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/UUID.hpp>

#include <math/Triangle.hpp>
#include <math/Ray.hpp>

#include <scene/Scene.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

#include <rendering/RenderComponent.hpp>
#include <rendering/EnvProbe.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

static constexpr int max_bounces_cpu = 2;

struct LightmapHitsBuffer;

enum LightmapTraceMode
{
    LIGHTMAP_TRACE_MODE_GPU,
    LIGHTMAP_TRACE_MODE_CPU
};

enum LightmapShadingType
{
    LIGHTMAP_SHADING_TYPE_IRRADIANCE,
    LIGHTMAP_SHADING_TYPE_RADIANCE
};

struct LightmapRay
{
    Ray         ray;
    ID<Mesh>    mesh_id;
    uint        triangle_index;
    uint        texel_index;

    HYP_FORCE_INLINE bool operator==(const LightmapRay &other) const
    {
        return ray == other.ray
            && mesh_id == other.mesh_id
            && triangle_index == other.triangle_index
            && texel_index == other.texel_index;
    }

    HYP_FORCE_INLINE bool operator!=(const LightmapRay &other) const
        { return !(*this == other); }
};

constexpr uint max_ray_hits_gpu = 512 * 512;
constexpr uint max_ray_hits_cpu = 128 * 128;

struct alignas(16) LightmapHit
{
    Vec4f   color;
};

static_assert(sizeof(LightmapHit) == 16);

struct alignas(16) LightmapHitsBuffer
{
    FixedArray<LightmapHit, max_ray_hits_gpu>   hits;
};

static_assert(sizeof(LightmapHitsBuffer) == max_ray_hits_gpu * 16);

class LightmapTopLevelAccelerationStructure;

struct LightmapRayHitPayload
{
    Vec4f       throughput;
    Vec4f       emissive;
    Vec4f       radiance;
    Vec3f       normal;
    float       distance = -1.0f;
    Vec3f       barycentric_coords;
    ID<Mesh>    mesh_id;
    uint32      triangle_index = ~0u;
};

class HYP_API LightmapPathTracer
{
public:
    LightmapPathTracer(const TLASRef &tlas, LightmapShadingType shading_type);
    LightmapPathTracer(const LightmapPathTracer &other)                 = delete;
    LightmapPathTracer &operator=(const LightmapPathTracer &other)      = delete;
    LightmapPathTracer(LightmapPathTracer &&other) noexcept             = delete;
    LightmapPathTracer &operator=(LightmapPathTracer &&other) noexcept  = delete;
    ~LightmapPathTracer();

    HYP_FORCE_INLINE const RaytracingPipelineRef &GetPipeline() const
        { return m_raytracing_pipeline; }

    void Create();
    
    void ReadHitsBuffer(LightmapHitsBuffer *ptr, uint frame_index);
    void Trace(Frame *frame, const Array<LightmapRay> &rays, uint32 ray_offset);

private:
    void CreateUniformBuffer();
    void UpdateUniforms(Frame *frame, uint32 ray_offset);

    TLASRef                                             m_tlas;
    LightmapShadingType                                 m_shading_type;
    
    FixedArray<GPUBufferRef, max_frames_in_flight>      m_uniform_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>      m_rays_buffers;
    FixedArray<GPUBufferRef, max_frames_in_flight>      m_hits_buffers;
    HeapArray<LightmapHitsBuffer, max_frames_in_flight> m_previous_hits_buffers;
    RaytracingPipelineRef                               m_raytracing_pipeline;
};

struct LightmapJobGPUParams
{
    RC<LightmapPathTracer>  path_tracer_radiance;
    RC<LightmapPathTracer>  path_tracer_irradiance;
};

struct LightmapJobCPUParams
{
    UniquePtr<LightmapTopLevelAccelerationStructure>    acceleration_structure;
};

struct LightmapJobParams
{
    LightmapTraceMode                                   trace_mode;
    Handle<Scene>                                       scene;
    Span<LightmapElement>                               elements_view;
    HashMap<Handle<Entity>, LightmapElement *>          *all_elements_map;
    Variant<LightmapJobCPUParams, LightmapJobGPUParams> params;
};

class HYP_API LightmapJob
{
public:
    friend struct RenderCommand_LightmapTraceRaysOnGPU;

    static constexpr uint num_multisamples = 1;

    LightmapJob(LightmapJobParams &&params);
    LightmapJob(const LightmapJob &other)                   = delete;
    LightmapJob &operator=(const LightmapJob &other)        = delete;
    LightmapJob(LightmapJob &&other) noexcept               = delete;
    LightmapJob &operator=(LightmapJob &&other) noexcept    = delete;
    ~LightmapJob();

    HYP_FORCE_INLINE const LightmapJobParams &GetParams() const
        { return m_params; }

    HYP_FORCE_INLINE const UUID &GetUUID() const
        { return m_uuid; }

    HYP_FORCE_INLINE LightmapUVMap &GetUVMap()
        { return *m_uv_map; }

    HYP_FORCE_INLINE const LightmapUVMap &GetUVMap() const
        { return *m_uv_map; }

    HYP_FORCE_INLINE Scene *GetScene() const
        { return m_params.scene.Get(); }

    HYP_FORCE_INLINE Span<LightmapElement> GetElements() const
        { return m_params.elements_view; }

    HYP_FORCE_INLINE uint32 GetTexelIndex() const
        { return m_texel_index; }

    HYP_FORCE_INLINE const Array<uint> &GetTexelIndices() const
        { return m_texel_indices; }

    HYP_FORCE_INLINE void GetPreviousFrameRays(uint frame_index, Array<LightmapRay> &out_rays) const
    {
        Mutex::Guard guard(m_previous_frame_rays_mutex);

        out_rays = m_previous_frame_rays[frame_index];
    }
        
    HYP_FORCE_INLINE void SetPreviousFrameRays(uint frame_index, const Array<LightmapRay> &rays)
    {
        Mutex::Guard guard(m_previous_frame_rays_mutex);

        m_previous_frame_rays[frame_index] = rays;
    }

    void Start();

    void Process();

    void GatherRays(uint max_ray_hits, Array<LightmapRay> &out_rays);

    /*! \brief Integrate ray hits into the lightmap.
     *  \param rays The rays that were traced.
     *  \param hits The hits to integrate.
     *  \param num_hits The number of hits (must be the same as the number of rays).
     */
    void IntegrateRayHits(const LightmapRay *rays, const LightmapHit *hits, uint num_hits, LightmapShadingType shading_type);
    
    bool IsCompleted() const;

    HYP_FORCE_INLINE bool IsRunning() const
        { return m_running_semaphore.IsInSignalState(); }

private:
    /*! \brief Trace rays on the CPU.
     *  \param rays The rays to trace.    
     */
    void TraceRaysOnCPU(const Array<LightmapRay> &rays, LightmapShadingType shading_type);
    void TraceSingleRayOnCPU(const LightmapRay &ray, LightmapRayHitPayload &out_payload);

    Optional<LightmapUVMap> BuildUVMap();

    HYP_FORCE_INLINE LightmapTopLevelAccelerationStructure *GetAccelerationStructure() const
    {
        if (m_params.trace_mode == LIGHTMAP_TRACE_MODE_CPU) {
            return m_params.params.Get<LightmapJobCPUParams>().acceleration_structure.Get();
        }

        return nullptr;
    }
    
    LightmapJobParams                                       m_params;

    UUID                                                    m_uuid;

    Array<uint>                                             m_texel_indices; // flattened texel indices, flattened so that meshes are grouped together

    Array<LightmapRay>                                      m_current_rays;

    FixedArray<Array<LightmapRay>, max_frames_in_flight>    m_previous_frame_rays;
    mutable Mutex                                           m_previous_frame_rays_mutex;

    Optional<LightmapUVMap>                                 m_uv_map;
    Task<Optional<LightmapUVMap>>                           m_build_uv_map_task;
    Array<Task<void>>                                       m_current_tasks;

    Semaphore<int32>                                        m_running_semaphore;
    uint                                                    m_texel_index;
};

class HYP_API Lightmapper
{
public:
    Lightmapper(LightmapTraceMode trace_mode, const Handle<Scene> &scene);
    Lightmapper(const Lightmapper &other)                   = delete;
    Lightmapper &operator=(const Lightmapper &other)        = delete;
    Lightmapper(Lightmapper &&other) noexcept               = delete;
    Lightmapper &operator=(Lightmapper &&other) noexcept    = delete;
    ~Lightmapper();

    bool IsComplete() const;

    void PerformLightmapping();
    void Update(GameCounter::TickUnit delta);

    Delegate<void>  OnComplete;

private:
    LightmapJobParams CreateLightmapJobParams(
        SizeType start_index,
        SizeType end_index,
        UniquePtr<LightmapTopLevelAccelerationStructure> &&acceleration_structure = nullptr
    );

    void AddJob(UniquePtr<LightmapJob> &&job)
    {
        Mutex::Guard guard(m_queue_mutex);
        
        m_queue.Push(std::move(job));

        m_num_jobs.Increment(1, MemoryOrder::RELEASE);
    }

    void HandleCompletedJob(LightmapJob *job);

    LightmapTraceMode                           m_trace_mode;

    Handle<Scene>                               m_scene;

    RC<LightmapPathTracer>                      m_path_tracer_radiance;
    RC<LightmapPathTracer>                      m_path_tracer_irradiance;

    Queue<UniquePtr<LightmapJob>>               m_queue;
    Mutex                                       m_queue_mutex;
    AtomicVar<uint>                             m_num_jobs;

    Array<LightmapElement>                      m_lightmap_elements;
    HashMap<Handle<Entity>, LightmapElement *>  m_all_elements_map;
};

} // namespace hyperion

#endif