/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#include <WebGPUPch.hpp>

#include <Rendering/WebGPU/WebGPUFramebuffer.hpp>
#include <Rendering/WebGPU/WebGPUCommandBuffer.hpp>
#include <Rendering/WebGPU/WebGPUGpuImage.hpp>
#include <Rendering/WebGPU/WebGPUGpuImageView.hpp>
#include <Rendering/WebGPU/WebGPURenderInterface.hpp>
#include <Rendering/WebGPU/WebGPUHelpers.hpp>

#include <Core/Threading/Mutex.hpp>

#include <WebGPUFramebuffer.generated.inl>

namespace Hyperion {

ENGINE_API HYP_DECLARE_LOG_CHANNEL(RenderingBackend);

extern WebGPURenderInterface RI;

static constexpr uint32 g_externalAttachmentBit = 1u << 31;

#pragma region Rect clear resources

// a load operation always covers the whole attachment, so clearing part of a depth target draws at the far plane
static const char* const g_rectClearShaderSource = R"(
@vertex
fn VSMain(@builtin(vertex_index) vertexIndex : u32) -> @builtin(position) vec4f
{
    let corner = vec2f(f32((vertexIndex << 1u) & 2u), f32(vertexIndex & 2u));

    return vec4f(corner * 2.0 - 1.0, 1.0, 1.0);
}

@fragment
fn PSMain()
{
}
)";

struct RectClearResources
{
    struct Entry
    {
        uint64 key;
        WGPURenderPipeline pipeline;
    };

    Mutex mutex;
    WGPUShaderModule shaderModule = nullptr;
    WGPUPipelineLayout pipelineLayout = nullptr;
    Array<Entry, WebGPUAllocator> entries;

    WGPURenderPipeline GetOrCreatePipeline(const WGPUTextureFormat* colorFormats, uint32 numColorFormats, WGPUTextureFormat depthFormat)
    {
        HashCode hashCode;
        hashCode.Add(uint32(depthFormat));

        for (uint32 formatIndex = 0; formatIndex < numColorFormats; formatIndex++)
        {
            hashCode.Add(uint32(colorFormats[formatIndex]));
        }

        const uint64 key = hashCode.Value();

        for (const Entry& entry : entries)
        {
            if (entry.key == key)
            {
                return entry.pipeline;
            }
        }

        if (shaderModule == nullptr)
        {
            WGPUShaderSourceWGSL shaderSource = WGPU_SHADER_SOURCE_WGSL_INIT;
            shaderSource.code = ToWGPUStringView(g_rectClearShaderSource);

            WGPUShaderModuleDescriptor moduleDescriptor = WGPU_SHADER_MODULE_DESCRIPTOR_INIT;
            moduleDescriptor.nextInChain = &shaderSource.chain;

            shaderModule = wgpuDeviceCreateShaderModule(RI.GetDevice(), &moduleDescriptor);

            WGPUPipelineLayoutDescriptor pipelineLayoutDescriptor = WGPU_PIPELINE_LAYOUT_DESCRIPTOR_INIT;
            pipelineLayout = wgpuDeviceCreatePipelineLayout(RI.GetDevice(), &pipelineLayoutDescriptor);
        }

        WGPUColorTargetState targets[FramebufferDesc::MaxAttachments];

        for (uint32 formatIndex = 0; formatIndex < numColorFormats; formatIndex++)
        {
            targets[formatIndex] = WGPU_COLOR_TARGET_STATE_INIT;
            targets[formatIndex].format = colorFormats[formatIndex];
            targets[formatIndex].writeMask = WGPUColorWriteMask_None;
        }

        WGPUFragmentState fragment = WGPU_FRAGMENT_STATE_INIT;
        fragment.module = shaderModule;
        fragment.entryPoint = ToWGPUStringView("PSMain");
        fragment.targetCount = numColorFormats;
        fragment.targets = targets;

        WGPUDepthStencilState depthStencil = WGPU_DEPTH_STENCIL_STATE_INIT;
        depthStencil.format = depthFormat;
        depthStencil.depthWriteEnabled = WGPUOptionalBool_True;
        depthStencil.depthCompare = WGPUCompareFunction_Always;

        WGPURenderPipelineDescriptor descriptor = WGPU_RENDER_PIPELINE_DESCRIPTOR_INIT;
        descriptor.layout = pipelineLayout;
        descriptor.vertex.module = shaderModule;
        descriptor.vertex.entryPoint = ToWGPUStringView("VSMain");
        descriptor.fragment = &fragment;
        descriptor.depthStencil = &depthStencil;

        WGPURenderPipeline pipeline = wgpuDeviceCreateRenderPipeline(RI.GetDevice(), &descriptor);

        entries.PushBack(Entry { key, pipeline });

        return pipeline;
    }

    void Release()
    {
        for (Entry& entry : entries)
        {
            if (entry.pipeline != nullptr)
            {
                wgpuRenderPipelineRelease(entry.pipeline);
            }
        }

        entries.Clear();

        if (pipelineLayout != nullptr)
        {
            wgpuPipelineLayoutRelease(pipelineLayout);
            pipelineLayout = nullptr;
        }

        if (shaderModule != nullptr)
        {
            wgpuShaderModuleRelease(shaderModule);
            shaderModule = nullptr;
        }
    }
};

static RectClearResources g_rectClearResources;

#pragma endregion Rect clear resources

WebGPUFramebuffer::WebGPUFramebuffer(const FramebufferDesc& framebufferDesc)
    : FramebufferBase(framebufferDesc),
      m_externalTextureView(nullptr),
      m_pendingClearMask(0),
      m_isRecording(false),
      m_isCreated(false)
{
}

WebGPUFramebuffer::~WebGPUFramebuffer()
{
    for (auto& it : m_attachments)
    {
        if (it.second != nullptr)
        {
            it.second->Release();
        }
    }

    m_attachments.Clear();
}

void WebGPUFramebuffer::ReleaseSharedResources()
{
    Mutex::Guard guard(g_rectClearResources.mutex);

    g_rectClearResources.Release();
}

RendererResult WebGPUFramebuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }

    if (m_externalTextureView == nullptr)
    {
        m_framebufferDesc.numAttachments = 0;
    }

    Vec2u imageExtent;

    for (auto& it : m_attachments)
    {
        WebGPUAttachment* attachment = it.second;
        Assert(attachment != nullptr);

        WebGPUGpuImageRef image = attachment->GetGpuImage();
        Assert(image != nullptr);

        Assert(imageExtent == Vec2u::Zero() || imageExtent == image->GetExtent().GetXY(),
            "Attachment dimensions do not match!");

        imageExtent = image->GetExtent().GetXY();

        m_framebufferDesc.AddAttachment(attachment->GetAttachmentDesc());

        if (!image->IsCreated())
        {
#ifdef HYP_RHI_DEBUG_NAMES
            if (!image->GetDebugName().IsValid())
            {
                image->SetDebugName(NAME_FMT("{}_RT_{}", Id().Value(), it.first));
            }
#endif

            CheckResultOrReturn(image->Create());
        }

        if (!attachment->IsCreated())
        {
            CheckResultOrReturn(attachment->Create());
        }
    }

    m_isCreated = true;

    return {};
}

void WebGPUFramebuffer::SetExternalTextureView(WGPUTextureView textureView, const Vec2u& extent, TextureFormat format)
{
    m_externalTextureView = textureView;

    m_framebufferDesc.numAttachments = 1;
    m_framebufferDesc.extent = extent;
    m_framebufferDesc.attachments[0] = AttachmentDesc(TextureType::Texture2D, format);
}

WebGPUAttachment* WebGPUFramebuffer::AddAttachment(WebGPUAttachment* attachment)
{
    Assert(attachment != nullptr);
    Assert(attachment->GetGpuImage() != nullptr);
    Assert(attachment->HasBinding(), "Attachment must have a binding");

    const uint32 binding = attachment->GetBinding();
    Assert(!m_attachments.Contains(binding), "Attachment already exists at binding: {}", binding);

    m_attachments[binding] = attachment;

    return attachment;
}

WebGPUAttachment* WebGPUFramebuffer::AddAttachment(
    uint32 binding,
    const AttachmentDesc& desc,
    const WebGPUGpuImageViewRef& imageView)
{
    Assert(imageView != nullptr);

    WebGPUAttachment* attachment = new WebGPUAttachment(
        imageView->GetImage(),
        imageView,
        MakeWeakRef(this),
        m_framebufferDesc.renderPassMode,
        desc);

    attachment->SetBinding(binding);

    return AddAttachment(attachment);
}

WebGPUAttachment* WebGPUFramebuffer::AddAttachment(uint32 binding, const AttachmentDesc& desc)
{
    TextureDesc textureDesc;
    textureDesc.type = desc.imageType;
    textureDesc.format = desc.format;
    textureDesc.extent = Vec3u { m_framebufferDesc.extent.x, m_framebufferDesc.extent.y, 1 };
    textureDesc.imageUsage = ImageUsage::Sampled | ImageUsage::Attachment;

    WebGPUAttachment* attachment = new WebGPUAttachment(
        textureDesc,
        MakeWeakRef(this),
        m_framebufferDesc.renderPassMode,
        desc);

    attachment->SetBinding(binding);

    return AddAttachment(attachment);
}

bool WebGPUFramebuffer::RemoveAttachment(uint32 binding)
{
    WebGPUAttachment* attachment = GetAttachment(binding);

    if (attachment == nullptr)
    {
        return false;
    }

    attachment->Release();
    m_attachments.Erase(binding);

    return true;
}

WebGPUAttachment* WebGPUFramebuffer::GetAttachment(uint32 binding) const
{
    const auto it = m_attachments.Find(binding);

    if (it == m_attachments.End())
    {
        return nullptr;
    }

    return it->second;
}

Vec2u WebGPUFramebuffer::GetAttachmentExtent() const
{
    for (const auto& it : m_attachments)
    {
        const WebGPUAttachment* attachment = it.second;

        const uint32 mipLevel = attachment->GetImageView()->GetImageSubResource().baseMipLevel;

        return attachment->GetGpuImage()->GetTextureDesc().GetMipExtent(uint8(mipLevel)).GetXY();
    }

    return GetExtent();
}

uint32 WebGPUFramebuffer::GetAllAttachmentsMask() const
{
    uint32 mask = 0;

    for (const auto& it : m_attachments)
    {
        mask |= 1u << it.first;
    }

    if (m_attachments.Empty() && m_externalTextureView != nullptr)
    {
        mask |= g_externalAttachmentBit;
    }

    return mask;
}

void WebGPUFramebuffer::BeginCapture(WebGPUCommandBuffer* commandBuffer)
{
    Assert(!m_isRecording);

    m_pendingClearMask = 0;

    for (const auto& it : m_attachments)
    {
        WebGPUAttachment* attachment = it.second;

        if (attachment->GetLoadOperation() == LoadOperation::Clear)
        {
            m_pendingClearMask |= 1u << it.first;
        }

        attachment->GetGpuImage()->InsertBarrier(commandBuffer, ResourceState::RenderTarget, ShaderModuleType::Pixel);
    }

    if (m_attachments.Empty() && m_externalTextureView != nullptr)
    {
        m_pendingClearMask |= g_externalAttachmentBit;
    }

    commandBuffer->BeginRenderPass(this);

    m_isRecording = true;
}

void WebGPUFramebuffer::EndCapture(WebGPUCommandBuffer* commandBuffer)
{
    Assert(m_isRecording);

    commandBuffer->EndRenderPass(this);

    m_pendingClearMask = 0;
    m_isRecording = false;
}

WGPURenderPassEncoder WebGPUFramebuffer::BeginPass(WGPUCommandEncoder encoder)
{
    WGPURenderPassColorAttachment colorAttachments[FramebufferDesc::MaxAttachments];
    uint32 numColorAttachments = 0;

    WGPURenderPassDepthStencilAttachment depthStencilAttachment = WGPU_RENDER_PASS_DEPTH_STENCIL_ATTACHMENT_INIT;
    // browsers reject the header's NaN default even when the pass loads depth
    depthStencilAttachment.depthClearValue = 1.0f;
    bool hasDepthStencilAttachment = false;

    for (const auto& it : m_attachments)
    {
        WebGPUAttachment* attachment = it.second;

        const bool shouldClear = (m_pendingClearMask & (1u << it.first)) != 0;

        WGPUTextureView view = attachment->GetImageView()->GetAttachmentView();

        if (view == nullptr)
        {
            HYP_LOG(RenderingBackend, Error, "Framebuffer attachment {} has no usable view", it.first);

            return nullptr;
        }

        if (attachment->IsDepthAttachment())
        {
            const AttachmentDesc& attachmentDesc = attachment->GetAttachmentDesc();
            const bool hasStencil = TextureUtils::HasStencilComponent(attachment->GetGpuImage()->GetTextureFormat());

            depthStencilAttachment.view = view;

            if (attachmentDesc.onlyStencil)
            {
                depthStencilAttachment.depthReadOnly = true;
            }
            else
            {
                depthStencilAttachment.depthLoadOp = shouldClear ? WGPULoadOp_Clear : WGPULoadOp_Load;
                depthStencilAttachment.depthStoreOp = WGPUStoreOp_Store;
                depthStencilAttachment.depthClearValue = 1.0f;
            }

            if (hasStencil)
            {
                if (attachmentDesc.onlyDepth)
                {
                    depthStencilAttachment.stencilReadOnly = true;
                }
                else
                {
                    depthStencilAttachment.stencilLoadOp = shouldClear ? WGPULoadOp_Clear : WGPULoadOp_Load;
                    depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Store;
                    depthStencilAttachment.stencilClearValue = 0;
                }
            }

            hasDepthStencilAttachment = true;

            continue;
        }

        Assert(numColorAttachments < FramebufferDesc::MaxAttachments);

        const Vec4f clearColor = attachment->GetClearColor();

        WGPURenderPassColorAttachment& colorAttachment = colorAttachments[numColorAttachments++];
        colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = view;
        colorAttachment.loadOp = shouldClear ? WGPULoadOp_Clear : WGPULoadOp_Load;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = WGPUColor { clearColor.x, clearColor.y, clearColor.z, clearColor.w };
    }

    if (m_attachments.Empty() && m_externalTextureView != nullptr)
    {
        WGPURenderPassColorAttachment& colorAttachment = colorAttachments[numColorAttachments++];
        colorAttachment = WGPU_RENDER_PASS_COLOR_ATTACHMENT_INIT;
        colorAttachment.view = m_externalTextureView;
        colorAttachment.loadOp = (m_pendingClearMask & g_externalAttachmentBit) ? WGPULoadOp_Clear : WGPULoadOp_Load;
        colorAttachment.storeOp = WGPUStoreOp_Store;
        colorAttachment.clearValue = WGPUColor { 0.0, 0.0, 0.0, 1.0 };
    }

    if (numColorAttachments == 0 && !hasDepthStencilAttachment)
    {
        return nullptr;
    }

    m_pendingClearMask = 0;

    WGPURenderPassDescriptor descriptor = WGPU_RENDER_PASS_DESCRIPTOR_INIT;
    descriptor.colorAttachmentCount = numColorAttachments;
    descriptor.colorAttachments = colorAttachments;
    descriptor.depthStencilAttachment = hasDepthStencilAttachment ? &depthStencilAttachment : nullptr;

    return wgpuCommandEncoderBeginRenderPass(encoder, &descriptor);
}

void WebGPUFramebuffer::Clear(WebGPUCommandBuffer* commandBuffer, uint8 attachmentsMask)
{
    if (attachmentsMask == 0)
    {
        return;
    }

    Assert(m_isRecording);

    const uint32 allAttachmentsMask = GetAllAttachmentsMask();

    commandBuffer->EndActivePass();

    m_pendingClearMask |= attachmentsMask == uint8(-1)
        ? allAttachmentsMask
        : (uint32(attachmentsMask) & allAttachmentsMask);
}

void WebGPUFramebuffer::Clear(WebGPUCommandBuffer* commandBuffer, const Rect<uint32>& rect, uint8 attachmentsMask)
{
    if (m_attachments.Empty() || attachmentsMask == 0)
    {
        return;
    }

    Assert(m_isRecording);

    const Vec2u extent = GetAttachmentExtent();

    if (rect.x0 == 0 && rect.y0 == 0 && rect.x1 >= extent.x && rect.y1 >= extent.y)
    {
        Clear(commandBuffer, attachmentsMask);

        return;
    }

    WGPUTextureFormat colorFormats[FramebufferDesc::MaxAttachments];
    uint32 numColorFormats = 0;

    WGPUTextureFormat depthFormat = WGPUTextureFormat_Undefined;
    bool shouldClearDepth = false;
    bool shouldClearColor = false;

    for (const auto& it : m_attachments)
    {
        WebGPUAttachment* attachment = it.second;

        const bool isMasked = attachmentsMask == uint8(-1) || (attachmentsMask & (1u << it.first));
        const WGPUTextureFormat format = ToWGPUTextureFormat(attachment->GetGpuImage()->GetTextureFormat());

        if (attachment->IsDepthAttachment())
        {
            depthFormat = format;
            shouldClearDepth = isMasked && !attachment->GetAttachmentDesc().onlyStencil;
        }
        else
        {
            colorFormats[numColorFormats++] = format;
            shouldClearColor |= isMasked;
        }
    }

    if (shouldClearColor)
    {
        static bool hasWarned = false;

        if (!hasWarned)
        {
            hasWarned = true;

            HYP_LOG(RenderingBackend, Warning, "Clearing part of a color attachment is not supported on WebGPU");
        }
    }

    if (!shouldClearDepth)
    {
        return;
    }

    WGPURenderPipeline pipeline;

    {
        Mutex::Guard guard(g_rectClearResources.mutex);

        pipeline = g_rectClearResources.GetOrCreatePipeline(colorFormats, numColorFormats, depthFormat);
    }

    if (pipeline == nullptr)
    {
        return;
    }

    commandBuffer->DrawRectClear(pipeline, rect);
}

#ifdef HYP_RHI_DEBUG_NAMES
void WebGPUFramebuffer::SetDebugName(Name name)
{
    FramebufferBase::SetDebugName(name);

    for (const auto& it : m_attachments)
    {
        WebGPUAttachment* attachment = it.second;

        if (attachment == nullptr)
        {
            continue;
        }

        if (WebGPUGpuImageRef image = attachment->GetGpuImage(); image.IsValid())
        {
            image->SetDebugName(NAME_FMT("{}_RT_{}", *name, it.first));
        }
    }
}
#endif

} // namespace Hyperion
