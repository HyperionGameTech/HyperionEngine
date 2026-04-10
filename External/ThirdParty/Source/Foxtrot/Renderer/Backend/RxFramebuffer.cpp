#include "RxFramebuffer.hpp"

#include "RxDevice.hpp"

#include <Core/FxDefines.hpp>
#include <Core/FxPanic.hpp>
#include <Core/FxTypes.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>


FX_SET_MODULE_NAME("Framebuffer")

void RxFramebuffer::Create(const FxSizedArray<VkImageView>& image_views, const RxRenderPass& render_pass, FxVec2u size)
{
    if (size == RxTarget::scFullScreen) {
        size = gRenderer->Swapchain.Extent;
    }

    const VkFramebufferCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = render_pass.RenderPass,
        .attachmentCount = static_cast<uint32>(image_views.Size),
        .pAttachments = image_views.pData,
        .width = size.X,
        .height = size.Y,
        .layers = 1,
    };

    mDevice = gRenderer->GetDevice();

    const VkResult status = vkCreateFramebuffer(mDevice->Device, &create_info, nullptr, &Framebuffer);

    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Failed to create framebuffer", status);
    }
}

void RxFramebuffer::Destroy()
{
    if (!Framebuffer) {
        return;
    }
    vkDestroyFramebuffer(mDevice->Device, Framebuffer, nullptr);
    Framebuffer = nullptr;
}
