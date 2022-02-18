#include "./framebuffer_2d.h"
#include "../gl_util.h"
#include "../util.h"

namespace hyperion {

std::shared_ptr<Texture> Framebuffer2D::MakeTexture(
    Framebuffer::FramebufferAttachment attachment,
    int width,
    int height,
    unsigned char *bytes
)
{
    auto attributes = Framebuffer::default_texture_attributes[Framebuffer::AttachmentToOrdinal(attachment)];

    auto texture = std::make_shared<Texture2D>(width, height, bytes);
    texture->SetInternalFormat(attributes.internal_format);
    texture->SetFormat(attributes.format);
    texture->SetFilter(attributes.mag_filter, attributes.min_filter);
    texture->SetWrapMode(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);

    return texture;
}

Framebuffer2D::Framebuffer2D(
    int width,
    int height,
    bool has_color_texture,
    bool has_depth_texture,
    bool has_normal_texture,
    bool has_position_texture,
    bool has_data_texture,
    bool has_ao_texture,
    bool has_tangents_texture,
    bool has_bitangents_texture
) : Framebuffer(width, height)
{
    // TODO: refactor this jank
    has_color_texture && (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_COLOR)] = MakeTexture(FRAMEBUFFER_ATTACHMENT_COLOR, width, height));
    has_depth_texture && (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_DEPTH)] = MakeTexture(FRAMEBUFFER_ATTACHMENT_DEPTH, width, height));
    has_normal_texture && (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_NORMALS)] = MakeTexture(FRAMEBUFFER_ATTACHMENT_NORMALS, width, height));
    has_position_texture && (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_POSITIONS)] = MakeTexture(FRAMEBUFFER_ATTACHMENT_POSITIONS, width, height));
    has_data_texture && (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_USERDATA)] = MakeTexture(FRAMEBUFFER_ATTACHMENT_USERDATA, width, height));
    has_ao_texture && (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_SSAO)] = MakeTexture(FRAMEBUFFER_ATTACHMENT_SSAO, width, height));
    has_tangents_texture && (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_TANGENTS)] = MakeTexture(FRAMEBUFFER_ATTACHMENT_TANGENTS, width, height));
    has_bitangents_texture && (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_BITANGENTS)] = MakeTexture(FRAMEBUFFER_ATTACHMENT_BITANGENTS, width, height));
}

Framebuffer2D::~Framebuffer2D()
{
    // deleted in parent destructor
}

void Framebuffer2D::Use()
{
    if (!is_created) {
        glGenFramebuffers(1, &id);
        CatchGLErrors("Failed to generate framebuffer.");

        is_created = true;
    }

    glBindFramebuffer(GL_FRAMEBUFFER, id);
    CatchGLErrors("Failed to bind framebuffer.", false);

    glViewport(0, 0, width, height);

    if (!is_uploaded) {
        unsigned int draw_buffers[FRAMEBUFFER_MAX_ATTACHMENTS - 1] = { GL_NONE }; // - 1 for depth
        int draw_buffer_index = 0;

        for (int i = 0; i < sizeof(draw_buffers) / sizeof(draw_buffers[0]); i++) {
            if (m_attachments[i] == nullptr) {
                continue;
            }

            m_attachments[i]->Begin();
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0 + i,
                GL_TEXTURE_2D,
                m_attachments[i]->GetId(),
                0
            );
            CatchGLErrors("Failed to attach color attachment to framebuffer.", false);
            m_attachments[i]->End();

            draw_buffers[draw_buffer_index++] = GL_COLOR_ATTACHMENT0 + i;
        }

        if (m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_DEPTH)] != nullptr) {
            m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_DEPTH)]->Begin();
            glFramebufferTexture2D(
                GL_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                GL_TEXTURE_2D,
                m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_DEPTH)]->GetId(),
                0
            );
            CatchGLErrors("Failed to attach depth texture to framebuffer.", false);
            m_attachments[AttachmentToOrdinal(FRAMEBUFFER_ATTACHMENT_DEPTH)]->End();
        }

        glDrawBuffers(draw_buffer_index, draw_buffers);

        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            throw std::runtime_error("Could not create framebuffer");
        }

        is_uploaded = true;
    }

}

void Framebuffer2D::Store(FramebufferAttachment attachment, std::shared_ptr<Texture> &texture)
{
    AssertSoft(m_attachments[AttachmentToOrdinal(attachment)] != nullptr);

    // What happens for depth tex?
    glReadBuffer(GL_COLOR_ATTACHMENT0 + AttachmentToOrdinal(attachment));
    CatchGLErrors("Failed to set read buffer");

    texture->Begin(false); // do not upload data (eg glTexImage2D)
    texture->CopyData(m_attachments[AttachmentToOrdinal(attachment)].get());
    texture->End();
}

} // namespace hyperion
