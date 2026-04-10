#pragma once

#include <vulkan/vulkan.h>

#include <Core/FxSlice.hpp>
#include <Math/FxVec2.hpp>
#include <Renderer/RxAttachment.hpp>

class RxSwapchain;
class RxGpuDevice;
class RxCommandBuffer;

class RxRenderPass
{
public:
    void Create(RxTargetList& color_attachments, FxVec2u size, const FxVec2u& offset = FxVec2u::sZero);

    void Begin(RxCommandBuffer* cmd, VkFramebuffer framebuffer, const FxSlice<VkClearValue>& clear_colors);
    void End();

    void Destroy();

    FX_FORCE_INLINE VkRenderPass Get() const { return RenderPass; }

    ~RxRenderPass() { Destroy(); }

public:
    VkRenderPass RenderPass = nullptr;
    RxCommandBuffer* pCommandBuffer = nullptr;

    FxVec2u Size = FxVec2u::sZero;
    FxVec2u Offset = FxVec2u::sZero;

private:
    RxGpuDevice* mpDevice = nullptr;
};
