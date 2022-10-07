#ifndef HYPERION_V2_DEFERRED_HPP
#define HYPERION_V2_DEFERRED_HPP

#include "FullScreenPass.hpp"
#include "PostFX.hpp"
#include "ParticleSystem.hpp"
#include "Texture.hpp"
#include "Compute.hpp"
#include "IndirectDraw.hpp"
#include "CullData.hpp"
#include "DepthPyramidRenderer.hpp"
#include "ScreenspaceReflectionRenderer.hpp"

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::Image;
using renderer::ImageView;
using renderer::Sampler;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::AttachmentRef;

using DeferredFlagBits = UInt;

class IndirectDrawState;
class RenderEnvironment;

enum DeferredFlags : DeferredFlagBits
{
    DEFERRED_FLAGS_NONE = 0,
    DEFERRED_FLAGS_SSR_ENABLED = 1 << 0,
    DEFERRED_FLAGS_VCT_ENABLED = 1 << 1,
    DEFERRED_FLAGS_ENV_PROBE_ENABLED = 1 << 2
};

class DeferredPass : public FullScreenPass
{
    friend class DeferredRenderer;

public:
    DeferredPass(bool is_indirect_pass);
    DeferredPass(const DeferredPass &other) = delete;
    DeferredPass &operator=(const DeferredPass &other) = delete;
    virtual ~DeferredPass();

    void CreateShader(Engine *engine);
    virtual void CreateRenderPass(Engine *engine) override;
    virtual void CreateDescriptors(Engine *engine) override;
    virtual void Create(Engine *engine) override;
    virtual void Destroy(Engine *engine) override;
    virtual void Record(Engine *engine, UInt frame_index) override;
    virtual void Render(Engine *engine, Frame *frame) override;

private:
    const bool m_is_indirect_pass;
};

class DeferredRenderer
{
    static constexpr bool ssr_enabled = true;
    // perform occlusion culling using indirect draw
    static constexpr bool use_draw_indirect = true;
    static const Extent2D mipmap_chain_extent;

public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    PostProcessing &GetPostProcessing() { return m_post_processing; }
    const PostProcessing &GetPostProcessing() const  { return m_post_processing; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(
        Engine *engine,
        Frame *frame,
        RenderEnvironment *environment
    );

    void RenderUI(Engine *engine, Frame *frame);

private:
    void CreateComputePipelines(Engine *engine);
    void CreateDescriptorSets(Engine *engine);

    void CollectDrawCalls(Engine *engine, Frame *frame);
    void RenderOpaqueObjects(Engine *engine, Frame *frame);
    void RenderTranslucentObjects(Engine *engine, Frame *frame);

    void GenerateMipChain(Engine *engine, Frame *frame, Image *image);

    void UpdateParticles(Engine *engine, Frame *frame, RenderEnvironment *environment);
    void RenderParticles(Engine *engine, Frame *frame, RenderEnvironment *environment);

    DeferredPass m_indirect_pass;
    DeferredPass m_direct_pass;
    PostProcessing m_post_processing;

    FixedArray<Handle<Framebuffer>, max_frames_in_flight> m_opaque_fbos;
    FixedArray<Handle<Framebuffer>, max_frames_in_flight> m_translucent_fbos;

    Handle<ComputePipeline> m_combine;
    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_combine_descriptor_sets;

    ScreenspaceReflectionRenderer m_ssr;
    DepthPyramidRenderer m_dpr;

    FixedArray<Handle<Texture>, max_frames_in_flight> m_results;
    FixedArray<Handle<Texture>, max_frames_in_flight> m_mipmapped_results;
    UniquePtr<Sampler> m_sampler;
    UniquePtr<Sampler> m_depth_sampler;
    
    CullData m_cull_data;
};

} // namespace hyperion::v2

#endif