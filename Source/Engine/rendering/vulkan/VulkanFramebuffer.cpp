/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanRenderPass.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <rendering/CommandRecorder.hpp>

#include <Core/math/MathUtil.hpp>

#include <new>

#include <VulkanFramebuffer.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

static void TransitionFramebufferAttachments(CommandRecorder& cr, VulkanFramebuffer* framebuffer, Span<VulkanAttachment*> attachments)
{
    Assert(framebuffer != nullptr);

    for (VulkanAttachment* attachment : attachments)
    {
        const VulkanGpuImageRef& image = attachment->GetGpuImage();
        Assert(image.IsValid());

        switch (framebuffer->GetRenderTargetDesc().renderPassMode)
        {
        case RenderPassMode::Present:
            // cr << InsertBarrier(image, RS_PRESENT);
            break;
        case RenderPassMode::RenderTarget:
            cr << InsertBarrier(image, RS_SHADER_RESOURCE);
            break;
        default:
            HYP_NOT_IMPLEMENTED();
            break;
        }
    }
}

#pragma region VulkanAttachmentMap

RendererResult VulkanAttachmentMap::Create()
{
    VulkanFramebufferRef framebuffer = framebufferWeak.Lock();
    if (!framebuffer.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Framebuffer is not valid");
    }

    Array<VulkanAttachment*, VulkanTempAllocator> attachments;
    attachments.Reserve(this->attachments.Size());

    for (KeyValuePair<uint32, VulkanAttachment*>& pair : this->attachments)
    {
        VulkanAttachment* attachment = pair.second;

        Assert(attachment->GetGpuImage().IsValid());

        if (!attachment->GetGpuImage()->IsCreated())
        {
#if HYP_DEBUG_MODE
            if (!attachment->GetGpuImage()->GetDebugName().IsValid())
            {
                if (framebuffer->GetDebugName().IsValid())
                {
                    attachment->GetGpuImage()->SetDebugName(NAME_FMT("{}_Target{}", framebuffer->GetDebugName(), pair.first));
                }
                else
                {
                    attachment->GetGpuImage()->SetDebugName(NAME_FMT("{}_Target{}", framebuffer->Id(), pair.first));
                }
            }
#endif

            CheckResultOrReturn(attachment->GetGpuImage()->Create());
        }

        attachments.PushBack(attachment);

        if (!attachment->IsCreated())
        {
            CheckResultOrReturn(attachment->Create());
        }
    }

    UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderInterface->GetSingleTimeCommands();

    singleTimeCommands->Push([&](CommandRecorder& cr) -> RendererResult
        {
            TransitionFramebufferAttachments(cr, framebuffer, attachments.ToSpan());

            return {};
        });

    return singleTimeCommands->Execute();
}

#pragma endregion VulkanAttachmentMap

#pragma region VulkanFramebuffer

VulkanFramebuffer::VulkanFramebuffer(const RenderTargetDesc& renderTargetDesc)
    : FramebufferBase(renderTargetDesc),
      m_handle(VK_NULL_HANDLE),
      m_isRecording(false)
{
    m_attachmentMap.framebufferWeak = WeakHandleFromThis();
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    if (!IsCreated())
    {
        return;
    }

    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
            {
                vkDestroyFramebuffer(g_renderInterface->GetDevice()->GetDevice(), handle, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
    }

    m_attachmentMap.Reset();
}

bool VulkanFramebuffer::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

RendererResult VulkanFramebuffer::Create()
{
    if (IsCreated())
    {
        return {};
    }

    CheckResultOrReturn(m_attachmentMap.Create());

    Vec2u imageExtent;

    m_renderTargetDesc.numAttachments = 0;

    for (const auto& it : m_attachmentMap.attachments)
    {
        VulkanAttachment* attachment = it.second;
        Assert(attachment != nullptr);

        VulkanGpuImage* image = attachment->GetGpuImage();
        Assert(image != nullptr);

        Assert(imageExtent == Vec2u::Zero() || imageExtent == image->GetExtent().GetXY(),
            "Attachment dimensions do not match!");

        imageExtent = image->GetExtent().GetXY();

        m_renderTargetDesc.AddAttachment(attachment->GetAttachmentDesc());
    }

    m_renderPass = VulkanRenderPass(m_renderTargetDesc);
    CheckResultOrReturn(m_renderPass.Create());

    Array<VkImageView> attachmentImageViews;
    attachmentImageViews.Reserve(m_attachmentMap.attachments.Size());

    uint32 numLayers = 1;
    bool shouldClearFramebuffer = true;

    for (const auto& it : m_attachmentMap.attachments)
    {
        VulkanAttachment* attachment = it.second;
        Assert(attachment != nullptr);

        if (attachment->GetLoadOperation() == LoadOperation::LOAD)
        {
            shouldClearFramebuffer = false;
        }

        Assert(attachment->GetImageView() != nullptr);
        Assert(attachment->GetImageView()->IsCreated());

        attachmentImageViews.PushBack(attachment->GetImageView()->GetVulkanHandle());
    }

    VkFramebufferCreateInfo framebufferCreateInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebufferCreateInfo.renderPass = m_renderPass.GetVulkanHandle();
    framebufferCreateInfo.attachmentCount = uint32(attachmentImageViews.Size());
    framebufferCreateInfo.pAttachments = attachmentImageViews.Data();
    framebufferCreateInfo.width = imageExtent.x;
    framebufferCreateInfo.height = imageExtent.y;
    framebufferCreateInfo.layers = numLayers;

    VULKAN_CHECK(vkCreateFramebuffer(g_renderInterface->GetDevice()->GetDevice(), &framebufferCreateInfo, nullptr, &m_handle));

    if (shouldClearFramebuffer)
    {
        UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderInterface->GetSingleTimeCommands();

        singleTimeCommands->Push([this](CommandRecorder& cr) -> RendererResult
            {
                cr << SetCurrentFramebuffer(this);
                cr << ClearFramebuffer(this);
                cr << SetCurrentFramebuffer(nullptr);

                return {};
            });

        RendererResult result = singleTimeCommands->Execute();
        if (!result)
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to clear framebuffer on create! Error was: {}", result.GetError().GetErrorCode(), result.GetError().GetMessage());
        }
    }

#if HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    // ok
    return {};
}

VulkanAttachment* VulkanFramebuffer::AddAttachment(VulkanAttachment* attachment)
{
    if (!attachment)
    {
        return nullptr;
    }

    Assert(attachment->GetFramebuffer().GetUnsafe() == this,
        "Attachment framebuffer does not match framebuffer");

    // external attachment so we need to add a reference
    attachment->AddRef();

    return m_attachmentMap.AddAttachment(attachment);
}

VulkanAttachment* VulkanFramebuffer::AddAttachment(
    uint32 binding,
    const AttachmentDesc& desc,
    const VulkanGpuImageViewRef& imageView)
{
    Assert(imageView.IsValid() && imageView->GetImage().IsValid());

    VulkanAttachment* attachment = new VulkanAttachment(
        imageView->GetImage(),
        imageView,
        WeakHandleFromThis(),
        m_renderTargetDesc.renderPassMode,
        desc);

    attachment->SetBinding(binding);

    return m_attachmentMap.AddAttachment(attachment);
}

VulkanAttachment* VulkanFramebuffer::AddAttachment(
    uint32 binding,
    const AttachmentDesc& attachmentDesc)
{
    return m_attachmentMap.AddAttachment(
        binding,
        m_renderTargetDesc.extent,
        attachmentDesc,
        m_renderTargetDesc.renderPassMode);
}

bool VulkanFramebuffer::RemoveAttachment(uint32 binding)
{
    const auto it = m_attachmentMap.attachments.Find(binding);

    if (it == m_attachmentMap.attachments.End())
    {
        return false;
    }

    VulkanAttachment* attachment = it->second;
    AssertDebug(attachment != nullptr);

    attachment->Release();

    m_attachmentMap.attachments.Erase(it);

    return true;
}

VulkanAttachment* VulkanFramebuffer::GetAttachment(uint32 binding) const
{
    return m_attachmentMap.GetAttachment(binding);
}

void VulkanFramebuffer::BeginCapture(VulkanCommandBuffer* commandBuffer)
{
    Assert(!commandBuffer->IsInRenderPass());
    Assert(!m_isRecording);

    commandBuffer->ResetBoundDescriptorSets();

    m_renderPass.Begin(commandBuffer, this);
    
    commandBuffer->m_isInRenderPass = true;

    m_isRecording = true;
}

void VulkanFramebuffer::EndCapture(VulkanCommandBuffer* commandBuffer)
{
    Assert(commandBuffer->IsInRenderPass());
    Assert(m_isRecording);

    m_renderPass.End(commandBuffer);

    commandBuffer->m_isInRenderPass = false;
    m_isRecording = false;
}

void VulkanFramebuffer::Clear(
    VulkanCommandBuffer* commandBuffer,
    uint8 attachmentsMask)
{
    Rect<uint32> rect {};
    rect.x0 = m_renderTargetDesc.offset.x;
    rect.y0 = m_renderTargetDesc.offset.y;
    rect.x1 = m_renderTargetDesc.offset.x + m_renderTargetDesc.extent.x;
    rect.y1 = m_renderTargetDesc.offset.y + m_renderTargetDesc.extent.y;

    Clear(commandBuffer, rect, attachmentsMask);
}

void VulkanFramebuffer::Clear(
    VulkanCommandBuffer* commandBuffer,
    const Rect<uint32>& rect,
    uint8 attachmentsMask)
{
    if (m_attachmentMap.Size() == 0 || attachmentsMask == 0)
    {
        return; // nothing to clear
    }

    Assert(m_isRecording);

    VkCommandBuffer vkCommandBuffer = commandBuffer->GetVulkanHandle();

    AssertDebug(m_attachmentMap.attachments.Size() == m_renderPass.GetRenderTargetDesc().numAttachments);

    Array<const AttachmentDesc*, VulkanTempAllocator> colorAttachments;
    colorAttachments.Reserve(m_renderTargetDesc.numAttachments);
    
    for (uint32 attachmentIndex = 0; attachmentIndex < m_renderTargetDesc.numAttachments; attachmentIndex++)
    {
        if (!TextureUtils::IsDepthFormat(m_renderTargetDesc.attachments[attachmentIndex].format))
        {
            colorAttachments.PushBack(&m_renderTargetDesc.attachments[attachmentIndex]);
        }
    }

    Array<VkClearAttachment, VulkanTempAllocator> clearAttachments;
    clearAttachments.Reserve(m_attachmentMap.attachments.Size());

    Array<VkClearRect, VulkanTempAllocator> clearRects;
    clearRects.Resize(m_attachmentMap.attachments.Size());
    
    const Vec2u& maxExtent = GetExtent();

    for (const auto& it : m_attachmentMap.attachments)
    {
        const uint32 binding = it.first;

        if (attachmentsMask != uint8(-1) && !(attachmentsMask & (1u << binding)))
            continue; // skip target

        VulkanAttachment* attachment = it.second;
        AssertDebug(attachment != nullptr && attachment->IsCreated());
        AssertDebug(binding < m_renderTargetDesc.numAttachments);

        const AttachmentDesc& attachmentDesc = m_renderTargetDesc.attachments[binding];

        VkClearRect& clearRect = clearRects[clearAttachments.Size()];
        clearRect = {};
        clearRect.rect.offset.x = rect.x0;
        clearRect.rect.offset.y = rect.y0;
        clearRect.rect.extent.width = rect.x1 - rect.x0;
        clearRect.rect.extent.height = rect.y1 - rect.y0;
        clearRect.layerCount = 1;
        
        AssertDebug(clearRect.rect.extent.width - clearRect.rect.offset.x <= maxExtent.x
                    && clearRect.rect.extent.height - clearRect.rect.offset.y <= maxExtent.y);

        VkClearAttachment& clearAttachment = clearAttachments.EmplaceBack();
        clearAttachment = {};

        if (TextureUtils::IsDepthFormat(attachment->GetFormat()))
        {
            clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            if (TextureUtils::HasStencilComponent(attachment->GetFormat()))
            {
                clearAttachment.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }

            clearAttachment.colorAttachment = VK_ATTACHMENT_UNUSED;
            clearAttachment.clearValue.depthStencil = { 1.0f, 0 };
        }
        else
        {
            auto colorAttachmentIt = colorAttachments.Find(&attachmentDesc);
            AssertDebug(colorAttachmentIt != colorAttachments.End());

            const size_t colorAttachmentIndex = std::distance(colorAttachments.Begin(), colorAttachmentIt);

            clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clearAttachment.colorAttachment = uint32(colorAttachmentIndex);
            clearAttachment.clearValue.color.float32[0] = attachment->GetClearColor().x;
            clearAttachment.clearValue.color.float32[1] = attachment->GetClearColor().y;
            clearAttachment.clearValue.color.float32[2] = attachment->GetClearColor().z;
            clearAttachment.clearValue.color.float32[3] = attachment->GetClearColor().w;
        }
    }
    

    if (clearAttachments.Any())
    {
        vkCmdClearAttachments(
            vkCommandBuffer,
            uint32(clearAttachments.Size()),
            clearAttachments.Data(),
            uint32(clearAttachments.Size()),
            clearRects.Data());
    }
}

#if HYP_DEBUG_MODE
void VulkanFramebuffer::SetDebugName(Name name)
{
    FramebufferBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_FRAMEBUFFER;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(g_renderInterface->GetDevice()->GetDevice(), &objectNameInfo);
}
#endif

#pragma endregion VulkanFramebuffer

} // namespace Hyperion
