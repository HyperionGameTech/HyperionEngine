/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUGraphicsPipeline.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <Rendering/Shader.hpp>
#include <Rendering/Vertex.hpp>

#include <WebGPUGraphicsPipeline.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

static constexpr uint32 g_maxVertexAttributes = 16;

// locations follow the order of the bits in the layout mask, the same numbering the Vulkan backend uses
static uint32 BuildVertexAttributes(const VertexInputLayoutDesc& inputLayout, WGPUVertexAttribute* outAttributes, uint64& outStride)
{
    static constexpr WGPUVertexFormat floatFormats[] = {
        WGPUVertexFormat_Float32,
        WGPUVertexFormat_Float32,
        WGPUVertexFormat_Float32x2,
        WGPUVertexFormat_Float32x3,
        WGPUVertexFormat_Float32x4
    };

    uint32 numAttributes = 0;
    uint64 offset = 0;

    auto addAttribute = [&](WGPUVertexFormat format, uint64 size)
    {
        Assert(numAttributes < g_maxVertexAttributes);

        WGPUVertexAttribute& attribute = outAttributes[numAttributes];
        attribute = WGPU_VERTEX_ATTRIBUTE_INIT;
        attribute.format = format;
        attribute.offset = offset;
        attribute.shaderLocation = numAttributes;

        offset += size;
        ++numAttributes;
    };

    FOR_EACH_BIT(inputLayout.mask, bit)
    {
        const VertexType vertexType = VertexType(1 << bit);

        if (vertexType == VT_Skeletal)
        {
            addAttribute(WGPUVertexFormat_Uint32, sizeof(uint32));
            addAttribute(WGPUVertexFormat_Float32x4, sizeof(float) * 4);

            continue;
        }

        if (vertexType == VT_Tree)
        {
            addAttribute(WGPUVertexFormat_Uint32x4, sizeof(uint32) * 4);
            addAttribute(WGPUVertexFormat_Uint32x2, sizeof(uint32) * 2);

            continue;
        }

        if (vertexType == VT_Foliage)
        {
            addAttribute(WGPUVertexFormat_Uint32x2, sizeof(uint32) * 2);

            continue;
        }

        const size_t attributeSize = VertexUtils::PacketSize(vertexType);
        AssertDebug(attributeSize <= 16, "Attribute size too large for supported formats!");

        addAttribute(floatFormats[attributeSize / sizeof(float)], attributeSize);
    }

    outStride = offset;

    return numAttributes;
}

WebGPUGraphicsPipeline::WebGPUGraphicsPipeline()
    : GraphicsPipelineBase(),
      m_isCreated(false)
{
}

WebGPUGraphicsPipeline::WebGPUGraphicsPipeline(const WebGPUShaderInstanceRef& shaderInstance)
    : GraphicsPipelineBase(),
      m_isCreated(false)
{
    m_shaderInstance = shaderInstance;
}

WebGPUGraphicsPipeline::~WebGPUGraphicsPipeline()
{
    ReleaseVariants();

    m_shaderInstance.Reset();
}

void WebGPUGraphicsPipeline::ReleaseVariants()
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
            wgpuRenderPipelineRelease(variant->pipeline);
        }

        delete variant;
    }

    m_variants.Clear();
}

void WebGPUGraphicsPipeline::OnVariantCompiled(WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, WGPUStringView message, void* userdata1, void* userdata2)
{
    PipelineVariant* variant = static_cast<PipelineVariant*>(userdata1);

    RI.GetPipelineCompiler().OnCompileFinished(variant->compileMode);

    if (status != WGPUCreatePipelineAsyncStatus_Success)
    {
        HYP_LOG(RenderingBackend, Error, "Failed to compile a WebGPU render pipeline: {}", ANSIStringView(message.data, message.data + message.length));

        pipeline = nullptr;
    }

    if (variant->isOrphaned)
    {
        if (pipeline != nullptr)
        {
            wgpuRenderPipelineRelease(pipeline);
        }

        delete variant;

        return;
    }

    variant->pipeline = pipeline;
    variant->isCompiling = false;
}

bool WebGPUGraphicsPipeline::IsCreated() const
{
    return m_isCreated;
}

RendererResult WebGPUGraphicsPipeline::Create()
{
    return Rebuild();
}

RendererResult WebGPUGraphicsPipeline::Rebuild()
{
    ReleaseVariants();

    m_isCreated = false;

    if (!m_shaderInstance.IsValid() || !m_shaderInstance->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Graphics pipeline has no created shader");
    }

    if (m_shaderInstance->GetShaderModule(ShaderModuleType::Vertex) == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Graphics pipeline shader has no vertex stage");
    }

    m_isCreated = true;

    return {};
}

WGPURenderPipeline WebGPUGraphicsPipeline::GetOrCreateVariant(const WGPUBindGroupLayout* layouts, uint32 numLayouts)
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

    WGPUVertexAttribute vertexAttributes[g_maxVertexAttributes];
    uint64 vertexStride = 0;

    const uint32 numVertexAttributes = BuildVertexAttributes(m_inputLayout, vertexAttributes, vertexStride);

    WGPUVertexBufferLayout vertexBufferLayout = WGPU_VERTEX_BUFFER_LAYOUT_INIT;
    vertexBufferLayout.stepMode = WGPUVertexStepMode_Vertex;
    vertexBufferLayout.arrayStride = vertexStride;
    vertexBufferLayout.attributeCount = numVertexAttributes;
    vertexBufferLayout.attributes = vertexAttributes;

    const ANSIStringView vertexEntryPoint = m_shaderInstance->GetEntryPointName(ShaderModuleType::Vertex);
    const ANSIStringView fragmentEntryPoint = m_shaderInstance->GetEntryPointName(ShaderModuleType::Pixel);

    Array<WebGPUStorageTextureOverride, WebGPUAllocator> storageTextureOverrides;

    for (uint32 layoutIndex = 0; layoutIndex < numLayouts; layoutIndex++)
    {
        RI.GetStorageTextureOverrides(layouts[layoutIndex], layoutIndex, storageTextureOverrides);
    }

    WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
    descriptor.layout = pipelineLayout;
    descriptor.vertex.module = m_shaderInstance->GetShaderModuleVariant(ShaderModuleType::Vertex, storageTextureOverrides.ToSpan());
    descriptor.vertex.entryPoint = ToWGPUStringView(vertexEntryPoint.Data(), vertexEntryPoint.Size());
    descriptor.vertex.bufferCount = numVertexAttributes != 0 ? 1 : 0;
    descriptor.vertex.buffers = &vertexBufferLayout;

    descriptor.primitive.topology = ToWGPUPrimitiveTopology(m_topology);
    descriptor.primitive.frontFace = WGPUFrontFace_CCW;
    descriptor.primitive.cullMode = ToWGPUCullMode(m_faceCullMode);
    descriptor.primitive.unclippedDepth = m_depthClamp && RI.GetDeviceFeatures().depthClipControl;

    WGPUColorTargetState colorTargets[FramebufferDesc::MaxAttachments];
    WGPUBlendState blendStates[FramebufferDesc::MaxAttachments];
    uint32 numColorTargets = 0;

    WGPUDepthStencilState depthStencil = WGPU_DEPTH_STENCIL_STATE_INIT;
    bool hasDepthStencil = false;

    const uint32 fragmentOutputMask = m_shaderInstance->GetFragmentOutputMask();

    for (uint32 attachmentIndex = 0; attachmentIndex < m_framebufferDesc.numAttachments; attachmentIndex++)
    {
        const AttachmentDesc& attachmentDesc = m_framebufferDesc.attachments[attachmentIndex];

        if (TextureUtils::IsDepthFormat(attachmentDesc.format))
        {
            const bool hasStencil = TextureUtils::HasStencilComponent(attachmentDesc.format);

            depthStencil.format = ToWGPUTextureFormat(attachmentDesc.format);
            depthStencil.depthCompare = m_depthTest ? ToWGPUCompareFunction(m_depthCompareOp) : WGPUCompareFunction_Always;
            depthStencil.depthWriteEnabled = (m_depthTest && m_depthWrite && !attachmentDesc.onlyStencil) ? WGPUOptionalBool_True : WGPUOptionalBool_False;
            depthStencil.depthBias = m_depthBias;
            depthStencil.depthBiasSlopeScale = m_depthBiasSlope;

            if (hasStencil && m_stencilFunction.HasValue())
            {
                WGPUStencilFaceState stencilFace = WGPU_STENCIL_FACE_STATE_INIT;
                stencilFace.compare = ToWGPUCompareFunction(m_stencilFunction->compareOp);
                stencilFace.failOp = ToWGPUStencilOperation(m_stencilFunction->failOp);
                stencilFace.depthFailOp = ToWGPUStencilOperation(m_stencilFunction->depthFailOp);
                stencilFace.passOp = ToWGPUStencilOperation(m_stencilFunction->passOp);

                depthStencil.stencilFront = stencilFace;
                depthStencil.stencilBack = stencilFace;
                depthStencil.stencilReadMask = m_stencilCompareMask;
                depthStencil.stencilWriteMask = attachmentDesc.onlyDepth ? 0 : m_stencilWriteMask;
            }

            hasDepthStencil = true;

            continue;
        }

        const uint32 targetIndex = numColorTargets++;

        WGPUColorTargetState& colorTarget = colorTargets[targetIndex];
        colorTarget = WGPU_COLOR_TARGET_STATE_INIT;
        colorTarget.format = ToWGPUTextureFormat(attachmentDesc.format);
        colorTarget.writeMask = (fragmentOutputMask & (1u << targetIndex)) ? WGPUColorWriteMask_All : WGPUColorWriteMask_None;

        const BlendFunction& blendFunction = attachmentDesc.blendFunction != BlendFunction::None()
            ? attachmentDesc.blendFunction
            : m_blendFunction;

        const bool isBlendable = TextureUtils::FormatSupportsBlending(attachmentDesc.format)
            && GetDefaultSampleType(attachmentDesc.format, false) == WGPUTextureSampleType_Float;

        if (blendFunction != BlendFunction::None() && isBlendable)
        {
            WGPUBlendState& blendState = blendStates[targetIndex];
            blendState = WGPU_BLEND_STATE_INIT;
            blendState.color.operation = WGPUBlendOperation_Add;
            blendState.color.srcFactor = ToWGPUBlendFactor(blendFunction.GetSrcColor());
            blendState.color.dstFactor = ToWGPUBlendFactor(blendFunction.GetDstColor());
            blendState.alpha.operation = WGPUBlendOperation_Add;
            blendState.alpha.srcFactor = ToWGPUBlendFactor(blendFunction.GetSrcAlpha());
            blendState.alpha.dstFactor = ToWGPUBlendFactor(blendFunction.GetDstAlpha());

            colorTarget.blend = &blendState;
        }
    }

    WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;

    if (WGPUShaderModule fragmentModule = m_shaderInstance->GetShaderModuleVariant(ShaderModuleType::Pixel, storageTextureOverrides.ToSpan()))
    {
        fragment.module = fragmentModule;
        fragment.entryPoint = ToWGPUStringView(fragmentEntryPoint.Data(), fragmentEntryPoint.Size());
        fragment.targetCount = numColorTargets;
        fragment.targets = colorTargets;

        descriptor.fragment = &fragment;
    }

    descriptor.depthStencil = hasDepthStencil ? &depthStencil : nullptr;

#ifdef HYP_RHI_DEBUG_NAMES
    if (m_debugName.IsValid())
    {
        descriptor.label = ToWGPUStringView(*m_debugName);
    }
#endif

    WebGPUPipelineCompiler& pipelineCompiler = RI.GetPipelineCompiler();

    PipelineVariant* variant = new PipelineVariant();
    variant->key = key;
    variant->compileMode = pipelineCompiler.GetCompileMode(*m_shaderInstance, /* canStartLate */ false);

    m_variants.PushBack(variant);

    pipelineCompiler.OnCompileStarted(variant->compileMode);

    if (variant->compileMode == WebGPUPipelineCompileMode::WithFrame)
    {
        variant->isCompiling = true;

        WGPUCreateRenderPipelineAsyncCallbackInfo callbackInfo = WGPU_CREATE_RENDER_PIPELINE_ASYNC_CALLBACK_INFO_INIT;
        callbackInfo.mode = WGPUCallbackMode_AllowProcessEvents;
        callbackInfo.callback = &OnVariantCompiled;
        callbackInfo.userdata1 = variant;

        wgpuDeviceCreateRenderPipelineAsync(RI.GetDevice(), &descriptor, callbackInfo);

        wgpuPipelineLayoutRelease(pipelineLayout);

        return nullptr;
    }

    variant->pipeline = wgpuDeviceCreateRenderPipeline(RI.GetDevice(), &descriptor);

    wgpuPipelineLayoutRelease(pipelineLayout);

    if (variant->pipeline == nullptr)
    {
        HYP_LOG(RenderingBackend, Error, "Failed to create WebGPU render pipeline for shader {}", m_shaderInstance->GetShader()->GetName());
    }

    return variant->pipeline;
}

void WebGPUGraphicsPipeline::Bind(WebGPUCommandBuffer* commandBuffer)
{
    Bind(commandBuffer, Vec2i::Zero(), m_framebufferDesc.extent);
}

void WebGPUGraphicsPipeline::Bind(WebGPUCommandBuffer* commandBuffer, Vec2i viewportOffset, Vec2u viewportExtent)
{
    Assert(m_isCreated);

    Viewport viewport;
    viewport.position = viewportOffset;
    viewport.extent = viewportExtent;

    commandBuffer->SetGraphicsPipeline(this, viewport, RI.state.stencilReference);
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUGraphicsPipeline::SetDebugName(Name name)
{
    GraphicsPipelineBase::SetDebugName(name);
}
#endif

} // namespace Hyperion
