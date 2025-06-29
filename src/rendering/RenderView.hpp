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
class RaytracingReflections;
struct RenderSetup;

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

    HYP_FORCE_INLINE const TResourceHandle<RenderCamera>& GetCamera() const
    {
        return m_render_camera;
    }

    HYP_FORCE_INLINE void SetCamera(const TResourceHandle<RenderCamera>& render_camera)
    {
        m_render_camera = render_camera;
    }

    GBuffer* GetGBuffer() const;

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
};

} // namespace hyperion

#endif
