/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderQueue.hpp>

#include <core/math/MathUtil.hpp>

#include <VulkanFramebuffer.generated.inl>

namespace hyperion {

extern VulkanRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return g_renderBackend;
}

static void TransitionFramebufferAttachments(RenderQueue& renderQueue, VulkanFramebuffer* framebuffer, Span<VulkanAttachmentDef*> attachmentDefs)
{
    Assert(framebuffer != nullptr);

    for (const VulkanAttachmentDef* attachmentDef : attachmentDefs)
    {
        const VulkanGpuImageRef& image = attachmentDef->image;
        HYP_GFX_ASSERT(image.IsValid());

        switch (framebuffer->GetRenderPass()->GetRenderTargetType())
        {
        case RTT_PRESENT:
            renderQueue << InsertBarrier(image, RS_PRESENT);
            break;
        case RTT_SHADER_RESOURCE:
            renderQueue << InsertBarrier(image, RS_SHADER_RESOURCE);
            break;
        case RTT_RENDER_TARGET:
            renderQueue << InsertBarrier(image, RS_RENDER_TARGET);
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

        HYP_GFX_ASSERT(def.image.IsValid());

        if (!def.image->IsCreated())
        {
            if (!def.image->GetDebugName().IsValid())
            {
                def.image->SetDebugName(NAME_FMT("{}_RT_{}", framebuffer->Id(), it.first));
            }

            HYP_GFX_CHECK(def.image->Create());
        }

        attachmentDefs.PushBack(&def);

        HYP_GFX_ASSERT(def.attachment.IsValid());

        if (!def.attachment->IsCreated())
        {
            HYP_GFX_CHECK(def.attachment->Create());
        }
    }

    VulkanFrame* frame = GetRenderBackend()->GetCurrentFrame();

    // frame may be nullptr if we are creating a swapchain
    if (frame != nullptr)
    {
        RenderQueue& renderQueue = frame->preRenderQueue;

        TransitionFramebufferAttachments(renderQueue, framebuffer, attachmentDefs.ToSpan());

        return {};
    }

    UniquePtr<SingleTimeCommands> singleTimeCommands = GetRenderBackend()->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue) -> RendererResult
        {
            TransitionFramebufferAttachments(renderQueue, framebuffer, attachmentDefs.ToSpan());

            return {};
        });

    return singleTimeCommands->Execute();
}

RendererResult VulkanAttachmentMap::Resize(Vec2u newSize)
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

        HYP_GFX_ASSERT(def.image.IsValid());

        VulkanGpuImageRef newImage = def.image;

        if (def.attachment->GetFramebuffer() == framebufferWeak)
        {
            TextureDesc textureDesc = def.image->GetTextureDesc();
            textureDesc.extent = Vec3u { newSize.x, newSize.y, 1 };

            newImage = CreateObject<VulkanGpuImage>(textureDesc);
            newImage->SetDebugName(def.image->GetDebugName());
            HYP_GFX_ASSERT(newImage->Create());

            if (def.image.IsValid())
            {
                SafeDelete(std::move(def.image));
            }
        }
        else
        {
            if (def.image->GetExtent().GetXY() != newSize)
            {
                return HYP_MAKE_ERROR(RendererError, "Expected image to have a size matching {} but got size: {}", 0,
                    newSize, def.image->GetExtent().GetXY());
            }
        }

        VulkanAttachmentRef newAttachment = CreateObject<VulkanAttachment>(
            newImage,
            framebufferWeak,
            framebuffer->GetRenderTargetType(),
            def.attachment->GetLoadOperation(),
            def.attachment->GetStoreOperation());

        newAttachment->SetBinding(def.attachment->GetBinding());

        HYP_GFX_ASSERT(newAttachment->Create());

        if (def.attachment.IsValid())
        {
            SafeDelete(std::move(def.attachment));
        }

        def = VulkanAttachmentDef {
            std::move(newImage),
            std::move(newAttachment)
        };

        attachmentDefs.PushBack(&def);
    }

    VulkanFrame* frame = GetRenderBackend()->GetCurrentFrame();

    // frame may be nullptr if we are creating a swapchain
    if (frame != nullptr)
    {
        RenderQueue& renderQueue = frame->renderQueue;

        TransitionFramebufferAttachments(renderQueue, framebuffer, attachmentDefs.ToSpan());

        return {};
    }

    UniquePtr<SingleTimeCommands> singleTimeCommands = GetRenderBackend()->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue) -> RendererResult
        {
            TransitionFramebufferAttachments(renderQueue, framebuffer, attachmentDefs.ToSpan());

            return {};
        });

    return singleTimeCommands->Execute();
}

#pragma endregion VulkanAttachmentMap

#pragma region VulkanFramebuffer

VulkanFramebuffer::VulkanFramebuffer(Vec2u extent, RenderTargetType renderTargetType, uint32 numMultiviewLayers)
    : FramebufferBase(extent, renderTargetType),
      m_handle(VK_NULL_HANDLE),
      m_renderPass(CreateObject<VulkanRenderPass>(renderTargetType, RenderPassMode::RENDER_PASS_INLINE, numMultiviewLayers))
{
    m_attachmentMap.framebufferWeak = VulkanFramebufferWeakRef(WeakHandleFromThis());
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    if (!IsCreated())
    {
        return;
    }

    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }

    SafeDelete(std::move(m_renderPass));

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
        HYPERION_RETURN_OK;
    }

    HYP_GFX_CHECK(m_attachmentMap.Create());

    for (const auto& it : m_attachmentMap.attachments)
    {
        const VulkanAttachmentDef& def = it.second;

        HYP_GFX_ASSERT(def.attachment.IsValid());
        m_renderPass->AddAttachment(def.attachment);
    }

    HYP_GFX_CHECK(m_renderPass->Create());

    Array<VkImageView> attachmentImageViews;
    attachmentImageViews.Reserve(m_attachmentMap.attachments.Size());

    uint32 numLayers = 1;
    bool shouldClearFramebuffer = true;

    for (const auto& it : m_attachmentMap.attachments)
    {
        VulkanAttachment* attachment = it.second.attachment.Get();
        HYP_GFX_ASSERT(attachment != nullptr);

        if (attachment->GetLoadOperation() == LoadOperation::LOAD)
        {
            shouldClearFramebuffer = false;
        }

        HYP_GFX_ASSERT(attachment->GetImageView() != nullptr);
        HYP_GFX_ASSERT(attachment->GetImageView()->IsCreated());

        attachmentImageViews.PushBack(attachment->GetImageView()->GetVulkanHandle());
    }

    VkFramebufferCreateInfo framebufferCreateInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebufferCreateInfo.renderPass = m_renderPass->GetVulkanHandle();
    framebufferCreateInfo.attachmentCount = uint32(attachmentImageViews.Size());
    framebufferCreateInfo.pAttachments = attachmentImageViews.Data();
    framebufferCreateInfo.width = m_extent.x;
    framebufferCreateInfo.height = m_extent.y;
    framebufferCreateInfo.layers = numLayers;

    for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
    {
        VULKAN_CHECK(vkCreateFramebuffer(GetRenderBackend()->GetDevice()->GetDevice(), &framebufferCreateInfo, nullptr, &m_handle));
    }

    if (shouldClearFramebuffer)
    {
        VulkanFrame* frame = GetRenderBackend()->GetCurrentFrame();

        // clear in current frame
        if (frame != nullptr)
        {
            RenderQueue& renderQueue = frame->renderQueue;
            renderQueue << ClearFramebuffer(this);

            return {};
        }

        UniquePtr<SingleTimeCommands> singleTimeCommands = GetRenderBackend()->GetSingleTimeCommands();

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

    // ok
    return {};
}

RendererResult VulkanFramebuffer::Resize(Vec2u newSize)
{
    if (m_extent == newSize)
    {
        HYPERION_RETURN_OK;
    }

    m_extent = newSize;

    if (!IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    HYP_GFX_CHECK(m_attachmentMap.Resize(newSize));

    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }

    Array<VkImageView> attachmentImageViews;
    attachmentImageViews.Reserve(m_attachmentMap.attachments.Size());

    uint32 numLayers = 1;

    for (const auto& it : m_attachmentMap.attachments)
    {
        HYP_GFX_ASSERT(it.second.attachment != nullptr);
        HYP_GFX_ASSERT(it.second.attachment->GetImageView() != nullptr);
        HYP_GFX_ASSERT(it.second.attachment->GetImageView()->IsCreated());

        attachmentImageViews.PushBack(it.second.attachment->GetImageView()->GetVulkanHandle());
    }

    VkFramebufferCreateInfo framebufferCreateInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebufferCreateInfo.renderPass = m_renderPass->GetVulkanHandle();
    framebufferCreateInfo.attachmentCount = uint32(attachmentImageViews.Size());
    framebufferCreateInfo.pAttachments = attachmentImageViews.Data();
    framebufferCreateInfo.width = newSize.x;
    framebufferCreateInfo.height = newSize.y;
    framebufferCreateInfo.layers = numLayers;

    VULKAN_CHECK(vkCreateFramebuffer(
        GetRenderBackend()->GetDevice()->GetDevice(),
        &framebufferCreateInfo,
        nullptr,
        &m_handle));

    RenderQueue& renderQueue = g_renderBackend->GetCurrentFrame()->preRenderQueue;
    renderQueue << ClearFramebuffer(this);

    HYPERION_RETURN_OK;
}

VulkanAttachmentRef VulkanFramebuffer::AddAttachment(const VulkanAttachmentRef& attachment)
{
    HYP_GFX_ASSERT(attachment->GetFramebuffer().GetUnsafe() == this,
        "Attachment framebuffer does not match framebuffer");

    return m_attachmentMap.AddAttachment(VulkanAttachmentRef(attachment));
}

VulkanAttachmentRef VulkanFramebuffer::AddAttachment(
    uint32 binding,
    const VulkanGpuImageRef& image,
    LoadOperation loadOp,
    StoreOperation storeOp)
{
    VulkanAttachmentRef attachment = CreateObject<VulkanAttachment>(
        VulkanGpuImageRef(image),
        VulkanFramebufferWeakRef(WeakHandleFromThis()),
        m_renderTargetType,
        loadOp,
        storeOp);

    attachment->SetBinding(binding);

    return AddAttachment(attachment);
}

VulkanAttachmentRef VulkanFramebuffer::AddAttachment(
    uint32 binding,
    TextureFormat format,
    TextureType type,
    LoadOperation loadOp,
    StoreOperation storeOp)
{
    return m_attachmentMap.AddAttachment(
        binding,
        m_extent,
        format,
        type,
        m_renderTargetType,
        loadOp,
        storeOp);
}

bool VulkanFramebuffer::RemoveAttachment(uint32 binding)
{
    const auto it = m_attachmentMap.attachments.Find(binding);

    if (it == m_attachmentMap.attachments.End())
    {
        return false;
    }

    SafeDelete(std::move(it->second.attachment));

    m_attachmentMap.attachments.Erase(it);

    return true;
}

VulkanAttachment* VulkanFramebuffer::GetAttachment(uint32 binding) const
{
    return m_attachmentMap.GetAttachment(binding).Get();
}

void VulkanFramebuffer::BeginCapture(VulkanCommandBuffer* commandBuffer)
{
    HYP_GFX_ASSERT(!commandBuffer->IsInRenderPass());

    commandBuffer->m_isInRenderPass = true;
    commandBuffer->ResetBoundDescriptorSets();

    m_renderPass->Begin(commandBuffer, this);
}

void VulkanFramebuffer::EndCapture(VulkanCommandBuffer* commandBuffer)
{
    HYP_GFX_ASSERT(commandBuffer->IsInRenderPass());

    m_renderPass->End(commandBuffer);

    commandBuffer->m_isInRenderPass = false;
}

void VulkanFramebuffer::Clear(VulkanCommandBuffer* commandBuffer)
{
    bool shouldCapture = !commandBuffer->IsInRenderPass();

    if (shouldCapture)
    {
        BeginCapture(commandBuffer);
    }

    VkCommandBuffer vkCommandBuffer = commandBuffer->GetVulkanHandle();

    for (const auto& it : m_attachmentMap.attachments)
    {
        const VulkanAttachmentRef& attachment = it.second.attachment;
        HYP_GFX_ASSERT(attachment.IsValid() && attachment->IsCreated());

        HYP_GFX_ASSERT(attachment->GetImage().IsValid());

        VkClearAttachment clearAttachment = {};

        VkClearRect clearRect = {};
        clearRect.rect.offset.x = 0;
        clearRect.rect.offset.y = 0;
        clearRect.rect.extent.width = m_extent.x;
        clearRect.rect.extent.height = m_extent.y;
        clearRect.layerCount = 1;

        if (attachment->IsDepthAttachment())
        {
            clearAttachment.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
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

#pragma endregion VulkanFramebuffer

} // namespace hyperion
