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

#include <rendering/RenderQueue.hpp>

#include <Core/math/MathUtil.hpp>

#include <new>

#include <VulkanFramebuffer.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface* g_renderInterface;

static void TransitionFramebufferAttachments(RenderQueue& renderQueue, VulkanFramebuffer* framebuffer, Span<VulkanAttachmentDef*> attachmentDefs)
{
    Assert(framebuffer != nullptr);

    for (const VulkanAttachmentDef* attachmentDef : attachmentDefs)
    {
        const VulkanGpuImageRef& image = attachmentDef->image;
        Assert(image.IsValid());

        switch (framebuffer->GetRenderTargetDesc().renderPassMode)
        {
        case RenderPassMode::Present:
            // renderQueue << InsertBarrier(image, RS_PRESENT);
            break;
        case RenderPassMode::RenderTarget:
            renderQueue << InsertBarrier(image, RS_SHADER_RESOURCE);
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

    Array<VulkanAttachmentDef*> attachmentDefs;

    for (KeyValuePair<uint32, VulkanAttachmentDef>& it : attachments)
    {
        VulkanAttachmentDef& def = it.second;

        Assert(def.image.IsValid());

        if (!def.image->IsCreated())
        {
#if HYP_DEBUG_MODE
            if (!def.image->GetDebugName().IsValid())
            {
                if (framebuffer->GetDebugName().IsValid())
                {
                    def.image->SetDebugName(NAME_FMT("{}_Target{}", framebuffer->GetDebugName(), it.first));
                }
                else
                {
                    def.image->SetDebugName(NAME_FMT("{}_Target{}", framebuffer->Id(), it.first));
                }
            }
#endif

            CheckResultOrReturn(def.image->Create());
        }

        attachmentDefs.PushBack(&def);

        Assert(def.attachment != nullptr);

        if (!def.attachment->IsCreated())
        {
            CheckResultOrReturn(def.attachment->Create());
        }
    }

    UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderInterface->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue) -> RendererResult
        {
            TransitionFramebufferAttachments(renderQueue, framebuffer, attachmentDefs.ToSpan());

            return {};
        });

    return singleTimeCommands->Execute();
}

#pragma endregion VulkanAttachmentMap

#pragma region VulkanFramebuffer

VulkanFramebuffer::VulkanFramebuffer(const RenderTargetDesc& renderTargetDesc)
    : FramebufferBase(renderTargetDesc),
      m_handle(VK_NULL_HANDLE)
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

    m_renderTargetDesc.numAttachments = 0;

    for (const auto& it : m_attachmentMap.attachments)
    {
        const VulkanAttachmentDef& def = it.second;

        Assert(def.attachment != nullptr);
        m_renderTargetDesc.AddAttachment(def.attachment->GetAttachmentDesc());
    }

    m_renderPass = VulkanRenderPass(m_renderTargetDesc);
    CheckResultOrReturn(m_renderPass.Create());

    Array<VkImageView> attachmentImageViews;
    attachmentImageViews.Reserve(m_attachmentMap.attachments.Size());

    uint32 numLayers = 1;
    bool shouldClearFramebuffer = true;

    for (const auto& it : m_attachmentMap.attachments)
    {
        VulkanAttachment* attachment = it.second.attachment;
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
    framebufferCreateInfo.width = m_renderTargetDesc.extent.x;
    framebufferCreateInfo.height = m_renderTargetDesc.extent.y;
    framebufferCreateInfo.layers = numLayers;

    VULKAN_CHECK(vkCreateFramebuffer(g_renderInterface->GetDevice()->GetDevice(), &framebufferCreateInfo, nullptr, &m_handle));

    if (shouldClearFramebuffer)
    {
        UniquePtr<SingleTimeCommands> singleTimeCommands = g_renderInterface->GetSingleTimeCommands();

        singleTimeCommands->Push([this](RenderQueue& renderQueue) -> RendererResult
            {
                renderQueue << ClearFramebuffer(this);

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

    Attachment* attachment = it->second.attachment;
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

    commandBuffer->ResetBoundDescriptorSets();

    m_renderPass.Begin(commandBuffer, this);
    
    commandBuffer->m_isInRenderPass = true;
}

void VulkanFramebuffer::EndCapture(VulkanCommandBuffer* commandBuffer)
{
    Assert(commandBuffer->IsInRenderPass());

    m_renderPass.End(commandBuffer);

    commandBuffer->m_isInRenderPass = false;
}

void VulkanFramebuffer::Clear(VulkanCommandBuffer* commandBuffer, uint8 attachmentsMask)
{
    if (m_attachmentMap.Size() == 0 || attachmentsMask == 0)
    {
        return; // nothing to clear
    }

    bool shouldCapture = !commandBuffer->IsInRenderPass();

    if (shouldCapture)
    {
        BeginCapture(commandBuffer);
    }

    VkCommandBuffer vkCommandBuffer = commandBuffer->GetVulkanHandle();

    for (const auto& it : m_attachmentMap.attachments)
    {
        const uint32 binding = it.first;

        if (attachmentsMask != uint8(-1) && !(attachmentsMask & (1u << binding)))
            continue; // skip target

        VulkanAttachment* attachment = it.second.attachment;
        Assert(attachment != nullptr && attachment->IsCreated());

        Assert(attachment->GetImage().IsValid());

        VkClearAttachment clearAttachment = {};

        VkClearRect clearRect = {};
        clearRect.rect.offset.x = 0;
        clearRect.rect.offset.y = 0;
        clearRect.rect.extent.width = m_renderTargetDesc.extent.x;
        clearRect.rect.extent.height = m_renderTargetDesc.extent.y;
        clearRect.layerCount = 1;

        if (attachment->IsDepthAttachment())
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
            clearAttachment.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            clearAttachment.colorAttachment = attachment->GetBinding();
            clearAttachment.clearValue.color.float32[0] = attachment->GetClearColor().x;
            clearAttachment.clearValue.color.float32[1] = attachment->GetClearColor().y;
            clearAttachment.clearValue.color.float32[2] = attachment->GetClearColor().z;
            clearAttachment.clearValue.color.float32[3] = attachment->GetClearColor().w;
        }

        vkCmdClearAttachments(vkCommandBuffer, 1, &clearAttachment, 1, &clearRect);
    }

    if (shouldCapture)
    {
        EndCapture(commandBuffer);
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
