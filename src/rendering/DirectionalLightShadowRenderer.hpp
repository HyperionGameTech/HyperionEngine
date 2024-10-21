/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP
#define HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP

#include <core/utilities/EnumFlags.hpp>
#include <core/threading/ThreadSignal.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/Shadows.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <math/BoundingBox.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <Types.hpp>

namespace hyperion {

struct ShadowMapCameraData
{
    Matrix4     view;
    Matrix4     projection;
    BoundingBox aabb;
};

class ShadowPass final : public FullScreenPass
{
public:
    ShadowPass(const Handle<Scene> &parent_scene, Extent2D extent, ShadowMode shadow_mode);
    ShadowPass(const ShadowPass &other)             = delete;
    ShadowPass &operator=(const ShadowPass &other)  = delete;
    virtual ~ShadowPass() override;

    HYP_FORCE_INLINE const Handle<Light> &GetLight() const
        { return m_light; }

    HYP_FORCE_INLINE RenderCollector &GetRenderCollectorStatics()
        { return m_render_collector_statics; }

    HYP_FORCE_INLINE  const RenderCollector &GetRenderCollectorStatics() const
        { return m_render_collector_statics; }

    HYP_FORCE_INLINE RenderCollector &GetRenderCollectorDynamics()
        { return m_render_collector_dynamics; }

    HYP_FORCE_INLINE const RenderCollector &GetRenderCollectorDynamics() const
        { return m_render_collector_dynamics; }

    void SetLight(const Handle<Light> &light)
    {
        m_light = light;

        if (m_light.IsValid()) {
            m_light->SetShadowMapIndex(m_shadow_map_index);
        }
    }

    HYP_FORCE_INLINE ShadowMode GetShadowMode() const
        { return m_shadow_mode; }

    HYP_FORCE_INLINE void SetShadowMode(ShadowMode shadow_mode)
        { m_shadow_mode = shadow_mode; }

    HYP_FORCE_INLINE const Vec3f &GetOrigin() const
        { return m_origin; }

    HYP_FORCE_INLINE void SetOrigin(const Vec3f &origin)
        { m_origin = origin; }

    HYP_FORCE_INLINE uint32 GetShadowMapIndex() const
        { return m_shadow_map_index; }

    HYP_FORCE_INLINE void SetShadowMapIndex(uint index)
    {
        m_shadow_map_index = index;

        if (m_light.IsValid()) {
            m_light->SetShadowMapIndex(index);
        }
    }

    HYP_FORCE_INLINE const Handle<Texture> &GetShadowMap() const
        { return m_shadow_map_all; }

    HYP_FORCE_INLINE ThreadSignal &GetShouldRerenderStaticObjectsSignal()
        { return m_should_rerender_static_objects_signal; }

    void CreateShader();
    virtual void CreateFramebuffer() override;
    virtual void CreateDescriptors() override;

    virtual void Create() override;
    virtual void Render(Frame *frame) override;

private:
    void CreateShadowMap();
    void CreateCombineShadowMapsPass();
    void CreateComputePipelines();

    Handle<Scene>                       m_parent_scene;
    Handle<Light>                       m_light;
    ShadowMode                          m_shadow_mode;
    RenderCollector                          m_render_collector_statics;
    RenderCollector                          m_render_collector_dynamics;
    Vec3f                               m_origin;
    uint32                              m_shadow_map_index;
    
    Handle<Texture>                     m_shadow_map_statics;
    Handle<Texture>                     m_shadow_map_dynamics;
    Handle<Texture>                     m_shadow_map_all;

    UniquePtr<FullScreenPass>           m_combine_shadow_maps_pass;
    ComputePipelineRef                  m_blur_shadow_map_pipeline;

    ThreadSignal                        m_should_rerender_static_objects_signal;
};

class DirectionalLightShadowRenderer : public RenderComponent<DirectionalLightShadowRenderer>
{
public:
    DirectionalLightShadowRenderer(Name name, Extent2D resolution, ShadowMode shadow_mode);
    DirectionalLightShadowRenderer(const DirectionalLightShadowRenderer &other) = delete;
    DirectionalLightShadowRenderer &operator=(const DirectionalLightShadowRenderer &other) = delete;
    virtual ~DirectionalLightShadowRenderer() override;

    HYP_FORCE_INLINE ShadowPass *GetPass() const
        { return m_shadow_pass.Get(); }

    HYP_FORCE_INLINE const Handle<Camera> &GetCamera() const
        { return m_camera; }

    HYP_FORCE_INLINE const BoundingBox &GetAABB() const
        { return m_aabb; }

    HYP_FORCE_INLINE void SetAABB(const BoundingBox &aabb)
        { m_aabb = aabb; }

    void Init();     // init on render thread
    void InitGame(); // init on game thread

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    UniquePtr<ShadowPass>   m_shadow_pass;
    Extent2D                m_resolution;
    ShadowMode              m_shadow_mode;

    Handle<Camera>          m_camera;
    BoundingBox             m_aabb;

    HashCode                m_cached_octant_hash_code_statics;
    Matrix4                 m_cached_view_matrix;
};

} // namespace hyperion

#endif
