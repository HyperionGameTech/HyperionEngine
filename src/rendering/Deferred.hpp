/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEFERRED_HPP
#define HYPERION_DEFERRED_HPP

#include <rendering/FullScreenPass.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/ParticleSystem.hpp>
#include <rendering/Texture.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/CullData.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/DOFBlur.hpp>
#include <rendering/HBAO.hpp>
#include <rendering/TemporalAA.hpp>

#include <rendering/backend/RenderObject.hpp>

namespace hyperion {

class IndirectDrawState;
class RenderEnvironment;
class GBuffer;
class DepthPyramidRenderer;

using DeferredFlagBits = uint32;

enum DeferredFlags : DeferredFlagBits
{
    DEFERRED_FLAGS_NONE                 = 0x0,
    DEFERRED_FLAGS_SSR_ENABLED          = 0x1,
    DEFERRED_FLAGS_VCT_ENABLED          = 0x2,
    DEFERRED_FLAGS_ENV_PROBE_ENABLED    = 0x4,
    DEFERRED_FLAGS_HBAO_ENABLED         = 0x8,
    DEFERRED_FLAGS_HBIL_ENABLED         = 0x10,
    DEFERRED_FLAGS_RT_RADIANCE_ENABLED  = 0x20,
    DEFERRED_FLAGS_DDGI_ENABLED         = 0x40
};

enum class DeferredPassMode
{
    INDIRECT_LIGHTING,
    DIRECT_LIGHTING
};

class DeferredPass : public FullScreenPass
{
    friend class DeferredRenderer;

public:
    DeferredPass(DeferredPassMode mode);
    DeferredPass(const DeferredPass &other)             = delete;
    DeferredPass &operator=(const DeferredPass &other)  = delete;
    virtual ~DeferredPass() override;

    virtual void Create() override;
    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

protected:
    void CreateShader();

    virtual void CreatePipeline() override;
    virtual void CreatePipeline(const RenderableAttributeSet &renderable_attributes) override;

    virtual void Resize_Internal(Extent2D new_size) override;

private:
    void AddToGlobalDescriptorSet();

    const DeferredPassMode                                  m_mode;

    FixedArray<ShaderRef, uint(LightType::MAX)>             m_direct_light_shaders;
    FixedArray<Handle<RenderGroup>, uint(LightType::MAX)>   m_direct_light_render_groups;

    Handle<Texture>                                         m_ltc_matrix_texture;
    Handle<Texture>                                         m_ltc_brdf_texture;
    SamplerRef                                              m_ltc_sampler;
};

enum class EnvGridPassMode
{
    RADIANCE,
    IRRADIANCE
};

class EnvGridPass : public FullScreenPass
{
public:
    EnvGridPass(EnvGridPassMode mode);
    EnvGridPass(const EnvGridPass &other)               = delete;
    EnvGridPass &operator=(const EnvGridPass &other)    = delete;
    virtual ~EnvGridPass() override;

    TemporalBlending *GetTemporalBlending() const
        { return m_temporal_blending.Get(); }

    virtual void Create() override;
    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

protected:
    void CreateShader();
    virtual void CreatePipeline() override;

private:
    void CreatePreviousTexture();
    void CreateRenderTextureToScreenPass();
    void CreateTemporalBlending();

    void AddToGlobalDescriptorSet();
    
    virtual void Resize_Internal(Extent2D new_size) override;

    const EnvGridPassMode       m_mode;
    UniquePtr<TemporalBlending> m_temporal_blending;
    UniquePtr<FullScreenPass>   m_render_texture_to_screen_pass;
    Handle<Texture>             m_previous_texture;
    bool                        m_is_first_frame;
};

class ReflectionProbePass : public FullScreenPass
{
    enum ApplyReflectionProbeMode : uint
    {
        DEFAULT = 0,
        PARALLAX_CORRECTED,

        MAX
    };

public:
    ReflectionProbePass();
    ReflectionProbePass(const ReflectionProbePass &other)               = delete;
    ReflectionProbePass &operator=(const ReflectionProbePass &other)    = delete;
    virtual ~ReflectionProbePass() override;
    
    virtual void Create() override;
    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

private:
    virtual void CreatePipeline() override;
    virtual void CreatePipeline(const RenderableAttributeSet &renderable_attributes) override;
    virtual void CreateCommandBuffers() override;

    void CreatePreviousTexture();
    void CreateRenderTextureToScreenPass();

    void AddToGlobalDescriptorSet();

    virtual void Resize_Internal(Extent2D new_size) override;

    FixedArray<Handle<RenderGroup>, ApplyReflectionProbeMode::MAX>                                  m_render_groups;
    FixedArray<FixedArray<CommandBufferRef, max_frames_in_flight>, ApplyReflectionProbeMode::MAX>   m_command_buffers;
    UniquePtr<FullScreenPass>                                                                       m_render_texture_to_screen_pass;
    Handle<Texture>                                                                                 m_previous_texture;
    bool                                                                                            m_is_first_frame;
};

class DeferredRenderer
{
public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other)             = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other)  = delete;
    ~DeferredRenderer();

    HYP_FORCE_INLINE GBuffer *GetGBuffer() const
        { return m_gbuffer.Get(); }

    HYP_FORCE_INLINE FullScreenPass *GetCombinePass() const
        { return m_combine_pass.Get(); }

    HYP_FORCE_INLINE PostProcessing &GetPostProcessing()
        { return m_post_processing; }

    HYP_FORCE_INLINE const PostProcessing &GetPostProcessing() const
        { return m_post_processing; }

    HYP_FORCE_INLINE DepthPyramidRenderer *GetDepthPyramidRenderer() const
        { return m_depth_pyramid_renderer.Get(); }

    HYP_FORCE_INLINE const AttachmentRef &GetCombinedResult() const
        { return m_combine_pass->GetAttachment(0); }

    HYP_FORCE_INLINE const Handle<Texture> &GetMipChain() const
        { return m_mip_chain; }

    HYP_FORCE_INLINE const GPUBufferRef &GetBlueNoiseBuffer() const
        { return m_blue_noise_buffer; }

    void Create();
    void Destroy();
    
    void Render(Frame *frame, RenderEnvironment *environment);

private:
    void ApplyCameraJitter();

    void CreateBlueNoiseBuffer();

    void CreateCombinePass();
    void CreateDescriptorSets();

    void CollectDrawCalls(Frame *frame);

    void RenderSkybox(Frame *frame);
    void RenderOpaqueObjects(Frame *frame);
    void RenderTranslucentObjects(Frame *frame);

    void GenerateMipChain(Frame *frame, Image *image);

    UniquePtr<GBuffer>                                  m_gbuffer;

    UniquePtr<DeferredPass>                             m_indirect_pass;
    UniquePtr<DeferredPass>                             m_direct_pass;

    UniquePtr<EnvGridPass>                              m_env_grid_radiance_pass;
    UniquePtr<EnvGridPass>                              m_env_grid_irradiance_pass;
    UniquePtr<ReflectionProbePass>                      m_reflection_probe_pass;

    PostProcessing                                      m_post_processing;
    UniquePtr<HBAO>                                     m_hbao;
    UniquePtr<TemporalAA>                               m_temporal_aa;

    FramebufferRef                                      m_opaque_fbo;
    FramebufferRef                                      m_translucent_fbo;

    UniquePtr<FullScreenPass>                           m_combine_pass;

    UniquePtr<SSRRenderer>                              m_ssr;
    UniquePtr<DepthPyramidRenderer>                     m_depth_pyramid_renderer;

    UniquePtr<DOFBlur>                                  m_dof_blur;

    FixedArray<Handle<Texture>, max_frames_in_flight>   m_results;
    Handle<Texture>                                     m_mip_chain;

    GPUBufferRef                                        m_blue_noise_buffer;
    
    CullData                                            m_cull_data;

    bool                                                m_is_initialized;
};

} // namespace hyperion

#endif