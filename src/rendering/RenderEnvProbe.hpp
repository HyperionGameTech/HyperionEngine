/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDER_ENV_PROBE_HPP
#define HYPERION_RENDER_ENV_PROBE_HPP

#include <core/Handle.hpp>

#include <core/math/Matrix4.hpp>

#include <rendering/Renderer.hpp>
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

class RenderEnvProbe final : public RenderResourceBase
{
public:
    friend class EnvProbeRenderer;
    friend class ReflectionProbeRenderer;

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

    EnvProbe* m_env_probe;

    EnvProbeShaderData m_buffer_data;

    uint32 m_texture_slot;

    Vec4i m_position_in_grid;

    ShaderRef m_shader;

    // temp
    EnvProbeSphericalHarmonics m_spherical_harmonics;

    TResourceHandle<RenderView> m_render_view;
    TResourceHandle<RenderShadowMap> m_shadow_map;
};

class EnvProbeRenderer : public IRenderer
{
public:
    virtual ~EnvProbeRenderer() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& render_setup) override final;

protected:
    EnvProbeRenderer();

    virtual void RenderProbe(FrameBase* frame, const RenderSetup& render_setup, EnvProbe* env_probe) = 0;

    void CreateViewPassData(View* view, PassData& pass_data) override
    {
        HYP_NOT_IMPLEMENTED();
    }
};

class ReflectionProbeRenderer : public EnvProbeRenderer
{
public:
    ReflectionProbeRenderer();
    virtual ~ReflectionProbeRenderer() override;

    virtual void Initialize() override;
    virtual void Shutdown() override;

protected:
    void CreateShader();

    virtual void RenderProbe(FrameBase* frame, const RenderSetup& render_setup, EnvProbe* env_probe) override;

    void ComputePrefilteredEnvMap(FrameBase* frame, const RenderSetup& render_setup, EnvProbe* env_probe);
    void ComputeSH(FrameBase* frame, const RenderSetup& render_setup, EnvProbe* env_probe);

    ShaderRef m_shader;
};

} // namespace hyperion

#endif
