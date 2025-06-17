/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_HPP
#define HYPERION_RENDERER_HPP

#include <core/config/Config.hpp>

#include <core/debug/Debug.hpp>

#include <core/utilities/Span.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class RenderView;
class RenderWorld;
class RenderEnvProbe;
class RenderEnvGrid;
struct CullData;

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
struct RenderSetup
{
    friend const RenderSetup& NullRenderSetup();

    RenderWorld* world;
    RenderView* view;
    RenderEnvProbe* env_probe;
    RenderEnvGrid* env_grid;

private:
    // Private constructor for null RenderSetup
    RenderSetup()
        : world(nullptr),
          view(nullptr),
          env_probe(nullptr),
          env_grid(nullptr)
    {
    }

public:
    RenderSetup(RenderWorld* world)
        : world(world),
          view(nullptr),
          env_probe(nullptr),
          env_grid(nullptr)
    {
        AssertDebugMsg(world != nullptr, "RenderSetup must have a valid RenderWorld");
    }

    RenderSetup(RenderWorld* world, RenderView* view)
        : world(world),
          view(view),
          env_probe(nullptr),
          env_grid(nullptr)
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

} // namespace hyperion

#endif
