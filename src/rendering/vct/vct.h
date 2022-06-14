#ifndef HYPERION_V2_VCT_H
#define HYPERION_V2_VCT_H

#include <rendering/texture.h>
#include <rendering/graphics.h>
#include <rendering/compute.h>
#include <rendering/framebuffer.h>
#include <rendering/shader.h>
#include <rendering/render_pass.h>
#include <scene/scene.h>

#include <rendering/backend/renderer_command_buffer.h>
#include <rendering/backend/renderer_image.h>
#include <rendering/backend/renderer_structs.h>
#include <rendering/backend/renderer_frame.h>

#include <core/lib/flat_map.h>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::StorageBuffer;
using renderer::Frame;

class Engine;

struct alignas(16) VoxelUniforms {
    Extent3D extent;
    Vector4  aabb_max;
    Vector4  aabb_min;
    uint32_t num_mipmaps;
};

static_assert(MathUtil::IsPowerOfTwo(sizeof(VoxelUniforms)));

class VoxelConeTracing : public EngineComponentBase<STUB_CLASS(VoxelConeTracing)> {
public:
    static const Extent3D voxel_map_size;

    struct Params {
        BoundingBox aabb;
    };

    VoxelConeTracing(Params &&params);
    VoxelConeTracing(const VoxelConeTracing &other) = delete;
    VoxelConeTracing &operator=(const VoxelConeTracing &other) = delete;
    ~VoxelConeTracing();

    Ref<Texture> &GetVoxelImage()             { return m_voxel_image; }
    const Ref<Texture> &GetVoxelImage() const { return m_voxel_image; }

    void Init(Engine *engine);
    void RenderVoxels(Engine *engine, Frame *frame);

private:
    void CreateImagesAndBuffers(Engine *engine);
    void CreateGraphicsPipeline(Engine *engine);
    void CreateComputePipelines(Engine *engine);
    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateFramebuffer(Engine *engine);
    void CreateDescriptors(Engine *engine);

    Params                m_params;

    Ref<Scene>            m_scene;
    Ref<Framebuffer>      m_framebuffer;
    Ref<Shader>           m_shader;
    Ref<RenderPass>       m_render_pass;
    Ref<GraphicsPipeline> m_pipeline;
    Ref<ComputePipeline>  m_clear_voxels;

    Ref<Texture>          m_voxel_image;
    UniformBuffer         m_uniform_buffer;
    
    std::vector<ObserverRef<Ref<GraphicsPipeline>>>          m_pipeline_observers;
    FlatMap<GraphicsPipeline::ID, ObserverRef<Spatial *>>    m_spatial_observers;

};

} // namespace hyperion::v2

#endif