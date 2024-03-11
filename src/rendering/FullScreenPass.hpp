#ifndef HYPERION_V2_FULL_SCREEN_PASS_H
#define HYPERION_V2_FULL_SCREEN_PASS_H

#include "Framebuffer.hpp"
#include "Shader.hpp"
#include "RenderGroup.hpp"
#include "Mesh.hpp"
#include <Constants.hpp>

#include <Types.hpp>
#include <core/Containers.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Attachment;
using renderer::Frame;
using renderer::CommandBuffer;
using renderer::Image;
using renderer::ImageView;
using renderer::Pipeline;

class Engine;

class FullScreenPass
{
    using PushConstantData = Pipeline::PushConstantData;

public:
    FullScreenPass(
        InternalFormat image_format = InternalFormat::RGB8_SRGB,
        Extent2D extent = Extent2D { 0, 0 }
    );

    FullScreenPass(
        const Handle<Shader> &shader,
        InternalFormat image_format = InternalFormat::RGB8_SRGB,
        Extent2D extent = Extent2D { 0, 0 }
    );

    FullScreenPass(
        const Handle<Shader> &shader,
        DescriptorTableRef descriptor_table,
        InternalFormat image_format = InternalFormat::RGB8_SRGB,
        Extent2D extent = Extent2D { 0, 0 }
    );

    FullScreenPass(const FullScreenPass &) = delete;
    FullScreenPass &operator=(const FullScreenPass &) = delete;
    virtual ~FullScreenPass();

    const AttachmentUsageRef &GetAttachmentUsage(uint attachment_index) const
        { return GetFramebuffer()->GetAttachmentUsages()[attachment_index]; }

    const Array<AttachmentRef> &GetAttachments() const
        { return m_attachments; }

    const CommandBufferRef &GetCommandBuffer(uint index) const
        { return m_command_buffers[index]; }

    const Handle<Framebuffer> &GetFramebuffer() const
        { return m_framebuffer; }

    const Handle<Shader> &GetShader() const
        { return m_shader; }

    void SetShader(const Handle<Shader> &shader);

    const Handle<Mesh> &GetQuadMesh() const
        { return m_full_screen_quad; }

    const Handle<RenderGroup> &GetRenderGroup() const
        { return m_render_group; }

    PushConstantData &GetPushConstants()
        { return m_push_constant_data; }

    const PushConstantData &GetPushConstants() const
        { return m_push_constant_data; }

    void SetPushConstants(const PushConstantData &pc)
        { m_push_constant_data = pc; }

    void SetPushConstants(const void *ptr, SizeType size)
    {
        AssertThrow(size <= sizeof(m_push_constant_data));

        Memory::MemCpy(&m_push_constant_data, ptr, size);

        if (size < sizeof(m_push_constant_data)) {
            Memory::MemSet(&m_push_constant_data + size, 0, sizeof(m_push_constant_data) - size);
        }
    }

    virtual void CreateCommandBuffers();
    virtual void CreateFramebuffer();
    virtual void CreatePipeline(const RenderableAttributeSet &renderable_attributes);
    virtual void CreatePipeline();
    virtual void CreateDescriptors();

    virtual void Create();
    virtual void Destroy();

    virtual void Render(Frame *frame);
    virtual void Record(uint frame_index);

    void Begin(Frame *frame);
    void End(Frame *frame);

protected:
    void CreateQuad();

    FixedArray<CommandBufferRef, max_frames_in_flight>  m_command_buffers;
    Handle<Framebuffer>                                 m_framebuffer;
    Handle<Shader>                                      m_shader;
    Handle<RenderGroup>                                 m_render_group;
    Handle<Mesh>                                        m_full_screen_quad;
    Extent2D                                            m_extent;

    Array<AttachmentRef>                                m_attachments;

    PushConstantData                                    m_push_constant_data;

    InternalFormat                                      m_image_format;

    Optional<DescriptorTableRef>                        m_descriptor_table;
};
} // namespace hyperion::v2

#endif
