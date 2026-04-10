#include "RxFrameData.hpp"

#include <vulkan/vulkan.h>


void RxFrameData::Create(RxGpuDevice* device)
{
    ImageAvailable.Create();
    RenderFinished.Create();
    OffscreenSem.Create();
    LightingSem.Create();
    ShadowsSem.Create();

    InFlight.Create();

    // Ubo.Create(1, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
    // Ubo.Map();
}

void RxFrameData::SubmitUbo(const RxUniformBufferObject& ubo)
{
    // memcpy(Ubo.MappedBuffer, &ubo, sizeof(RxUniformBufferObject));
}

void RxFrameData::Destroy()
{
    ImageAvailable.Destroy();
    RenderFinished.Destroy();
    OffscreenSem.Destroy();
    LightingSem.Destroy();
    ShadowsSem.Destroy();

    InFlight.Destroy();

    // Ubo.UnMap();
    // Ubo.Destroy();
};
