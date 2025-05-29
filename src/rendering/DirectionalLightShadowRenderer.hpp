/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP
#define HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP

#include <core/utilities/EnumFlags.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderResource.hpp>
#include <rendering/Shadows.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <scene/Scene.hpp>
#include <scene/Light.hpp>

#include <scene/camera/Camera.hpp>

#include <Types.hpp>

namespace hyperion {

using RerenderShadowsSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::detail::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>;

class RenderCamera;
class RenderShadowMap;
class RenderScene;
class RenderWorld;
class RenderLight;
class View;

struct ShadowMapCameraData
{
    Matrix4 view;
    Matrix4 projection;
    BoundingBox aabb;
};

class ShadowPass final : public FullScreenPass
{
public:
    ShadowPass(
        const Handle<Scene>& parent_scene,
        const TResourceHandle<RenderWorld>& world_resource_handle,
        const TResourceHandle<RenderCamera>& camera_resource_handle,
        const TResourceHandle<RenderShadowMap>& shadow_map_resource_handle,
        const TResourceHandle<RenderView>& view_statics_resource_handle,
        const TResourceHandle<RenderView>& view_dynamics_resource_handle,
        const ShaderRef& shader,
        RerenderShadowsSemaphore* rerender_semaphore);
    ShadowPass(const ShadowPass& other) = delete;
    ShadowPass& operator=(const ShadowPass& other) = delete;
    virtual ~ShadowPass() override;

    HYP_FORCE_INLINE const Vec3f& GetOrigin() const
    {
        return m_origin;
    }

    HYP_FORCE_INLINE void SetOrigin(const Vec3f& origin)
    {
        m_origin = origin;
    }

    virtual void CreateFramebuffer() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, RenderView* view) override;

    virtual void RenderToFramebuffer(FrameBase* frame, RenderView* view, const FramebufferRef& framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

private:
    void CreateShadowMap();
    void CreateCombineShadowMapsPass();
    void CreateComputePipelines();

    Handle<Scene> m_parent_scene;
    TResourceHandle<RenderWorld> m_world_resource_handle;
    TResourceHandle<RenderCamera> m_camera_resource_handle;
    TResourceHandle<RenderShadowMap> m_shadow_map_resource_handle;
    TResourceHandle<RenderView> m_view_statics_resource_handle;
    TResourceHandle<RenderView> m_view_dynamics_resource_handle;

    Vec3f m_origin;
    RerenderShadowsSemaphore* m_rerender_semaphore;

    Handle<Texture> m_shadow_map_statics;
    Handle<Texture> m_shadow_map_dynamics;

    UniquePtr<FullScreenPass> m_combine_shadow_maps_pass;
    ComputePipelineRef m_blur_shadow_map_pipeline;
};

HYP_CLASS()
class DirectionalLightShadowRenderer : public RenderSubsystem
{
    HYP_OBJECT_BODY(DirectionalLightShadowRenderer);

public:
    DirectionalLightShadowRenderer(
        Name name,
        const Handle<Scene>& parent_scene,
        const TResourceHandle<RenderLight>& render_light,
        Vec2u resolution,
        ShadowMapFilterMode filter_mode);

    DirectionalLightShadowRenderer(const DirectionalLightShadowRenderer& other) = delete;
    DirectionalLightShadowRenderer& operator=(const DirectionalLightShadowRenderer& other) = delete;
    virtual ~DirectionalLightShadowRenderer() override;

    HYP_FORCE_INLINE ShadowPass* GetPass() const
    {
        return m_shadow_pass.Get();
    }

    HYP_FORCE_INLINE const Handle<Camera>& GetCamera() const
    {
        return m_camera;
    }

    HYP_FORCE_INLINE const BoundingBox& GetAABB() const
    {
        return m_aabb;
    }

    HYP_FORCE_INLINE void SetAABB(const BoundingBox& aabb)
    {
        m_aabb = aabb;
    }

    HYP_FORCE_INLINE const TResourceHandle<RenderShadowMap>& GetShadowMapResourceHandle() const
    {
        return m_shadow_map_resource_handle;
    }

private:
    virtual void Init() override;     // init on render thread
    virtual void InitGame() override; // init on game thread
    virtual void OnUpdate(GameCounter::TickUnit delta) override;
    virtual void OnRender(FrameBase* frame) override;
    virtual void OnRemoved() override;

    void CreateShader();

    Handle<Scene> m_parent_scene;
    Handle<View> m_view_statics;
    Handle<View> m_view_dynamics;

    TResourceHandle<RenderLight> m_render_light;

    UniquePtr<ShadowPass> m_shadow_pass;
    Vec2u m_resolution;
    ShadowMapFilterMode m_filter_mode;

    RerenderShadowsSemaphore m_rerender_semaphore;

    ShaderRef m_shader;
    Handle<Camera> m_camera;
    BoundingBox m_aabb;

    TResourceHandle<RenderShadowMap> m_shadow_map_resource_handle;

    HashCode m_cached_octant_hash_code_statics;
    Matrix4 m_cached_view_matrix;
};

} // namespace hyperion

#endif
