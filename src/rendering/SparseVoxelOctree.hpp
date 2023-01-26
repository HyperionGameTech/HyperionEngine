#ifndef HYPERION_V2_SVO_H
#define HYPERION_V2_SVO_H

#include <core/Base.hpp>
#include <rendering/Voxelizer.hpp>
#include <rendering/Compute.hpp>
#include <rendering/Buffers.hpp>
#include <rendering/RenderComponent.hpp>
#include <rendering/backend/RendererStructs.hpp>

#include <core/Containers.hpp>

namespace hyperion::v2 {

using renderer::IndirectBuffer;
using renderer::StorageBuffer;
using renderer::DescriptorSet;
using renderer::ShaderVec2;

class SparseVoxelOctree
    : public EngineComponentBase<STUB_CLASS(SparseVoxelOctree)>,
      public RenderComponent<SparseVoxelOctree>
{
    static constexpr UInt32 min_nodes = 10000;
    static constexpr UInt32 max_nodes = 10000000;

    using OctreeNode = ShaderVec2<UInt32>;

public:
    static constexpr RenderComponentName component_name = RENDER_COMPONENT_SVO;

    SparseVoxelOctree();
    SparseVoxelOctree(const SparseVoxelOctree &other) = delete;
    SparseVoxelOctree &operator=(const SparseVoxelOctree &other) = delete;
    ~SparseVoxelOctree();

    Voxelizer *GetVoxelizer() const { return m_voxelizer.get(); }

    void Init();
    void InitGame(); // init on game thread

    void OnUpdate(GameCounter::TickUnit delta);
    void OnRender(Frame *frame);

private:
    UInt32 CalculateNumNodes() const;
    void CreateBuffers();
    void CreateDescriptors();
    void CreateComputePipelines();

    virtual void OnComponentIndexChanged(RenderComponentBase::Index new_index, RenderComponentBase::Index prev_index) override;

    void WriteMipmaps();

    void BindDescriptorSets(
        CommandBuffer *command_buffer,
        UInt frame_index,
        const ComputePipeline *pipeline
    ) const;

    FixedArray<UniquePtr<DescriptorSet>, max_frames_in_flight> m_descriptor_sets;
    GPUBufferRef m_voxel_uniforms;

    std::unique_ptr<Voxelizer> m_voxelizer;
    std::unique_ptr<AtomicCounter> m_counter;

    std::unique_ptr<IndirectBuffer> m_indirect_buffer;
    std::unique_ptr<StorageBuffer> m_build_info_buffer;
    std::unique_ptr<StorageBuffer> m_octree_buffer;
    
    Handle<ComputePipeline> m_init_nodes;
    Handle<ComputePipeline> m_tag_nodes;
    Handle<ComputePipeline> m_alloc_nodes;
    Handle<ComputePipeline> m_modify_args;
    Handle<ComputePipeline> m_write_mipmaps;
};

} // namespace hyperion::v2

#endif