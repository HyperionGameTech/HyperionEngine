#pragma once

#include "RxCommands.hpp"
#include "RxDescriptors.hpp"
#include "RxSynchro.hpp"

#include <Math/FxMat4.hpp>


struct alignas(16) RxUniformBufferObject
{
    FxMat4f MvpMatrix;
};


struct RxFrameData
{
public:
    void Create(RxGpuDevice* device);
    void SubmitUbo(const RxUniformBufferObject& ubo);
    void Destroy();

public:
    RxCommandPool CommandPool;

    RxCommandBuffer CommandBuffer;

    // RxDescriptorSet DescriptorSet;
    RxDescriptorSet CompDescriptorSet;

    // RxRawGpuBuffer<RxUniformBufferObject> Ubo;

    RxSemaphore OffscreenSem;
    RxSemaphore LightingSem;
    RxSemaphore ShadowsSem;

    RxSemaphore ImageAvailable;
    RxSemaphore RenderFinished;
    RxFence InFlight;
};
