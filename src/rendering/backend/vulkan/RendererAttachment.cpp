/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererAttachment.hpp>
#include <rendering/backend/vulkan/RendererImage.hpp>
#include <rendering/backend/vulkan/RendererImageView.hpp>

#include <rendering/backend/RendererHelpers.hpp>

namespace hyperion {
namespace renderer {

#pragma region Helpers

static VkImageLayout GetInitialLayout(LoadOperation load_operation)
{
    switch (load_operation) {
    case LoadOperation::CLEAR:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case LoadOperation::LOAD:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

static VkImageLayout GetFinalLayout(RenderPassStage stage, bool is_depth_attachment)
{
    switch (stage) {
    case RenderPassStage::NONE:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    case RenderPassStage::PRESENT:
        return is_depth_attachment
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
            : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    case RenderPassStage::SHADER:
        return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    default:
        return VK_IMAGE_LAYOUT_UNDEFINED;
    }
}

static VkAttachmentLoadOp ToVkLoadOp(LoadOperation load_operation)
{
    switch (load_operation) {
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

static VkAttachmentStoreOp ToVkStoreOp(StoreOperation store_operation)
{
    switch (store_operation) {
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

static VkImageLayout GetIntermediateLayout(bool is_depth_attachment)
{
    return is_depth_attachment
        ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
        : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

#pragma endregion Helpers

#pragma region VulkanAttachment

VulkanAttachment::VulkanAttachment(
    const VulkanImageRef &image,
    RenderPassStage stage,
    LoadOperation load_operation,
    StoreOperation store_operation,
    BlendFunction blend_function
) : AttachmentBase(image, load_operation, store_operation, blend_function),
    m_stage(stage)
{
    m_image_view = MakeRenderObject<VulkanImageView>();
}

VulkanAttachment::~VulkanAttachment()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));
}

bool VulkanAttachment::IsCreated() const
{
    return m_image_view != nullptr && m_image_view->IsCreated();
}

RendererResult VulkanAttachment::Create()
{
    AssertThrow(m_image != nullptr);

    if (!m_image->IsCreated()) {
        return HYP_MAKE_ERROR(RendererError, "Image is expected to be initialized before initializing attachment");
    }

    return m_image_view->Create(m_image);
}

RendererResult VulkanAttachment::Destroy()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));

    HYPERION_RETURN_OK;
}

VkAttachmentDescription VulkanAttachment::GetVulkanAttachmentDescription() const
{
    return VkAttachmentDescription {
        .format         = helpers::ToVkFormat(GetFormat()),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = ToVkLoadOp(GetLoadOperation()),
        .storeOp        = ToVkStoreOp(GetStoreOperation()),
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // <-- @TODO for stencil
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = GetInitialLayout(GetLoadOperation()),
        .finalLayout    = GetFinalLayout(GetRenderPassStage(), IsDepthAttachment())
    };
}

VkAttachmentReference VulkanAttachment::GetVulkanHandle() const
{
    if (!HasBinding()) {
        DebugLog(LogType::Warn, "Calling GetHandle() without a binding set on attachment ref -- binding will be set to %ul\n", GetBinding());
    }

    return VkAttachmentReference {
        .attachment = GetBinding(),
        .layout     = GetIntermediateLayout(IsDepthAttachment())
    };
}

#pragma endregion VulkanAttachment

} // namespace renderer
} // namespace hyperion