#ifndef HYPERION_V2_FINAL_PASS_HPP
#define HYPERION_V2_FINAL_PASS_HPP

#include <Config.hpp>

#include <rendering/DefaultFormats.hpp>
#include <rendering/SafeDeleter.hpp>
#include <rendering/backend/RenderObject.hpp>

#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>

#include <Types.hpp>

namespace hyperion::v2 {

class ShaderManagerSystem;

extern ShaderManagerSystem *g_shader_manager;

class FinalPass
{
public:
    FinalPass();
    ~FinalPass();

    const Array<AttachmentRef> &GetAttachments() const
        { return m_attachments; }

    const Handle<RenderGroup> &GetRenderGroup() const
        { return m_render_group; }

    void Create();
    void Destroy();

    void Render(Frame *frame);

private:
    Array<AttachmentRef>    m_attachments;
    Handle<RenderGroup>     m_render_group;
    Handle<Mesh>            m_quad;
};
} // namespace hyperion::v2

#endif