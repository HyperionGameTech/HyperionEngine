#ifndef HYPERION_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_COMMAND_BUFFER_HPP

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererRenderPass.hpp>
#include <rendering/backend/RendererSemaphore.hpp>
#include <rendering/backend/RendererFence.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererBuffer.hpp>

#include <core/lib/FixedArray.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

template <PlatformType PLATFORM>
class Pipeline;

template <PlatformType PLATFORM>
class GraphicsPipeline;

template <PlatformType PLATFORM>
class ComputePipeline;

template <PlatformType PLATFORM>
class RaytracingPipeline;

template <>
class CommandBuffer<Platform::VULKAN>
{
public:
    CommandBuffer(CommandBufferType type);
    CommandBuffer(const CommandBuffer &other) = delete;
    CommandBuffer &operator=(const CommandBuffer &other) = delete;
    ~CommandBuffer();

    CommandBufferType GetType() const
        { return m_type; }

    VkCommandBuffer GetCommandBuffer() const
        { return m_command_buffer; }

    Result Create(Device<Platform::VULKAN> *device, VkCommandPool command_pool);
    Result Destroy(Device<Platform::VULKAN> *device);
    Result Begin(Device<Platform::VULKAN> *device, const RenderPass<Platform::VULKAN> *render_pass = nullptr);
    Result End(Device<Platform::VULKAN> *device);
    Result Reset(Device<Platform::VULKAN> *device);
    Result SubmitPrimary(
        VkQueue queue,
        Fence *fence,
        SemaphoreChain *semaphore_chain
    );

    Result SubmitSecondary(CommandBuffer *primary);

    void BindVertexBuffer(const GPUBuffer<Platform::VULKAN> *buffer);
    void BindIndexBuffer(const GPUBuffer<Platform::VULKAN> *buffer, DatumType datum_type = DatumType::UNSIGNED_INT);

    void DrawIndexed(
        UInt32 num_indices,
        UInt32 num_instances  = 1,
        UInt32 instance_index = 0
    ) const;

    void DrawIndexedIndirect(
        const GPUBuffer<Platform::VULKAN> *buffer,
        UInt32 buffer_offset
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    template <SizeType Size>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const FixedArray<UInt32, Size> &offsets
    ) const
    {
        BindDescriptorSet(
            pool,
            pipeline,
            set,
            binding,
            offsets.Data(),
            offsets.Size()
        );
    }

    void BindDescriptorSets(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        const DescriptorSet::Index *sets,
        const DescriptorSet::Index *bindings,
        SizeType num_descriptor_sets,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    template <SizeType NumOffsets>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding,
        const FixedArray<UInt32, NumOffsets> &offsets
    ) const
    {
        BindDescriptorSet(
            pool,
            pipeline,
            descriptor_set,
            binding,
            offsets.Data(),
            NumOffsets
        );
    }

    template <SizeType NumDescriptorSets, SizeType NumOffsets>
    void BindDescriptorSets(
        const DescriptorPool &pool,
        const GraphicsPipeline<Platform::VULKAN> *pipeline,
        const FixedArray<DescriptorSet::Index, NumDescriptorSets> &sets,
        const FixedArray<DescriptorSet::Index, NumDescriptorSets> &bindings,
        const FixedArray<UInt32, NumOffsets> &offsets
    ) const
    {
        BindDescriptorSets(
            pool,
            pipeline,
            sets.Data(),
            bindings.Data(),
            NumDescriptorSets,
            offsets.Data(),
            NumOffsets
        );
    }

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    template <SizeType NumOffsets>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding,
        const FixedArray<UInt32, NumOffsets> &offsets
    ) const
    {
        BindDescriptorSet(
            pool,
            pipeline,
            descriptor_set,
            binding,
            offsets.Data(),
            NumOffsets
        );
    }

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    template <SizeType NumOffsets>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const FixedArray<UInt32, NumOffsets> &offsets
    ) const
    {
        BindDescriptorSet(
            pool,
            pipeline,
            set,
            binding,
            offsets.Data(),
            NumOffsets
        );
    }

    void BindDescriptorSets(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        const DescriptorSet::Index *sets,
        const DescriptorSet::Index *bindings,
        SizeType num_descriptor_sets,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    template <SizeType NumDescriptorSets, SizeType NumOffsets>
    void BindDescriptorSets(
        const DescriptorPool &pool,
        const ComputePipeline<Platform::VULKAN> *pipeline,
        const FixedArray<DescriptorSet::Index, NumDescriptorSets> &sets,
        const FixedArray<DescriptorSet::Index, NumDescriptorSets> &bindings,
        const FixedArray<UInt32, NumOffsets> &offsets
    ) const
    {
        BindDescriptorSets(
            pool,
            pipeline,
            sets.Data(),
            bindings.Data(),
            NumDescriptorSets,
            offsets.Data(),
            NumOffsets
        );
    }

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    template <SizeType NumOffsets>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding,
        const FixedArray<UInt32, NumOffsets> &offsets
    ) const
    {
        BindDescriptorSet(
            pool,
            pipeline,
            descriptor_set,
            binding,
            offsets.Data(),
            NumOffsets
        );
    }

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    template <SizeType NumOffsets>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const FixedArray<UInt32, NumOffsets> &offsets
    ) const
    {
        BindDescriptorSet(
            pool,
            pipeline,
            set,
            binding,
            offsets.Data(),
            NumOffsets
        );
    }

    void BindDescriptorSets(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        const DescriptorSet::Index *sets,
        const DescriptorSet::Index *bindings,
        SizeType num_descriptor_sets,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    template <SizeType NumDescriptorSets, SizeType NumOffsets>
    void BindDescriptorSets(
        const DescriptorPool &pool,
        const RaytracingPipeline<Platform::VULKAN> *pipeline,
        const FixedArray<DescriptorSet::Index, NumDescriptorSets> &sets,
        const FixedArray<DescriptorSet::Index, NumDescriptorSets> &bindings,
        const FixedArray<UInt32, NumOffsets> &offsets
    ) const
    {
        BindDescriptorSets(
            pool,
            pipeline,
            sets.Data(),
            bindings.Data(),
            NumDescriptorSets,
            offsets.Data(),
            NumOffsets
        );
    }

    void DebugMarkerBegin(const char *marker_name) const;
    void DebugMarkerEnd() const;

    template <class LambdaFunction>
    Result Record(Device<Platform::VULKAN> *device, const RenderPass<Platform::VULKAN> *render_pass, const LambdaFunction &fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device, render_pass));

        Result result = fn(this);

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

private:
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const Pipeline<Platform::VULKAN> *pipeline,
        VkPipelineBindPoint bind_point,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const Pipeline<Platform::VULKAN> *pipeline,
        VkPipelineBindPoint bind_point,
        const DescriptorSetRef &descriptor_set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    void BindDescriptorSets(
        const DescriptorPool &pool,
        const Pipeline<Platform::VULKAN> *pipeline,
        VkPipelineBindPoint bind_point,
        const DescriptorSet::Index *sets,
        const DescriptorSet::Index *bindings,
        SizeType num_descriptor_sets,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    void BindDescriptorSets(
        const DescriptorPool &pool,
        const Pipeline<Platform::VULKAN> *pipeline,
        VkPipelineBindPoint bind_point,
        const DescriptorSetRef *descriptor_sets,
        const DescriptorSet::Index *bindings,
        SizeType num_descriptor_sets,
        const UInt32 *offsets,
        SizeType num_offsets
    ) const;

    CommandBufferType   m_type;
    VkCommandBuffer     m_command_buffer;
    VkCommandPool       m_command_pool;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif