/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/ObjectBase.hpp>
#include <core/reflection/Handle.hpp>

#include <core/config/Config.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Span.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <rendering/CullData.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderObject.hpp>

namespace Hyperion {

class World;
class Light;
class EnvProbe;
class EnvGrid;
class LightmapVolume;
class ParticleVolume;
struct CullData;
class PassData;
class RendererBase;
class RenderGroup;
class View;
class VolumeBase;
class EntityBatchAllocatorBase;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

namespace memory {
class Pool;
} // namespace memory

using memory::Pool;

HYP_API extern Pool* g_renderPool;

HYP_STRUCT(ConfigName = "GlobalConfig", JsonPath = "Rendering")
struct RendererConfig : public ConfigBase<RendererConfig>
{
    HYP_STRUCT_BODY(RendererConfig);

    HYP_FIELD(JsonPath = "RayTracing.PathTracing.Enabled")
    bool pathTracer = false;

    HYP_FIELD(JsonPath = "RayTracing.Reflections.Enabled")
    bool rayTracingReflections = false;

    HYP_FIELD(JsonPath = "RayTracing.GI.Enabled")
    bool rayTracingGlobalIllumination = false;

    HYP_FIELD(JsonPath = "HBAO.Enabled")
    bool hbaoEnabled = false;

    HYP_FIELD(JsonPath = "HBIL.Enabled")
    bool hbilEnabled = false;

    HYP_FIELD(JsonPath = "SSGI.Enabled")
    bool ssgiEnabled = false;

    HYP_FIELD(JsonPath = "TAA.Enabled")
    bool taaEnabled = false;

    virtual ~RendererConfig() override = default;
};

/*! \brief Describes the setup for rendering a frame.  */
struct HYP_API RenderSetup
{
    friend const RenderSetup& NullRenderSetup();

    World* world;
    View* view;
    EnvProbe* envProbe;
    EnvGrid* envGrid;
    Light* light;
    VolumeBase* volume;

    Swapchain* swapchain;

    PassData* passData;

    RenderSetup* prev;

public:
    RenderSetup()
        : world(nullptr),
          view(nullptr),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          volume(nullptr),
          swapchain(nullptr),
          passData(nullptr),
          prev(nullptr)
    {
    }

    explicit RenderSetup(World* world)
        : world(world),
          view(nullptr),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          volume(nullptr),
          swapchain(nullptr),
          passData(nullptr),
          prev(nullptr)
    {
        AssertDebug(world != nullptr, "RenderSetup must have a valid World");
    }

    RenderSetup(World* world, View* view)
        : world(world),
          view(view),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          volume(nullptr),
          swapchain(nullptr),
          passData(nullptr),
          prev(nullptr)
    {
        AssertDebug(world != nullptr, "RenderSetup must have a valid World");
    }

    RenderSetup(const RenderSetup& other) = default;
    RenderSetup& operator=(const RenderSetup& other) = default;

    RenderSetup(RenderSetup&& other) noexcept = default;
    RenderSetup& operator=(RenderSetup&& other) noexcept = default;

    ~RenderSetup() = default;

    /*! \brief Returns true if this RenderSetup has a valid World set. */
    HYP_FORCE_INLINE bool HasWorld() const
    {
        return world != nullptr;
    }

    /*! \brief Returns true if this RenderSetup has a valid View set. */
    HYP_FORCE_INLINE bool HasView() const
    {
        return view != nullptr;
    }

    /*! \brief Creates a forked RenderSetup that has this RenderSetup as its previous setup.
     *  This is useful for creating nested RenderSetups that can refer back to their parent setup if needed. */
    RenderSetup Fork() const
    {
        RenderSetup forked = *this;
        forked.prev = const_cast<RenderSetup*>(this);
        return forked;
    }
};

/*! \brief Special null RenderSetup that can be used for simple rendering tasks that don't make sense to use a World, such as rendering texture mipmaps.
 *  \internal Use sparingly as most rendering tasks should have a valid World and using this will cause the IsValid() check to return false */
extern const RenderSetup& NullRenderSetup();

struct PassDataExt
{
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    TypeId typeId;

    PassDataExt()
        : typeId(TypeId::Void())
    {
    }
    virtual ~PassDataExt() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        return typeId != TypeId::Void();
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE OtherPassDataExt* AsType()
    {
        const TypeId otherTypeId = TypeId::ForType<OtherPassDataExt>();

        if (typeId != otherTypeId)
        {
            return nullptr;
        }

        return reinterpret_cast<OtherPassDataExt*>(this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE const OtherPassDataExt* AsType() const
    {
        const TypeId otherTypeId = TypeId::ForType<OtherPassDataExt>();

        if (typeId != otherTypeId)
        {
            return nullptr;
        }

        return reinterpret_cast<const OtherPassDataExt*>(this);
    }

    // Create a new instance of this PassDataExt (caller owns the allocation)
    virtual PassDataExt* Clone() = 0;

protected:
    PassDataExt(TypeId subtype)
        : typeId(subtype)
    {
    }
};

/*! \brief Data and passes used for rendering a View in the Deferred Renderer. */

HYP_CLASS(NoScriptBindings)
class HYP_API PassData : public ObjectBase
{
    HYP_OBJECT_BODY(PassData);

    struct RenderGroupCacheEntry
    {
        WeakHandle<RenderGroup> renderGroup;
        GraphicsPipelineCacheHandle cacheHandle;
    };

    using RenderGroupCache = SparsePagedArray<RenderGroupCacheEntry, 32, RenderAllocator>;

public:
    static Pool* GetAllocator() { return g_renderPool; }

    PassData() = default;

    PassData(PassData&& other) noexcept = default;
    PassData& operator=(PassData&& other) noexcept = default;

    virtual ~PassData();

    WeakHandle<View> view;
    Viewport viewport;

    CullData cullData;

    // cached by ObjId<RenderGroup>
    RenderGroupCache renderGroupCache;
    // iterator for removing cache data over frames
    typename RenderGroupCache::Iterator renderGroupCacheIterator;

    PassDataExt* next = nullptr;

    /*! \brief Safely remove unused graphics pipelines that are no longer used from the cache.
     *  A graphics pipeline is considered unused if the RenderGroup it is associated with has no more references remaining
     *  \param maxIter The maximum number of graphics pipelines to iterate over for this frame.
     *  \returns The number of graphics pipelines that were culled */
    int CullUnusedGraphicsPipelines(int maxIter = 10);
};

class HYP_API RendererBase
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_renderPool);

    using PassDataMap = SparsePagedArray<PassData*, 16, RenderAllocator>;

    virtual ~RendererBase();

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void RenderFrame(Frame* frame, const RenderSetup& renderSetup) = 0;

    /*! \brief Cleans up data no longer used for rendering, amortised.
     *  Returns number of cleanup iterations used by this execution */
    virtual int RunCleanupCycle(int maxIter = 10);

protected:
    RendererBase();

    virtual PassData* CreateViewPassData(View* view, PassDataExt& ext) = 0;

    PassData* TryGetViewPassData(View* view);
    PassData* FetchViewPassData(View* view, PassDataExt* ext = nullptr, bool forceNew = false);

    static int RunCleanupCycle(PassDataMap& passData, int maxIter, typename PassDataMap::Iterator* pIter = nullptr);

private:
    PassDataMap m_viewPassData;
    typename PassDataMap::Iterator m_viewPassDataCleanupIterator;
};

} // namespace Hyperion
