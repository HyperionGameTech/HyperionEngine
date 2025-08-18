/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Queue.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/object/HypObject.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Uuid.hpp>
#include <core/utilities/Result.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/config/Config.hpp>

#include <scene/Scene.hpp>

#include <rendering/lightmapper/LightmapUVBuilder.hpp>

namespace hyperion {

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

struct LightmapHitsBuffer;
class LightmapThreadPool;
class ILightmapAccelerationStructure;
class LightmapJob;
class LightmapVolume;
class Lightmapper;
struct LightmapElement;
class AssetObject;

class View;
struct RenderSetup;

HYP_ENUM()
enum class LightmapTraceMode : int
{
    GPU_PATH_TRACING = 0,
    CPU_PATH_TRACING,
    ENV_GRID, // unused

    MAX
};

HYP_ENUM()
enum class LightmapShadingType : int
{
    IRRADIANCE = 0,
    RADIANCE,
    MAX
};

HYP_STRUCT(ConfigName = "GlobalConfig", JsonPath = "lightmapper")
struct LightmapperConfig : public ConfigBase<LightmapperConfig>
{
    HYP_FIELD()
    LightmapTraceMode traceMode = LightmapTraceMode::GPU_PATH_TRACING;

    HYP_FIELD()
    bool radiance = true;

    HYP_FIELD()
    bool irradiance = true;

    HYP_FIELD()
    uint32 numSamples = 16;

    HYP_FIELD()
    uint32 maxRaysPerFrame = 512 * 512;

    HYP_FIELD()
    uint32 idealTrianglesPerJob = 8192;

    virtual ~LightmapperConfig() override = default;

    HYP_API void PostLoadCallback();

    bool Validate()
    {
        bool valid = true;

        if (uint32(traceMode) >= uint32(LightmapTraceMode::MAX))
        {
            AddError(HYP_MAKE_ERROR(Error, "Invalid trace mode"));

            valid = false;
        }

        if (!radiance && !irradiance)
        {
            AddError(HYP_MAKE_ERROR(Error, "At least one of radiance or irradiance must be enabled"));

            valid = false;
        }

        if (numSamples == 0)
        {
            AddError(HYP_MAKE_ERROR(Error, "Number of samples must be greater than zero"));

            valid = false;
        }

        if (maxRaysPerFrame == 0 || maxRaysPerFrame > 1024 * 1024)
        {
            AddError(HYP_MAKE_ERROR(Error, "Max rays per frame must be greater than zero and less than or equal to 1024*1024"));

            valid = false;
        }

        if (idealTrianglesPerJob == 0 || idealTrianglesPerJob > 100000)
        {
            AddError(HYP_MAKE_ERROR(Error, "Ideal triangles per job must be greater than zero and less than or equal to 100000"));

            valid = false;
        }

        return valid;
    }
};

struct LightmapHit
{
    Vec3f color;
};

static_assert(sizeof(LightmapHit) == 16);

class LightmapTopLevelAccelerationStructure;

struct LightmapRayHitPayload
{
    Vec3f albedo;
    Vec3f emissive;
    Vec3f radiance;
    Vec3f normal;
    float distance = -1.0f;
    Vec3f barycentricCoords;
    ObjId<Mesh> meshId;
    uint32 triangleIndex = ~0u;
};

class ILightmapRenderer
{
protected:
    ILightmapRenderer(Lightmapper* lightmapper)
        : m_lightmapper(lightmapper)
    {
        AssertDebug(lightmapper != nullptr);
    }

public:
    friend class Lightmapper;

    virtual ~ILightmapRenderer() = default;

    virtual uint32 MaxRaysPerFrame() const = 0;

    virtual LightmapShadingType GetShadingType() const = 0;

    virtual bool CanRender() const
    {
        return true;
    }

    virtual void Create() = 0;
    virtual void PrepareJob(LightmapJob* job) { };
    virtual void UpdateRays(Span<const LightmapRay> rays) = 0;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits) = 0;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJob* job, Span<const LightmapRay> rays, uint32 rayOffset) = 0;

protected:
    Lightmapper* m_lightmapper;
};

struct LightmapJobParams
{
    LightmapperConfig* config;

    Handle<Scene> scene;
    Handle<LightmapVolume> volume;

    Handle<View> view;

    Span<LightmapSubElement> subElementsView;
    HashMap<Handle<Entity>, LightmapSubElement*>* subElementsByEntity;

    Array<UniquePtr<ILightmapRenderer>>* renderers = nullptr;
};

class HYP_API LightmapJob
{
public:
    LightmapJob(LightmapJobParams&& params);
    LightmapJob(const LightmapJob& other) = delete;
    LightmapJob& operator=(const LightmapJob& other) = delete;
    LightmapJob(LightmapJob&& other) noexcept = delete;
    LightmapJob& operator=(LightmapJob&& other) noexcept = delete;
    virtual ~LightmapJob();

    HYP_FORCE_INLINE const LightmapJobParams& GetParams() const
    {
        return m_params;
    }

    HYP_FORCE_INLINE const UUID& GetUUID() const
    {
        return m_uuid;
    }

    HYP_FORCE_INLINE LightmapUVBuilder& GetUVBuilder()
    {
        return m_uvBuilder;
    }

    HYP_FORCE_INLINE const LightmapUVBuilder& GetUVBuilder() const
    {
        return m_uvBuilder;
    }

    HYP_FORCE_INLINE LightmapUVMap& GetUVMap()
    {
        Assert(m_uvMap.HasValue());
        return *m_uvMap;
    }

    HYP_FORCE_INLINE const LightmapUVMap& GetUVMap() const
    {
        Assert(m_uvMap.HasValue());
        return *m_uvMap;
    }

    HYP_FORCE_INLINE LightmapElement* GetLightmapElement() const
    {
        return m_lightmapElement;
    }

    HYP_FORCE_INLINE Scene* GetScene() const
    {
        return m_params.scene.Get();
    }

    HYP_FORCE_INLINE Span<LightmapSubElement> GetSubElements() const
    {
        return m_params.subElementsView;
    }

    HYP_FORCE_INLINE uint32 GetTexelIndex() const
    {
        return m_texelIndex;
    }

    HYP_FORCE_INLINE const Array<uint32>& GetTexelIndices() const
    {
        return m_texelIndices;
    }

    HYP_FORCE_INLINE void GetPreviousFrameRays(Array<LightmapRay>& outRays) const
    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        outRays = m_previousFrameRays;
    }

    HYP_FORCE_INLINE void SetPreviousFrameRays(const Array<LightmapRay>& rays)
    {
        Mutex::Guard guard(m_previousFrameRaysMutex);

        m_previousFrameRays = rays;
    }

    HYP_FORCE_INLINE const Result& GetResult() const
    {
        return m_result;
    }

    void Start();
    void Process();

    virtual void GatherRays(uint32 maxRayHits, Array<LightmapRay>& outRays) = 0;

    void AddTask(TaskBatch* taskBatch);

    /*! \brief Integrate ray hits into the lightmap.
     *  \param rays The rays that were traced.
     *  \param hits The hits to integrate.
     */
    virtual void IntegrateRayHits(Span<const LightmapRay> rays, Span<const LightmapHit> hits, LightmapShadingType shadingType) = 0;

    bool IsCompleted() const;

    HYP_FORCE_INLINE bool IsRunning() const
    {
        return m_runningSemaphore.IsInSignalState();
    }

    // Number of GPU path tracing tasks running, used to not overwhelm the gpu while rendering the frame
    AtomicVar<uint32> numConcurrentRenderingTasks;

protected:
    HYP_FORCE_INLINE bool HasRemainingTexels() const
    {
        return m_texelIndex < m_texelIndices.Size() * m_params.config->numSamples;
    }

    /*! \brief Get the next texel index to process, advancing the teexl counter
     *  \return The texel index
     */
    HYP_FORCE_INLINE uint32 NextTexel()
    {
        const uint32 currentTexelIndex = m_texelIndices[m_texelIndex % m_texelIndices.Size()];
        m_texelIndex++;

        return currentTexelIndex;
    }

    void Stop();
    void Stop(const Error& error);

    LightmapJobParams m_params;

    UUID m_uuid;

    Array<uint32> m_texelIndices; // flattened texel indices, flattened so that meshes are grouped together

    Array<LightmapRay> m_previousFrameRays;
    mutable Mutex m_previousFrameRaysMutex;

    LightmapUVBuilder m_uvBuilder;
    Optional<LightmapUVMap> m_uvMap;
    Task<TResult<LightmapUVMap>> m_buildUvMapTask;

    LightmapElement* m_lightmapElement;

    Array<TaskBatch*> m_currentTasks;
    mutable Mutex m_currentTasksMutex;

    Semaphore<int32> m_runningSemaphore;
    uint32 m_texelIndex;

    double m_lastLoggedPercentage;

    Result m_result;
};

HYP_CLASS(Abstract)
class HYP_API Lightmapper : public HypObjectBase
{
    HYP_OBJECT_BODY(Lightmapper);

public:
    Lightmapper(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb);
    Lightmapper(const Lightmapper& other) = delete;
    Lightmapper& operator=(const Lightmapper& other) = delete;
    Lightmapper(Lightmapper&& other) noexcept = delete;
    Lightmapper& operator=(Lightmapper&& other) noexcept = delete;
    virtual ~Lightmapper();

    HYP_FORCE_INLINE const LightmapperConfig& GetConfig() const
    {
        return m_config;
    }

    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_FORCE_INLINE const Handle<Scene>& GetScene() const
    {
        return m_scene;
    }

    HYP_FORCE_INLINE const Handle<View>& GetView() const
    {
        return m_view;
    }

    bool IsComplete() const;

    void Initialize();
    void Update(float delta);

    Delegate<void> OnComplete;

protected:
    virtual void Initialize_Internal()
    {
    }
    virtual void Build_Internal()
    {
    }

    virtual UniquePtr<LightmapJob> CreateJob(LightmapJobParams&& params) = 0;
    virtual UniquePtr<ILightmapRenderer> CreateRenderer(LightmapShadingType shadingType) = 0;

    LightmapperConfig m_config;

    Handle<Scene> m_scene;
    BoundingBox m_aabb;

    Handle<View> m_view;

    Handle<LightmapVolume> m_volume;

    Array<LightmapSubElement> m_subElements;
    HashMap<Handle<Entity>, LightmapSubElement*> m_subElementsByEntity;

private:
    void Build();

    LightmapJobParams CreateLightmapJobParams(SizeType startIndex, SizeType endIndex);

    void AddJob(UniquePtr<LightmapJob>&& job)
    {
        Mutex::Guard guard(m_queueMutex);

        m_queue.Push(std::move(job));

        m_numJobs.Increment(1, MemoryOrder::RELEASE);
    }

    void HandleCompletedJob(LightmapJob* job);

    Array<UniquePtr<ILightmapRenderer>> m_lightmapRenderers;

    Queue<UniquePtr<LightmapJob>> m_queue;
    Mutex m_queueMutex;
    AtomicVar<uint32> m_numJobs;
};

} // namespace hyperion
