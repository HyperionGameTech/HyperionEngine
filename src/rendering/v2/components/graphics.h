#ifndef HYPERION_V2_GRAPHICS_H
#define HYPERION_V2_GRAPHICS_H

#include "shader.h"
#include "spatial.h"
#include "texture.h"
#include "framebuffer.h"
#include "render_pass.h"
#include "util.h"

#include <rendering/backend/renderer_graphics_pipeline.h>

#include <memory>
#include <vector>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::DescriptorSet;
using renderer::GPUBuffer;
using renderer::UniformBuffer;
using renderer::MeshInputAttributeSet;
using renderer::PerFrameData;

class Engine;

class GraphicsPipeline : public EngineComponent<renderer::GraphicsPipeline> {
    friend class Engine;
    friend class Spatial;

public:
    enum Bucket {
        BUCKET_BUFFER      = 0, /* Pre-pass / buffer items */

        /* === Scene objects === */
        BUCKET_OPAQUE      = 1, /* Opaque items */
        BUCKET_TRANSLUCENT = 2, /* Transparent - rendering on top of opaque objects */
        BUCKET_PARTICLE    = 3, /* Specialized particle bucket */
        BUCKET_SKYBOX      = 4 /* Rendered without depth testing/writing, and rendered first */
    };

    struct ID : EngineComponent::ID {
        Bucket bucket;

        inline bool operator==(const ID &other) const
            { return value == other.value && bucket == other.bucket; }

        inline bool operator<(const ID &other) const
            { return value < other.value && bucket < other.bucket; }
    };

    GraphicsPipeline(Shader::ID shader_id, RenderPass::ID render_pass_id, Bucket bucket);
    GraphicsPipeline(const GraphicsPipeline &other) = delete;
    GraphicsPipeline &operator=(const GraphicsPipeline &other) = delete;
    ~GraphicsPipeline();

    inline Shader::ID GetShaderID() const { return m_shader_id; }
    inline RenderPass::ID GetRenderPassID() const { return m_render_pass_id; }
    inline Bucket GetBucket() const { return m_bucket; }

    inline const MeshInputAttributeSet &GetVertexAttributes() const { return m_vertex_attributes; }

    inline VkPrimitiveTopology GetTopology() const
        { return m_topology; }
    inline void SetTopology(VkPrimitiveTopology topology)
        { m_topology = topology; }

    inline auto GetFillMode() const
        { return m_fill_mode; }
    inline void SetFillMode(renderer::GraphicsPipeline::FillMode fill_mode)
        { m_fill_mode = fill_mode; }

    inline auto GetCullMode() const
        { return m_cull_mode; }
    inline void SetCullMode(renderer::GraphicsPipeline::CullMode cull_mode)
        { m_cull_mode = cull_mode; }

    inline bool GetDepthTest() const
        { return m_depth_test; }
    inline void SetDepthTest(bool depth_test)
        { m_depth_test = depth_test; }

    inline bool GetDepthWrite() const
        { return m_depth_write; }
    inline void SetDepthWrite(bool depth_write)
        { m_depth_write = depth_write; }

    inline bool GetBlendEnabled() const
        { return m_blend_enabled; }
    inline void SetBlendEnabled(bool blend_enabled)
        { m_blend_enabled = blend_enabled; }

    void AddSpatial(Engine *engine, Spatial::ID id);
    void RemoveSpatial(Engine *engine, Spatial::ID id);

    /* Non-owned objects - owned by `engine`, used by the pipeline */

    inline void AddFramebuffer(Framebuffer::ID id) { m_fbo_ids.Add(id); }
    inline void RemoveFramebuffer(Framebuffer::ID id) { m_fbo_ids.Remove(id); }
    
    /* Build pipeline */
    void Create(Engine *engine);
    void Destroy(Engine *engine);

private:
    /* Called from Spatial - remove the pointer */
    void OnSpatialRemoved(Spatial *spatial);

    void Render(Engine *engine, CommandBuffer *primary_command_buffer, uint32_t frame_index);

    Shader::ID m_shader_id;
    RenderPass::ID m_render_pass_id;
    Bucket m_bucket;
    VkPrimitiveTopology m_topology;
    renderer::GraphicsPipeline::CullMode m_cull_mode;
    renderer::GraphicsPipeline::FillMode m_fill_mode;
    bool m_depth_test;
    bool m_depth_write;
    bool m_blend_enabled;
    MeshInputAttributeSet m_vertex_attributes;

    ObjectIdHolder<Texture> m_texture_ids;
    ObjectIdHolder<Framebuffer> m_fbo_ids;

    std::vector<std::pair<Spatial::ID, Spatial *>> m_spatials;

    PerFrameData<CommandBuffer> *m_per_frame_data;
};

} // namespace hyperion::v2

#endif