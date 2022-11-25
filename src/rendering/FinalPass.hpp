#ifndef HYPERION_V2_FINAL_PASS_HPP
#define HYPERION_V2_FINAL_PASS_HPP

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/RendererSampler.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <rendering/Renderer.hpp>
#include <rendering/Mesh.hpp>

#include <core/Handle.hpp>

namespace hyperion::v2 {

class Engine;

class FinalPass
{
public:
    FinalPass();
    ~FinalPass();

    void Create();
    void Destroy();

    void Render( Frame *frame);

private:
    Handle<RendererInstance> m_renderer_instance;
    std::vector<std::unique_ptr<renderer::Attachment>> m_render_pass_attachments;
    Handle<Mesh> m_full_screen_quad;
};

} // namespace hyperion::v2

#endif

