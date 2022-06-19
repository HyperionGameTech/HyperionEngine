#ifndef HYPERION_V2_FULL_SCREEN_PASS_H
#define HYPERION_V2_FULL_SCREEN_PASS_H

#include "render_pass.h"
#include "framebuffer.h"
#include "shader.h"
#include "graphics.h"
#include "mesh.h"

#include <types.h>

#include <rendering/backend/renderer_frame.h>
#include <rendering/backend/renderer_structs.h>
#include <rendering/backend/renderer_command_buffer.h>

#include <memory>
#include <utility>

namespace hyperion::v2 {

using renderer::Frame;
using renderer::CommandBuffer;
using renderer::PerFrameData;
using renderer::VertexAttributeSet;
using renderer::DescriptorKey;

class Engine;

class FullScreenPass {
public:
    static std::unique_ptr<Mesh> full_screen_quad;
    
    FullScreenPass();
    FullScreenPass(Ref<Shader> &&shader);
    FullScreenPass(Ref<Shader> &&shader, DescriptorKey descriptor_key, UInt sub_descriptor_index);
    FullScreenPass(const FullScreenPass &) = delete;
    FullScreenPass &operator=(const FullScreenPass &) = delete;
    ~FullScreenPass();

    auto &GetFrameData()                          { return m_frame_data; }
    const auto &GetFrameData() const              { return m_frame_data; }
                                                  
    Framebuffer *GetFramebuffer() const           { return m_framebuffer.ptr; }
                                                  
    Shader *GetShader() const                     { return m_shader.ptr; }
    void SetShader(Ref<Shader> &&shader);
                                                  
    RenderPass *GetRenderPass() const             { return m_render_pass.ptr; }

    GraphicsPipeline *GetGraphicsPipeline() const { return m_pipeline.ptr; }

    UInt GetSubDescriptorIndex() const            { return m_sub_descriptor_index; }

    void CreateRenderPass(Engine *engine);
    void Create(Engine *engine);
    void CreateDescriptors(Engine *engine);
    void CreatePipeline(Engine *engine);

    void Destroy(Engine *engine);

    void Render(Engine *engine, Frame *frame);
    void Record(Engine *engine, UInt frame_index);

protected:
    void CreatePerFrameData(Engine *engine);

    std::unique_ptr<PerFrameData<CommandBuffer>> m_frame_data;
    Ref<Framebuffer>                             m_framebuffer;
    Ref<Shader>                                  m_shader;
    Ref<RenderPass>                              m_render_pass;
    Ref<GraphicsPipeline>                        m_pipeline;

    std::vector<std::unique_ptr<Attachment>>     m_attachments;

private:
    DescriptorKey                                m_descriptor_key;
    UInt                                         m_sub_descriptor_index;
};
} // namespace hyperion::v2

#endif