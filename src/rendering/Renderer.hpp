/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/reflection/Handle.hpp>

#include <core/reflection/HypObjectFwd.hpp>

#include <core/config/Config.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Span.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <rendering/RenderStats.hpp>
#include <rendering/CullData.hpp>
#include <rendering/GraphicsPipelineCache.hpp>
#include <rendering/RenderObject.hpp>
#include <rendering/RenderQueue.hpp>

namespace hyperion {

class World;
class Light;
class EnvProbe;
class EnvGrid;
struct CullData;
struct DrawCall;
struct InstancedDrawCall;
class PassData;
class RendererBase;
class RenderGroup;
class View;
class IDrawCallCollectionImpl;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

HYP_STRUCT(ConfigName = "GlobalConfig", JsonPath = "Rendering")
struct RendererConfig : public ConfigBase<RendererConfig>
{
    HYP_STRUCT_BODY(RendererConfig);

    HYP_FIELD(JsonPath = "RayTracing.PathTracing.Enabled")
    bool pathTracer = false;

    HYP_FIELD(JsonPath = "RayTracing.Reflections.Enabled")
    bool raytracingReflections = false;

    HYP_FIELD(JsonPath = "RayTracing.GI.Enabled")
    bool raytracingGlobalIllumination = false;

    HYP_FIELD(JsonPath = "HBAO.Enabled")
    bool hbaoEnabled = false;

    HYP_FIELD(JsonPath = "HBIL.Enabled")
    bool hbilEnabled = false;

    HYP_FIELD(JsonPath = "SSGI.Enabled")
    bool ssgiEnabled = false;

    HYP_FIELD(JsonPath = "EnvGrid.GI.Enabled")
    bool envGridGiEnabled = false;

    HYP_FIELD(JsonPath = "EnvGrid.Reflections.Enabled")
    bool envGridRadianceEnabled = false;

    HYP_FIELD(JsonPath = "TAA.Enabled")
    bool taaEnabled = false;

    virtual ~RendererConfig() override = default;
};

/*! \brief Describes the setup for rendering a frame. All RenderSetups must have a valid World set. Passed to almost all Render() functions throughout the renderer.
 *  Most of the time you'll want a RenderSetup with a View as well, but compute-only passes can use a RenderSetup without a view. Use HasView() to check if a view is set. */
struct HYP_API RenderSetup
{
    friend const RenderSetup& NullRenderSetup();

    World* world;
    View* view;
    EnvProbe* envProbe;
    EnvGrid* envGrid;
    Light* light;
    PassData* passData;

private:
    // Private constructor for null RenderSetup
    RenderSetup()
        : world(nullptr),
          view(nullptr),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          passData(nullptr)
    {
    }

public:
    RenderSetup(World* world)
        : world(world),
          view(nullptr),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          passData(nullptr)
    {
        AssertDebug(world != nullptr, "RenderSetup must have a valid World");
    }

    RenderSetup(World* world, View* view)
        : world(world),
          view(view),
          envProbe(nullptr),
          envGrid(nullptr),
          light(nullptr),
          passData(nullptr)
    {
        AssertDebug(world != nullptr, "RenderSetup must have a valid World");
    }

    RenderSetup(const RenderSetup& other) = default;
    RenderSetup& operator=(const RenderSetup& other) = default;

    RenderSetup(RenderSetup&& other) noexcept = default;
    RenderSetup& operator=(RenderSetup&& other) noexcept = default;

    ~RenderSetup() = default;

    /*! \see IsValid() */
    HYP_FORCE_INLINE explicit operator bool() const
    {
        return IsValid();
    }

    /*! \see IsValid() */
    HYP_FORCE_INLINE bool operator!() const
    {
        return !IsValid();
    }

    /*! \brief Returns true if this RenderSetup is useable for rendering.
     *  This means it has a valid World set. If you need to check if a View is set, use HasView() instead. */
    HYP_FORCE_INLINE bool IsValid() const
    {
        return world != nullptr;
    }

    /*! \brief Returns true if this RenderSetup has a valid View set. */
    HYP_FORCE_INLINE bool HasView() const
    {
        return view != nullptr;
    }
};

/*! \brief Special null RenderSetup that can be used for simple rendering tasks that don't make sense to use a World, such as rendering texture mipmaps.
 *  \internal Use sparingly as most rendering tasks should have a valid World and using this will cause the IsValid() check to return false */
extern const RenderSetup& NullRenderSetup();

struct PassDataExt
{
    TypeId typeId;

    PassDataExt()
        : typeId(TypeId::Void())
    {
    }
    virtual ~PassDataExt() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        constexpr TypeId invalidTypeId = TypeId::Void();

        return typeId != invalidTypeId;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE OtherPassDataExt* AsType()
    {
        constexpr TypeId otherTypeId = TypeId::ForType<OtherPassDataExt>();

        if (typeId != otherTypeId)
        {
            return nullptr;
        }

        return reinterpret_cast<OtherPassDataExt*>(this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE const OtherPassDataExt* AsType() const
    {
        constexpr TypeId otherTypeId = TypeId::ForType<OtherPassDataExt>();

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
class HYP_API PassData : public HypObjectBase
{
    HYP_OBJECT_BODY(PassData);

public:
    PassData() = default;
    virtual ~PassData();

    struct RenderGroupCacheEntry
    {
        WeakHandle<RenderGroup> renderGroup;
        GraphicsPipelineCacheHandle cacheHandle;
    };

    using RenderGroupCache = SparsePagedArray<RenderGroupCacheEntry, 32>;

    WeakHandle<View> view;
    Viewport viewport;

    // per-View descriptor sets
    FixedArray<DescriptorSetRef, NumFramesInFlight> descriptorSets;

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
    using ViewPassDataMap = SparsePagedArray<Handle<PassData>, 16>;

    virtual ~RendererBase();

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& renderSetup) = 0;

    /*! \brief Cleans up data no longer used for rendering, amortised.
     *  Returns number of cleanup iterations used by this execution */
    virtual int RunCleanupCycle(int maxIter = 10);

protected:
    RendererBase();

    virtual Handle<PassData> CreateViewPassData(View* view, PassDataExt& ext) = 0;

    const Handle<PassData>& TryGetViewPassData(View* view);
    const Handle<PassData>& FetchViewPassData(View* view, PassDataExt* ext = nullptr);

private:
    ViewPassDataMap m_viewPassData;
    typename ViewPassDataMap::Iterator m_viewPassDataCleanupIterator;
};

} // namespace hyperion
