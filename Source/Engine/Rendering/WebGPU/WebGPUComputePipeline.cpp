/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUComputePipeline.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPUGpuBuffer.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>

#include <Rendering/Shader.hpp>

#include <WebGPUComputePipeline.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

WebGPUComputePipeline::WebGPUComputePipeline()
    : ComputePipelineBase(),
      m_isCreated(false)
{
}

WebGPUComputePipeline::WebGPUComputePipeline(const WebGPUShaderInstanceRef& shaderInstance)
    : ComputePipelineBase(shaderInstance),
      m_isCreated(false)
{
}

WebGPUComputePipeline::~WebGPUComputePipeline()
{
    for (Variant& variant : m_variants)
    {
        if (variant.pipeline != nullptr)
        {
            wgpuComputePipelineRelease(variant.pipeline);
        }
    }
}

bool WebGPUComputePipeline::IsCreated() const
{
    return m_isCreated;
}

RendererResult WebGPUComputePipeline::Create()
{
    if (!m_shaderInstance.IsValid() || !m_shaderInstance->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Compute pipeline has no created shader");
    }

    if (m_shaderInstance->GetShaderModule(ShaderModuleType::Compute) == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Compute pipeline shader has no compute stage");
    }

    m_isCreated = true;

    return {};
}

WGPUComputePipeline WebGPUComputePipeline::GetOrCreateVariant(const WGPUBindGroupLayout* layouts, uint32 numLayouts)
{
    HashCode hashCode;

    for (uint32 layoutIndex = 0; layoutIndex < numLayouts; layoutIndex++)
    {
        hashCode.Add(uintptr_t(layouts[layoutIndex]));
    }

    const uint64 key = hashCode.Value();

    for (const Variant& variant : m_variants)
    {
        if (variant.key == key)
        {
            return variant.pipeline;
        }
    }

    WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
    pipelineLayoutDescriptor.bindGroupLayoutCount = numLayouts;
    pipelineLayoutDescriptor.bindGroupLayouts = layouts;

    WGPUPipelineLayout pipelineLayout = wgpuDeviceCreatePipelineLayout(RI.GetDevice(), &pipelineLayoutDescriptor);

    const ANSIStringView entryPoint = m_shaderInstance->GetEntryPointName(ShaderModuleType::Compute);

    Array<WebGPUStorageTextureOverride, WebGPUAllocator> storageTextureOverrides;

    for (uint32 layoutIndex = 0; layoutIndex < numLayouts; layoutIndex++)
    {
        RI.GetStorageTextureOverrides(layouts[layoutIndex], layoutIndex, storageTextureOverrides);
    }

    WGPUComputePipelineDescriptor descriptor = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
    descriptor.layout = pipelineLayout;
    descriptor.compute.module = m_shaderInstance->GetShaderModuleVariant(ShaderModuleType::Compute, storageTextureOverrides.ToSpan());
    descriptor.compute.entryPoint = ToWGPUStringView(entryPoint.Data(), entryPoint.Size());

#ifdef HYP_RHI_DEBUG_NAMES
    if (m_debugName.IsValid())
    {
        descriptor.label = ToWGPUStringView(*m_debugName);
    }
#endif

    WGPUComputePipeline pipeline = wgpuDeviceCreateComputePipeline(RI.GetDevice(), &descriptor);

    wgpuPipelineLayoutRelease(pipelineLayout);

    if (pipeline == nullptr)
    {
        HYP_LOG(RenderingBackend, Error, "Failed to create WebGPU compute pipeline for shader {}", m_shaderInstance->GetShader()->GetName());
    }

    m_variants.PushBack(Variant { key, pipeline });

    return pipeline;
}

void WebGPUComputePipeline::Bind(CommandBuffer* commandBuffer)
{
    Assert(m_isCreated);

    commandBuffer->SetComputePipeline(this);
}

void WebGPUComputePipeline::Dispatch(CommandBuffer* commandBuffer, const Vec3u& groupSize) const
{
    WGPUComputePassEncoder pass = commandBuffer->PrepareDispatch();

    if (pass == nullptr)
    {
        return;
    }

    wgpuComputePassEncoderDispatchWorkgroups(pass, groupSize.x, groupSize.y, groupSize.z);
}

void WebGPUComputePipeline::DispatchIndirect(CommandBuffer* commandBuffer, const WebGPUGpuBufferRef& indirectBuffer, size_t offset) const
{
    AssertDebug(indirectBuffer.IsValid() && indirectBuffer->IsCreated());

    WGPUComputePassEncoder pass = commandBuffer->PrepareDispatch();

    if (pass == nullptr)
    {
        return;
    }

    wgpuComputePassEncoderDispatchWorkgroupsIndirect(pass, indirectBuffer->GetWGPUBuffer(), offset);
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUComputePipeline::SetDebugName(Name name)
{
    ComputePipelineBase::SetDebugName(name);
}
#endif

} // namespace Hyperion
