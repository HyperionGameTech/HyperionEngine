/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Queue.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/config/Config.hpp>

#include <scene/Scene.hpp>

#include <lightmapper/LightmapData.hpp>
#include <lightmapper/LightmapJob.hpp>

#include <util/GameCounter.hpp>

namespace Hyperion {

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

struct LightmapHitsBuffer;
class LightmapThreadPool;
class ILightmapAccelerationStructure;
class LightmapTopLevelAccelerationStructure;
class LightmapJobBase;
class LightmapVolume;
class LightmapperBase;
struct LightmapElement;

class AssetObject;
class View;
class ReflectionProbe;
class FogVolume;
struct RenderSetup;

HYP_ENUM()
enum class LightmapTraceMode : int
{
    GPU_PATH_TRACING = 0,
    CPU_PATH_TRACING,

    MAX
};

HYP_ENUM()
enum class LightmapShadingType : int
{
    IRRADIANCE = 0, // Bake irradiance only
    RADIANCE,       // Bake radiance only (direct light)
    FULL,           // Full scene bake
    MAX
};

HYP_STRUCT(ConfigName = "GlobalConfig", JsonPath = "Lightmapper")
struct LightmapperConfig : public ConfigBase<LightmapperConfig>
{
    HYP_STRUCT_BODY(LightmapperConfig);

    HYP_FIELD()
    LightmapTraceMode traceMode = LightmapTraceMode::GPU_PATH_TRACING;

    HYP_FIELD()
    bool radiance = true;

    HYP_FIELD()
    bool irradiance = true;

    HYP_FIELD()
    uint32 numSamples = 16;

    HYP_FIELD()
    uint32 maxTexelsPerFrame = 512 * 512;

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

        if (maxTexelsPerFrame == 0)
        {
            AddError(HYP_MAKE_ERROR(Error, "Max texels per frame must be greater than zero"));

            valid = false;
        }

        if (idealTrianglesPerJob == 0)
        {
            AddError(HYP_MAKE_ERROR(Error, "Ideal triangles per job must be greater than zero"));

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
    ILightmapRenderer(LightmapperBase* lightmapper)
        : m_lightmapper(lightmapper)
    {
        AssertDebug(lightmapper != nullptr);
    }

public:
    friend class LightmapperBase;

    virtual ~ILightmapRenderer() = default;

    virtual uint32 MaxTexelsPerFrame() const = 0;

    virtual LightmapShadingType GetShadingType() const = 0;

    virtual bool CanRender() const
    {
        return true;
    }

    virtual void Create() = 0;
    virtual void PrepareJob(LightmapJobBase* job)
    {
    }
    virtual void CleanJobData(LightmapJobBase* job)
    {
    }
    virtual void ReadHitsBuffer(Frame* frame, LightmapJobBase* job, Span<LightmapHit> outHits) = 0;
    virtual void Render(Frame* frame, const RenderSetup& renderSetup, LightmapJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset) = 0;

protected:
    LightmapperBase* m_lightmapper;
};

HYP_CLASS(Abstract)
class HYP_API LightmapperBase : public ObjectBase
{
    HYP_OBJECT_BODY(LightmapperBase);

protected:
    struct CachedResource
    {
        Handle<AssetObject> assetObject;
        ResourceHandle resourceHandle;

        CachedResource() = default;

        CachedResource(const Handle<AssetObject>& assetObject, const ResourceHandle& resourceHandle)
            : assetObject(assetObject),
              resourceHandle(resourceHandle)
        {
        }

        CachedResource(const CachedResource& other) = delete;
        CachedResource& operator=(const CachedResource& other) = delete;

        CachedResource(CachedResource&& other) noexcept
            : assetObject(std::move(other.assetObject)),
              resourceHandle(std::move(other.resourceHandle))
        {
            other.resourceHandle.Reset();
        }

        CachedResource& operator=(CachedResource&& other) noexcept
        {
            if (this == &other)
            {
                return *this;
            }

            resourceHandle.Reset();

            assetObject = std::move(other.assetObject);
            resourceHandle = std::move(other.resourceHandle);

            return *this;
        }

        ~CachedResource()
        {
            // destruct the ResourceHandle before assetobject is destructed,
            // so it destructing the AssetObject doesn't try to wait for the resource's ref count to reach zero
            resourceHandle.Reset();
        }
    };

    using ResourceCache = HashSet<CachedResource, &CachedResource::assetObject, DynamicNodeAllocator>;

public:
    LightmapperBase(LightmapperConfig&& config, ObjectBase* source, const Handle<Scene>& scene, const BoundingBox& aabb);

    LightmapperBase(const LightmapperBase& other) = delete;
    LightmapperBase& operator=(const LightmapperBase& other) = delete;

    LightmapperBase(LightmapperBase&& other) noexcept = delete;
    LightmapperBase& operator=(LightmapperBase&& other) noexcept = delete;

    virtual ~LightmapperBase() override;

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

    /*! \brief Should we consider only elements overlapping our AABB for lightmapping? */
    virtual bool OnlyOverlappingElements() const
    {
        return true;
    }

    virtual bool PerformsRayTracing() const
    {
        return true;
    }

    virtual uint32 NumTexelSamples() const;
    virtual uint32 MaxTexelsPerFrame() const;

    bool IsComplete() const;

    void Initialize();
    void Update(float delta);

    void HandleCompletedJob(LightmapJobBase* job);

    Delegate<void> OnComplete;

protected:
    virtual void Initialize_Internal()
    {
    }

    virtual Result Build_Internal()
    {
        return {};
    }

    virtual void HandleCompletedJob_Internal(LightmapJobBase* job)
    {
    }

    virtual LightmapDataBase& GetLightmapData() = 0;

    HYP_FORCE_INLINE const LightmapDataBase& GetLightmapData() const
    {
        return const_cast<LightmapperBase*>(this)->GetLightmapData();
    }

    virtual UniquePtr<LightmapJobBase> CreateJob(LightmapJobParams&& params) = 0;
    virtual UniquePtr<ILightmapRenderer> CreateRenderer(LightmapShadingType shadingType, uint32 maxTexelsPerFrame);

    void CreateLightmapRenderers();

    /// ===== CPU tracing only =====
    void BuildResourceCache();
    void BuildAccelerationStructures();
    /// ============================

    LightmapperConfig m_config;

    ObjectBase* m_source;

    Handle<Scene> m_scene;
    BoundingBox m_aabb;

    Handle<View> m_view;

    Array<LightmapSubElement> m_subElements;
    HashMap<Handle<Entity>, LightmapSubElement*> m_subElementsByEntity;

    /// ===== CPU tracing only =====
    UniquePtr<LightmapTopLevelAccelerationStructure> m_accelerationStructure;
    ResourceCache m_resourceCache;
    LightmapThreadPool* m_threadPool;
    /// ============================

    Array<UniquePtr<ILightmapRenderer>> m_lightmapRenderers;

    LockstepGameCounter m_updateTimer;

protected:
    virtual void Build();
    void DispatchJobs();

    LightmapJobParams CreateLightmapJobParams(SizeType startIndex, SizeType endIndex);

    void AddJob(UniquePtr<LightmapJobBase>&& job)
    {
        if (!job)
        {
            return;
        }

        job->m_lightmapper = this;

        Mutex::Guard guard(m_queueMutex);
        m_queue.PushBack(std::move(job));

        ++m_numJobs;
    }

    Array<UniquePtr<LightmapJobBase>> m_queue;
    Mutex m_queueMutex;
    uint32 m_numJobs;
    uint32 m_initialNumJobs;
};

template <class T>
class Lightmapper;

enum class LightmapElementId : uint32;

template <>
class Lightmapper<LightmapVolume> final : public LightmapperBase
{
public:
    Lightmapper(LightmapperConfig&& config, const Handle<LightmapVolume>& volume);

    Lightmapper(const Lightmapper& other) = delete;
    Lightmapper& operator=(const Lightmapper& other) = delete;

    Lightmapper(Lightmapper&& other) noexcept = delete;
    Lightmapper& operator=(Lightmapper&& other) noexcept = delete;

    virtual ~Lightmapper() override = default;

    virtual bool ShouldSplitIntoJobs() const override
    {
        return true;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        return (1u << int(LightmapShadingType::IRRADIANCE))
            | (1u << int(LightmapShadingType::RADIANCE));
    }

protected:
    virtual LightmapDataBase& GetLightmapData() override
    {
        return m_lightmapData;
    }

    virtual UniquePtr<LightmapJobBase> CreateJob(LightmapJobParams&& params) override
    {
        return MakeUnique<LightmapJob<LightmapVolume>>(std::move(params), m_volume, &m_lightmapData);
    }

    virtual void Initialize_Internal() override;
    virtual void HandleCompletedJob_Internal(LightmapJobBase* job) override;
    virtual void Build() override;

    Handle<LightmapVolume> m_volume;
    LightmapData<LightmapVolume> m_lightmapData;
    LightmapElementId m_lightmapElementId;
};

template <>
class Lightmapper<ReflectionProbe> final : public LightmapperBase
{
public:
    Lightmapper(LightmapperConfig&& config, const Handle<ReflectionProbe>& envProbe);

    Lightmapper(const Lightmapper& other) = delete;
    Lightmapper& operator=(const Lightmapper& other) = delete;

    Lightmapper(Lightmapper&& other) noexcept = delete;
    Lightmapper& operator=(Lightmapper&& other) noexcept = delete;

    virtual ~Lightmapper() override = default;

    virtual bool ShouldSplitIntoJobs() const override
    {
        return true;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        return 1u << int(LightmapShadingType::FULL);
    }

protected:
    virtual LightmapDataBase& GetLightmapData() override
    {
        return m_lightmapData;
    }

    virtual UniquePtr<LightmapJobBase> CreateJob(LightmapJobParams&& params) override
    {
        return MakeUnique<LightmapJob<ReflectionProbe>>(std::move(params), m_envProbe, &m_lightmapData);
    }

    virtual Result Build_Internal() override;
    virtual void HandleCompletedJob_Internal(LightmapJobBase* job) override;

    Handle<ReflectionProbe> m_envProbe;
    LightmapData<ReflectionProbe> m_lightmapData;
};

template <>
class Lightmapper<FogVolume> final : public LightmapperBase
{
public:
    Lightmapper(LightmapperConfig&& config, const Handle<FogVolume>& fogVolume);

    Lightmapper(const Lightmapper& other) = delete;
    Lightmapper& operator=(const Lightmapper& other) = delete;

    Lightmapper(Lightmapper&& other) noexcept = delete;
    Lightmapper& operator=(Lightmapper&& other) noexcept = delete;

    virtual ~Lightmapper() override = default;

    virtual bool PerformsRayTracing() const override
    {
        return false;
    }

    virtual uint32 NumTexelSamples() const override
    {
        return 1;
    }

    virtual uint32 GetShadingTypesMask() const override
    {
        return 1u << int(LightmapShadingType::FULL);
    }

    virtual bool ShouldSplitIntoJobs() const override
    {
        return false;
    }

protected:
    virtual LightmapDataBase& GetLightmapData() override
    {
        return m_lightmapData;
    }

    virtual UniquePtr<LightmapJobBase> CreateJob(LightmapJobParams&& params) override
    {
        return MakeUnique<LightmapJob<FogVolume>>(std::move(params), m_fogVolume, &m_lightmapData);
    }

    virtual Result Build_Internal() override;
    virtual void HandleCompletedJob_Internal(LightmapJobBase* job) override;

    Handle<FogVolume> m_fogVolume;
    LightmapData<FogVolume> m_lightmapData;
};

} // namespace Hyperion
