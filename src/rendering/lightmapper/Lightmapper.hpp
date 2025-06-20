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

#include <scene/Scene.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

namespace hyperion {

static constexpr int max_bounces_cpu = 2;

namespace threading {
struct TaskBatch;
} // namespace threading

using threading::TaskBatch;

struct LightmapHitsBuffer;
class LightmapThreadPool;
class ILightmapAccelerationStructure;
class LightmapJob;
class LightmapVolume;

class View;
struct RenderSetup;

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

HYP_STRUCT(ConfigName = "app", JSONPath = "lightmapper")
struct LightmapperConfig : public ConfigBase<LightmapperConfig>
{
    HYP_FIELD(JSONPath = "trace_mode")
    LightmapTraceMode trace_mode = LightmapTraceMode::GPU_PATH_TRACING;

    HYP_FIELD(JSONPath = "radiance")
    bool radiance = true;

    HYP_FIELD(JSONPath = "irradiance")
    bool irradiance = true;

    HYP_FIELD(JSONPath = "num_samples")
    uint32 num_samples = 16;

    HYP_FIELD(JSONPath = "max_rays_per_frame")
    uint32 max_rays_per_frame = 512 * 512;

    HYP_FIELD(JSONPath = "ideal_triangles_per_job")
    uint32 ideal_triangles_per_job = 8192;

    virtual ~LightmapperConfig() override = default;

    HYP_API void PostLoadCallback();

    bool Validate()
    {
        bool valid = true;

        if (uint32(trace_mode) >= uint32(LightmapTraceMode::MAX))
        {
            AddError(HYP_MAKE_ERROR(Error, "Invalid trace mode"));

            valid = false;
        }

        if (!radiance && !irradiance)
        {
            AddError(HYP_MAKE_ERROR(Error, "At least one of radiance or irradiance must be enabled"));

            valid = false;
        }

        if (num_samples == 0)
        {
            AddError(HYP_MAKE_ERROR(Error, "Number of samples must be greater than zero"));

            valid = false;
        }

        if (max_rays_per_frame == 0 || max_rays_per_frame > 1024 * 1024)
        {
            AddError(HYP_MAKE_ERROR(Error, "Max rays per frame must be greater than zero and less than or equal to 1024*1024"));

            valid = false;
        }

        if (ideal_triangles_per_job == 0 || ideal_triangles_per_job > 100000)
        {
            AddError(HYP_MAKE_ERROR(Error, "Ideal triangles per job must be greater than zero and less than or equal to 100000"));

            valid = false;
        }

        return valid;
    }
};

struct LightmapHit
{
    Vec4f color;
};

static_assert(sizeof(LightmapHit) == 16);

class LightmapTopLevelAccelerationStructure;

struct LightmapRayHitPayload
{
    Vec4f throughput;
    Vec4f emissive;
    Vec4f radiance;
    Vec3f normal;
    float distance = -1.0f;
    Vec3f barycentric_coords;
    ID<Mesh> mesh_id;
    uint32 triangle_index = ~0u;
};

class ILightmapRenderer
{
public:
    virtual ~ILightmapRenderer() = default;

    virtual uint32 MaxRaysPerFrame() const = 0;

    virtual LightmapShadingType GetShadingType() const = 0;

    virtual void Create() = 0;
    virtual void UpdateRays(Span<const LightmapRay> rays) = 0;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> out_hits) = 0;
    virtual void Render(FrameBase* frame, const RenderSetup& render_setup, LightmapJob* job, Span<const LightmapRay> rays, uint32 ray_offset) = 0;
};

struct LightmapJobParams
{
    LightmapperConfig* config;

    Handle<Scene> scene;
    Handle<LightmapVolume> volume;

    Span<LightmapSubElement> sub_elements_view;
    HashMap<Handle<Entity>, LightmapSubElement*>* sub_elements_by_entity;

    LightmapTopLevelAccelerationStructure* acceleration_structure;

    Array<ILightmapRenderer*> renderers;
};

class HYP_API LightmapJob
{
public:
    friend struct RenderCommand_LightmapRender;

    LightmapJob(LightmapJobParams&& params);
    LightmapJob(const LightmapJob& other) = delete;
    LightmapJob& operator=(const LightmapJob& other) = delete;
    LightmapJob(LightmapJob&& other) noexcept = delete;
    LightmapJob& operator=(LightmapJob&& other) noexcept = delete;
    ~LightmapJob();

    HYP_FORCE_INLINE const LightmapJobParams& GetParams() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    HYP_FORCE_INLINE LightmapUVBuilder& GetUVBuilder()
    {
        return m_uv_builder;
    }

    HYP_FORCE_INLINE const LightmapUVBuilder& GetUVBuilder() const
    {
        return m_uv_builder;
    }

    HYP_FORCE_INLINE LightmapUVMap& GetUVMap()
    {
        return *m_uv_map;
    }

    HYP_FORCE_INLINE const LightmapUVMap& GetUVMap() const
    {
        return *m_uv_map;
    }

    HYP_FORCE_INLINE uint32 GetElementIndex() const
    {
        return m_element_index;
    }

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_params.scene.Get();
    }

    HYP_FORCE_INLINE Span<LightmapSubElement> GetSubElements() const
    {
        return m_params.sub_elements_view;
    }

    HYP_FORCE_INLINE uint32 GetTexelIndex() const
    {
        return m_texel_index;
    }

    HYP_FORCE_INLINE const Array<uint32>& GetTexelIndices() const
    {
        return m_texel_indices;
    }

    HYP_FORCE_INLINE void GetPreviousFrameRays(Array<LightmapRay>& out_rays) const
    {
        Mutex::Guard guard(m_previous_frame_rays_mutex);

        out_rays = m_previous_frame_rays;
    }

    HYP_FORCE_INLINE void SetPreviousFrameRays(const Array<LightmapRay>& rays)
    {
        Mutex::Guard guard(m_previous_frame_rays_mutex);

        m_previous_frame_rays = rays;
    }

    HYP_FORCE_INLINE const Result& GetResult() const
    {
        return m_result;
    }

    void Start();
    void Process();

    void GatherRays(uint32 max_ray_hits, Array<LightmapRay>& out_rays);

    void AddTask(TaskBatch* task_batch);

    /*! \brief Integrate ray hits into the lightmap.
     *  \param rays The rays that were traced.
     *  \param hits The hits to integrate.
     */
    void IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shading_type);

    bool IsCompleted() const;

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_running_semaphore.IsInSignalState();
    }

private:
    HYP_FORCE_INLINE bool HasRemainingTexels() const
    {
        return m_texel_index < m_texel_indices.Size() * m_params.config->num_samples;
    }

    /*! \brief Get the next texel index to process, advancing the teexl counter
     *  \return The texel index
     */
    HYP_FORCE_INLINE uint32 NextTexel()
    {
        const uint32 current_texel_index = m_texel_indices[m_texel_index % m_texel_indices.Size()];
        m_texel_index++;

        return current_texel_index;
    }

    void Stop();
    void Stop(const Error& error);

    LightmapJobParams m_params;

    UUID m_uuid;

    Handle<View> m_view;

    Array<uint32> m_texel_indices; // flattened texel indices, flattened so that meshes are grouped together

    Array<LightmapRay> m_previous_frame_rays;
    mutable Mutex m_previous_frame_rays_mutex;

    LightmapUVBuilder m_uv_builder;
    Optional<LightmapUVMap> m_uv_map;
    Task<TResult<LightmapUVMap>> m_build_uv_map_task;

    uint32 m_element_index;

    Array<TaskBatch*> m_current_tasks;
    mutable Mutex m_current_tasks_mutex;

    Semaphore<int32> m_running_semaphore;
    uint32 m_texel_index;

    double m_last_logged_percentage;

    AtomicVar<uint32> m_num_concurrent_rendering_tasks;

    Array<ResourceHandle> m_resource_cache;

    Result m_result;
};

class HYP_API Lightmapper
{
public:
    Lightmapper(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb);
    Lightmapper(const Lightmapper& other) = delete;
    Lightmapper& operator=(const Lightmapper& other) = delete;
    Lightmapper(Lightmapper&& other) noexcept = delete;
    Lightmapper& operator=(Lightmapper&& other) noexcept = delete;
    ~Lightmapper();

    bool IsComplete() const;

    void PerformLightmapping();
    void Update(float delta);

    Delegate<void> OnComplete;

private:
    LightmapJobParams CreateLightmapJobParams(
        SizeType start_index,
        SizeType end_index,
        LightmapTopLevelAccelerationStructure* acceleration_structure);

    void AddJob(UniquePtr<LightmapJob>&& job)
    {
        Mutex::Guard guard(m_queue_mutex);

        m_queue.Push(std::move(job));

        m_num_jobs.Increment(1, MemoryOrder::RELEASE);
    }

    void HandleCompletedJob(LightmapJob* job);

    LightmapperConfig m_config;

    Handle<Scene> m_scene;
    BoundingBox m_aabb;

    Handle<LightmapVolume> m_volume;

    Array<UniquePtr<ILightmapRenderer>> m_lightmap_renderers;

    UniquePtr<LightmapTopLevelAccelerationStructure> m_acceleration_structure;

    Queue<UniquePtr<LightmapJob>> m_queue;
    Mutex m_queue_mutex;
    AtomicVar<uint32> m_num_jobs;

    Array<LightmapSubElement> m_sub_elements;
    HashMap<Handle<Entity>, LightmapSubElement*> m_sub_elements_by_entity;
};

} // namespace hyperion

#endif