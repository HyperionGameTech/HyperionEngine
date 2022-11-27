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

#include <rendering/backend/RendererDescriptorSet.hpp>
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
using renderer::DescriptorSet;

class Engine;

struct alignas(16) VoxelUniforms
{
    Extent3D extent;
    Vector4 aabb_max;
    Vector4 aabb_min;
    UInt32 num_mipmaps;
};

static_assert(MathUtil::IsPowerOfTwo(sizeof(VoxelUniforms)));

class VoxelConeTracing
    : public EngineComponentBase<STUB_CLASS(VoxelConeTracing)>,
      public RenderComponent<VoxelConeTracing>
{
    static constexpr bool manual_mipmap_generation = true;

public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_VCT;

    static const Extent3D voxel_map_extent;
    static const Extent3D temporal_image_extent;

    struct Params
    {
        BoundingBox aabb;
    };

    VoxelConeTracing(Params &&params);
    VoxelConeTracing(const VoxelConeTracing &other) = delete;
    VoxelConeTracing &operator=(const VoxelConeTracing &other) = delete;
    ~VoxelConeTracing();

    Handle<Texture> &GetVoxelImage() { return m_voxel_image; }
    const Handle<Texture> &GetVoxelImage() const { return m_voxel_image; }

    void Init();
    void InitGame(); // init on game thread

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

    // void RenderVoxels(Frame *frame);

private:
    void CreateImagesAndBuffers();
    void CreateRenderGroup();
    void CreateComputePipelines();
    void CreateShader();
    void CreateRenderPass();
    void CreateFramebuffers();
    void CreateDescriptors();

    virtual void OnEntityAdded(Handle<Entity> &entity) override;
    virtual void OnEntityRemoved(Handle<Entity> &entity) override;
    virtual void OnEntityRenderableAttributesChanged(Handle<Entity> &entity) override;
    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Params m_params;

    Handle<Scene> m_scene;
    FixedArray<Handle<Framebuffer>, max_frames_in_flight> m_framebuffers;
    Handle<Shader> m_shader;
    Handle<RenderPass> m_render_pass;
    Handle<RenderGroup> m_render_group;
    Handle<ComputePipeline> m_clear_voxels;
    Handle<ComputePipeline> m_generate_mipmap;
    Handle<ComputePipeline> m_perform_temporal_blending;
    FixedArray<Array<std::unique_ptr<ImageView>>, max_frames_in_flight> m_mips;
    FixedArray<Array<std::unique_ptr<DescriptorSet>>, max_frames_in_flight> m_generate_mipmap_descriptor_sets;
                                                       
    Handle<Texture> m_voxel_image;
    Handle<Texture> m_temporal_blending_image;
    UniformBuffer m_uniform_buffer;
};

} // namespace hyperion::v2

#endif