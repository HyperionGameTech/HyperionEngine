#ifndef HYPERION_V2_COMPUTE_H
#define HYPERION_V2_COMPUTE_H

#include "Shader.hpp"

#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::IndirectBuffer;
using renderer::DescriptorSet;

class ComputePipeline : public EngineComponentBase<STUB_CLASS(ComputePipeline)>
{
public:
    ComputePipeline(Handle<Shader> &&shader);
    ComputePipeline(Handle<Shader> &&shader, const Array<const DescriptorSet *> &used_descriptor_sets);
    ComputePipeline(const ComputePipeline &) = delete;
    ComputePipeline &operator=(const ComputePipeline &) = delete;
    ~ComputePipeline();

    renderer::ComputePipeline *GetPipeline() { return &m_pipeline; }
    const renderer::ComputePipeline *GetPipeline() const { return &m_pipeline; }

    void Init(Engine *engine);

private:
    renderer::ComputePipeline m_pipeline;
    Handle<Shader> m_shader;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPUTE_H

