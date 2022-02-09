#include "post_processing.h"
#include "filters/default_filter.h"
#include "../../math/math_util.h"
#include "../../core_engine.h"
#include "../../gl_util.h"
#include "../../util/mesh_factory.h"

namespace hyperion {
PostProcessing::PostProcessing()
    : m_render_scale(Vector2::One()),
      m_chained_textures_initialized(false),
      m_blit_framebuffer(nullptr)
{
    m_quad = MeshFactory::CreateQuad();

    AddFilter<DefaultFilter>("default", MathUtil::MaxSafeValue<int>());
}

PostProcessing::~PostProcessing()
{
    if (m_blit_framebuffer != nullptr) {
        delete m_blit_framebuffer;
    }
}

void PostProcessing::RemoveFilter(const std::string &tag)
{
    const auto it = std::find_if(m_filters.begin(), m_filters.end(), [&tag](const Filter &filter) {
        return filter.tag == tag;
    });

    if (it != m_filters.end()) {
        m_filters.erase(it);
    }
}

void PostProcessing::Render(Camera *cam, Framebuffer2D *fbo)
{
    hard_assert(!m_filters.empty());

    if (!m_chained_textures_initialized) {
        for (int i = 0; i < m_chained_textures.size(); i++) {
            Framebuffer::FramebufferAttachment attachment = Framebuffer::OrdinalToAttachment(i);

            if (Framebuffer::default_texture_attributes[i].is_volatile) {
                m_chained_textures[i] = Framebuffer2D::MakeTexture(
                    attachment,
                    fbo->GetWidth(),
                    fbo->GetHeight(),
                    nullptr
                );
            } else {
                // no need to copy
                m_chained_textures[i] = fbo->GetAttachment(attachment);
            }
        }

        m_chained_textures_initialized = true;
    }

    if (m_blit_framebuffer == nullptr) {
        m_blit_framebuffer = new Framebuffer2D(
            fbo->GetWidth(),
            fbo->GetHeight(),
            true,
            true,
            true,
            true,
            true,
            true
        );
    }

    // Copy data from FBO into our textures
    CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, fbo->GetId());

    // TODO: maybe for first pass we can just use texture from fbo without copying...
    /// but is there a point if we have to copy anyway?
    for (int i = 0; i < m_chained_textures.size(); i++) {
        Framebuffer::FramebufferAttachment attachment = Framebuffer::OrdinalToAttachment(i);

        if (!Framebuffer::default_texture_attributes[i].is_volatile) {
            continue;
        }

        fbo->Store(attachment, m_chained_textures[i]);
    }

    CoreEngine::GetInstance()->BindFramebuffer(GL_READ_FRAMEBUFFER, 0);

    CoreEngine::GetInstance()->DepthMask(false);
    CoreEngine::GetInstance()->Disable(CoreEngine::GLEnums::DEPTH_TEST);
    CoreEngine::GetInstance()->Viewport(0, 0, cam->GetWidth(), cam->GetHeight());

    m_blit_framebuffer->Use();

    // optimization: if counter == m_filters.size() - 1, end the fbo. then we can avoid copying textures one time,
    // as well as not having to blit the whole fbo to the backbuffer.
    int counter = 0;
    bool in_fbo = true;
    const int counter_max = m_filters.size() - 1;

    for (auto &&it : m_filters) {
        CoreEngine::GetInstance()->Clear(CoreEngine::GLEnums::COLOR_BUFFER_BIT | CoreEngine::GLEnums::DEPTH_BUFFER_BIT);

        if (counter++ == counter_max) {
            m_blit_framebuffer->End();
            in_fbo = false;
        }

        it.filter->Begin(cam, m_chained_textures);

        m_quad->Render(nullptr, cam); // TODO

        it.filter->End(cam, m_blit_framebuffer, m_chained_textures, in_fbo); // store
    }

    CoreEngine::GetInstance()->DepthMask(true);
    CoreEngine::GetInstance()->Enable(CoreEngine::GLEnums::DEPTH_TEST);
}

} // namespace hyperion
