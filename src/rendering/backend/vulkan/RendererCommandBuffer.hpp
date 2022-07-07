#ifndef HYPERION_RENDERER_COMMAND_BUFFER_H
#define HYPERION_RENDERER_COMMAND_BUFFER_H

#include "RendererDevice.hpp"
#include "RendererRenderPass.hpp"
#include "RendererSemaphore.hpp"
#include "RendererFence.hpp"
#include "RendererDescriptorSet.hpp"

#include <core/lib/FixedArray.hpp>

#include <vulkan/vulkan.h>

#include <Types.hpp>

namespace hyperion {
namespace renderer {

class Pipeline;
class GraphicsPipeline;
class ComputePipeline;
class RaytracingPipeline;

class CommandBuffer {
public:
    enum Type {
        COMMAND_BUFFER_PRIMARY,
        COMMAND_BUFFER_SECONDARY
    };

    static Result SubmitSecondary(
        CommandBuffer *primary,
        const std::vector<std::unique_ptr<CommandBuffer>> &command_buffers
    );

    CommandBuffer(Type type);
    CommandBuffer(const CommandBuffer &other) = delete;
    CommandBuffer &operator=(const CommandBuffer &other) = delete;
    ~CommandBuffer();

    Type GetType() const                     { return m_type; }

    VkCommandBuffer GetCommandBuffer() const { return m_command_buffer; }

    Result Create(Device *device, VkCommandPool command_pool);
    Result Destroy(Device *device, VkCommandPool command_pool);
    Result Begin(Device *device, const RenderPass *render_pass = nullptr);
    Result End(Device *device);
    Result Reset(Device *device);
    Result SubmitPrimary(
        VkQueue queue,
        Fence *fence,
        SemaphoreChain *semaphore_chain
    );

    Result SubmitSecondary(CommandBuffer *primary);

    void DrawIndexed(
        UInt32 num_indices,
        UInt32 num_instances  = 1,
        UInt32 instance_index = 0
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
        DescriptorSet::Index set
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        size_t num_offsets
    ) const;

    template <size_t Size>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
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
        const GraphicsPipeline *pipeline,
        const DescriptorSet::Index *sets,
        const DescriptorSet::Index *bindings,
        size_t num_descriptor_sets,
        const UInt32 *offsets,
        size_t num_offsets
    ) const;

    template <size_t NumDescriptorSets, size_t NumOffsets>
    void BindDescriptorSets(
        const DescriptorPool &pool,
        const GraphicsPipeline *pipeline,
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
        const ComputePipeline *pipeline,
        DescriptorSet::Index set
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding
    ) const;

    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline *pipeline,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        size_t num_offsets
    ) const;

    template <size_t NumOffsets>
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const ComputePipeline *pipeline,
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
        const ComputePipeline *pipeline,
        const DescriptorSet::Index *sets,
        const DescriptorSet::Index *bindings,
        size_t num_descriptor_sets,
        const UInt32 *offsets,
        size_t num_offsets
    ) const;

    template <size_t NumDescriptorSets, size_t NumOffsets>
    void BindDescriptorSets(
        const DescriptorPool &pool,
        const ComputePipeline *pipeline,
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

    template <class LambdaFunction>
    Result Record(Device *device, const RenderPass *render_pass, const LambdaFunction &fn)
    {
        HYPERION_BUBBLE_ERRORS(Begin(device, render_pass));

        Result result = fn(this);

        HYPERION_PASS_ERRORS(End(device), result);

        return result;
    }

private:
    void BindDescriptorSet(
        const DescriptorPool &pool,
        const Pipeline *pipeline,
        VkPipelineBindPoint bind_point,
        DescriptorSet::Index set,
        DescriptorSet::Index binding,
        const UInt32 *offsets,
        size_t num_offsets
    ) const;

    void BindDescriptorSets(
        const DescriptorPool &pool,
        const Pipeline *pipeline,
        VkPipelineBindPoint bind_point,
        const DescriptorSet::Index *sets,
        const DescriptorSet::Index *bindings,
        size_t num_descriptor_sets,
        const UInt32 *offsets,
        size_t num_offsets
    ) const;

    Type m_type;
    VkCommandBuffer m_command_buffer;
};

} // namespace renderer
} // namespace hyperion

#endif