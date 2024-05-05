/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_FULL_SCREEN_PASS_HPP
#define HYPERION_FULL_SCREEN_PASS_HPP

#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererFramebuffer.hpp>
#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <core/Containers.hpp>

#include <Constants.hpp>

#include <Types.hpp>

namespace hyperion {

using renderer::Frame;
using renderer::CommandBuffer;
using renderer::Image;
using renderer::ImageView;
using renderer::Pipeline;
using renderer::PushConstantData;

class Engine;

class HYP_API FullScreenPass
{
public:
    FullScreenPass();

    FullScreenPass(
        InternalFormat image_format,
        Extent2D extent = Extent2D { 0, 0 }
    );

    FullScreenPass(
        const ShaderRef &shader,
        InternalFormat image_format = InternalFormat::RGB8_SRGB,
        Extent2D extent = Extent2D { 0, 0 }
    );

    FullScreenPass(
        const ShaderRef &shader,
        const DescriptorTableRef &descriptor_table,
        InternalFormat image_format = InternalFormat::RGB8_SRGB,
        Extent2D extent = Extent2D { 0, 0 }
    );

    FullScreenPass(const FullScreenPass &)              = delete;
    FullScreenPass &operator=(const FullScreenPass &)   = delete;
    virtual ~FullScreenPass();

    Extent2D GetExtent() const
        { return m_extent; }

    InternalFormat GetFormat() const
        { return m_image_format; }

    const AttachmentRef &GetAttachment(uint attachment_index) const
        { return GetFramebuffer()->GetAttachment(attachment_index); }

    const CommandBufferRef &GetCommandBuffer(uint index) const
        { return m_command_buffers[index]; }

    const FramebufferRef &GetFramebuffer() const
        { return m_framebuffer; }

    const ShaderRef &GetShader() const
        { return m_shader; }

    void SetShader(const ShaderRef &shader);

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
        { SetPushConstants(PushConstantData(ptr, size)); }

    const BlendFunction &GetBlendFunction() const
        { return m_blend_function; }

    /*! \brief Sets the blend function of the render pass.
        Must be set before Create() is called. */
    void SetBlendFunction(const BlendFunction &blend_function);

    virtual void CreateCommandBuffers();
    virtual void CreateFramebuffer();
    virtual void CreatePipeline(const RenderableAttributeSet &renderable_attributes);
    virtual void CreatePipeline();
    virtual void CreateDescriptors();

    /*! \brief Create the full screen pass */
    virtual void Create();

    /*! \brief Destroy the full screen pass */
    virtual void Destroy();

    virtual void Render(Frame *frame);
    virtual void Record(uint frame_index);

    void Begin(Frame *frame);
    void End(Frame *frame);

protected:
    void CreateQuad();

    FixedArray<CommandBufferRef, max_frames_in_flight>  m_command_buffers;
    FramebufferRef                                      m_framebuffer;
    ShaderRef                                           m_shader;
    Handle<RenderGroup>                                 m_render_group;
    Handle<Mesh>                                        m_full_screen_quad;
    Extent2D                                            m_extent;

    PushConstantData                                    m_push_constant_data;

    InternalFormat                                      m_image_format;

    BlendFunction                                       m_blend_function;

    Optional<DescriptorTableRef>                        m_descriptor_table;
};
} // namespace hyperion

#endif
