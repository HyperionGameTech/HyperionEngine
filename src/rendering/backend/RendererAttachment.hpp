#ifndef HYPERION_V2_BACKEND_RENDERER_ATTACHMENT_H
#define HYPERION_V2_BACKEND_RENDERER_ATTACHMENT_H

#include <util/Defines.hpp>

#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererAttachment.hpp>
#else
#error Unsupported rendering backend
#endif

#endif