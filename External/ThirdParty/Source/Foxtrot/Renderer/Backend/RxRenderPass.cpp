#include "RxRenderPass.hpp"

#include "RxDevice.hpp"
#include "RxSwapchain.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxPanic.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>


FX_SET_MODULE_NAME("RxRenderPass")

void RxRenderPass::Create(RxTargetList& attachments, FxVec2u size, const FxVec2u& offset)
{
    if (size == RxTarget::scFullScreen) {
        size = gRenderer->Swapchain.Extent;
    }

    Size = size;
    Offset = offset;

    FxAssert(size.X > 0.0f && size.Y > 0.0f);

    mpDevice = gRenderer->GetDevice();

    FxSizedArray<VkAttachmentReference> color_refs(attachments.Targets.Size);

    bool has_depth_attachment = false;
    VkAttachmentReference depth_attachment_ref {};

    for (int i = 0; i < attachments.Targets.Size; i++) {
        const RxTarget& attachment = attachments.Targets[i];

        if (attachment.IsDepth()) {
            has_depth_attachment = true;

            depth_attachment_ref.attachment = i;
            depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        }
        else {
            VkAttachmentReference* ref = color_refs.Insert();

            ref->attachment = i;
            ref->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
    }

    VkSubpassDescription subpass {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = static_cast<uint32>(color_refs.Size),
        .pColorAttachments = color_refs.pData,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = has_depth_attachment ? &depth_attachment_ref : nullptr,
    };

    // VkSubpassDependency subpass_dependencies[] = {
    //     VkSubpassDependency {
    //         .srcSubpass = 0,
    //         .dstSubpass = VK_SUBPASS_EXTERNAL,

    //         .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    //                       | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

    //         .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,

    //         .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    //                        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,

    //         .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,

    //         .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    //     },
    //     VkSubpassDependency {
    //         .srcSubpass = VK_SUBPASS_EXTERNAL,
    //         .dstSubpass = 0,

    //         .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,

    //         .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    //                       | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

    //         .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,

    //         .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT
    //                        | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT,

    //         .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    //     }
    // };
    //
    VkSubpassDependency subpass_dependencies[] = {
        VkSubpassDependency {
            .srcSubpass = 0,
            .dstSubpass = VK_SUBPASS_EXTERNAL,

            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

            .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,

            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,

            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        },
        VkSubpassDependency {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,

            .srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,

            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,

            .srcAccessMask = VK_ACCESS_SHADER_READ_BIT,

            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,

            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
        }
    };

    FxSizedArray<VkAttachmentDescription>& descriptions = attachments.GetDescriptions();

    VkRenderPassCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = static_cast<uint32>(descriptions.Size),
        .pAttachments = descriptions.pData,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = std::size(subpass_dependencies),
        .pDependencies = subpass_dependencies,
    };

    const VkResult status = vkCreateRenderPass(mpDevice->Device, &create_info, nullptr, &RenderPass);

    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Failed to create render pass", status);
    }
}

void RxRenderPass::Begin(RxCommandBuffer* cmd, VkFramebuffer framebuffer, const FxSlice<VkClearValue>& clear_values)
{
    pCommandBuffer = cmd;

    if (RenderPass == nullptr) {
        FxModulePanic("Render pass has not been previously created", 0);
    }

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = RenderPass,
        .framebuffer = framebuffer,
        .renderArea = { .offset = { .x = static_cast<int32>(Offset.X), .y = static_cast<int32>(Offset.Y) },
                        .extent = { .width = Size.Width(), .height = Size.Height() } },

        .clearValueCount = clear_values.Size,
        .pClearValues = clear_values.pData,
    };

    vkCmdBeginRenderPass(cmd->CommandBuffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
}

void RxRenderPass::End()
{
    FxAssert(pCommandBuffer != nullptr);

    vkCmdEndRenderPass(pCommandBuffer->CommandBuffer);
}

void RxRenderPass::Destroy()
{
    if (RenderPass != nullptr) {
        vkDestroyRenderPass(mpDevice->Device, RenderPass, nullptr);
    }
    RenderPass = nullptr;
}
