/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once
#include <rendering/vulkan/VulkanGpuImage.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanSampler.hpp>
#include <rendering/vulkan/VulkanAttachment.hpp>

#include <rendering/RenderTypes.hpp>
#include <rendering/Shared.hpp>

#include <Core/math/Vector4.hpp>
#include <Core/containers/Array.hpp>

#include <Core/Types.hpp>

#include <vulkan/vulkan.h>

namespace Hyperion {

extern Pool* g_vulkanPool;

HYP_CLASS(NoScriptBindings)
class VulkanRenderPass final : public ObjectBase
{
    HYP_OBJECT_BODY(VulkanRenderPass);

public:
    static Pool* GetAllocator() { return g_vulkanPool; }

    VulkanRenderPass();

    explicit VulkanRenderPass(const FramebufferDesc& framebufferDesc);

    VulkanRenderPass(VulkanRenderPass&& other) noexcept;
    VulkanRenderPass& operator=(VulkanRenderPass&& other) noexcept;

    ~VulkanRenderPass() override;

    HYP_FORCE_INLINE VkRenderPass GetVulkanHandle() const
    {
        return m_handle;
    }

    HYP_FORCE_INLINE FramebufferDesc& GetFramebufferDesc()
    {
        return m_framebufferDesc;
    }

    HYP_FORCE_INLINE const FramebufferDesc& GetFramebufferDesc() const
    {
        return m_framebufferDesc;
    }

    HYP_FORCE_INLINE bool IsMultiview() const
    {
        return m_framebufferDesc.numLayers > 1;
    }

    HYP_FORCE_INLINE uint32 NumMultiviewLayers() const
    {
        return m_framebufferDesc.numLayers;
    }

    Span<const AttachmentDesc> GetAttachmentDescs() const
    {
        return m_framebufferDesc.attachments;
    }

    RendererResult Create();

#if HYP_DEBUG_MODE
    void SetDebugName(Name name);
#endif

    void Begin(VulkanCommandBuffer* cmd, VulkanFramebuffer* framebuffer);
    void End(VulkanCommandBuffer* cmd);

#if HYP_DEBUG_MODE
    Name debugName;
#endif

private:
    void CreateDependencies();

    void AddDependency(const VkSubpassDependency& dependency)
    {
        m_dependencies.PushBack(dependency);
    }

    FramebufferDesc m_framebufferDesc;

    Array<VkSubpassDependency, VulkanAllocator> m_dependencies;
    Array<VkClearValue, VulkanAllocator> m_vkClearValues;

    VkRenderPass m_handle;

    VulkanFramebuffer* m_recordingFramebuffer;
};

using VulkanRenderPassRef = Handle<VulkanRenderPass>;
using VulkanRenderPassWeakRef = WeakHandle<VulkanRenderPass>;

} // namespace Hyperion
