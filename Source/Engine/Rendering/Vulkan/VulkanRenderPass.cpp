/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanRenderPass.hpp>
#include <Rendering/Vulkan/VulkanFramebuffer.hpp>
#include <Rendering/Vulkan/VulkanCommandBuffer.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanMemory.hpp>
#include <Rendering/Vulkan/VulkanResult.hpp>
#include <Rendering/Vulkan/VulkanHelpers.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <VulkanRenderPass.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

VulkanRenderPass::VulkanRenderPass()
    : VulkanRenderPass(FramebufferDesc {})
{
}

VulkanRenderPass::VulkanRenderPass(const FramebufferDesc& framebufferDesc)
    : m_framebufferDesc(framebufferDesc),
      m_handle(VK_NULL_HANDLE),
      m_recordingFramebuffer(nullptr)
{
}

VulkanRenderPass::VulkanRenderPass(VulkanRenderPass&& other) noexcept
    : m_framebufferDesc(other.m_framebufferDesc),
      m_dependencies(std::move(other.m_dependencies)),
      m_vkClearValues(std::move(other.m_vkClearValues)),
      m_handle(other.m_handle),
      m_recordingFramebuffer(other.m_recordingFramebuffer)
{
    other.m_handle = VK_NULL_HANDLE;
    other.m_recordingFramebuffer = nullptr;
}

VulkanRenderPass& VulkanRenderPass::operator=(VulkanRenderPass&& other) noexcept
{
    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
            {
                vkDestroyRenderPass(RI.GetDevice()->GetDevice(), handle, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
    }

    m_framebufferDesc = other.m_framebufferDesc;

    m_dependencies = std::move(other.m_dependencies);

    m_vkClearValues = std::move(other.m_vkClearValues);

    m_handle = other.m_handle;
    other.m_handle = VK_NULL_HANDLE;

    m_recordingFramebuffer = other.m_recordingFramebuffer;
    other.m_recordingFramebuffer = nullptr;

    return *this;
}

VulkanRenderPass::~VulkanRenderPass()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle]()
            {
                vkDestroyRenderPass(RI.GetDevice()->GetDevice(), handle, nullptr);
            }));

        m_handle = VK_NULL_HANDLE;
    }
}

void VulkanRenderPass::CreateDependencies()
{
    m_dependencies.Clear();

    Optional<VkSubpassDependency> loadDependency;
    Optional<VkSubpassDependency> storeDependency;

    switch (m_framebufferDesc.renderPassMode)
    {
    case RenderPassMode::Present:
        loadDependency = VkSubpassDependency {
            .srcSubpass = VK_SUBPASS_EXTERNAL,
            .dstSubpass = 0,
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
        };

        break;
    case RenderPassMode::RenderTarget:
    {
        for (uint32 attachmentIdx = 0; attachmentIdx < m_framebufferDesc.numAttachments; attachmentIdx++)
        {
            const AttachmentDesc& attachmentDesc = m_framebufferDesc.attachments[attachmentIdx];

            switch (attachmentDesc.loadOp)
            {
            case LoadOperation::CLEAR: // fallthrough
            case LoadOperation::LOAD:
                if (!loadDependency.HasValue())
                {
                    loadDependency = VkSubpassDependency {
                        .srcSubpass = VK_SUBPASS_EXTERNAL,
                        .dstSubpass = 0,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                    };
                }

                if (TextureUtils::IsDepthFormat(attachmentDesc.format))
                {
                    loadDependency->dstStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                    loadDependency->dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                    loadDependency->srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
                    loadDependency->srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;

                    if (attachmentDesc.loadOp == LoadOperation::LOAD)
                    {
                        loadDependency->dstAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
                    }
                }
                else
                {
                    loadDependency->dstStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    loadDependency->dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    loadDependency->srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
                    loadDependency->srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

                    if (attachmentDesc.loadOp == LoadOperation::LOAD)
                    {
                        loadDependency->dstAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
                    }
                }
                break;
            default:
                break;
            }

            switch (attachmentDesc.storeOp)
            {
            case StoreOperation::STORE:
                if (!storeDependency.HasValue())
                {
                    storeDependency = VkSubpassDependency {
                        .srcSubpass = 0,
                        .dstSubpass = VK_SUBPASS_EXTERNAL,
                        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT
                    };
                }

                if (TextureUtils::IsDepthFormat(attachmentDesc.format))
                {
                    storeDependency->srcStageMask |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
                    storeDependency->srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                    storeDependency->dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; /// \todo : revisit if compute bit needed if we change SSR to be fragment shader
                    storeDependency->dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                }
                else
                {
                    storeDependency->srcStageMask |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
                    storeDependency->srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

                    storeDependency->dstStageMask |= VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; /// \todo : revisit if compute bit needed if we change SSR to be fragment shader
                    storeDependency->dstAccessMask |= VK_ACCESS_SHADER_READ_BIT;
                }

                break;
            }
        }

        break;
    }
    default:
        HYP_UNREACHABLE();
    }

    if (loadDependency.HasValue())
    {
        m_dependencies.PushBack(*loadDependency);
    }

    if (storeDependency.HasValue())
    {
        m_dependencies.PushBack(*storeDependency);
    }
}

RendererResult VulkanRenderPass::Create()
{
    CreateDependencies();

    Array<VkAttachmentDescription, VulkanTempAllocator> vkAttachmentDescriptions;
    vkAttachmentDescriptions.Reserve(m_framebufferDesc.numAttachments);

    VkAttachmentReference depthAttachmentReference {};

    Array<VkAttachmentReference, VulkanTempAllocator> colorAttachmentReferences;
    colorAttachmentReferences.Reserve(4);

    VkSubpassDescription subpassDescription {};
    subpassDescription.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpassDescription.pDepthStencilAttachment = nullptr;

    for (uint32 attachmentIdx = 0; attachmentIdx < m_framebufferDesc.numAttachments; attachmentIdx++)
    {
        const AttachmentDesc& attachmentDesc = m_framebufferDesc.attachments[attachmentIdx];

        uint32 attachmentIndex = uint32(vkAttachmentDescriptions.Size());
        vkAttachmentDescriptions.PushBack(ToVkAttachmentDescription(attachmentDesc, m_framebufferDesc.renderPassMode));

        if (TextureUtils::IsDepthFormat(attachmentDesc.format))
        {
            depthAttachmentReference = ToVkAttachmentReference(attachmentIndex, attachmentDesc);
            subpassDescription.pDepthStencilAttachment = &depthAttachmentReference;

            VkClearValue& clearValue = m_vkClearValues.EmplaceBack();
            clearValue.depthStencil = { 1.0f, 0 };
        }
        else
        {
            colorAttachmentReferences.PushBack(ToVkAttachmentReference(attachmentIndex, attachmentDesc));

            VkClearValue& clearValue = m_vkClearValues.EmplaceBack();

            if (attachmentDesc.clearColorIsF16)
            {
                clearValue.color = {
                    .float32 = {
                        float(attachmentDesc.clearColorF16[0]),
                        float(attachmentDesc.clearColorF16[1])
                    }
                };
            }
            else
            {
                clearValue.color = {
                    .float32 = {
                        attachmentDesc.clearColor.GetRed(),
                        attachmentDesc.clearColor.GetGreen(),
                        attachmentDesc.clearColor.GetBlue(),
                        attachmentDesc.clearColor.GetAlpha()
                    }
                };
            }
        }
    }

    subpassDescription.colorAttachmentCount = uint32(colorAttachmentReferences.Size());
    subpassDescription.pColorAttachments = colorAttachmentReferences.Data();

    // Create the actual renderpass
    VkRenderPassCreateInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO };
    renderPassInfo.attachmentCount = uint32(vkAttachmentDescriptions.Size());
    renderPassInfo.pAttachments = vkAttachmentDescriptions.Data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpassDescription;
    renderPassInfo.dependencyCount = uint32(m_dependencies.Size());
    renderPassInfo.pDependencies = m_dependencies.Data();

    // Disabling multiview for now to keep at par with DX12.
#if 0
    uint32 multiviewViewMask = 0;
    uint32 multiviewCorrelationMask = 0;

    VkRenderPassMultiviewCreateInfo multiviewInfo { VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO };
    multiviewInfo.subpassCount = 1;
    multiviewInfo.pViewMasks = &multiviewViewMask;
    multiviewInfo.pCorrelationMasks = &multiviewCorrelationMask;
    multiviewInfo.correlationMaskCount = 1;

    if (IsMultiview())
    {
        for (uint32 i = 0; i < m_framebufferDesc.numLayers; i++)
        {
            multiviewViewMask |= 1 << i;
            multiviewCorrelationMask |= 1 << i;
        }

        renderPassInfo.pNext = &multiviewInfo;
    }
#endif

    VULKAN_CHECK(vkCreateRenderPass(RI.GetDevice()->GetDevice(), &renderPassInfo, nullptr, &m_handle));
#ifdef HYP_RHI_DEBUG_NAMES
    SetDebugName(debugName);
#endif

    return {};
}

#ifdef HYP_RHI_DEBUG_NAMES
void VulkanRenderPass::SetDebugName(Name name)
{
    if (m_handle == VK_NULL_HANDLE)
    {
        return;
    }

    if (RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_RENDER_PASS;
        objectNameInfo.objectHandle = (uint64)m_handle;
        objectNameInfo.pObjectName = name.LookupString();

        RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
    }
}
#endif

void VulkanRenderPass::Begin(VulkanCommandBuffer* cmd, VulkanFramebuffer* framebuffer)
{
    if (m_recordingFramebuffer)
    {
        return;
    }

    Assert(!cmd->IsInRenderPass());

    Assert(framebuffer != nullptr);
    Assert(m_handle != VK_NULL_HANDLE);

    VkRenderPassBeginInfo renderPassInfo { VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO };
    renderPassInfo.renderPass = m_handle;
    renderPassInfo.framebuffer = framebuffer->GetVulkanHandle();
    renderPassInfo.renderArea.offset = VkOffset2D { m_framebufferDesc.offset.x, m_framebufferDesc.offset.y };
    renderPassInfo.renderArea.extent = VkExtent2D { m_framebufferDesc.extent.x, m_framebufferDesc.extent.y };
    renderPassInfo.clearValueCount = uint32(m_vkClearValues.Size());
    renderPassInfo.pClearValues = m_vkClearValues.Data();

    VulkanFrame* currentFrame = RI.GetCurrentFrame();
    if (currentFrame != nullptr)
    {
        currentFrame->AddRenderPass(this);
    }

    // transition render targets to initial layout for render passes
    for (uint32 attachmentIndex = 0; attachmentIndex < framebuffer->NumAttachments(); attachmentIndex++)
    {
        VulkanAttachment* attachment = framebuffer->GetAttachment(attachmentIndex);
        AssertDebug(attachment != nullptr);

        const AttachmentDesc& attachmentDesc = attachment->GetAttachmentDesc();

        VulkanGpuImage* image = attachment->GetGpuImage();

        const ImageSubResource& subResource = attachment->GetImageView()->GetImageSubResource();

        const TextureDesc& textureDesc = image->GetTextureDesc();

        const bool isDepthStencil = textureDesc.IsDepthStencil();
        const bool hasStencil = TextureUtils::HasStencilComponent(textureDesc.format);

        const bool fullSubResource = image->IsFullSubResource(subResource);

        if (hasStencil && (attachmentDesc.onlyStencil || attachmentDesc.onlyDepth))
        {
            AssertDebug(fullSubResource,
                "OnlyStencil / OnlyDepth can only be used with attachments that cover the full range of the image");
        }

        if (hasStencil && fullSubResource)
        {
            const bool transitionDepth = !attachmentDesc.onlyStencil && image->GetResourceState() != RS_RENDER_TARGET;
            const bool transitionStencil = !attachmentDesc.onlyDepth && image->GetStencilState() != RS_RENDER_TARGET;

            if (transitionDepth ^ transitionStencil)
            {
                if (transitionDepth)
                {
                    image->InsertBarrier(cmd, RS_RENDER_TARGET, ShaderModuleType::Pixel, /* onlyDepth */ true, /* onlyStencil */ false);
                }

                if (transitionStencil)
                {
                    image->InsertBarrier(cmd, RS_RENDER_TARGET, ShaderModuleType::Pixel, /* onlyDepth */ false, /* onlyStencil */ true);
                }
            }
            else if (transitionDepth && transitionStencil)
            {
                image->InsertBarrier(cmd, RS_RENDER_TARGET, ShaderModuleType::Pixel);
            }

            continue;
        }

        if (fullSubResource)
        {
            image->InsertBarrier(cmd, RS_RENDER_TARGET, ShaderModuleType::Pixel);
        }
        else if (image->GetSubResourceState(subResource) != RS_RENDER_TARGET)
        {
            image->InsertBarrier(cmd, subResource, RS_RENDER_TARGET, ShaderModuleType::Pixel);
        }
    }

    vkCmdBeginRenderPass(cmd->GetVulkanHandle(), &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    m_recordingFramebuffer = framebuffer;
}

void VulkanRenderPass::End(VulkanCommandBuffer* cmd)
{
    if (!m_recordingFramebuffer)
    {
        return;
    }

    vkCmdEndRenderPass(cmd->GetVulkanHandle());

    // handle implicit layout transitions after render pass end
    for (uint32 attachmentIndex = 0; attachmentIndex < m_recordingFramebuffer->NumAttachments(); attachmentIndex++)
    {
        VulkanAttachment* attachment = m_recordingFramebuffer->GetAttachment(attachmentIndex);
        AssertDebug(attachment != nullptr);

        const ResourceState newState = PostRenderResourceStates[uint32(m_framebufferDesc.renderPassMode)];

        const ImageSubResource& subResource = attachment->GetImageView()->GetImageSubResource();

        const bool fullSubResource = attachment->GetGpuImage()->IsFullSubResource(subResource);

        if (attachment->IsDepthAttachment())
        {
            if (attachment->GetAttachmentDesc().onlyStencil)
            {
                AssertDebug(fullSubResource);

                attachment->GetGpuImage()->SetStencilState(newState);

                continue;
            }

            if (attachment->GetAttachmentDesc().onlyDepth)
            {
                AssertDebug(fullSubResource);

                const ResourceState stencilState = attachment->GetGpuImage()->GetStencilState();

                attachment->GetGpuImage()->SetResourceState(newState);
                attachment->GetGpuImage()->SetStencilState(stencilState);

                continue;
            }
        }

        if (fullSubResource)
        {
            attachment->GetGpuImage()->SetResourceState(newState);
        }
        else
        {
            attachment->GetGpuImage()->SetSubResourceState(subResource, newState);
        }
    }

    m_recordingFramebuffer = nullptr;
}

} // namespace Hyperion
