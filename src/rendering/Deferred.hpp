/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEFERRED_HPP
#define HYPERION_DEFERRED_HPP

#include <rendering/Renderer.hpp>
#include <rendering/FullScreenPass.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/ParticleSystem.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/CullData.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/DOFBlur.hpp>
#include <rendering/HBAO.hpp>
#include <rendering/TemporalAA.hpp>

#include <rendering/backend/RenderObject.hpp>

#include <scene/Light.hpp> // For LightType

namespace hyperion {

class IndirectDrawState;
class RenderEnvironment;
class GBuffer;
class Texture;
class DepthPyramidRenderer;
class SSRRenderer;
class SSGI;
class ShaderProperties;

using DeferredFlagBits = uint32;

enum DeferredFlags : DeferredFlagBits
{
    DEFERRED_FLAGS_NONE = 0x0,
    DEFERRED_FLAGS_VCT_ENABLED = 0x2,
    DEFERRED_FLAGS_ENV_PROBE_ENABLED = 0x4,
    DEFERRED_FLAGS_HBAO_ENABLED = 0x8,
    DEFERRED_FLAGS_HBIL_ENABLED = 0x10,
    DEFERRED_FLAGS_RT_RADIANCE_ENABLED = 0x20,
    DEFERRED_FLAGS_DDGI_ENABLED = 0x40
};

enum class DeferredPassMode
{
    INDIRECT_LIGHTING,
    DIRECT_LIGHTING
};

void GetDeferredShaderProperties(ShaderProperties& out_shader_properties);

class DeferredPass final : public FullScreenPass
{
    friend class DeferredRenderer;

public:
    DeferredPass(DeferredPassMode mode, GBuffer* gbuffer);
    DeferredPass(const DeferredPass& other) = delete;
    DeferredPass& operator=(const DeferredPass& other) = delete;
    virtual ~DeferredPass() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, ViewRenderResource* view) override;

protected:
    void CreateShader();

    virtual void CreatePipeline(const RenderableAttributeSet& renderable_attributes) override;

    virtual void Resize_Internal(Vec2u new_size) override;

private:
    const DeferredPassMode m_mode;

    FixedArray<ShaderRef, uint32(LightType::MAX)> m_direct_light_shaders;
    FixedArray<Handle<RenderGroup>, uint32(LightType::MAX)> m_direct_light_render_groups;

    Handle<Texture> m_ltc_matrix_texture;
    Handle<Texture> m_ltc_brdf_texture;
    SamplerRef m_ltc_sampler;
};

enum class EnvGridPassMode
{
    RADIANCE,
    IRRADIANCE
};

enum class ApplyEnvGridMode : uint32
{
    SH,
    VOXEL,
    LIGHT_FIELD,

    MAX
};

class TonemapPass final : public FullScreenPass
{
public:
    TonemapPass(GBuffer* gbuffer);
    TonemapPass(const TonemapPass& other) = delete;
    TonemapPass& operator=(const TonemapPass& other) = delete;
    virtual ~TonemapPass() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, ViewRenderResource* view) override;

protected:
    virtual void CreatePipeline() override;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u new_size) override;
};

class LightmapPass final : public FullScreenPass
{
public:
    LightmapPass(const FramebufferRef& framebuffer, GBuffer* gbuffer);
    LightmapPass(const LightmapPass& other) = delete;
    LightmapPass& operator=(const LightmapPass& other) = delete;
    virtual ~LightmapPass() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, ViewRenderResource* view) override;

protected:
    virtual void CreatePipeline() override;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u new_size) override;
};

class EnvGridPass final : public FullScreenPass
{
public:
    EnvGridPass(EnvGridPassMode mode, GBuffer* gbuffer);
    EnvGridPass(const EnvGridPass& other) = delete;
    EnvGridPass& operator=(const EnvGridPass& other) = delete;
    virtual ~EnvGridPass() override;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, ViewRenderResource* view) override;

    virtual void RenderToFramebuffer(FrameBase* frame, ViewRenderResource* view, const FramebufferRef& framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

protected:
    virtual void CreatePipeline() override;

private:
    virtual bool UsesTemporalBlending() const override
    {
        return m_mode == EnvGridPassMode::RADIANCE;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void Resize_Internal(Vec2u new_size) override;

    const EnvGridPassMode m_mode;
    FixedArray<Handle<RenderGroup>, uint32(ApplyEnvGridMode::MAX)> m_render_groups;
    bool m_is_first_frame;
};

class ReflectionsPass final : public FullScreenPass
{
    enum ApplyReflectionProbeMode : uint32
    {
        DEFAULT = 0,
        PARALLAX_CORRECTED,

        MAX
    };

public:
    ReflectionsPass(GBuffer* gbuffer, const ImageViewRef& mip_chain_image_view, const ImageViewRef& deferred_result_image_view);
    ReflectionsPass(const ReflectionsPass& other) = delete;
    ReflectionsPass& operator=(const ReflectionsPass& other) = delete;
    virtual ~ReflectionsPass() override;

    HYP_FORCE_INLINE const ImageViewRef& GetMipChainImageView() const
    {
        return m_mip_chain_image_view;
    }

    HYP_FORCE_INLINE const ImageViewRef& GetDeferredResultImageView() const
    {
        return m_deferred_result_image_view;
    }

    HYP_FORCE_INLINE SSRRenderer* GetSSRRenderer() const
    {
        return m_ssr_renderer.Get();
    }

    bool ShouldRenderSSR() const;

    virtual void Create() override;
    virtual void Render(FrameBase* frame, ViewRenderResource* view) override;

    virtual void RenderToFramebuffer(FrameBase* frame, ViewRenderResource* view, const FramebufferRef& framebuffer) override
    {
        HYP_NOT_IMPLEMENTED();
    }

private:
    virtual bool UsesTemporalBlending() const override
    {
        return false;
    }

    virtual bool ShouldRenderHalfRes() const override
    {
        return false;
    }

    virtual void CreatePipeline() override;
    virtual void CreatePipeline(const RenderableAttributeSet& renderable_attributes) override;

    void CreateSSRRenderer();

    virtual void Resize_Internal(Vec2u new_size) override;

    ImageViewRef m_mip_chain_image_view;
    ImageViewRef m_deferred_result_image_view;

    FixedArray<Handle<RenderGroup>, ApplyReflectionProbeMode::MAX> m_render_groups;

    UniquePtr<SSRRenderer> m_ssr_renderer;

    UniquePtr<FullScreenPass> m_render_ssr_to_screen_pass;

    bool m_is_first_frame;
};

} // namespace hyperion

#endif