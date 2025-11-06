/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/containers/Queue.hpp>

#include <core/threading/Mutex.hpp>
#include <core/threading/AtomicVar.hpp>
#include <core/threading/Task.hpp>
#include <core/threading/Semaphore.hpp>

#include <core/reflection/HypObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/utilities/Span.hpp>
#include <core/utilities/Result.hpp>

#include <core/math/BoundingBox.hpp>

#include <core/profiling/PerformanceClock.hpp>

#include <core/config/Config.hpp>

#include <scene/Scene.hpp>

#include <rendering/lightmapper/LightmapData.hpp>
#include <rendering/lightmapper/LightmapJob.hpp>

namespace hyperion {

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
class EnvProbe;
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
    IRRADIANCE = 0,
    RADIANCE,
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
    ILightmapRenderer(LightmapperBase* lightmapper)
        : m_lightmapper(lightmapper)
    {
        AssertDebug(lightmapper != nullptr);
    }

public:
    friend class LightmapperBase;

    virtual ~ILightmapRenderer() = default;

    virtual uint32 MaxRaysPerFrame() const = 0;

    virtual LightmapShadingType GetShadingType() const = 0;

    virtual bool CanRender() const
    {
        return true;
    }

    virtual void Create() = 0;
    virtual void PrepareJob(LightmapJobBase* job) {};
    virtual void UpdateRays(Span<const LightmapRay> rays) = 0;
    virtual void ReadHitsBuffer(FrameBase* frame, Span<LightmapHit> outHits) = 0;
    virtual void Render(FrameBase* frame, const RenderSetup& renderSetup, LightmapJobBase* job, Span<const LightmapRay> rays, uint32 rayOffset) = 0;

protected:
    LightmapperBase* m_lightmapper;
};

HYP_CLASS(Abstract)
class HYP_API LightmapperBase : public HypObjectBase
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
    LightmapperBase(LightmapperConfig&& config, const Handle<Scene>& scene, const BoundingBox& aabb);

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

    bool IsComplete() const;

    void Initialize();
    void Update(float delta);

    void HandleCompletedJob(LightmapJobBase* job);

    Delegate<void> OnComplete;

protected:
    virtual void Initialize_Internal()
    {
    }

    virtual void Build_Internal()
    {
    }

    virtual void HandleCompletedJob_Internal(LightmapJobBase* job)
    {
    }

    virtual UniquePtr<LightmapJobBase> CreateJob(LightmapJobParams&& params) = 0;
    virtual UniquePtr<ILightmapRenderer> CreateRenderer(LightmapShadingType shadingType);

    /// ===== CPU tracing only =====
    void BuildResourceCache();
    void BuildAccelerationStructures();
    /// ============================

    LightmapperConfig m_config;

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

protected:
private:
    void Build();

    LightmapJobParams CreateLightmapJobParams(SizeType startIndex, SizeType endIndex, LightmapShadingType shadingType);

    void AddJob(UniquePtr<LightmapJobBase>&& job)
    {
        Mutex::Guard guard(m_queueMutex);

        m_queue.Push(std::move(job));

        m_numJobs.Increment(1, MemoryOrder::RELEASE);
    }

    Array<UniquePtr<ILightmapRenderer>> m_lightmapRenderers;

    Queue<UniquePtr<LightmapJobBase>> m_queue;
    Mutex m_queueMutex;
    AtomicVar<uint32> m_numJobs;
};

template <class T>
class Lightmapper;

template <>
class Lightmapper<LightmapVolume> : public LightmapperBase
{
public:
    Lightmapper(LightmapperConfig&& config, const Handle<LightmapVolume>& volume);

    Lightmapper(const Lightmapper& other) = delete;
    Lightmapper& operator=(const Lightmapper& other) = delete;

    Lightmapper(Lightmapper&& other) noexcept = delete;
    Lightmapper& operator=(Lightmapper&& other) noexcept = delete;

    virtual ~Lightmapper() override = default;

protected:
    virtual UniquePtr<LightmapJobBase> CreateJob(LightmapJobParams&& params) override
    {
        return MakeUnique<LightmapJob<LightmapVolume>>(std::move(params), m_volume);
    }

    virtual void Initialize_Internal() override;
    virtual void HandleCompletedJob_Internal(LightmapJobBase* job) override;

    Handle<LightmapVolume> m_volume;
};

template <>
class Lightmapper<EnvProbe> : public LightmapperBase
{
public:
    Lightmapper(LightmapperConfig&& config, const Handle<EnvProbe>& envProbe);

    Lightmapper(const Lightmapper& other) = delete;
    Lightmapper& operator=(const Lightmapper& other) = delete;

    Lightmapper(Lightmapper&& other) noexcept = delete;
    Lightmapper& operator=(Lightmapper&& other) noexcept = delete;

    virtual ~Lightmapper() override = default;

protected:
    virtual UniquePtr<LightmapJobBase> CreateJob(LightmapJobParams&& params) override
    {
        return MakeUnique<LightmapJob<EnvProbe>>(std::move(params), m_envProbe);
    }

    virtual void Initialize_Internal() override;
    virtual void HandleCompletedJob_Internal(LightmapJobBase* job) override;

    Handle<EnvProbe> m_envProbe;
};

} // namespace hyperion
