/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererHelpers.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

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

#pragma region Attachment

template <>
Attachment<Platform::VULKAN>::Attachment(
    const ImageRef<Platform::VULKAN> &image,
    RenderPassStage stage,
    LoadOperation load_operation,
    StoreOperation store_operation,
    BlendFunction blend_function
) : m_platform_impl { this },
    m_image(image),
    m_image_view(MakeRenderObject<ImageView<Platform::VULKAN>>()),
    m_stage(stage),
    m_load_operation(load_operation),
    m_store_operation(store_operation),
    m_blend_function(blend_function),
    m_clear_color(Vec4f::Zero())
{
}

template <>
Attachment<Platform::VULKAN>::~Attachment()
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));
}

template <>
bool Attachment<Platform::VULKAN>::IsCreated() const
{
    return m_image_view != nullptr && m_image_view->IsCreated();
}

template <>
RendererResult Attachment<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_image != nullptr);

    if (!m_image->IsCreated()) {
        return HYP_MAKE_ERROR(RendererError, "Image is expected to be initialized before initializing attachment");
    }

    return m_image_view->Create(device, m_image);
}

template <>
RendererResult Attachment<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    SafeRelease(std::move(m_image));
    SafeRelease(std::move(m_image_view));

    HYPERION_RETURN_OK;
}

#pragma endregion Attachment

#pragma region AttachmentPlatformImpl

VkAttachmentDescription AttachmentPlatformImpl<Platform::VULKAN>::GetAttachmentDescription() const
{
    return VkAttachmentDescription {
        .format         = helpers::ToVkFormat(self->GetFormat()),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = ToVkLoadOp(self->GetLoadOperation()),
        .storeOp        = ToVkStoreOp(self->GetStoreOperation()),
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE, // <-- @TODO for stencil
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = GetInitialLayout(self->GetLoadOperation()),
        .finalLayout    = GetFinalLayout(self->GetRenderPassStage(), self->IsDepthAttachment())
    };
}

VkAttachmentReference AttachmentPlatformImpl<Platform::VULKAN>::GetHandle() const
{
    if (!self->HasBinding()) {
        DebugLog(LogType::Warn, "Calling GetHandle() without a binding set on attachment ref -- binding will be set to %ul\n", self->GetBinding());
    }

    return VkAttachmentReference {
        .attachment = self->GetBinding(),
        .layout     = GetIntermediateLayout(self->IsDepthAttachment())
    };
}

#pragma endregion AttachmentPlatformImpl

} // namespace platform
} // namespace renderer
} // namespace hyperion