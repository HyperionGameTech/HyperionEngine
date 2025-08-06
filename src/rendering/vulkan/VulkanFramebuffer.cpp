/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanResult.hpp>

#include <rendering/RenderQueue.hpp>

#include <core/math/MathUtil.hpp>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

static void TransitionFramebufferAttachments(RenderQueue& renderQueue, VulkanFramebuffer* framebuffer, Span<VulkanAttachmentDef*> attachmentDefs)
{
    Assert(framebuffer != nullptr);

    for (const VulkanAttachmentDef* attachmentDef : attachmentDefs)
    {
        const VulkanImageRef& image = attachmentDef->image;
        HYP_GFX_ASSERT(image.IsValid());

        if (framebuffer->GetRenderPass()->GetStage() == RenderPassStage::PRESENT)
        {
            renderQueue << InsertBarrier(image, RS_PRESENT);
        }
        else
        {
            renderQueue << InsertBarrier(image, RS_SHADER_RESOURCE);
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
            HYP_GFX_CHECK(def.image->Create());
        }

        attachmentDefs.PushBack(&def);

        HYP_GFX_ASSERT(def.attachment.IsValid());

        if (!def.attachment->IsCreated())
        {
            HYP_GFX_CHECK(def.attachment->Create());
        }
    }

    FrameBase* frame = GetRenderBackend()->GetCurrentFrame();

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

        VulkanImageRef newImage = def.image;

        if (def.attachment->GetFramebuffer() == framebufferWeak)
        {
            TextureDesc textureDesc = def.image->GetTextureDesc();
            textureDesc.extent = Vec3u { newSize.x, newSize.y, 1 };

            newImage = MakeRenderObject<VulkanImage>(textureDesc);
            HYP_GFX_ASSERT(newImage->Create());

            if (def.image.IsValid())
            {
                SafeRelease(std::move(def.image));
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

        VulkanAttachmentRef newAttachment = MakeRenderObject<VulkanAttachment>(
            newImage,
            framebufferWeak,
            def.attachment->GetRenderPassStage(),
            def.attachment->GetLoadOperation(),
            def.attachment->GetStoreOperation());

        newAttachment->SetBinding(def.attachment->GetBinding());

        HYP_GFX_ASSERT(newAttachment->Create());

        if (def.attachment.IsValid())
        {
            SafeRelease(std::move(def.attachment));
        }

        def = VulkanAttachmentDef {
            std::move(newImage),
            std::move(newAttachment)
        };

        attachmentDefs.PushBack(&def);
    }

    FrameBase* frame = GetRenderBackend()->GetCurrentFrame();

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

VulkanFramebuffer::VulkanFramebuffer(Vec2u extent, RenderPassStage stage, uint32 numMultiviewLayers)
    : FramebufferBase(extent),
      m_handle(VK_NULL_HANDLE),
      m_renderPass(MakeRenderObject<VulkanRenderPass>(stage, RenderPassMode::RENDER_PASS_INLINE, numMultiviewLayers))
{
    m_attachmentMap.framebufferWeak = VulkanFramebufferWeakRef(WeakHandleFromThis());
}

VulkanFramebuffer::~VulkanFramebuffer()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE, "Expected framebuffer to have been destroyed");
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

    for (const auto& it : m_attachmentMap.attachments)
    {
        HYP_GFX_ASSERT(it.second.attachment != nullptr);
        HYP_GFX_ASSERT(it.second.attachment->GetImageView() != nullptr);
        HYP_GFX_ASSERT(it.second.attachment->GetImageView()->IsCreated());

        attachmentImageViews.PushBack(VULKAN_CAST(it.second.attachment->GetImageView())->GetVulkanHandle());
    }

    VkFramebufferCreateInfo framebufferCreateInfo { VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO };
    framebufferCreateInfo.renderPass = m_renderPass->GetVulkanHandle();
    framebufferCreateInfo.attachmentCount = uint32(attachmentImageViews.Size());
    framebufferCreateInfo.pAttachments = attachmentImageViews.Data();
    framebufferCreateInfo.width = m_extent.x;
    framebufferCreateInfo.height = m_extent.y;
    framebufferCreateInfo.layers = numLayers;

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        VULKAN_CHECK(vkCreateFramebuffer(GetRenderBackend()->GetDevice()->GetDevice(), &framebufferCreateInfo, nullptr, &m_handle));
    }

    FrameBase* frame = GetRenderBackend()->GetCurrentFrame();

    if (frame != nullptr)
    {
        RenderQueue& renderQueue = frame->renderQueue;
        renderQueue << ClearFramebuffer(HandleFromThis());

        return {};
    }

    UniquePtr<SingleTimeCommands> singleTimeCommands = GetRenderBackend()->GetSingleTimeCommands();

    singleTimeCommands->Push([&](RenderQueue& renderQueue) -> RendererResult
        {
            renderQueue << ClearFramebuffer(HandleFromThis());

            return {};
        });

    RendererResult result = singleTimeCommands->Execute();
    if (!result)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to clear framebuffer on create! Error was: {}", result.GetError().GetErrorCode(), result.GetError().GetMessage());
    }
    
    // ok
    return {};
}

RendererResult VulkanFramebuffer::Destroy()
{
    if (!IsCreated())
    {
        HYPERION_RETURN_OK;
    }

    if (m_handle != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(GetRenderBackend()->GetDevice()->GetDevice(), m_handle, nullptr);
        m_handle = VK_NULL_HANDLE;
    }

    SafeRelease(std::move(m_renderPass));

    m_attachmentMap.Reset();

    HYPERION_RETURN_OK;
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

        attachmentImageViews.PushBack(VULKAN_CAST(it.second.attachment->GetImageView().Get())->GetVulkanHandle());
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

    RenderQueue& renderQueue = g_renderBackend->GetCurrentFrame()->renderQueue;
    renderQueue << ClearFramebuffer(HandleFromThis());

    HYPERION_RETURN_OK;
}

AttachmentRef VulkanFramebuffer::AddAttachment(const AttachmentRef& attachment)
{
    HYP_GFX_ASSERT(attachment->GetFramebuffer() == this,
        "Attachment framebuffer does not match framebuffer");

    return m_attachmentMap.AddAttachment(VulkanAttachmentRef(attachment));
}

AttachmentRef VulkanFramebuffer::AddAttachment(
    uint32 binding,
    const ImageRef& image,
    LoadOperation loadOp,
    StoreOperation storeOp)
{
    VulkanAttachmentRef attachment = MakeRenderObject<VulkanAttachment>(
        VulkanImageRef(image),
        VulkanFramebufferWeakRef(WeakHandleFromThis()),
        m_renderPass->GetStage(),
        loadOp,
        storeOp);

    attachment->SetBinding(binding);

    return AddAttachment(attachment);
}

AttachmentRef VulkanFramebuffer::AddAttachment(
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
        m_renderPass->GetStage(),
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

    SafeRelease(std::move(it->second.attachment));

    m_attachmentMap.attachments.Erase(it);

    return true;
}

AttachmentBase* VulkanFramebuffer::GetAttachment(uint32 binding) const
{
    return m_attachmentMap.GetAttachment(binding).Get();
}

void VulkanFramebuffer::BeginCapture(CommandBufferBase* commandBuffer)
{
    HYP_GFX_ASSERT(!VULKAN_CAST(commandBuffer)->IsInRenderPass());

    VULKAN_CAST(commandBuffer)->m_isInRenderPass = true;
    VULKAN_CAST(commandBuffer)->ResetBoundDescriptorSets();

    m_renderPass->Begin(VULKAN_CAST(commandBuffer), this);
}

void VulkanFramebuffer::EndCapture(CommandBufferBase* commandBuffer)
{
    HYP_GFX_ASSERT(VULKAN_CAST(commandBuffer)->IsInRenderPass());

    m_renderPass->End(VULKAN_CAST(commandBuffer));

    VULKAN_CAST(commandBuffer)->m_isInRenderPass = false;
}

void VulkanFramebuffer::Clear(CommandBufferBase* commandBuffer)
{
    bool shouldCapture = !VULKAN_CAST(commandBuffer)->IsInRenderPass();

    if (shouldCapture)
    {
        BeginCapture(commandBuffer);
    }

    VkCommandBuffer vkCommandBuffer = VULKAN_CAST(commandBuffer)->GetVulkanHandle();

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
