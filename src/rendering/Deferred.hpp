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
class TemporalAA;
class PostProcessing;
class DeferredPass;
class HBAO;
class DOFBlur;
class Texture;
class RTRadianceRenderer;
struct RenderSetup;
enum LightType : uint32;
enum EnvProbeType : uint32;

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
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

protected:
    void CreateShader();

    virtual void CreatePipeline(const RenderableAttributeSet& renderable_attributes) override;

    virtual void Resize_Internal(Vec2u new_size) override;

private:
    const DeferredPassMode m_mode;

    FixedArray<ShaderRef, uint32(LT_MAX)> m_direct_light_shaders;
    FixedArray<Handle<RenderGroup>, uint32(LT_MAX)> m_direct_light_render_groups;

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
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

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
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

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
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

    virtual void RenderToFramebuffer(FrameBase* frame, const RenderSetup& rs, const FramebufferRef& framebuffer) override
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
    virtual void Render(FrameBase* frame, const RenderSetup& rs) override;

    virtual void RenderToFramebuffer(FrameBase* frame, const RenderSetup& rs, const FramebufferRef& framebuffer) override
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

/*! \brief Data and passes used for rendering a View in the Deferred Renderer. */
struct PassData
{
    Viewport viewport;

    // per-View descriptor sets
    FixedArray<DescriptorSetRef, max_frames_in_flight> descriptor_sets;

    CullData cull_data;

    virtual ~PassData() = default;
};

struct DeferredPassData : PassData
{
    int priority = 0;

    // Descriptor set used when rendering the View in FinalPass.
    DescriptorSetRef final_pass_descriptor_set;

    Handle<Texture> mip_chain;

    UniquePtr<DeferredPass> indirect_pass;
    UniquePtr<DeferredPass> direct_pass;
    UniquePtr<EnvGridPass> env_grid_radiance_pass;
    UniquePtr<EnvGridPass> env_grid_irradiance_pass;
    UniquePtr<ReflectionsPass> reflections_pass;
    UniquePtr<LightmapPass> lightmap_pass;
    UniquePtr<TonemapPass> tonemap_pass;
    UniquePtr<PostProcessing> post_processing;
    UniquePtr<HBAO> hbao;
    UniquePtr<TemporalAA> temporal_aa;
    UniquePtr<SSGI> ssgi;
    UniquePtr<FullScreenPass> combine_pass;
    UniquePtr<DepthPyramidRenderer> depth_pyramid_renderer;
    UniquePtr<DOFBlur> dof_blur;

    virtual ~DeferredPassData() override;
};

class DeferredRenderer final : public IRenderer
{
public:
    struct LastFrameData
    {
        // View pass data from the most recent frame, sorted by View priority
        uint8 frame_id = uint8(-1);

        // The pass data for the last frame (per-View), sorted by View priority.
        Array<Pair<View*, DeferredPassData*>> pass_data;

        DeferredPassData* GetPassDataForView(const View* view) const
        {
            for (const auto& pair : pass_data)
            {
                if (pair.first == view)
                {
                    return pair.second;
                }
            }

            return nullptr;
        }
    };

    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer& other) = delete;
    DeferredRenderer& operator=(const DeferredRenderer& other) = delete;
    virtual ~DeferredRenderer() override;

    HYP_FORCE_INLINE const LastFrameData& GetLastFrameData() const
    {
        return m_last_frame_data;
    }

    HYP_FORCE_INLINE const RendererConfig& GetRendererConfig() const
    {
        return m_renderer_config;
    }

    virtual void Initialize() override;
    virtual void Shutdown() override;

    virtual void RenderFrame(FrameBase* frame, const RenderSetup& rs) override;

private:
    void RenderFrameForView(FrameBase* frame, const RenderSetup& rs);

    // Called on initialization or when the view changes
    void CreateViewPassData(View* view, DeferredPassData& pass_data);
    void CreateViewFinalPassDescriptorSet(View* view, DeferredPassData& pass_data);
    void CreateViewDescriptorSets(View* view, DeferredPassData& pass_data);
    void CreateViewCombinePass(View* view, DeferredPassData& pass_data);

    void ResizeView(Viewport viewport, View* view, DeferredPassData& pass_data);

    void PerformOcclusionCulling(FrameBase* frame, const RenderSetup& rs);
    void ExecuteDrawCalls(FrameBase* frame, const RenderSetup& rs, uint32 bucket_mask);
    void GenerateMipChain(FrameBase* frame, const RenderSetup& rs, const ImageRef& src_image);

    LinkedList<DeferredPassData> m_pass_data;
    HashMap<View*, DeferredPassData*> m_view_pass_data;

    LastFrameData m_last_frame_data;

    RendererConfig m_renderer_config;
};

} // namespace hyperion

#endif