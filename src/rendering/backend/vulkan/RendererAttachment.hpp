/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#ifndef HYPERION_RENDERER_BACKEND_VULKAN_ATTACHMENT_HPP
#define HYPERION_RENDERER_BACKEND_VULKAN_ATTACHMENT_HPP

#include <rendering/backend/RenderObject.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererImage.hpp>
#include <rendering/backend/RendererImageView.hpp>
#include <rendering/backend/Platform.hpp>

#include <core/math/MathUtil.hpp>

#include <HashCode.hpp>
#include <Types.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
struct AttachmentPlatformImpl<Platform::VULKAN>
{
    Attachment<Platform::VULKAN>    *self = nullptr;

    HYP_API VkAttachmentDescription GetAttachmentDescription() const;
    HYP_API VkAttachmentReference GetHandle() const;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion

#endif
