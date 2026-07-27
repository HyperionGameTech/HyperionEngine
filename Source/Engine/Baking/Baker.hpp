/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Baking/BakerMemory.hpp>

#include <Core/Containers/Queue.hpp>

#include <Core/Threading/Mutex.hpp>
#include <Core/Threading/AtomicVar.hpp>
#include <Core/Threading/Task.hpp>
#include <Core/Threading/Semaphore.hpp>
#include <Core/Threading/ThreadSignal.hpp>

#include <Core/Reflection/ObjectBase.hpp>
#include <Core/Reflection/Handle.hpp>

#include <Core/Utilities/Span.hpp>
#include <Core/Utilities/Result.hpp>

#include <Core/Math/BoundingBox.hpp>

#include <Core/Profiling/PerformanceClock.hpp>

#include <Core/Config/Config.hpp>

#include <Scene/Scene.hpp>

#include <Core/Utilities/ClockTimer.hpp>

namespace Hyperion {

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

HYP_ENUM()
enum class BakerState : uint8
{
    Initialized = 0,
    Building,
    Running,
    Complete
};

class LightmapVolume;
struct LightmapElement;

class View;

namespace Baking {

class BakerBase;
class BakeDataBase;
class BakeJobBase;
class BakerThreadPool;
struct BakeJobParams;
struct BakeEntity;

struct LightmapRay;

class PathTracer;

HYP_ENUM()
enum class LightmapShadingType : uint32
{
    IRRADIANCE = 0, // Bake irradiance only
    FULL,           // Full scene bake
    SHADOW,         // Bake static shadow map for a light (ray-traced)
    DISTANCE,       // Bake ray hit distance (for variance shadow maps / visibility)
    BENT_NORMAL,    // Bake bent normal 
    MAX
};

HYP_STRUCT(ConfigName = "EngineConfig", JsonPath = "Baker")
struct BakerConfig : public Config<BakerConfig>
{
    HYP_STRUCT_BODY(BakerConfig);

    HYP_FIELD()
    uint32 numSamples = 16;

    HYP_FIELD(Description = "Number of samples to use when baking bent normals - typically needs fewer samples than irradiance to converge.")
    uint32 bentNormalSamples = 8;

    HYP_FIELD()
    uint32 maxTexelsPerFrame = 512 * 512;

    HYP_FIELD(Property = "OnlyGenerateUVs", Description = "Skip tracing rays when baking lightmap volumes, only generate atlas UVs and assign to meshes.")
    bool onlyGenerateUVs = false;

    virtual ~BakerConfig() override = default;

    bool Validate()
    {
        bool valid = true;

        if (numSamples == 0)
        {
            AddError(HYP_MAKE_ERROR(Error, "Number of samples must be greater than zero"));

            valid = false;
        }

        if (bentNormalSamples == 0)
        {
            AddError(HYP_MAKE_ERROR(Error, "Number of bent normal samples must be greater than zero"));

            valid = false;
        }

        if (maxTexelsPerFrame == 0)
        {
            AddError(HYP_MAKE_ERROR(Error, "Max texels per frame must be greater than zero"));

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

HYP_CLASS(Abstract)
class ENGINE_API BakerBase : public ObjectBase
{
    HYP_OBJECT_BODY(BakerBase);

public:
    BakerBase(BakerConfig&& config, ObjectBase* source, const Handle<Scene>& scene, const BoundingBox& aabb);

    BakerBase(const BakerBase& other) = delete;
    BakerBase& operator=(const BakerBase& other) = delete;

    BakerBase(BakerBase&& other) noexcept = delete;
    BakerBase& operator=(BakerBase&& other) noexcept = delete;

    virtual ~BakerBase() override;

    HYP_FORCE_INLINE const BakerConfig& GetConfig() const
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

    /*! \brief Get the source object that is being baked. This could be a LightmapVolume, ReflectionProbe, etc */
    HYP_FORCE_INLINE ObjectBase* GetSource() const
    {
        return m_source;
    }

    /*! \brief Should the lightmapping process be split into multiple independent jobs? (i.e splitting scene into multiple lightmap atlases) */
    virtual bool ShouldSplitIntoJobs() const
    {
        return false;
    }

    /*! \brief Get the bitmask of shading types this Lightmapper instance should bake for. */
    virtual uint32 GetShadingTypesMask() const
    {
        return 1u << int(LightmapShadingType::FULL);
    }

    /*! \brief Restrict this bake to a specific subset of shading types instead of the baker's default mask
     *  (e.g. baking bent normals only, without recomputing irradiance/radiance). A value of 0 means "use the default". */
    HYP_FORCE_INLINE void SetShadingTypesMaskOverride(uint32 shadingTypesMaskOverride)
    {
        m_shadingTypesMaskOverride = shadingTypesMaskOverride;
    }

    /*! \brief Should we consider only elements overlapping our AABB for lightmapping? */
    virtual bool OnlyOverlappingElements() const
    {
        return true;
    }

    virtual bool PerformsRayTracing() const
    {
        return true;
    }

    /*! \brief Should the build phase be executed asynchronously on a background thread? */
    virtual bool IsBuildAsync() const
    {
        return false;
    }

    /*! \brief Check if an async build has completed. Called from the sim thread during Update(). */
    virtual bool PollBuildReady()
    {
        return true;
    }

    /*! \brief Called on the sim thread when an async build has completed.
     *  Subclasses should move results and dispatch jobs here. */
    virtual void OnBuildReady()
    {
    }

    virtual uint32 NumThreads() const
    {
        return 0; // no thread pool by default
    }

    virtual uint32 NumTexelSamples() const;
    virtual uint32 MaxTexelsPerFrame() const;

    virtual const TypeInfo& GetInnerType() const = 0;

    HYP_FORCE_INLINE BakerState GetState() const
    {
        return m_state;
    }

    bool IsComplete() const
    {
        return m_state == BakerState::Complete;
    }

    void Initialize();
    void Shutdown();

    void Update(float delta);

    void HandleCompletedJob(BakeJobBase* job);

    Delegate<void> OnComplete;

protected:
    virtual void Initialize_Internal()
    {
    }

    virtual Result Build_Internal()
    {
        return {};
    }

    virtual void HandleCompletedJob_Internal(BakeJobBase* job)
    {
    }

    virtual void OnCompleted_Internal()
    {
    }

    virtual BakeDataBase& GetBakeData() = 0;

    HYP_FORCE_INLINE const BakeDataBase& GetBakeData() const
    {
        return const_cast<BakerBase*>(this)->GetBakeData();
    }

    virtual UniquePtr<BakeJobBase> CreateJob(BakeJobParams&& params) = 0;
    virtual UniquePtr<PathTracer> CreatePathTracer(LightmapShadingType shadingType, uint32 maxTexelsPerFrame);

    virtual void CreateLightmapRenderers();

    BakerConfig m_config;

    uint32 m_shadingTypesMaskOverride = 0;

    ObjectBase* m_source;

    Handle<Scene> m_scene;
    BoundingBox m_aabb;

    Handle<View> m_view;

    Handle<Camera> m_camera;

    Array<BakeEntity, BakerAllocator> m_bakeEntities;
    Map<Handle<Entity>, BakeEntity*, BakerAllocator> m_bakeEntitiesByEntity;

    BakerThreadPool* m_threadPool;

    Array<UniquePtr<PathTracer>, BakerAllocator> m_pathTracers;

    ClockTimer m_updateTimer;

protected:
    virtual void Build();
    void DispatchJobs();
    void OnCompleted();

    BakeJobParams CreateLightmapJobParams(size_t startIndex, size_t endIndex);

    void AddJob(UniquePtr<BakeJobBase>&& job);

    Array<UniquePtr<BakeJobBase>, BakerAllocator> m_queue;
    Mutex m_queueMutex;
    uint32 m_numJobs;
    uint32 m_initialNumJobs;

    PerformanceClock m_bakingClock;
    double m_lastProgressPercent;
    Array<Pair<double, double>, BakerAllocator> m_progressSamples;

    double m_accumulatedTexelBudget;

    BakerState m_state;
};

template <class T>
class Baker;

} // namespace Baking

} // namespace Hyperion
