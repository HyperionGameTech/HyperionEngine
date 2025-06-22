/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERING_VIEW_HPP
#define HYPERION_RENDERING_VIEW_HPP

#include <core/memory/UniquePtr.hpp>
#include <core/Handle.hpp>

#include <rendering/Renderer.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/CullData.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <HashCode.hpp>

namespace hyperion {

class View;
class DeferredRenderer;
class RenderWorld;
class RenderScene;
class RenderCamera;
class RenderLight;
class RenderLightmapVolume;
class RenderEnvGrid;
class RenderEnvProbe;
class GBuffer;
class EnvGrid;
class EnvGridPass;
class EnvProbe;
class ReflectionsPass;
class TonemapPass;
class LightmapPass;
class FullScreenPass;
class DepthPyramidRenderer;
class TemporalAA;
class PostProcessing;
class DeferredPass;
class HBAO;
class SSGI;
class DOFBlur;
class Texture;
class RTRadianceRenderer;
struct RenderSetup;
enum class LightType : uint32;
enum class EnvProbeType : uint32;

class RenderView : public RenderResourceBase
{
public:
    friend class DeferredRenderer; // temp

    RenderView(View* view);
    RenderView(const RenderView& other) = delete;
    RenderView& operator=(const RenderView& other) = delete;
    virtual ~RenderView() override;

    HYP_FORCE_INLINE View* GetView() const
    {
        return m_view;
    }

    HYP_FORCE_INLINE const Viewport& GetViewport() const
    {
        return m_viewport;
    }

    void SetViewport(const Viewport& viewport);

    HYP_FORCE_INLINE int GetPriority() const
    {
        return m_priority;
    }

    void SetPriority(int priority);

    /*! \brief Get the currently bound Lights with the given LightType.
     *  \note Only call from render thread or from task on a task thread that is initiated by the render thread.
     *  \param type The type of light to get. */
    HYP_FORCE_INLINE const Array<RenderLight*>& GetLights(LightType type) const
    {
        AssertDebug(uint32(type) < m_lights.Size());

        return m_lights[uint32(type)];
    }

    HYP_FORCE_INLINE SizeType NumLights() const
    {
        SizeType num_lights = 0;

        for (const auto& lights : m_lights)
        {
            num_lights += lights.Size();
        }

        return num_lights;
    }

    HYP_FORCE_INLINE const Array<RenderEnvGrid*>& GetEnvGrids() const
    {
        return m_env_grids;
    }

    HYP_FORCE_INLINE const Array<RenderEnvProbe*>& GetEnvProbes(EnvProbeType type) const
    {
        AssertDebug(uint32(type) < m_env_probes.Size());

        return m_env_probes[uint32(type)];
    }

    HYP_FORCE_INLINE SizeType NumEnvProbes() const
    {
        SizeType num_env_probes = 0;

        for (const auto& env_probes : m_env_probes)
        {
            num_env_probes += env_probes.Size();
        }

        return num_env_probes;
    }

    HYP_FORCE_INLINE const TResourceHandle<RenderCamera>& GetCamera() const
    {
        return m_render_camera;
    }

    HYP_FORCE_INLINE void SetCamera(const TResourceHandle<RenderCamera>& render_camera)
    {
        m_render_camera = render_camera;
    }

    GBuffer* GetGBuffer() const;

    HYP_FORCE_INLINE RenderCollector& GetRenderCollector()
    {
        return m_render_collector;
    }

    HYP_FORCE_INLINE const RenderCollector& GetRenderCollector() const
    {
        return m_render_collector;
    }

    /*! \brief Update the render collector on the render thread to reflect the state of \ref{render_proxy_tracker} */
    typename RenderProxyTracker::Diff UpdateTrackedRenderProxies(const RenderProxyTracker& render_proxy_tracker);
    void UpdateTrackedLights(const ResourceTracker<ID<Light>, RenderLight*>& tracked_lights);
    void UpdateTrackedLightmapVolumes(const ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*>& tracked_lightmap_volumes);
    void UpdateTrackedEnvGrids(const ResourceTracker<ID<EnvGrid>, RenderEnvGrid*>& tracked_env_grids);
    void UpdateTrackedEnvProbes(const ResourceTracker<ID<EnvProbe>, RenderEnvProbe*>& tracked_env_probes);

    virtual void PreRender(FrameBase* frame);
    virtual void PostRender(FrameBase* frame);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    void CreateRenderer();
    void DestroyRenderer();

    View* m_view;

    Viewport m_viewport;

    int m_priority;

    TResourceHandle<RenderCamera> m_render_camera;

    RenderCollector m_render_collector;

    Array<Array<RenderLight*>> m_lights;
    ResourceTracker<ID<Light>, RenderLight*> m_tracked_lights;

    Array<RenderLightmapVolume*> m_lightmap_volumes;
    ResourceTracker<ID<LightmapVolume>, RenderLightmapVolume*> m_tracked_lightmap_volumes;

    Array<RenderEnvGrid*> m_env_grids;
    ResourceTracker<ID<EnvGrid>, RenderEnvGrid*> m_tracked_env_grids;

    Array<Array<RenderEnvProbe*>> m_env_probes;
    ResourceTracker<ID<EnvProbe>, RenderEnvProbe*> m_tracked_env_probes;

    // Temp
    RenderProxyList m_render_proxy_list;
};

} // namespace hyperion

#endif
