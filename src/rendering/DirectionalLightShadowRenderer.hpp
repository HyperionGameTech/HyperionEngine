/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP
#define HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP

#include <core/utilities/EnumFlags.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/Light.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/Shadows.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <math/BoundingBox.hpp>

#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

#include <Types.hpp>

namespace hyperion {

using renderer::Frame;

enum class ShadowRenderPassFlags : uint32
{
    NONE                    = 0x0,
    RERENDER_STATIC_OBJECTS = 0x1
};

HYP_MAKE_ENUM_FLAGS(ShadowRenderPassFlags)

struct ShadowMapCameraData
{
    Matrix4     view;
    Matrix4     projection;
    BoundingBox aabb;
};

class ShadowPass : public FullScreenPass
{
public:
    ShadowPass(const Handle<Scene> &parent_scene, Extent2D extent, ShadowMode shadow_mode);
    ShadowPass(const ShadowPass &other)             = delete;
    ShadowPass &operator=(const ShadowPass &other)  = delete;
    virtual ~ShadowPass();

    const Handle<Light> &GetLight() const { return m_light; }

    RenderList &GetRenderListStatics() { return m_render_list_statics; }
    const RenderList &GetRenderListStatics() const { return m_render_list_statics; }

    RenderList &GetRenderListDynamics() { return m_render_list_dynamics; }
    const RenderList &GetRenderListDynamics() const { return m_render_list_dynamics; }

    void SetLight(const Handle<Light> &light)
    {
        m_light = light;

        if (m_light.IsValid()) {
            m_light->SetShadowMapIndex(m_shadow_map_index);
        }
    }

    ShadowMode GetShadowMode() const { return m_shadow_mode; }
    void SetShadowMode(ShadowMode shadow_mode) { m_shadow_mode = shadow_mode; }

    const Vector3 &GetOrigin() const { return m_origin; }
    void SetOrigin(const Vector3 &origin) { m_origin = origin; }

    uint GetShadowMapIndex() const
        { return m_shadow_map_index; }

    void SetShadowMapIndex(uint index)
    {
        m_shadow_map_index = index;

        if (m_light.IsValid()) {
            m_light->SetShadowMapIndex(index);
        }
    }

    const Handle<Texture> &GetShadowMap() const
        { return m_shadow_map_all; }

    EnumFlags<ShadowRenderPassFlags> GetFlags() const
        { return m_flags; }

    void SetFlags(EnumFlags<ShadowRenderPassFlags> flags)
        { m_flags = flags; }

    void CreateShader();
    virtual void CreateFramebuffer() override;
    virtual void CreateDescriptors() override;

    virtual void Create() override;
    virtual void Destroy() override;
    virtual void Render(Frame *frame) override;

private:
    void CreateShadowMap();
    void CreateCombineShadowMapsPass();
    void CreateComputePipelines();

    Handle<Scene>                       m_parent_scene;
    Handle<Light>                       m_light;
    ShadowMode                          m_shadow_mode;
    RenderList                          m_render_list_statics;
    RenderList                          m_render_list_dynamics;
    Vec3f                               m_origin;
    uint                                m_shadow_map_index;
    
    Handle<Texture>                     m_shadow_map_statics;
    Handle<Texture>                     m_shadow_map_dynamics;
    Handle<Texture>                     m_shadow_map_all;

    UniquePtr<FullScreenPass>           m_combine_shadow_maps_pass;
    ComputePipelineRef                  m_blur_shadow_map_pipeline;

    EnumFlags<ShadowRenderPassFlags>    m_flags;
};

class DirectionalLightShadowRenderer : public RenderComponent<DirectionalLightShadowRenderer>
{
public:
    DirectionalLightShadowRenderer(Name name, Extent2D resolution, ShadowMode shadow_mode);
    DirectionalLightShadowRenderer(const DirectionalLightShadowRenderer &other) = delete;
    DirectionalLightShadowRenderer &operator=(const DirectionalLightShadowRenderer &other) = delete;
    virtual ~DirectionalLightShadowRenderer();

    ShadowPass *GetPass() const
        { return m_shadow_pass.Get(); }

    const Handle<Camera> &GetCamera() const
        { return m_camera; }

    const BoundingBox &GetAABB() const
        { return m_aabb; }

    void SetAABB(const BoundingBox &aabb)
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
