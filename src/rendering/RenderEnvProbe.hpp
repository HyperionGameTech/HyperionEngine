/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_ENV_PROBE_HPP
#define HYPERION_RENDER_ENV_PROBE_HPP

#include <core/Handle.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/RenderResource.hpp>
#include <rendering/RenderCollection.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <Types.hpp>

namespace hyperion {

class EnvProbe;
class Texture;
class CameraRenderResource;
class SceneRenderResource;
class ShadowMapRenderResource;
class ViewRenderResource;

struct EnvProbeSphericalHarmonics
{
    Vec4f values[9];
};

struct EnvProbeShaderData
{
    Matrix4 face_view_matrices[6];

    Vec4f aabb_max;
    Vec4f aabb_min;
    Vec4f world_position;

    uint32 texture_index;
    uint32 flags;
    float camera_near;
    float camera_far;

    Vec2u dimensions;
    uint64 visibility_bits;
    Vec4i position_in_grid;

    EnvProbeSphericalHarmonics sh;
};

static constexpr uint32 max_env_probes = (32ull * 1024ull * 1024ull) / sizeof(EnvProbeShaderData);

class EnvProbeRenderResource final : public RenderResourceBase
{
public:
    EnvProbeRenderResource(EnvProbe* env_probe);
    virtual ~EnvProbeRenderResource() override;

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE EnvProbe* GetEnvProbe() const
    {
        return m_env_probe;
    }

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE uint32 GetTextureSlot() const
    {
        return m_texture_slot;
    }

    void SetTextureSlot(uint32 texture_slot);

    HYP_FORCE_INLINE const Vec4i& GetPositionInGrid() const
    {
        return m_position_in_grid;
    }

    void SetPositionInGrid(const Vec4i& position_in_grid);

    /*! \note Only to be called from render thread or render task */
    HYP_FORCE_INLINE const EnvProbeShaderData& GetBufferData() const
    {
        return m_buffer_data;
    }

    void SetBufferData(const EnvProbeShaderData& buffer_data);

    HYP_FORCE_INLINE const TResourceHandle<CameraRenderResource>& GetCameraRenderResourceHandle() const
    {
        return m_camera_render_resource_handle;
    }

    void SetCameraResourceHandle(TResourceHandle<CameraRenderResource>&& camera_render_resource_handle);

    HYP_FORCE_INLINE const TResourceHandle<SceneRenderResource>& GetSceneRenderResourceHandle() const
    {
        return m_scene_render_resource_handle;
    }

    void SetSceneResourceHandle(TResourceHandle<SceneRenderResource>&& scene_render_resource_handle);

    HYP_FORCE_INLINE const TResourceHandle<ViewRenderResource>& GetViewRenderResourceHandle() const
    {
        return m_view_render_resource_handle;
    }

    void SetViewResourceHandle(TResourceHandle<ViewRenderResource>&& view_render_resource_handle);

    HYP_FORCE_INLINE const TResourceHandle<ShadowMapRenderResource>& GetShadowMapRenderResourceHandle() const
    {
        return m_shadow_map_render_resource_handle;
    }

    void SetShadowMapResourceHandle(TResourceHandle<ShadowMapRenderResource>&& shadow_map_render_resource_handle);

    HYP_FORCE_INLINE const Handle<Texture>& GetPrefilteredEnvMap() const
    {
        return m_prefiltered_env_map;
    }

    HYP_FORCE_INLINE const FramebufferRef& GetFramebuffer() const
    {
        return m_framebuffer;
    }

    HYP_FORCE_INLINE const ShaderRef& GetShader() const
    {
        return m_shader;
    }

    HYP_FORCE_INLINE const EnvProbeSphericalHarmonics& GetSphericalHarmonics() const
    {
        return m_spherical_harmonics;
    }

    void SetSphericalHarmonics(const EnvProbeSphericalHarmonics& spherical_harmonics);

    void EnqueueBind();
    void EnqueueUnbind();

    void Render(FrameBase* frame);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const override;

private:
    void CreateShader();
    void CreateFramebuffer();

    void UpdateBufferData();

    void SetEnvProbeTexture();

    bool ShouldComputePrefilteredEnvMap() const;
    void ComputePrefilteredEnvMap(FrameBase* frame);

    bool ShouldComputeSphericalHarmonics() const;
    void ComputeSH(FrameBase* frame);

    EnvProbe* m_env_probe;

    EnvProbeShaderData m_buffer_data;

    uint32 m_texture_slot;

    Vec4i m_position_in_grid;

    FramebufferRef m_framebuffer;
    ShaderRef m_shader;

    Handle<Texture> m_prefiltered_env_map;

    EnvProbeSphericalHarmonics m_spherical_harmonics;

    TResourceHandle<CameraRenderResource> m_camera_render_resource_handle;
    TResourceHandle<SceneRenderResource> m_scene_render_resource_handle;
    TResourceHandle<ViewRenderResource> m_view_render_resource_handle;
    TResourceHandle<ShadowMapRenderResource> m_shadow_map_render_resource_handle;
};

} // namespace hyperion

#endif
