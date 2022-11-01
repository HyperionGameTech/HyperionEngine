#ifndef HYPERION_RENDERER_COMPUTE_PIPELINE_H
#define HYPERION_RENDERER_COMPUTE_PIPELINE_H

#include <rendering/backend/RendererPipeline.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererSwapchain.hpp>
#include <rendering/backend/RendererBuffer.hpp>
#include <rendering/backend/RendererShader.hpp>
#include <rendering/backend/RendererDescriptorSet.hpp>

namespace hyperion {
namespace renderer {
class ComputePipeline : public Pipeline
{
public:
    ComputePipeline();
    /*! \brief Construct a pipeline using the given \ref used_descriptor_set as the descriptor sets to be
        used with this pipeline.  */
    ComputePipeline(const Array<const DescriptorSet *> &used_descriptor_sets);
    ComputePipeline(const ComputePipeline &other) = delete;
    ComputePipeline &operator=(const ComputePipeline &other) = delete;
    ~ComputePipeline();

    Result Create(Device *device, ShaderProgram *shader, DescriptorPool *pool);
    Result Destroy(Device *device);

    void Bind(CommandBuffer *command_buffer) const;
    void Bind(CommandBuffer *command_buffer, const PushConstantData &push_constant_data);

    void SubmitPushConstants(CommandBuffer *cmd) const;

    void Dispatch(CommandBuffer *command_buffer, Extent3D group_size) const;
    void DispatchIndirect(
        CommandBuffer *command_buffer,
        const IndirectBuffer *indirect,
        size_t offset = 0
    ) const;
};

} // namespace renderer
}; // namespace hyperion

#endif //HYPERION_RENDERER_COMPUTE_PIPELINE_H
