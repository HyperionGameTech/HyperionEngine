#ifndef HYPERION_RENDERER_ATTACHMENT_H
#define HYPERION_RENDERER_ATTACHMENT_H

#include "renderer_device.h"
#include "renderer_image.h"

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
class AttachmentBase {
    friend class RenderPass;
public:
    AttachmentBase(
        VkFormat format,
        VkAttachmentLoadOp load_op,
        VkAttachmentStoreOp store_op,
        VkAttachmentLoadOp stencil_load_op,
        VkAttachmentStoreOp stencil_store_op,
        VkImageLayout final_layout,
        uint32_t ref_attachment,
        VkImageLayout ref_layout
    );
    AttachmentBase(const AttachmentBase &other) = delete;
    AttachmentBase &operator=(const AttachmentBase &other) = delete;
    ~AttachmentBase();

    Result Create(Device *device);
    Result Destroy(Device *device);

private:
    VkFormat m_format;
    VkAttachmentLoadOp m_load_op;
    VkAttachmentStoreOp m_store_op;
    VkAttachmentLoadOp m_stencil_load_op;
    VkAttachmentStoreOp m_stencil_store_op;
    VkImageLayout m_final_layout;
    uint32_t m_ref_attachment;

    VkImageLayout m_ref_layout;
    VkAttachmentDescription m_attachment_description;
    VkAttachmentReference m_attachment_reference;
};

template<VkImageLayout RefLayout, VkImageLayout FinalLayout>
class Attachment : public AttachmentBase {
public:
    Attachment() = delete;
};

/* === Presentation === */

template<>
class Attachment<
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
> : public AttachmentBase {
public:
    Attachment(uint32_t binding, VkFormat format) : AttachmentBase(
        format,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        binding,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    ) {}
};

template<>
class Attachment<
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
> : public AttachmentBase {
public:
    Attachment(uint32_t binding, VkFormat format) : AttachmentBase(
        format,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        binding,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    ) {}
};

/* Render-to-texture passes */

template<>
class Attachment<
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
> : public AttachmentBase {
public:
    Attachment(uint32_t binding, VkFormat format) : AttachmentBase(
        format,
        VK_ATTACHMENT_LOAD_OP_CLEAR,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        binding,
        VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
    ) {}
};

template<>
class Attachment<
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
> : public AttachmentBase {
public:
    Attachment(uint32_t binding, VkFormat format) : AttachmentBase(
        format,
        VK_ATTACHMENT_LOAD_OP_LOAD,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        binding,
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
    ) {}
};

template<>
class Attachment<
    VK_IMAGE_LAYOUT_GENERAL,
    VK_IMAGE_LAYOUT_GENERAL
> : public AttachmentBase {
public:
    Attachment(uint32_t binding, VkFormat format) : AttachmentBase(
        format,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_STORE,
        VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        VK_ATTACHMENT_STORE_OP_DONT_CARE,
        VK_IMAGE_LAYOUT_GENERAL,
        binding,
        VK_IMAGE_LAYOUT_GENERAL
    ) {}
};

} // namespace renderer
} // namespace hyperion

#endif