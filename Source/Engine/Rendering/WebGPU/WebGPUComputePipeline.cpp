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
    for (PipelineVariant* variant : m_variants)
    {
        if (variant->isCompiling)
        {
            // the compile hands it back, see OnVariantCompiled
            variant->isOrphaned = true;

            continue;
        }

        if (variant->pipeline != nullptr)
        {
            wgpuComputePipelineRelease(variant->pipeline);
        }

        delete variant;
    }
}

void WebGPUComputePipeline::OnVariantCompiled(WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, WGPUStringView message, void* userdata1, void* userdata2)
{
    PipelineVariant* variant = static_cast<PipelineVariant*>(userdata1);

    RI.GetPipelineCompiler().OnCompileFinished(variant->compileMode);

    if (status != WGPUCreatePipelineAsyncStatus_Success)
    {
        HYP_LOG(RenderingBackend, Error, "Failed to compile a WebGPU compute pipeline: {}", ANSIStringView(message.data, message.data + message.length));

        pipeline = nullptr;
    }

    if (variant->isOrphaned)
    {
        if (pipeline != nullptr)
        {
            wgpuComputePipelineRelease(pipeline);
        }

        delete variant;

        return;
    }

    variant->pipeline = pipeline;
    variant->isCompiling = false;
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

    for (const PipelineVariant* variant : m_variants)
    {
        if (variant->key == key)
        {
            return variant->pipeline;
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

    WebGPUPipelineCompiler& pipelineCompiler = RI.GetPipelineCompiler();

    PipelineVariant* variant = new PipelineVariant();
    variant->key = key;
    variant->compileMode = pipelineCompiler.GetCompileMode(*m_shaderInstance, /* canStartLate */ true);

    m_variants.PushBack(variant);

    pipelineCompiler.OnCompileStarted(variant->compileMode);

    if (variant->compileMode == WebGPUPipelineCompileMode::Immediate)
    {
        variant->pipeline = wgpuDeviceCreateComputePipeline(RI.GetDevice(), &descriptor);

        wgpuPipelineLayoutRelease(pipelineLayout);

        if (variant->pipeline == nullptr)
        {
            HYP_LOG(RenderingBackend, Error, "Failed to create WebGPU compute pipeline for shader {}", m_shaderInstance->GetShader()->GetName());
        }

        return variant->pipeline;
    }

    variant->isCompiling = true;

#ifndef HYP_WEB
    if (variant->compileMode == WebGPUPipelineCompileMode::InBackground)
    {
        RI.GetPipelineManifest().Record(cacheDesc, layouts, numLayouts);
    }
#endif

    WGPUCreateComputePipelineAsyncCallbackInfo callbackInfo = WGPU_CREATE_COMPUTE_PIPELINE_ASYNC_CALLBACK_INFO_INIT;
    callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
    callbackInfo.callback = &OnVariantCompiled;
    callbackInfo.userdata1 = variant;

    if (variant->compileMode == WebGPUPipelineCompileMode::WithFrame)
    {
        wgpuDeviceCreateComputePipelineAsync(RI.GetDevice(), &descriptor, callbackInfo);

        wgpuPipelineLayoutRelease(pipelineLayout);

        return nullptr;
    }

    // the descriptor points into this call, so what it needs is kept until the compile starts
    WGPUShaderModule shaderModule = descriptor.compute.module;
    wgpuShaderModuleAddRef(shaderModule);

    pipelineCompiler.QueueBackgroundCompile([variant, pipelineLayout, shaderModule, callbackInfo, queuedEntryPoint = ANSIString(entryPoint)]() -> bool
        {
            const bool isWanted = !variant->isOrphaned;

            if (isWanted)
            {
                WGPUComputePipelineDescriptor queuedDescriptor = WGPU_COMPUTE_PIPELINE_DESCRIPTOR_INIT;
                queuedDescriptor.layout = pipelineLayout;
                queuedDescriptor.compute.module = shaderModule;
                queuedDescriptor.compute.entryPoint = ToWGPUStringView(queuedEntryPoint.Data(), queuedEntryPoint.Size());

                wgpuDeviceCreateComputePipelineAsync(RI.GetDevice(), &queuedDescriptor, callbackInfo);
            }
            else
            {
                delete variant;
            }

            wgpuShaderModuleRelease(shaderModule);
            wgpuPipelineLayoutRelease(pipelineLayout);

            return isWanted;
        });

    return nullptr;
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
