#ifndef HYPERION_V2_COMPUTE_H
#define HYPERION_V2_COMPUTE_H

#include "Shader.hpp"

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>

#include <memory>

namespace hyperion::v2 {

using renderer::CommandBuffer;
using renderer::DescriptorSet;

class ComputePipeline : public BasicObject<STUB_CLASS(ComputePipeline)>
{
public:
    ComputePipeline(Handle<Shader> shader);
    ComputePipeline(Handle<Shader> shader, const Array<DescriptorSetRef> &used_descriptor_sets);
    ComputePipeline(const ComputePipeline &) = delete;
    ComputePipeline &operator=(const ComputePipeline &) = delete;
    ~ComputePipeline();

    Handle<Shader> &GetShader() { return m_shader; }
    const Handle<Shader> &GetShader() const { return m_shader; }

    ComputePipelineRef &GetPipeline() { return m_pipeline; }
    const ComputePipelineRef &GetPipeline() const { return m_pipeline; }

    void Init();

private:
    Handle<Shader>      m_shader;
    ComputePipelineRef  m_pipeline;
};

} // namespace hyperion::v2

#endif // !HYPERION_V2_COMPUTE_H

