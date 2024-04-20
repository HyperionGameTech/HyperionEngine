/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_DEFERRED_HPP
#define HYPERION_DEFERRED_HPP

#include <rendering/FullScreenPass.hpp>
#include <rendering/PostFX.hpp>
#include <rendering/ParticleSystem.hpp>
#include <rendering/Texture.hpp>
#include <rendering/IndirectDraw.hpp>
#include <rendering/CullData.hpp>
#include <rendering/DepthPyramidRenderer.hpp>
#include <rendering/SSRRenderer.hpp>
#include <rendering/rt/RTRadianceRenderer.hpp>
#include <rendering/DOFBlur.hpp>
#include <rendering/HBAO.hpp>
#include <rendering/TemporalAA.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>

namespace hyperion {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::AttachmentUsage;

class IndirectDrawState;
class RenderEnvironment;

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

class DeferredPass : public FullScreenPass
{
    friend class DeferredRenderer;

public:
    DeferredPass(bool is_indirect_pass);
    DeferredPass(const DeferredPass &other)             = delete;
    DeferredPass &operator=(const DeferredPass &other)  = delete;
    virtual ~DeferredPass() override;

    void CreateShader();
    virtual void CreatePipeline(const RenderableAttributeSet &renderable_attributes) override;
    virtual void Create() override;
    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

private:
    const bool                                              m_is_indirect_pass;

    FixedArray<Handle<Shader>, uint(LightType::MAX)>        m_direct_light_shaders;
    FixedArray<Handle<RenderGroup>, uint(LightType::MAX)>   m_direct_light_render_groups;

    Handle<Texture>                                         m_ltc_matrix_texture;
    Handle<Texture>                                         m_ltc_brdf_texture;
    SamplerRef                                              m_ltc_sampler;
};

enum EnvGridPassMode
{
    ENV_GRID_PASS_MODE_RADIANCE,
    ENV_GRID_PASS_MODE_IRRADIANCE
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

    void CreateShader();
    virtual void Create() override;
    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

private:
    const EnvGridPassMode       m_mode;
    UniquePtr<TemporalBlending> m_temporal_blending;
    UniquePtr<FullScreenPass>   m_render_texture_to_screen_pass;
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

    TemporalBlending *GetTemporalBlending() const
        { return m_temporal_blending.Get(); }
    
    virtual void CreatePipeline(const RenderableAttributeSet &renderable_attributes) override;
    virtual void CreateCommandBuffers() override;
    virtual void Create() override;
    virtual void Record(uint frame_index) override;
    virtual void Render(Frame *frame) override;

private:
    FixedArray<Handle<RenderGroup>, ApplyReflectionProbeMode::MAX>                                  m_render_groups;
    FixedArray<FixedArray<CommandBufferRef, max_frames_in_flight>, ApplyReflectionProbeMode::MAX>   m_command_buffers;
    UniquePtr<TemporalBlending>                                                                     m_temporal_blending;
    UniquePtr<FullScreenPass>                                                                       m_render_texture_to_screen_pass;
    bool                                                                                            m_is_first_frame;
};

class DeferredRenderer
{
public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other)             = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other)  = delete;
    ~DeferredRenderer();

    FullScreenPass *GetCombinePass() const
        { return m_combine_pass.Get(); }

    PostProcessing &GetPostProcessing()
        { return m_post_processing; }

    const PostProcessing &GetPostProcessing() const
        { return m_post_processing; }

    DepthPyramidRenderer &GetDepthPyramidRenderer()
        { return m_dpr; }

    const DepthPyramidRenderer &GetDepthPyramidRenderer() const
        { return m_dpr; }

    AttachmentUsage *GetCombinedResult()
        { return m_combine_pass->GetAttachmentUsage(0); }

    const AttachmentUsage *GetCombinedResult() const
        { return m_combine_pass->GetAttachmentUsage(0); }

    const Handle<Texture> &GetMipChain() const
        { return m_mip_chain; }

    const GPUBufferRef &GetBlueNoiseBuffer() const
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

    DeferredPass                                        m_indirect_pass;
    DeferredPass                                        m_direct_pass;

    EnvGridPass                                         m_env_grid_radiance_pass;
    EnvGridPass                                         m_env_grid_irradiance_pass;
    ReflectionProbePass                                 m_reflection_probe_pass;

    PostProcessing                                      m_post_processing;
    UniquePtr<HBAO>                                     m_hbao;
    UniquePtr<TemporalAA>                               m_temporal_aa;

    Handle<Framebuffer>                                 m_opaque_fbo;
    Handle<Framebuffer>                                 m_translucent_fbo;

    UniquePtr<FullScreenPass>                           m_combine_pass;

    UniquePtr<SSRRenderer>                              m_ssr;
    DepthPyramidRenderer                                m_dpr;

    UniquePtr<DOFBlur>                                  m_dof_blur;

    FixedArray<Handle<Texture>, max_frames_in_flight>   m_results;
    Handle<Texture>                                     m_mip_chain;

    GPUBufferRef                                        m_blue_noise_buffer;
    
    CullData                                            m_cull_data;
};

} // namespace hyperion

#endif