/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Framebuffer.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <DX12Framebuffer.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12Framebuffer

DX12Framebuffer::DX12Framebuffer(const RenderTargetDesc& renderTargetDesc)
    : FramebufferBase(renderTargetDesc),
      m_isCreated(false),
      m_attachments()
{
}

DX12Framebuffer::~DX12Framebuffer()
{
    g_renderBackend->descriptorHeapManager->Free(DX12DescriptorHeapType::RTV, std::move(m_rtvDescriptorHandle));
    g_renderBackend->descriptorHeapManager->Free(DX12DescriptorHeapType::DSV, std::move(m_dsvDescriptorHandle));

    m_attachments.Clear();
}

bool DX12Framebuffer::IsCreated() const
{
    return m_isCreated;
}

RendererResult DX12Framebuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }

    // Initialize all attachments
    for (auto& it : m_attachments)
    {
        DX12AttachmentRef& attachment = it.second;
        Assert(attachment.IsValid());
        
        // Ensure image is created
        DX12GpuImageRef image = attachment->GetImage();

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

    for (const auto& it : m_attachments)
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
        m_rtvDescriptorHandle = g_renderBackend->descriptorHeapManager->Allocate(DX12DescriptorHeapType::RTV, numRTVs);
        
        if (!m_rtvDescriptorHandle.IsValid())
            return HYP_MAKE_ERROR(RendererError, "Failed to allocate RTV descriptors");
    }

    if (hasDepth)
    {
        m_dsvDescriptorHandle = g_renderBackend->descriptorHeapManager->Allocate(DX12DescriptorHeapType::DSV, 1);
        
        if (!m_dsvDescriptorHandle.IsValid())
            return HYP_MAKE_ERROR(RendererError, "Failed to allocate DSV descriptors");
    }

    // Create Views
    ID3D12Device* device = g_renderBackend->GetDevice();
    const uint32 rtvIncrement = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    
    uint32 rtvIndex = 0;

    for (auto& it : m_attachments)
    {
        DX12AttachmentRef& attachment = it.second;
        DX12GpuImage* image = attachment->GetImage();

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


RendererResult DX12Framebuffer::Resize(Vec2u newSize)
{
    if (GetExtent() == newSize)
    {
        return {};
    }

    m_renderTargetDesc.extent = newSize;

    if (!IsCreated())
    {
        return {};
    }

    // Resize all attachments
    for (auto& it : m_attachments)
    {
        DX12AttachmentRef& attachment = it.second;
        Assert(attachment.IsValid());

        DX12GpuImageRef image = attachment->GetImage();
        TextureDesc textureDesc = image->GetTextureDesc();
        textureDesc.extent = Vec3u {
            newSize.x,
            newSize.y,
            1
        };

        DX12GpuImageRef newImage = MakeHandle<DX12GpuImage>(textureDesc);
        newImage->SetDebugName(image->GetDebugName());
        CheckResultOrReturn(newImage->Create());

        DX12AttachmentRef newAttachment = MakeHandle<DX12Attachment>(
            newImage,
            MakeWeakRef(this),
            AttachmentDesc {
                .imageType = newImage->GetTextureDesc().type,
                .format = newImage->GetTextureDesc().format,
                .loadOp = attachment->GetLoadOperation(),
                .storeOp = attachment->GetStoreOperation(),
                .blendFunction = BlendFunction::None(),
                .clearColor = { }
            });

        newAttachment->SetBinding(attachment->GetBinding());
        CheckResultOrReturn(newAttachment->Create());

        it.second = newAttachment;

        attachment.Reset();
        image.Reset();
    }

    return {};
}

AttachmentRef DX12Framebuffer::AddAttachment(const AttachmentRef& attachment)
{
    // @TODO
    return attachment;
}

DX12AttachmentRef DX12Framebuffer::AddAttachment(
    uint32 binding,
    const DX12GpuImageRef& image,
    LoadOperation loadOp,
    StoreOperation storeOp)
{
    DX12AttachmentRef attachment = MakeHandle<DX12Attachment>(
        image,
        MakeWeakRef(this),
        AttachmentDesc {
            .imageType = image->GetTextureDesc().type,
            .format = image->GetTextureDesc().format,
            .loadOp = loadOp,
            .storeOp = storeOp,
            .blendFunction = BlendFunction::None(),
            .clearColor = { }
        });

    attachment->SetBinding(binding);

    return AddAttachment(attachment);
}

DX12AttachmentRef DX12Framebuffer::AddAttachment(
    uint32 binding,
    TextureFormat format,
    TextureType type,
    LoadOperation loadOp,
    StoreOperation storeOp)
{
    TextureDesc textureDesc;
    textureDesc.type = type;
    textureDesc.format = format;
    textureDesc.extent = Vec3u {
        m_renderTargetDesc.extent.x,
        m_renderTargetDesc.extent.y,
        1
    };
    textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

    DX12GpuImageRef image = MakeHandle<DX12GpuImage>(textureDesc);

    DX12AttachmentRef attachment = MakeHandle<DX12Attachment>(
        image,
        MakeWeakRef(this),
        AttachmentDesc {
            .imageType = image->GetTextureDesc().type,
            .format = image->GetTextureDesc().format,
            .loadOp = loadOp,
            .storeOp = storeOp,
            .blendFunction = BlendFunction::None(),
            .clearColor = { }
        });

    attachment->SetBinding(binding);

    return AddAttachment(attachment);
}

bool DX12Framebuffer::RemoveAttachment(uint32 binding)
{
    const auto it = m_attachments.Find(binding);

    if (it == m_attachments.End())
    {
        return false;
    }

    m_attachments.Erase(it);

    return true;
}

DX12Attachment* DX12Framebuffer::GetAttachment(uint32 binding) const
{
    const auto it = m_attachments.Find(binding);

    if (it == m_attachments.End())
    {
        return nullptr;
    }

    return it->second;
}

int DX12Framebuffer::NumAttachments() const
{
    return int(m_attachments.Size());
}

void DX12Framebuffer::BeginCapture(DX12CommandBuffer* commandBuffer)
{
    // @TODO
}

void DX12Framebuffer::EndCapture(DX12CommandBuffer* commandBuffer)
{
    // @TODO

    // @TODO: Remember the transition is not implicit like vulkan with renderpass
}

void DX12Framebuffer::Clear(DX12CommandBuffer* commandBuffer)
{
    // @TODO
}

#pragma endregion DX12Framebuffer

} // namespace Hyperion
