#ifndef HYPERION_V2_VCT_H
#define HYPERION_V2_VCT_H

#include <Constants.hpp>

#include <rendering/RenderComponent.hpp>
#include <rendering/Texture.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Compute.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/EntityDrawCollection.hpp>
#include <rendering/Shader.hpp>
#include <scene/Scene.hpp>
#include <scene/camera/Camera.hpp>

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

struct RenderCommand_DestroyVCT;

class VoxelConeTracing
    : public EngineComponentBase<STUB_CLASS(VoxelConeTracing)>,
      public RenderComponent<VoxelConeTracing>
{
public:
    friend struct RenderCommand_DestroyVCT;

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

private:
    void CreateImagesAndBuffers();
    void CreateComputePipelines();
    void CreateShader();
    void CreateFramebuffer();
    void CreateDescriptors();

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    Params m_params;

    Handle<Camera> m_camera;
    Handle<Framebuffer> m_framebuffer;
    Handle<Shader> m_shader;
    Handle<ComputePipeline> m_clear_voxels;
    Handle<ComputePipeline> m_generate_mipmap;
    Handle<ComputePipeline> m_perform_temporal_blending;
    FixedArray<Array<std::unique_ptr<ImageView>>, max_frames_in_flight> m_mips;
    FixedArray<Array<DescriptorSetRef>, max_frames_in_flight> m_generate_mipmap_descriptor_sets;

    RenderList m_render_list;

    Handle<Texture> m_voxel_image;
    Handle<Texture> m_temporal_blending_image;
    UniformBuffer m_uniform_buffer;
};

} // namespace hyperion::v2

#endif