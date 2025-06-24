/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP
#define HYPERION_DIRECTIONAL_LIGHT_SHADOW_RENDERER_HPP

#include <scene/Scene.hpp>
#include <scene/Light.hpp>
#include <scene/Subsystem.hpp>

#include <scene/camera/Camera.hpp>

#include <core/utilities/EnumFlags.hpp>

#include <core/threading/Semaphore.hpp>

#include <core/object/HypObject.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderSubsystem.hpp>
#include <rendering/RenderCollection.hpp>
#include <rendering/RenderResource.hpp>
#include <rendering/RenderShadowMap.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <core/math/BoundingBox.hpp>

#include <Types.hpp>

namespace hyperion {

using RerenderShadowsSemaphore = Semaphore<int32, SemaphoreDirection::WAIT_FOR_POSITIVE, threading::AtomicSemaphoreImpl<int32, SemaphoreDirection::WAIT_FOR_POSITIVE>>;

class RenderCamera;
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
    virtual void Render(FrameBase* frame, const RenderSetup& render_setup) override;

    virtual void RenderToFramebuffer(FrameBase* frame, const RenderSetup& render_setup, const FramebufferRef& framebuffer) override
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
    TResourceHandle<RenderView> m_render_view_statics;
    TResourceHandle<RenderView> m_render_view_dynamics;

    Vec3f m_origin;
    RerenderShadowsSemaphore* m_rerender_semaphore;

    Handle<Texture> m_shadow_map_statics;
    Handle<Texture> m_shadow_map_dynamics;

    UniquePtr<FullScreenPass> m_combine_shadow_maps_pass;
    ComputePipelineRef m_blur_shadow_map_pipeline;
};

HYP_CLASS(NoScriptBindings)
class DirectionalLightShadowRenderer : public Subsystem
{
    HYP_OBJECT_BODY(DirectionalLightShadowRenderer);

public:
    DirectionalLightShadowRenderer(const Handle<Scene>& parent_scene, const Handle<Light>& light, Vec2u resolution, ShadowMapFilter filter_mode);
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

    HYP_FORCE_INLINE const TResourceHandle<RenderShadowMap>& GetShadowMap() const
    {
        return m_shadow_map_resource_handle;
    }

    virtual void OnAddedToWorld() override;
    virtual void OnRemovedFromWorld() override;
    virtual void Update(float delta) override;

private:
    virtual void Init() override;     // init on render thread

    void CreateShader();

    Handle<Scene> m_parent_scene;
    Handle<Light> m_light;
    Handle<View> m_view_statics;
    Handle<View> m_view_dynamics;

    UniquePtr<ShadowPass> m_shadow_pass;
    Vec2u m_resolution;
    ShadowMapFilter m_filter_mode;

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
