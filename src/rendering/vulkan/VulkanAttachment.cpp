/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanAttachment.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>

#include <rendering/RenderBackend.hpp>

namespace hyperion {
#pragma region Helpers

static VkImageLayout GetInitialLayout(LoadOperation loadOperation)
{
    switch (loadOperation)
    {
    case LoadOperation::CLEAR:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case LoadOperation::LOAD:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

static VkImageLayout GetFinalLayout(RenderPassStage stage, bool isDepthAttachment)
{
    switch (stage)
    {
    case RenderPassStage::NONE:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case RenderPassStage::PRESENT:
        return isDepthAttachment
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case RenderPassStage::SHADER:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

static VkAttachmentLoadOp ToVkLoadOp(LoadOperation loadOperation)
{
    switch (loadOperation)
    {
    case LoadOperation::UNDEFINED:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case LoadOperation::NONE:
        return VK_ATTACHMENT_LOAD_OP_NONE_EXT;
    case LoadOperation::CLEAR:
        return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case LoadOperation::LOAD:
        return VK_ATTACHMENT_LOAD_OP_LOAD;
    default:
        return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    }
}

static VkAttachmentStoreOp ToVkStoreOp(StoreOperation storeOperation)
{
    switch (storeOperation)
    {
    case StoreOperation::UNDEFINED:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case StoreOperation::NONE:
        return VK_ATTACHMENT_STORE_OP_NONE_EXT;
    case StoreOperation::STORE:
        return VK_ATTACHMENT_STORE_OP_STORE;
    default:
        return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    }
}

static VkImageLayout GetIntermediateLayout(bool isDepthAttachment)
{
    return isDepthAttachment
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

#pragma endregion Helpers

#pragma region VulkanAttachment

VulkanAttachment::VulkanAttachment(
    const VulkanGpuImageRef& image,
    const VulkanFramebufferWeakRef& framebuffer,
    RenderPassStage stage,
    LoadOperation loadOperation,
    StoreOperation storeOperation,
    BlendFunction blendFunction)
    : AttachmentBase(image, framebuffer, loadOperation, storeOperation, blendFunction),
      m_stage(stage)
{
    m_imageView = MakeRenderObject<VulkanGpuImageView>(image);
}

VulkanAttachment::~VulkanAttachment()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_imageView));
}

bool VulkanAttachment::IsCreated() const
{
    return m_imageView != nullptr && m_imageView->IsCreated();
}

RendererResult VulkanAttachment::Create()
{
    HYP_GFX_ASSERT(m_image != nullptr);

    if (!m_image->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Image is expected to be initialized before initializing attachment");
    }

    return m_imageView->Create();
}

RendererResult VulkanAttachment::Destroy()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_imageView));

    return {};
}

VkAttachmentDescription VulkanAttachment::GetVulkanAttachmentDescription() const
{
    return VkAttachmentDescription {
        .format = ToVkFormat(GetFormat()),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = ToVkLoadOp(GetLoadOperation()),
        .storeOp = ToVkStoreOp(GetStoreOperation()),
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // <-- @TODO for stencil
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = GetInitialLayout(GetLoadOperation()),
        .finalLayout = GetFinalLayout(GetRenderPassStage(), IsDepthAttachment())
    };
}

VkAttachmentReference VulkanAttachment::GetVulkanHandle() const
{
    if (!HasBinding())
    {
        DebugLog(LogType::Warn, "Calling GetHandle() without a binding set on attachment ref -- binding will be set to %ul\n", GetBinding());
    }

    return VkAttachmentReference {
        .attachment = GetBinding(),
        .layout = GetIntermediateLayout(IsDepthAttachment())
    };
}

#pragma endregion VulkanAttachment

} // namespace hyperion