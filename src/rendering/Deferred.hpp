#ifndef HYPERION_V2_DEFERRED_H
#define HYPERION_V2_DEFERRED_H

#include "FullScreenPass.hpp"
#include "PostFX.hpp"
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

enum DeferredFlags : DeferredFlagBits
{
    DEFERRED_FLAGS_NONE              = 0,
    DEFERRED_FLAGS_SSR_ENABLED       = 1 << 0,
    DEFERRED_FLAGS_VCT_ENABLED       = 1 << 1,
    DEFERRED_FLAGS_ENV_PROBE_ENABLED = 1 << 2
};

class DeferredPass : public FullScreenPass
{
    friend class DeferredRenderer;
public:
    DeferredPass(bool is_indirect_pass);
    DeferredPass(const DeferredPass &other) = delete;
    DeferredPass &operator=(const DeferredPass &other) = delete;
    ~DeferredPass();

    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void Create(Engine *engine);

    void Destroy(Engine *engine);
    void Record(Engine *engine, UInt frame_index);
    void Render(Engine *engine, Frame *frame);

private:
    const bool m_is_indirect_pass;
};

class DeferredRenderer
{
    static constexpr bool ssr_enabled = true;
    // perform occlusion culling using indirect draw
    static constexpr bool use_draw_indirect = true;

public:
    DeferredRenderer();
    DeferredRenderer(const DeferredRenderer &other) = delete;
    DeferredRenderer &operator=(const DeferredRenderer &other) = delete;
    ~DeferredRenderer();

    PostProcessing &GetPostProcessing()              { return m_post_processing; }
    const PostProcessing &GetPostProcessing() const  { return m_post_processing; }

    void Create(Engine *engine);
    void Destroy(Engine *engine);
    void Render(Engine *engine, Frame *frame);

private:
    void RenderOpaqueObjects(Engine *engine, Frame *frame, bool collect);
    void RenderTranslucentObjects(Engine *engine, Frame *frame, bool collect);

    DeferredPass                                                               m_indirect_pass;
    DeferredPass                                                               m_direct_pass;
    PostProcessing                                                             m_post_processing;

    ScreenspaceReflectionRenderer                                              m_ssr;
    DepthPyramidRenderer                                                       m_dpr;

    FixedArray<Handle<Texture>, max_frames_in_flight>                          m_mipmapped_results;
    std::unique_ptr<Sampler>                                                   m_sampler;
    std::unique_ptr<Sampler>                                                   m_depth_sampler;

    IndirectDrawState                                                          m_indirect_draw_state;
    CullData                                                                   m_cull_data;
};

} // namespace hyperion::v2

#endif