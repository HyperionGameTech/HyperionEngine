#pragma once

#include "RxFramebuffer.hpp"
#include "RxImage.hpp"
#include "RxSampler.hpp"

#include <vulkan/vulkan.h>

#include <Core/FxSizedArray.hpp>
#include <Math/FxVec2.hpp>

class RxGpuDevice;

class RxSwapchain
{
public:
    RxSwapchain() = default;
    ~RxSwapchain();

    void Init(FxVec2u size, VkSurfaceKHR& surface, RxGpuDevice* device);

    VkSwapchainKHR GetSwapchain() const { return mSwapchain; }

    float GetAspectRatio() const
    {
        FxAssert(Extent.X > 0 && Extent.Y > 0);
        return static_cast<float>(Extent.X) / Extent.Y;
    }

    void Destroy();

private:
    void CreateSwapchain(FxVec2u size, VkSurfaceKHR& surface);
    void CreateSwapchainImages();
    void CreateImageViews();
    void CreateFramebuffers();
    void CreateSamplers();

    void DestroyFramebuffersAndImageViews();
    void DestroyInternalSwapchain();

public:
    FxSizedArray<RxImage> OutputImages;

    RxSampler ColorSampler;
    RxSampler DepthSampler;
    RxSampler ShadowDepthSampler;
    RxSampler NormalsSampler;
    RxSampler LightsSampler;

    FxVec2u Extent = FxVec2u::sZero;

    // VkSurfaceFormatKHR SurfaceFormat;
    struct
    {
        RxImageFormat Format = RxImageFormat::eNone;
        VkColorSpaceKHR ColorSpace;
    } Surface;

    bool bInitialized = false;

private:
    RxGpuDevice* mDevice = nullptr;

    FxSizedArray<RxFramebuffer> mFramebuffers;

    // RxGraphicsPipeline* mPipeline = nullptr;
    // RxGraphicsPipeline* mCompPipeline = nullptr;
    VkSwapchainKHR mSwapchain = nullptr;
};
