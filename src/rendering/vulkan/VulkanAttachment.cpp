/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanAttachment.hpp>
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderBackend.hpp>

#include <VulkanAttachment.generated.inl>

namespace Hyperion {

#pragma region Helpers

extern VkImageLayout GetVkImageLayout(ResourceState state);

static VkImageLayout GetInitialLayout(LoadOperation loadOperation, bool isDepthAttachment)
{
    const int loadOperationIndex = loadOperation == LoadOperation::LOAD ? 1 : 0;

    return GetVkImageLayout(isDepthAttachment ? PreRenderResourceStatesDepth[loadOperationIndex] : PreRenderResourceStates[loadOperationIndex]);
}

static VkImageLayout GetFinalLayout(RenderTargetType renderTargetType, bool isDepthAttachment)
{
    return GetVkImageLayout(isDepthAttachment ? PostRenderResourceStatesDepth[renderTargetType] : PostRenderResourceStates[renderTargetType]);
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
      m_renderTargetType(renderTargetType),
      m_vkAttachmentReference {},
      m_vkAttachmentDescription {}
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

    m_vkAttachmentDescription = VkAttachmentDescription {
        .format = ToVkFormat(m_image->GetTextureFormat()),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = ToVkLoadOp(m_loadOperation),
        .storeOp = ToVkStoreOp(m_storeOperation),
        .stencilLoadOp = IsDepthAttachment() ? ToVkLoadOp(m_loadOperation) : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = IsDepthAttachment() ? ToVkStoreOp(m_storeOperation) : VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = GetInitialLayout(m_loadOperation, IsDepthAttachment()),
        .finalLayout = GetFinalLayout(m_renderTargetType, IsDepthAttachment())
    };

    AssertDebug(HasBinding());

    m_vkAttachmentReference = VkAttachmentReference {
        .attachment = m_binding,
        .layout = GetIntermediateLayout(IsDepthAttachment())
    };

    if (!m_image->IsCreated())
    {
        return HYP_MAKE_ERROR(RendererError, "Image is expected to be initialized before initializing attachment");
    }

    return m_imageView->Create();
}

#pragma endregion VulkanAttachment

} // namespace Hyperion
