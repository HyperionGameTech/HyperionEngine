/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Core/containers/Array.hpp>

#include <rendering/dx12/DX12Framebuffer.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Helpers.hpp>
#include <rendering/Shared.hpp>

#include <DX12Framebuffer.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12Framebuffer

DX12Framebuffer::DX12Framebuffer(const FramebufferDesc& framebufferDesc)
    : FramebufferBase(framebufferDesc),
      m_attachmentMap(),
      m_isRecording(false),
      m_isCreated(false),
      m_hasBeenCleared(false)
{
    m_attachmentMap.framebufferWeak = MakeWeakRef(this);
}

DX12Framebuffer::~DX12Framebuffer()
{
    if (!m_isExternalRTV)
    {
        g_renderInterface->descriptorHeapManager->Free(DX12DescriptorHeapType::RTV, std::move(m_rtvDescriptorHandle));
    }
    g_renderInterface->descriptorHeapManager->Free(DX12DescriptorHeapType::DSV, std::move(m_dsvDescriptorHandle));

    m_attachmentMap.Reset();
}

RendererResult DX12Framebuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }
    
    Vec2u imageExtent;

    if (!m_isExternalRTV)
    {
        m_framebufferDesc.numAttachments = 0;
    }

    for (const auto& it : m_attachmentMap.attachments)
    {
        DX12Attachment* attachment = it.second;
        Assert(attachment != nullptr);

        DX12GpuImage* image = attachment->GetGpuImage();
        Assert(image != nullptr);

        Assert(imageExtent == Vec2u::Zero() || imageExtent == image->GetExtent().GetXY(),
            "Attachment dimensions do not match!");

        imageExtent = image->GetExtent().GetXY();

        m_framebufferDesc.AddAttachment(attachment->GetAttachmentDesc());
    }

    // Initialize all attachments
    for (auto& it : m_attachmentMap)
    {
        DX12Attachment* attachment = it.second;
        Assert(attachment != nullptr);
        
        // Ensure image is created
        DX12GpuImageRef image = attachment->GetGpuImage();

        if (!image->IsCreated())
        {
            if (!image->GetDebugName().IsValid())
            {
                image->SetDebugName(NAME_FMT("{}_RT_{}", Id().Value(), it.first));
            }

            CheckResultOrReturn(image->Create());
        }

        // Ensure attachment is created
        if (!attachment->IsCreated())
        {
            CheckResultOrReturn(attachment->Create());
        }
    }

    // Count RTVs and DSVs
    uint32 numRTVs = 0;
    bool hasDepth = false;

    for (const auto& it : m_attachmentMap)
    {
        if (it.second->IsDepthAttachment())
        {
            hasDepth = true;
        }
        else
        {
            numRTVs++;
        }
    }

    // Allocate RTV/DSV descriptors
    if (numRTVs > 0)
    {
        m_rtvDescriptorHandle = g_renderInterface->descriptorHeapManager->Allocate(DX12DescriptorHeapType::RTV, numRTVs);
        
        if (!m_rtvDescriptorHandle.IsValid())
            return HYP_MAKE_ERROR(RendererError, "Failed to allocate RTV descriptors");
    }

    if (hasDepth)
    {
        m_dsvDescriptorHandle = g_renderInterface->descriptorHeapManager->Allocate(DX12DescriptorHeapType::DSV, 1);
        
        if (!m_dsvDescriptorHandle.IsValid())
            return HYP_MAKE_ERROR(RendererError, "Failed to allocate DSV descriptors");
    }

    // Create Views
    ID3D12Device* device = g_renderInterface->GetDevice();
    const uint32 rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    uint32 rtvIndex = 0;

    for (auto& it : m_attachmentMap)
    {
        DX12Attachment* attachment = it.second;
        DX12GpuImage* image = attachment->GetGpuImage();

        if (attachment->IsDepthAttachment())
        {
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
            dsvDesc.Format = ToDXGIFormat(image->GetTextureFormat(), DX12ViewType::RTV_DSV);
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
            dsvDesc.Texture2D.MipSlice = 0;

            device->CreateDepthStencilView(image->GetResource(), &dsvDesc, m_dsvDescriptorHandle.cpuHandle);
        }
        else
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
            rtvDesc.Format = ToDXGIFormat(image->GetTextureFormat(), DX12ViewType::RTV_DSV);
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = 0;
            rtvDesc.Texture2D.PlaneSlice = 0;

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHandle.cpuHandle;
            rtvHandle.ptr += rtvIndex * rtvIncrement;
            
            device->CreateRenderTargetView(image->GetResource(), &rtvDesc, rtvHandle);

            rtvIndex++;
        }
    }

    m_isCreated = true;

    return {};
}

DX12Attachment* DX12Framebuffer::AddAttachment(DX12Attachment* attachment)
{
    return m_attachmentMap.AddAttachment(attachment);
}

DX12Attachment* DX12Framebuffer::AddAttachment(
    uint32 binding,
    const AttachmentDesc& desc,
    const DX12GpuImageViewRef& imageView)
{
    Assert(imageView != nullptr);

    DX12Attachment* attachment = new DX12Attachment(
        imageView->GetImage(),
        imageView,
        MakeWeakRef(this),
        m_framebufferDesc.renderPassMode,
        desc);

    attachment->SetBinding(binding);
    m_attachmentMap.AddAttachment(attachment);

    return attachment;
}

DX12Attachment* DX12Framebuffer::AddAttachment(
    uint32 binding,
    const AttachmentDesc& desc)
{
    TextureDesc textureDesc;
    textureDesc.type = desc.imageType;
    textureDesc.format = desc.format;
    textureDesc.extent = Vec3u { m_framebufferDesc.extent.x, m_framebufferDesc.extent.y, 1 };
    textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

    DX12Attachment* attachment = new DX12Attachment(
        textureDesc,
        MakeWeakRef(this),
        m_framebufferDesc.renderPassMode,
        desc);

    attachment->SetBinding(binding);
    m_attachmentMap.AddAttachment(attachment);

    return attachment;
}

bool DX12Framebuffer::RemoveAttachment(uint32 binding)
{
    DX12Attachment* attachment = m_attachmentMap.GetAttachment(binding);
    if (!attachment)
    {
        return false;
    }

    attachment->Release();
    m_attachmentMap.attachments.Erase(binding);

    return true;
}

DX12Attachment* DX12Framebuffer::GetAttachment(uint32 binding) const
{
    return m_attachmentMap.GetAttachment(binding);
}

void DX12Framebuffer::BeginCapture(DX12CommandBuffer* commandBuffer)
{
    Assert(!m_isRecording);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();
    ID3D12Device* device = g_renderInterface->GetDevice();
    const uint32 rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    // Transition attachments to render target state and collect handles
    Array<D3D12_CPU_DESCRIPTOR_HANDLE> rtvHandles;
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
    bool hasDSV = false;
    LoadOperation depthLoadOp = LoadOperation::UNDEFINED;

    uint32 colorAttachmentIndex = 0;
    for (auto& it : m_attachmentMap)
    {
        DX12Attachment* attachment = it.second;
        DX12GpuImage* image = attachment->GetGpuImage();
        const AttachmentDesc& attachmentDesc = attachment->GetAttachmentDesc();

        if (attachment->IsDepthAttachment())
        {
            dsvHandle = m_dsvDescriptorHandle.cpuHandle;
            hasDSV = true;
            depthLoadOp = attachment->GetLoadOperation();
        }
        else
        {
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHandle.cpuHandle;
            rtvHandle.ptr += colorAttachmentIndex * rtvIncrement;
            rtvHandles.PushBack(rtvHandle);
            colorAttachmentIndex++;
        }

        // Transition to render target state - matching VulkanRenderPass::Begin() logic.
        // Uses RS_RENDER_TARGET for all attachments; InsertBarrier internally remaps
        // depth attachments to DEPTH_WRITE.
        const GpuImageViewRef& imageView = attachment->GetImageView();
        const ImageSubResource& subResource = imageView->GetImageSubResource();
        const TextureDesc& textureDesc = image->GetTextureDesc();

        const bool hasStencil = TextureUtils::HasStencilComponent(textureDesc.format);
        const bool fullSubResource = image->IsFullSubResource(subResource);

        if (hasStencil && fullSubResource)
        {
            const bool transitionDepth = !attachmentDesc.onlyStencil && image->GetResourceState() != RS_RENDER_TARGET;
            const bool transitionStencil = !attachmentDesc.onlyDepth && image->GetStencilState() != RS_RENDER_TARGET;

            if (transitionDepth ^ transitionStencil)
            {
                if (transitionDepth)
                    image->InsertBarrier(commandBuffer, RS_RENDER_TARGET, ShaderModuleType::Pixel, /* onlyDepth */ true, /* onlyStencil */ false);
                if (transitionStencil)
                    image->InsertBarrier(commandBuffer, RS_RENDER_TARGET, ShaderModuleType::Pixel, /* onlyDepth */ false, /* onlyStencil */ true);
            }
            else if (transitionDepth && transitionStencil)
            {
                image->InsertBarrier(commandBuffer, RS_RENDER_TARGET, ShaderModuleType::Pixel);
            }
        }
        else if (fullSubResource)
        {
            image->InsertBarrier(commandBuffer, RS_RENDER_TARGET, ShaderModuleType::Pixel);
        }
        else if (image->GetSubResourceState(subResource) != RS_RENDER_TARGET)
        {
            image->InsertBarrier(commandBuffer, subResource, RS_RENDER_TARGET, ShaderModuleType::Pixel);
        }
    }

    // Set render targets
    if (rtvHandles.Any())
    {
        commandList->OMSetRenderTargets(
            uint32(rtvHandles.Size()),
            rtvHandles.Data(),
            FALSE,
            hasDSV ? &dsvHandle : nullptr
        );

        colorAttachmentIndex = 0;
        for (auto& it : m_attachmentMap)
        {
            DX12Attachment* attachment = it.second;
            if (attachment->IsDepthAttachment())
            {
                continue;
            }

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHandle.cpuHandle;
            rtvHandle.ptr += colorAttachmentIndex * rtvIncrement;

            if (attachment->GetLoadOperation() == LoadOperation::CLEAR)
            {
                const Vec4f clearColor = attachment->GetClearColor();
                commandList->ClearRenderTargetView(rtvHandle, clearColor.values, 0, nullptr);
            }

            colorAttachmentIndex++;
        }

        if (hasDSV && depthLoadOp == LoadOperation::CLEAR)
        {
            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        }
    }
    else if (hasDSV)
    {
        // Depth only rendering
        commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

        if (depthLoadOp == LoadOperation::CLEAR)
        {
            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        }
    }
    else if (m_rtvDescriptorHandle.IsValid() && m_attachmentMap.Size() == 0)
    {
        // External RTV handle (e.g. from swapchain back buffer) with no managed attachments
        if (m_externalRTResource != nullptr)
        {
            const D3D12_RESOURCE_STATES stateBefore = m_externalRTResourceState;
            const D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

            if (stateBefore != stateAfter)
            {
                D3D12_RESOURCE_BARRIER barrier {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = m_externalRTResource;
                barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                barrier.Transition.StateBefore = stateBefore;
                barrier.Transition.StateAfter = stateAfter;
                commandList->ResourceBarrier(1, &barrier);

                m_externalRTResourceState = stateAfter;
            }
        }

        commandList->OMSetRenderTargets(
            1,
            &m_rtvDescriptorHandle.cpuHandle,
            FALSE,
            nullptr
        );
    }

    m_isRecording = true;
}

void DX12Framebuffer::EndCapture(DX12CommandBuffer* commandBuffer)
{
    Assert(m_isRecording);

    // Attachments stay in RS_RENDER_TARGET state -- no barriers needed here.
    // SRV transitions are handled by DX12RenderInterface when the images
    // are bound as shader resources later in the frame.

    // Transition external RTV (swapchain back buffer) back to present state
    if (m_externalRTResource != nullptr && m_attachmentMap.Size() == 0)
    {
        const D3D12_RESOURCE_STATES stateBefore = m_externalRTResourceState;
        const D3D12_RESOURCE_STATES stateAfter = D3D12_RESOURCE_STATE_PRESENT;

        if (stateBefore != stateAfter)
        {
            D3D12_RESOURCE_BARRIER barrier {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = m_externalRTResource;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barrier.Transition.StateBefore = stateBefore;
            barrier.Transition.StateAfter = stateAfter;
            commandBuffer->GetCommandList()->ResourceBarrier(1, &barrier);

            m_externalRTResourceState = stateAfter;
        }
    }

    m_isRecording = false;
}

void DX12Framebuffer::Clear(
    DX12CommandBuffer* commandBuffer,
    uint8 attachmentsMask)
{
    Rect<uint32> rect {};
    rect.x0 = m_framebufferDesc.offset.x;
    rect.y0 = m_framebufferDesc.offset.y;
    rect.x1 = m_framebufferDesc.offset.x + m_framebufferDesc.extent.x;
    rect.y1 = m_framebufferDesc.offset.y + m_framebufferDesc.extent.y;

    Clear(commandBuffer, rect, attachmentsMask);
}

void DX12Framebuffer::Clear(
    DX12CommandBuffer* commandBuffer,
    const Rect<uint32>& rect,
    uint8 attachmentsMask)
{
    if (m_attachmentMap.Size() == 0 || attachmentsMask == 0)
    {
        return;
    }

    Assert(m_isRecording);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();
    ID3D12Device* device = g_renderInterface->GetDevice();

    const uint32 rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    uint32 colorAttachmentIndex = 0;

    for (const auto& it : m_attachmentMap)
    {
        const uint32 binding = it.first;

        if (attachmentsMask != uint8(-1) && !(attachmentsMask & (1u << binding)))
            continue;

        DX12Attachment* attachment = it.second;
        Assert(attachment != nullptr && attachment->IsCreated());

        DX12GpuImage* image = attachment->GetGpuImage();

        if (image->GetTextureDesc().IsDepthStencil())
        {
            D3D12_CLEAR_VALUE clearValue {};
            clearValue.Format = ToDXGIFormat(image->GetTextureFormat(), DX12ViewType::RTV_DSV);
            clearValue.DepthStencil.Depth = 1.0f;
            clearValue.DepthStencil.Stencil = 0;

            D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = m_dsvDescriptorHandle.cpuHandle;
            commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
        }
        else
        {
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvDescriptorHandle.cpuHandle;
            rtvHandle.ptr += colorAttachmentIndex * rtvIncrement;

            Vec4f clearColor = attachment->GetClearColor();

            D3D12_CLEAR_VALUE clearValue {};
            clearValue.Format = ToDXGIFormat(image->GetTextureFormat(), DX12ViewType::RTV_DSV);
            clearValue.Color[0] = clearColor.x;
            clearValue.Color[1] = clearColor.y;
            clearValue.Color[2] = clearColor.z;
            clearValue.Color[3] = clearColor.w;

            commandList->ClearRenderTargetView(rtvHandle, clearColor.values, 0, nullptr);

            colorAttachmentIndex++;
        }
    }
}

#ifdef HYP_DEBUG_MODE
void DX12Framebuffer::SetDebugName(Name name)
{
    FramebufferBase::SetDebugName(name);

    if (!name.IsValid())
    {
        return;
    }

    // Propagate debug name to all attachments
    for (const auto& it : m_attachmentMap)
    {
        DX12Attachment* attachment = it.second;
        if (attachment != nullptr)
        {
            if (DX12GpuImageRef image = attachment->GetGpuImage(); image.IsValid())
            {
                image->SetDebugName(NAME_FMT("{}_RT_{}", *name, it.first));
            }
        }
    }
}
#endif

#pragma endregion DX12Framebuffer

} // namespace Hyperion
