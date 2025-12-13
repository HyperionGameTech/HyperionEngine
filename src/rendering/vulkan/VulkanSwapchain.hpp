/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/RenderSwapchain.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/RenderObject.hpp>
#include <rendering/vulkan/VulkanSemaphore.hpp>
#include <rendering/vulkan/VulkanFramebuffer.hpp>
#include <rendering/vulkan/VulkanStructs.hpp>

#include <rendering/Shared.hpp>

#include <core/containers/Array.hpp>

#include <core/math/Vector2.hpp>

#include <core/Types.hpp>
#include <core/Constants.hpp>

namespace hyperion {

struct VulkanDeviceQueue;

HYP_CLASS(NoScriptBindings)
class VulkanSwapchain final : public SwapchainBase
{
    HYP_OBJECT_BODY(VulkanSwapchain);

public:
    VulkanSwapchain(VkSurfaceKHR surface, const Vec2u& extent);
    virtual ~VulkanSwapchain() override;

    HYP_FORCE_INLINE VkSwapchainKHR GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE VkSurfaceKHR GetVulkanSurface() const
    {
        return m_surface;
    }

    virtual bool IsCreated() const override;

    void NextFrame();

    void PrepareForFrame(VulkanFrame* frame);
    void PresentFrame(VulkanFrame* frame, VulkanDeviceQueue* queue);

    virtual RendererResult Create() override;
    virtual void Resize(Vec2u newExtent) override;
    virtual void Recreate() override;

private:
    RendererResult ChooseSurfaceFormat();
    RendererResult RetrieveImageHandles();

    VkSwapchainKHR m_handle;
    VkSwapchainKHR m_oldHandle;
    VkSurfaceKHR m_surface;
    VkSurfaceFormatKHR m_surfaceFormat;
    VkPresentModeKHR m_presentMode;
    VulkanSwapchainSupportDetails m_supportDetails;
};

} // namespace hyperion
