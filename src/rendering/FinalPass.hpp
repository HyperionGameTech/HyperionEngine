#ifndef HYPERION_V2_FINAL_PASS_HPP
#define HYPERION_V2_FINAL_PASS_HPP

#include <Config.hpp>

#include <rendering/DefaultFormats.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/FullScreenPass.hpp>

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class ShaderManagerSystem;

extern HYP_API ShaderManagerSystem *g_shader_manager;

// Performs tonemapping, samples last postfx in chain
class CompositePass : public FullScreenPass
{
public:
    CompositePass();
    CompositePass(const CompositePass &other)               = delete;
    CompositePass &operator=(const CompositePass &other)    = delete;
    virtual ~CompositePass() override;

    void CreateShader();
    virtual void Create() override;
};

class FinalPass
{
public:
    FinalPass();
    FinalPass(const FinalPass &other)               = delete;
    FinalPass &operator=(const FinalPass &other)    = delete;
    ~FinalPass();

    const ImageRef &GetLastFrameImage() const
        { return m_last_frame_image; }

    void Create();
    void Destroy();

    void Render(Frame *frame);

private:
    Array<AttachmentRef>    m_attachments;
    Handle<RenderGroup>     m_render_group;
    Handle<Mesh>            m_quad;
    CompositePass           m_composite_pass;
    ImageRef                m_last_frame_image;
};
} // namespace hyperion::v2

#endif