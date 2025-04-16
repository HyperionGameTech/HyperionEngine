/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_LIGHTMAPPER_HPP
#define HYPERION_LIGHTMAPPER_HPP

#include <core/Base.hpp>

#include <core/containers/Queue.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/UUID.hpp>
#include <core/utilities/Result.hpp>

#include <core/config/Config.hpp>

#include <core/math/Ray.hpp>

#include <scene/Scene.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

namespace hyperion {

static constexpr int max_bounces_cpu = 2;

struct LightmapHitsBuffer;
class LightmapTaskThreadPool;
class ILightmapAccelerationStructure;
class LightmapJob;
class LightmapVolume;

enum class LightmapTraceMode : int
{
    GPU_PATH_TRACING = 0,
    CPU_PATH_TRACING,

    MAX
};

enum class LightmapShadingType
{
    IRRADIANCE,
    RADIANCE,
    MAX
};

HYP_STRUCT(ConfigName="app", ConfigPath="lightmapper")
struct LightmapperConfig : public ConfigBase<LightmapperConfig>
{
    HYP_FIELD()
    LightmapTraceMode   trace_mode = LightmapTraceMode::GPU_PATH_TRACING;

    HYP_FIELD()
    bool                radiance = true;

    HYP_FIELD()
    bool                irradiance = true;

    HYP_FIELD()
    uint32              num_samples = 16;

    HYP_FIELD()
    uint32              max_rays_per_frame = 512 * 512;

    HYP_FIELD()
    uint32              ideal_triangles_per_job = 8192;

    virtual ~LightmapperConfig() override = default;

    HYP_API void PostLoadCallback();

    bool Validate()
    {
        bool valid = true;

        if (uint32(trace_mode) >= uint32(LightmapTraceMode::MAX)) {
            AddError(HYP_MAKE_ERROR(Error, "Invalid trace mode"));

            valid = false;
        }

        if (!radiance && !irradiance) {
            AddError(HYP_MAKE_ERROR(Error, "At least one of radiance or irradiance must be enabled"));

            valid = false;
        }

        if (num_samples == 0) {
            AddError(HYP_MAKE_ERROR(Error, "Number of samples must be greater than zero"));

            valid = false;
        }

        if (max_rays_per_frame == 0 || max_rays_per_frame > 1024 * 1024) {
            AddError(HYP_MAKE_ERROR(Error, "Max rays per frame must be greater than zero and less than or equal to 1024*1024"));

            valid = false;
        }

        if (ideal_triangles_per_job == 0 || ideal_triangles_per_job > 100000) {
            AddError(HYP_MAKE_ERROR(Error, "Ideal triangles per job must be greater than zero and less than or equal to 100000"));

            valid = false;
        }
        
        return valid;
    }
};

struct LightmapRay
{
    Ray         ray;
    ID<Mesh>    mesh_id;
    uint32      triangle_index;
    uint32      texel_index;

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

struct LightmapHit
{
    Vec4f   color;
};

static_assert(sizeof(LightmapHit) == 16);

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

class ILightmapRenderer
{
public:
    virtual ~ILightmapRenderer() = default;

    virtual uint32 MaxRaysPerFrame() const = 0;

    virtual LightmapShadingType GetShadingType() const = 0;

    virtual void Create() = 0;
    virtual void UpdateRays(Span<const LightmapRay> rays) = 0;
    virtual void ReadHitsBuffer(IFrame *frame, Span<LightmapHit> out_hits) = 0;
    virtual void Render(IFrame *frame, LightmapJob *job, Span<const LightmapRay> rays, uint32 ray_offset) = 0;
};

struct LightmapJobParams
{
    LightmapperConfig                               *config;

    Handle<Scene>                                   scene;
    Handle<LightmapVolume>                          volume;

    uint32                                          element_index; // corresponds to element index in the lightmap volume

    Span<LightmapSubElement>                        sub_elements_view;
    HashMap<Handle<Entity>, LightmapSubElement *>   *sub_elements_by_entity;

    LightmapTopLevelAccelerationStructure           *acceleration_structure;

    Array<ILightmapRenderer *>                      renderers;
};

class HYP_API LightmapJob
{
public:
    friend struct RenderCommand_LightmapRender;

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

    HYP_FORCE_INLINE Span<LightmapSubElement> GetSubElements() const
        { return m_params.sub_elements_view; }

    HYP_FORCE_INLINE uint32 GetTexelIndex() const
        { return m_texel_index; }

    HYP_FORCE_INLINE const Array<uint32> &GetTexelIndices() const
        { return m_texel_indices; }

    HYP_FORCE_INLINE void GetPreviousFrameRays(Array<LightmapRay> &out_rays) const
    {
        Mutex::Guard guard(m_previous_frame_rays_mutex);

        out_rays = m_previous_frame_rays;
    }
        
    HYP_FORCE_INLINE void SetPreviousFrameRays(const Array<LightmapRay> &rays)
    {
        Mutex::Guard guard(m_previous_frame_rays_mutex);

        m_previous_frame_rays = rays;
    }

    void Start();
    
    void Process();
    
    void GatherRays(uint32 max_ray_hits, Array<LightmapRay> &out_rays);
    
    void AddTask(Task<void> &&task);

    /*! \brief Integrate ray hits into the lightmap.
     *  \param rays The rays that were traced.
     *  \param hits The hits to integrate.
     */
    void IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shading_type);
    
    bool IsCompleted() const;

    HYP_FORCE_INLINE bool IsRunning() const
        { return m_running_semaphore.IsInSignalState(); }

private:
    void Stop();

    TResult<LightmapUVMap> BuildUVMap();
    
    LightmapJobParams                                       m_params;

    UUID                                                    m_uuid;

    Array<uint32>                                           m_texel_indices; // flattened texel indices, flattened so that meshes are grouped together

    Array<LightmapRay>                                      m_previous_frame_rays;
    mutable Mutex                                           m_previous_frame_rays_mutex;

    Optional<LightmapUVMap>                                 m_uv_map;
    Task<TResult<LightmapUVMap>>                             m_build_uv_map_task;

    Array<Task<void>>                                       m_current_tasks;
    mutable Mutex                                           m_current_tasks_mutex;

    Semaphore<int32>                                        m_running_semaphore;
    uint32                                                  m_texel_index;

    double                                                  m_last_logged_percentage;

    AtomicVar<uint32>                                       m_num_concurrent_rendering_tasks;
};

class HYP_API Lightmapper
{
public:
    Lightmapper(LightmapperConfig &&config, const Handle<Scene> &scene);
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
        uint32 element_index,
        SizeType start_index,
        SizeType end_index,
        LightmapTopLevelAccelerationStructure *acceleration_structure
    );

    void AddJob(UniquePtr<LightmapJob> &&job)
    {
        Mutex::Guard guard(m_queue_mutex);
        
        m_queue.Push(std::move(job));

        m_num_jobs.Increment(1, MemoryOrder::RELEASE);
    }

    void HandleCompletedJob(LightmapJob *job);

    LightmapperConfig                                   m_config;

    Handle<Scene>                                       m_scene;

    Handle<LightmapVolume>                              m_volume;

    Array<UniquePtr<ILightmapRenderer>>                 m_lightmap_renderers;

    UniquePtr<LightmapTopLevelAccelerationStructure>    m_acceleration_structure;

    Queue<UniquePtr<LightmapJob>>                       m_queue;
    Mutex                                               m_queue_mutex;
    AtomicVar<uint32>                                   m_num_jobs;

    Array<LightmapSubElement>                           m_sub_elements;
    HashMap<Handle<Entity>, LightmapSubElement *>       m_sub_elements_by_entity;
};

} // namespace hyperion

#endif