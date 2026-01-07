/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Framebuffer.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <DX12Framebuffer.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

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
            dsvDesc.Format = ToDXGIFormat(image->GetTextureFormat());
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
            dsvDesc.Texture2D.MipSlice = 0;

            device->CreateDepthStencilView(image->GetResource(), &dsvDesc, m_dsvDescriptorHandle.cpuHandle);
        }
        else
        {
            D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
            rtvDesc.Format = ToDXGIFormat(image->GetTextureFormat());
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
    if (m_extent == newSize)
    {
        // same size, ok
        return {};
    }

    m_extent = newSize;

    if (!IsCreated())
    {
        // not created yet, when Create() is called we'll create with the new size
        return {};
    }

    g_renderBackend->descriptorHeapManager->Free(DX12DescriptorHeapType::RTV, std::move(m_rtvDescriptorHandle));
    g_renderBackend->descriptorHeapManager->Free(DX12DescriptorHeapType::DSV, std::move(m_dsvDescriptorHandle));
    
    m_isCreated = false;

    struct AttachmentDef {
        uint32 binding;
        DX12AttachmentRef attachment;
        DX12GpuImageRef image;
    };

    Array<AttachmentDef> definitions;
    definitions.Reserve(m_attachments.Size());
    
    for (auto& it : m_attachments)
    {
        definitions.PushBack({ it.first, it.second, it.second->GetImage() });
    }

    for (const auto& def : definitions)
    {
        DX12GpuImageRef newImage = def.image;

        if (def.attachment->GetFramebuffer().GetUnsafe() == this)
        {
            // owned, resize it
            TextureDesc textureDesc = def.image->GetTextureDesc();
            textureDesc.extent = Vec3u { newSize.x, newSize.y, 1 };
            
            newImage = CreateObject<DX12GpuImage>(textureDesc);
            newImage->SetDebugName(def.image->GetDebugName());
            CheckResultOrReturn(newImage->Create());
        }
        else
        {
            // sizes must match
            Assert(def.image->GetExtent().GetXY() == newSize);
        }

        DX12AttachmentRef newAttachment = CreateObject<DX12Attachment>(
            newImage,
            MakeWeakRef(this),
            m_renderTargetType,
            def.attachment->GetLoadOperation(),
            def.attachment->GetStoreOperation());
            
        newAttachment->SetBinding(def.binding);
        CheckResultOrReturn(newAttachment->Create());
        
        // Update map
        AddAttachment(newAttachment);
    }
    
    return Create();
}

AttachmentRef DX12Framebuffer::AddAttachment(const AttachmentRef& attachment)
{
    // @TODO
    return attachment;
}

DX12AttachmentRef DX12Framebuffer::AddAttachment(uint32 binding, const DX12GpuImageRef& image, LoadOperation loadOp, StoreOperation storeOp)
{
    DX12AttachmentRef attachment = CreateObject<DX12Attachment>(
        image,
        MakeWeakRef(this),
        m_renderTargetType,
        loadOp,
        storeOp);

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
    textureDesc.extent = Vec3u { m_extent.x, m_extent.y, 1 };
    textureDesc.imageUsage = IU_SAMPLED | IU_ATTACHMENT;

    DX12GpuImageRef image = CreateObject<DX12GpuImage>(textureDesc);

    DX12AttachmentRef attachment = CreateObject<DX12Attachment>(
        image,
        MakeWeakRef(this),
        m_renderTargetType,
        loadOp,
        storeOp);

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

    return it->second.Get();
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
