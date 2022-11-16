#ifndef HYPERION_V2_FULL_SCREEN_PASS_H
#define HYPERION_V2_FULL_SCREEN_PASS_H

#include "RenderPass.hpp"
#include "Framebuffer.hpp"
#include "Shader.hpp"
#include "Renderer.hpp"
#include "Mesh.hpp"
#include <Constants.hpp>

#include <Types.hpp>
#include <core/Containers.hpp>

#include <rendering/backend/RendererFrame.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>

#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::CommandBuffer;
using renderer::PerFrameData;
using renderer::VertexAttributeSet;
using renderer::DescriptorKey;
using renderer::Image;
using renderer::Pipeline;

class Engine;

class FullScreenPass
{
    using PushConstantData = Pipeline::PushConstantData;

public:
    FullScreenPass(
        InternalFormat image_format = InternalFormat::RGB8_SRGB
    );
    FullScreenPass(
        Handle<Shader> &&shader,
        InternalFormat image_format = InternalFormat::RGB8_SRGB
    );
    FullScreenPass(
        Handle<Shader> &&shader,
        DescriptorKey descriptor_key,
        UInt sub_descriptor_index,
        InternalFormat image_format = InternalFormat::RGB8_SRGB
    );
    FullScreenPass(const FullScreenPass &) = delete;
    FullScreenPass &operator=(const FullScreenPass &) = delete;
    virtual ~FullScreenPass();
    
    CommandBuffer *GetCommandBuffer(UInt index) const { return m_command_buffers[index].Get(); }

    Handle<Framebuffer> &GetFramebuffer(UInt index) { return m_framebuffers[index]; }
    const Handle<Framebuffer> &GetFramebuffer(UInt index) const { return m_framebuffers[index]; }
                                                      
    Handle<Shader> &GetShader() { return m_shader; }
    const Handle<Shader> &GetShader() const { return m_shader; }

    void SetShader(Handle<Shader> &&shader);

    Handle<RenderPass> &GetRenderPass() { return m_render_pass; }
    const Handle<RenderPass> &GetRenderPass() const { return m_render_pass; }

    Handle<RendererInstance> &GetRendererInstance() { return m_renderer_instance; }
    const Handle<RendererInstance> &GetRendererInstance() const { return m_renderer_instance; }

    UInt GetSubDescriptorIndex() const { return m_sub_descriptor_index; }

    PushConstantData &GetPushConstants()
        { return m_push_constant_data; }

    const PushConstantData &GetPushConstants() const
        { return m_push_constant_data; }

    void SetPushConstants(const PushConstantData &pc)
        { m_push_constant_data = pc; }

    void SetPushConstants(const void *ptr, SizeType size)
    {
        AssertThrow(size <= sizeof(m_push_constant_data));

        Memory::Copy(&m_push_constant_data, ptr, size);

        if (size < sizeof(m_push_constant_data)) {
            Memory::Set(&m_push_constant_data + size, 0x00, sizeof(m_push_constant_data) - size);
        }
    }

    virtual void CreateRenderPass(Engine *engine);
    virtual void CreateCommandBuffers(Engine *engine);
    virtual void CreateFramebuffers(Engine *engine);
    virtual void CreatePipeline(Engine *engine, const RenderableAttributeSet &renderable_attributes);
    virtual void CreatePipeline(Engine *engine);
    virtual void CreateDescriptors(Engine *engine) = 0;

    virtual void Create(Engine *engine);
    virtual void Destroy(Engine *engine);

    virtual void Render(Engine *engine, Frame *frame);
    virtual void Record(Engine *engine, UInt frame_index);

protected:
    void CreateQuad(Engine *engine);

    FixedArray<UniquePtr<CommandBuffer>, max_frames_in_flight> m_command_buffers;
    FixedArray<Handle<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
    Handle<RendererInstance> m_renderer_instance;
    Handle<Mesh> m_full_screen_quad;

    Array<std::unique_ptr<Attachment>> m_attachments;

    PushConstantData m_push_constant_data;

    // TODO: move to PostFXPass?                        
    InternalFormat m_image_format;                                    
    DescriptorKey m_descriptor_key;
    UInt m_sub_descriptor_index;
};
} // namespace hyperion::v2

#endif