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
        Image::InternalFormat image_format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB
    );
    FullScreenPass(
        Handle<Shader> &&shader,
        Image::InternalFormat image_format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB
    );
    FullScreenPass(
        Handle<Shader> &&shader,
        DescriptorKey descriptor_key,
        UInt sub_descriptor_index,
        Image::InternalFormat image_format = Image::InternalFormat::TEXTURE_INTERNAL_FORMAT_RGB8_SRGB
    );
    FullScreenPass(const FullScreenPass &) = delete;
    FullScreenPass &operator=(const FullScreenPass &) = delete;
    ~FullScreenPass();
    
    CommandBuffer *GetCommandBuffer(UInt index) const { return m_command_buffers[index].get(); }

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

    PushConstantData &GetPushConstants() { return m_push_constant_data; }
    const PushConstantData &GetPushConstants() const { return m_push_constant_data; }
    void SetPushConstants(const PushConstantData &pc) { m_push_constant_data = pc; }

    void CreateRenderPass(Engine *engine);
    void Create(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void CreatePipeline(Engine *engine, const RenderableAttributeSet &renderable_attributes);
    void CreatePipeline(Engine *engine);

    void Destroy(Engine *engine);

    void Render(Engine *engine, Frame *frame);
    void Record(Engine *engine, UInt frame_index);

protected:
    void CreateQuad(Engine *engine);

    FixedArray<std::unique_ptr<CommandBuffer>, max_frames_in_flight> m_command_buffers;
    FixedArray<Handle<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
    Handle<RendererInstance> m_renderer_instance;
    Handle<Mesh> m_full_screen_quad;

    DynArray<std::unique_ptr<Attachment>> m_attachments;

    PushConstantData m_push_constant_data;

private:                         
    Image::InternalFormat m_image_format;                                    
    DescriptorKey m_descriptor_key;
    UInt m_sub_descriptor_index;
};
} // namespace hyperion::v2

#endif