#ifndef HYPERION_V2_VCT_H
#define HYPERION_V2_VCT_H

#include <Constants.hpp>

#include <rendering/RenderComponent.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Renderer.hpp>
#include <rendering/Compute.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/Shader.hpp>
#include <rendering/RenderPass.hpp>
#include <scene/Scene.hpp>

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <core/lib/FlatMap.hpp>
#include <core/lib/FixedArray.hpp>

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

class VoxelConeTracing : public EngineComponentBase<STUB_CLASS(VoxelConeTracing)>, public RenderComponent<VoxelConeTracing> {
public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_VCT;

    static const Extent3D voxel_map_size;

    struct Params {
        BoundingBox aabb;
    };

    VoxelConeTracing(Params &&params);
    VoxelConeTracing(const VoxelConeTracing &other) = delete;
    VoxelConeTracing &operator=(const VoxelConeTracing &other) = delete;
    ~VoxelConeTracing();

    Handle<Texture> &GetVoxelImage() { return m_voxel_image; }
    const Handle<Texture> &GetVoxelImage() const { return m_voxel_image; }

    void Init(Engine *engine);
    void InitGame(Engine *engine); // init on game thread

    void OnUpdate(Engine *engine, GameCounter::TickUnit delta);
    void OnRender(Engine *engine, Frame *frame);

    // void RenderVoxels(Engine *engine, Frame *frame);

private:
    void CreateImagesAndBuffers(Engine *engine);
    void CreateRendererInstance(Engine *engine);
    void CreateComputePipelines(Engine *engine);
    void CreateShader(Engine *engine);
    void CreateRenderPass(Engine *engine);
    void CreateFramebuffers(Engine *engine);
    void CreateDescriptors(Engine *engine);

    virtual void OnEntityAdded(Ref<Entity> &entity) override;
    virtual void OnEntityRemoved(Ref<Entity> &entity) override;
    virtual void OnEntityRenderableAttributesChanged(Ref<Entity> &entity) override;
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Params m_params;

    Handle<Scene> m_scene;
    FixedArray<Handle<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
    Handle<RendererInstance> m_renderer_instance;
    Handle<ComputePipeline> m_clear_voxels;
                                                       
    Handle<Texture> m_voxel_image;
    UniformBuffer m_uniform_buffer;
};

} // namespace hyperion::v2

#endif