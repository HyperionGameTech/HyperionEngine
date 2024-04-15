/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererAttachment.hpp>
#include <rendering/backend/RendererHelpers.hpp>

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region Helpers

static VkImageLayout GetInitialLayout()
{
    return VK_IMAGE_LAYOUT_UNDEFINED;
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

#pragma region AttachmentUsage

AttachmentUsage<Platform::VULKAN>::AttachmentUsage(
    AttachmentRef<Platform::VULKAN> attachment,
    LoadOperation load_operation,
    StoreOperation store_operation,
    BlendFunction blend_function
) : AttachmentUsage(
        std::move(attachment),
        MakeRenderObject<ImageView<Platform::VULKAN>>(),
        MakeRenderObject<Sampler<Platform::VULKAN>>(),
        load_operation,
        store_operation,
        blend_function
    )
{
    m_image_view_owned = true;
    m_sampler_owned = true;
}

AttachmentUsage<Platform::VULKAN>::AttachmentUsage(
    AttachmentRef<Platform::VULKAN> attachment,
    ImageViewRef<Platform::VULKAN> image_view,
    SamplerRef<Platform::VULKAN> sampler,
    LoadOperation load_operation,
    StoreOperation store_operation,
    BlendFunction blend_function
) : m_attachment(std::move(attachment)),
    m_load_operation(load_operation),
    m_store_operation(store_operation),
    m_blend_function(blend_function),
    m_image_view(std::move(image_view)),
    m_sampler(std::move(sampler)),
    m_image_view_owned(false),
    m_sampler_owned(false)
{
}

AttachmentUsage<Platform::VULKAN>::~AttachmentUsage()
{
}

InternalFormat AttachmentUsage<Platform::VULKAN>::GetFormat() const
{
    if (m_attachment == nullptr) {
        DebugLog(LogType::Warn, "No attachment set on attachment ref, cannot check format\n");

        return InternalFormat::NONE;
    }

    return m_attachment->GetFormat();
}

bool AttachmentUsage<Platform::VULKAN>::IsDepthAttachment() const
{
    if (m_attachment == nullptr) {
        DebugLog(LogType::Warn, "No attachment set on attachment ref, cannot check format\n");

        return false;
    }

    return m_attachment->IsDepthAttachment();
}

VkAttachmentDescription AttachmentUsage<Platform::VULKAN>::GetAttachmentDescription() const
{
    return VkAttachmentDescription {
        .format         = helpers::ToVkFormat(m_attachment->GetFormat()),
        .samples        = VK_SAMPLE_COUNT_1_BIT,
        .loadOp         = ToVkLoadOp(m_load_operation),
        .storeOp        = ToVkStoreOp(m_store_operation),
        .stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout  = GetInitialLayout(),
        .finalLayout    = GetFinalLayout(m_attachment->GetRenderPassStage(), m_attachment->IsDepthAttachment())
    };
}

VkAttachmentReference AttachmentUsage<Platform::VULKAN>::GetHandle() const
{
    if (!HasBinding()) {
        DebugLog(LogType::Warn, "Calling GetHandle() without a binding set on attachment ref -- binding will be set to %ul\n", GetBinding());
    }

    const bool is_depth_attachment = m_attachment
        ? m_attachment->IsDepthAttachment()
        : false;

    return VkAttachmentReference {
        .attachment = GetBinding(),
        .layout     = GetIntermediateLayout(is_depth_attachment)
    };
}

Result AttachmentUsage<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_image_view != nullptr);
    AssertThrow(m_sampler != nullptr);

    if (m_image_view_owned) {
        AssertThrowMsg(m_attachment != nullptr, "Cannot create ImageView for AttachmentUsage without a parent Attachment");

        HYPERION_BUBBLE_ERRORS(m_image_view->Create(device, m_attachment->GetImage()));
    }

    if (m_sampler_owned) {
        HYPERION_BUBBLE_ERRORS(m_sampler->Create(device));
    }

    HYPERION_RETURN_OK;
}

Result AttachmentUsage<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    SafeRelease(std::move(m_image_view));
    SafeRelease(std::move(m_sampler));

    HYPERION_RETURN_OK;
}

#pragma endregion AttachmentUsage

#pragma region Attachment

template <>
Attachment<Platform::VULKAN>::Attachment(ImageRef<Platform::VULKAN> image, RenderPassStage stage)
    : m_image(std::move(image)),
      m_stage(stage)
{
}

template <>
Attachment<Platform::VULKAN>::~Attachment()
{
}

template <>
Result Attachment<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_image != nullptr);

    Result result;

    HYPERION_BUBBLE_ERRORS(m_image->Create(device));

    return result;
}

template <>
Result Attachment<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_image != nullptr);

    Result result;

    SafeRelease(std::move(m_image));

    return result;
}

#pragma endregion Attachment

} // namespace platform
} // namespace renderer
} // namespace hyperion