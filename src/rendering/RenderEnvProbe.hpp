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
class RenderEnvProbe;
class Texture;
class RenderCamera;
class RenderScene;
class RenderShadowMap;
class RenderView;

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

class RenderEnvProbe final : public RenderResourceBase
{
public:
    RenderEnvProbe(EnvProbe* env_probe);
    virtual ~RenderEnvProbe() override;

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

    HYP_FORCE_INLINE const TResourceHandle<RenderView>& GetViewRenderResourceHandle() const
    {
        return m_render_view;
    }

    void SetViewResourceHandle(TResourceHandle<RenderView>&& render_view);

    HYP_FORCE_INLINE const TResourceHandle<RenderShadowMap>& GetShadowMapRenderResourceHandle() const
    {
        return m_shadow_map;
    }

    void SetShadowMap(TResourceHandle<RenderShadowMap>&& shadow_map);

    HYP_FORCE_INLINE const Handle<Texture>& GetPrefilteredEnvMap() const
    {
        return m_prefiltered_env_map;
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

    void Render(FrameBase* frame, const RenderSetup& render_setup);

protected:
    virtual void Initialize_Internal() override;
    virtual void Destroy_Internal() override;
    virtual void Update_Internal() override;

    virtual GPUBufferHolderBase* GetGPUBufferHolder() const override;

private:
    void CreateShader();

    void UpdateBufferData();

    bool ShouldComputePrefilteredEnvMap() const;
    void ComputePrefilteredEnvMap(FrameBase* frame, const RenderSetup& render_setup);

    bool ShouldComputeSphericalHarmonics() const;
    void ComputeSH(FrameBase* frame, const RenderSetup& render_setup);

    EnvProbe* m_env_probe;

    EnvProbeShaderData m_buffer_data;

    uint32 m_texture_slot;

    Vec4i m_position_in_grid;

    ShaderRef m_shader;

    Handle<Texture> m_prefiltered_env_map;

    EnvProbeSphericalHarmonics m_spherical_harmonics;

    TResourceHandle<RenderView> m_render_view;
    TResourceHandle<RenderShadowMap> m_shadow_map;
};

} // namespace hyperion

#endif
