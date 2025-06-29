/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_HPP
#define HYPERION_RENDERER_HPP

#include <core/Handle.hpp>

#include <core/config/Config.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Span.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <rendering/EngineRenderStats.hpp>
#include <rendering/CullData.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/rhi/RHICommandList.hpp>

namespace hyperion {

class RenderView;
class RenderWorld;
class RenderEnvGrid;
class RenderLight;
class Light;
class EnvProbe;
class EnvGrid;
struct CullData;
struct DrawCall;
struct InstancedDrawCall;
struct PassData;
class RendererBase;
class RenderGroup;
class View;
class IDrawCallCollectionImpl;

namespace threading {
class TaskBatch;
} // namespace threading

using threading::TaskBatch;

HYP_STRUCT(ConfigName = "app", JSONPath = "rendering")
struct RendererConfig : public ConfigBase<RendererConfig>
{
    HYP_FIELD(JSONPath = "rt.path_tracing.enabled")
    bool path_tracer_enabled = false;

    HYP_FIELD(JSONPath = "rt.reflections.enabled")
    bool rt_reflections_enabled = false;

    HYP_FIELD(JSONPath = "rt.gi.enabled")
    bool rt_gi_enabled = false;

    HYP_FIELD(JSONPath = "hbao.enabled")
    bool hbao_enabled = false;

    HYP_FIELD(JSONPath = "hbil.enabled")
    bool hbil_enabled = false;

    HYP_FIELD(JSONPath = "ssgi.enabled")
    bool ssgi_enabled = false;

    HYP_FIELD(JSONPath = "env_grid.gi.enabled")
    bool env_grid_gi_enabled = false;

    HYP_FIELD(JSONPath = "env_grid.reflections.enabled")
    bool env_grid_radiance_enabled = false;

    HYP_FIELD(JSONPath = "taa.enabled")
    bool taa_enabled = false;

    virtual ~RendererConfig() override = default;
};

/*! \brief Describes the setup for rendering a frame. All RenderSetups must have a valid RenderWorld set. Passed to almost all Render() functions throughout the renderer.
 *  Most of the time you'll want a RenderSetup with a RenderView as well, but compute-only passes can use a RenderSetup without a view. Use HasView() to check if a view is set. */
struct HYP_API RenderSetup
{
    friend const RenderSetup& NullRenderSetup();

    RenderWorld* world;
    RenderView* view;
    EnvProbe* env_probe;
    EnvGrid* env_grid;
    Light* light;
    PassData* pass_data;

private:
    // Private constructor for null RenderSetup
    RenderSetup()
        : world(nullptr),
          view(nullptr),
          env_probe(nullptr),
          env_grid(nullptr),
          light(nullptr),
          pass_data(nullptr)
    {
    }

public:
    RenderSetup(RenderWorld* world)
        : world(world),
          view(nullptr),
          env_probe(nullptr),
          env_grid(nullptr),
          light(nullptr),
          pass_data(nullptr)
    {
        AssertDebugMsg(world != nullptr, "RenderSetup must have a valid RenderWorld");
    }

    RenderSetup(RenderWorld* world, RenderView* view)
        : world(world),
          view(view),
          env_probe(nullptr),
          env_grid(nullptr),
          light(nullptr),
          pass_data(nullptr)
    {
        AssertDebugMsg(world != nullptr, "RenderSetup must have a valid RenderWorld");
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
     *  This means it has a valid RenderWorld set. If you need to check if a RenderView is set, use HasView() instead. */
    HYP_FORCE_INLINE bool IsValid() const
    {
        return world != nullptr;
    }

    /*! \brief Returns true if this RenderSetup has a valid RenderView set. */
    HYP_FORCE_INLINE bool HasView() const
    {
        return view != nullptr;
    }
};

/*! \brief Special null RenderSetup that can be used for simple rendering tasks that don't make sense to use a RenderWorld, such as rendering texture mipmaps.
 *  \internal Use sparingly as most rendering tasks should have a valid RenderWorld and using this will cause the IsValid() check to return false */
extern const RenderSetup& NullRenderSetup();

struct PassDataExt
{
    TypeID type_id;

    virtual ~PassDataExt() = default;

    HYP_FORCE_INLINE explicit operator bool() const
    {
        constexpr TypeID invalid_type_id = TypeID::Void();

        return type_id != invalid_type_id;
    }

    HYP_FORCE_INLINE bool operator!() const
    {
        return !bool(*this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE OtherPassDataExt* AsType()
    {
        constexpr TypeID other_type_id = TypeID::ForType<OtherPassDataExt>();

        if (type_id != other_type_id)
        {
            return nullptr;
        }

        return reinterpret_cast<OtherPassDataExt*>(this);
    }

    template <class OtherPassDataExt>
    HYP_FORCE_INLINE const OtherPassDataExt* AsType() const
    {
        constexpr TypeID other_type_id = TypeID::ForType<OtherPassDataExt>();

        if (type_id != other_type_id)
        {
            return nullptr;
        }

        return reinterpret_cast<const OtherPassDataExt*>(this);
    }
};

/*! \brief Data and passes used for rendering a View in the Deferred Renderer. */
struct HYP_API PassData
{
    struct RenderGroupCacheEntry
    {
        WeakHandle<RenderGroup> render_group;
        GraphicsPipelineRef graphics_pipeline;
    };

    WeakHandle<View> view;
    Viewport viewport;

    // per-View descriptor sets
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    CullData cull_data;

    // cached by ID<RenderGroup>
    SparsePagedArray<RenderGroupCacheEntry, 32> render_group_cache;
    // iterator for removing cache data over frames
    typename SparsePagedArray<RenderGroupCacheEntry, 32>::Iterator render_group_cache_iterator;

    PassDataExt* next = nullptr;

    virtual ~PassData();

    /*! \brief Safely remove unused graphics pipelines that are no longer used from the cache.
     *  A graphics pipeline is considered unused if the RenderGroup it is associated with has no more references remaining
     *  \param max_iter The maximum number of graphics pipelines to iterate over for this frame. */
    void CullUnusedGraphicsPipelines(uint32 max_iter = 10);

    static GraphicsPipelineRef CreateGraphicsPipeline(
        PassData* pd,
        const ShaderRef& shader,
        const RenderableAttributeSet& renderable_attributes,
        const DescriptorTableRef& descriptor_table = DescriptorTableRef::Null(),
        IDrawCallCollectionImpl* impl = nullptr); // @TODO: Make this param part of renderable attributes
};

class HYP_API RendererBase
{
public:
    virtual ~RendererBase();

    virtual void Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& render_setup) = 0;

protected:
    virtual PassData* CreateViewPassData(View* view, PassDataExt& ext) = 0;

    PassData* TryGetViewPassData(View* view);
    PassData* FetchViewPassData(View* view, PassDataExt* ext = nullptr);

private:
    SparsePagedArray<PassData*, 16> m_view_pass_data;
};

} // namespace hyperion

#endif
