#ifndef HYPERION_V2_GAUSSIAN_SPLATTING_HPP
#define HYPERION_V2_GAUSSIAN_SPLATTING_HPP

#include <Constants.hpp>
#include <core/Containers.hpp>
#include <core/ThreadSafeContainer.hpp>
#include <Threads.hpp>

#include <math/Vector3.hpp>
#include <math/BoundingBox.hpp>

#include <util/img/Bitmap.hpp>

#include <rendering/FullScreenPass.hpp>
#include <rendering/RenderGroup.hpp>
#include <rendering/Compute.hpp>
#include <rendering/Framebuffer.hpp>
#include <rendering/Shader.hpp>
#include <rendering/Mesh.hpp>
#include <rendering/Texture.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererStructs.hpp>
#include <rendering/backend/RendererFrame.hpp>

#include <atomic>
#include <mutex>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::Frame;
using renderer::Device;
using renderer::DescriptorSet;
using renderer::Result;

class Engine;

struct alignas(16) GaussianSplattingPoint {
    alignas(16) Vector4    position;
    alignas(16) Quaternion rotation;
    alignas(16) Vector4    scale;
    alignas(16) Vector4    color;
};

static_assert(sizeof(GaussianSplattingPoint) == 64);

struct GaussianSplattingModelData {
    Array<GaussianSplattingPoint> points;
    Transform transform;
};

class GaussianSplattingInstance
    : public EngineComponentBase<STUB_CLASS(GaussianSplattingInstance)>
{
public:
    enum SortStage
    {
        SORT_STAGE_FIRST,
        SORT_STAGE_SECOND,
        SORT_STAGE_MAX
    };

    GaussianSplattingInstance(RC<GaussianSplattingModelData> model);
    GaussianSplattingInstance(const GaussianSplattingInstance &other) = delete;
    GaussianSplattingInstance &operator=(const GaussianSplattingInstance &other) = delete;
    ~GaussianSplattingInstance();

    const RC<GaussianSplattingModelData> &GetModel() const
        { return m_model; }

    const FixedArray<FixedArray<DescriptorSetRef, SortStage::SORT_STAGE_MAX>, max_frames_in_flight> &GetDescriptorSets() const
        { return m_descriptor_sets; }

    GPUBufferRef &GetSplatBuffer()
        { return m_splat_buffer; }

    const GPUBufferRef &GetSplatBuffer() const
        { return m_splat_buffer; }

    GPUBufferRef &GetIndirectBuffer()
        { return m_indirect_buffer; }

    const GPUBufferRef &GetIndirectBuffer() const
        { return m_indirect_buffer; }

    Handle<RenderGroup> &GetRenderGroup()
        { return m_render_group; }

    const Handle<RenderGroup> &GetRenderGroup() const
        { return m_render_group; }

    Handle<ComputePipeline> &GetUpdateSplatsComputePipeline()
        { return m_update_splats; }

    const Handle<ComputePipeline> &GetUpdateSplatsComputePipeline() const
        { return m_update_splats; }

    Handle<ComputePipeline> &GetSortSplatsComputePipeline()
        { return m_sort_splats; }

    const Handle<ComputePipeline> &GetSortSplatsComputePipeline() const
        { return m_sort_splats; }

    void Init();
    void Record(Frame *frame);

private:
    void CreateBuffers();
    void CreateShader();
    void CreateDescriptorSets();
    void CreateRenderGroup();
    void CreateComputePipelines();

    RC<GaussianSplattingModelData> m_model;
    FixedArray<FixedArray<DescriptorSetRef, SortStage::SORT_STAGE_MAX>, max_frames_in_flight> m_descriptor_sets;
    GPUBufferRef m_splat_buffer;
    GPUBufferRef m_splat_indices_buffer;
    GPUBufferRef m_scene_buffer;
    GPUBufferRef m_indirect_buffer;
    Handle<ComputePipeline> m_update_splats;
    Handle<ComputePipeline> m_update_splat_distances;
    Handle<ComputePipeline> m_sort_splats;
    Handle<ComputePipeline> m_sort_splats_transpose;
    Handle<Shader> m_shader;
    Handle<RenderGroup> m_render_group;

    // inefficient cpu-based sort, just to test
    Array<UInt32> m_cpu_sorted_indices;
    Array<Float32> m_cpu_distances;
};

class GaussianSplatting
    : public EngineComponentBase<STUB_CLASS(GaussianSplatting)>
{
public:
    GaussianSplatting();
    GaussianSplatting(const GaussianSplatting &other) = delete;
    GaussianSplatting &operator=(const GaussianSplatting &other) = delete;
    ~GaussianSplatting();

    const Handle<GaussianSplattingInstance> &GetGaussianSplattingInstance() const
        { return m_gaussian_splatting_instance; }

    void SetGaussianSplattingInstance(Handle<GaussianSplattingInstance> gaussian_splatting_instance);

    void Init();

    void UpdateSplats(Frame *frame);

    void Render(Frame *frame);

private:
    void CreateBuffers();
    void CreateCommandBuffers();

    Handle<Mesh> m_quad_mesh;

    // for zeroing out data
    GPUBufferRef m_staging_buffer;

    // for each frame in flight - have an array of command buffers to use
    // for async command buffer recording. size will never change once created
    FixedArray<CommandBufferRef, max_frames_in_flight> m_command_buffers;

    Handle<GaussianSplattingInstance> m_gaussian_splatting_instance;
};

} // namespace hyperion::v2

#endif