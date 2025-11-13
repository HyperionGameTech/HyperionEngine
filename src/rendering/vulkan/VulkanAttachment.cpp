/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <HyperionPch.hpp>

#include <rendering/vulkan/VulkanAttachment.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderBackend.hpp>

#include <VulkanAttachment.generated.inl>

namespace hyperion {
#pragma region Helpers

static VkImageLayout GetInitialLayout(LoadOperation loadOperation, bool isDepthAttachment)
{
    switch (loadOperation)
    {
    case LoadOperation::CLEAR:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case LoadOperation::LOAD:
        return isDepthAttachment
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

static VkImageLayout GetFinalLayout(RenderTargetType renderTargetType, bool isDepthAttachment)
{
    switch (renderTargetType)
    {
    case RTT_NONE:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case RTT_PRESENT:
        return isDepthAttachment
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case RTT_SHADER_RESOURCE:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    case RTT_RENDER_TARGET:
        return isDepthAttachment
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    default:
        HYP_UNREACHABLE();
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
        HYP_UNREACHABLE();
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
        HYP_UNREACHABLE();
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
    RenderTargetType renderTargetType,
    LoadOperation loadOperation,
    StoreOperation storeOperation,
    BlendFunction blendFunction)
    : AttachmentBase(image, framebuffer, loadOperation, storeOperation, blendFunction),
      m_renderTargetType(renderTargetType)
{
    m_imageView = CreateObject<VulkanGpuImageView>(image);
}

VulkanAttachment::~VulkanAttachment()
{
    SafeDelete(std::move(m_image));
    SafeDelete(std::move(m_imageView));
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

VkAttachmentDescription VulkanAttachment::GetVulkanAttachmentDescription() const
{
    return VkAttachmentDescription {
        .format = ToVkFormat(GetFormat()),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = ToVkLoadOp(GetLoadOperation()),
        .storeOp = ToVkStoreOp(GetStoreOperation()),
        .stencilLoadOp = IsDepthAttachment() ? ToVkLoadOp(GetLoadOperation()) : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = GetInitialLayout(GetLoadOperation(), IsDepthAttachment()),
        .finalLayout = GetFinalLayout(m_renderTargetType, IsDepthAttachment())
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
