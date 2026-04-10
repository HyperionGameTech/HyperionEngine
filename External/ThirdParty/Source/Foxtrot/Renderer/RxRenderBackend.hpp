#pragma once

#include "Backend/RxCommands.hpp"
#include "Backend/RxFrameData.hpp"
#include "Backend/RxPipeline.hpp"
#include "Backend/RxSwapchain.hpp"
#include "Backend/RxSynchro.hpp"
#include "FxDeletionObject.hpp"
#include "FxWindow.hpp"
#include "RxDeferred.hpp"
#include "RxUniformBuffer.hpp"

#include <ThirdParty/vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <Core/FxDefer.hpp>
#include <Core/FxRef.hpp>
#include <deque>
#include <mutex>


enum class RxFrameResult
{
    Success,
    GraphicsOutOfDate,
    RenderError,
};

class RxDeferredRenderer;
class RxDeferredGPass;
class RxDeferredCompPass;

struct RxGpuUploadContext
{
    RxCommandPool CommandPool;
    RxCommandBuffer CommandBuffer;

    RxFence UploadFence;

    ~RxGpuUploadContext() = default;
};

class FxCamera;

class RxRenderBackend
{
    const uint32 scDeletionFrameSpacing = 3;

    static constexpr uint32 scDefaultUniformSize = 512;
    static constexpr uint32 scNumBones = 50;

public:
    using SubmitFunc = std::function<void(RxCommandBuffer& cmd)>;

public:
    RxRenderBackend() = default;

    using ExtensionList = FxSizedArray<VkExtensionProperties>;
    using ExtensionNames = std::vector<const char*>;

    void Init(FxVec2u window_size);
    void Destroy();

    RxFrameResult BeginFrame();
    void BeginGeometry();
    void BeginLighting();
    void BeginUnlit();
    void DoComposition(FxCamera& render_cam);

    void SelectWindow(const FxRef<FxWindow>& window) { mpWindow = window; }

    FX_FORCE_INLINE FxRef<FxWindow> GetWindow() { return mpWindow; }
    FX_FORCE_INLINE RxGpuDevice* GetDevice() { return &mDevice; }

    RxUniformBufferObject& GetUbo();

    void AddGpuBufferToDeletionQueue(VkBuffer buffer, VmaAllocation allocation)
    {
        std::lock_guard<std::mutex> guard(mInDeletionQueue);

        mDeletionQueue.push_back(FxDeletionObject {
            .Buffer = buffer,
            .Allocation = allocation,
            .DeletionFrameNumber = mInternalFrameCounter + scDeletionFrameSpacing,
            .bIsGpuBuffer = true,
        });
    }

    VkInstance GetVulkanInstance() { return mInstance; }

    RxFrameData* GetFrame();

    uint32 GetImageIndex() const { return mImageIndex; }
    VmaAllocator* GetGPUAllocator() { return &GpuAllocator; }

    void SubmitUploadCmd(SubmitFunc func);
    void SubmitOneTimeCmd(SubmitFunc func);

    ~RxRenderBackend() { Destroy(); }

    bool ProcessDeletionQueue(bool immediate = false)
    {
        if (immediate) {
            mInDeletionQueue.lock();
        }
        else if (!mInDeletionQueue.try_lock()) {
            return false;
        }

        if (mDeletionQueue.empty()) {
            mInDeletionQueue.unlock();
            return false;
        }

        FxDeletionObject& object = mDeletionQueue.front();

        const bool is_frame_spaced = (mInternalFrameCounter >= object.DeletionFrameNumber);

        bool did_delete = false;

        if (immediate || is_frame_spaced) {
            if (object.bIsGpuBuffer) {
                vmaDestroyBuffer(GpuAllocator, object.Buffer, object.Allocation);
                did_delete = true;
            }
            else {
                object.Func(&object);
                did_delete = true;
            }

            mDeletionQueue.pop_front();
        }

        mInDeletionQueue.unlock();

        return did_delete;
    }

    void AddToDeletionQueue(FxDeletionObject::FuncType func)
    {
        // FxLogInfo("Adding object to deletion queue at frame {}", mInternalFrameCounter);

        std::lock_guard<std::mutex> guard(mInDeletionQueue);

        mDeletionQueue.push_back(FxDeletionObject {
            .DeletionFrameNumber = mInternalFrameCounter + scDeletionFrameSpacing,
            .Func = func,
        });
    }

    uint32 GetElapsedFrameCount() const { return mInternalFrameCounter.load(); }
    uint32 GetFrameNumber() const { return mFrameNumber; }

private:
    void InitVulkan();
    void CreateSurfaceFromWindow();

    void InitGPUAllocator();
    void DestroyGPUAllocator();

    void InitUploadContext();
    void DestroyUploadContext();

    void PresentFrame();

    void InitFrames();
    void DestroyFrames();

    RxFrameResult GetNextSwapchainImage(RxFrameData* frame);

    ExtensionList& QueryInstanceExtensions(bool invalidate_previous = false);
    ExtensionNames MakeInstanceExtensionList(ExtensionNames& user_requested_extensions);
    ExtensionNames CheckExtensionsAvailable(ExtensionNames& requested_extensions);

    FxSizedArray<VkLayerProperties> GetAvailableValidationLayers();

public:
    RxSwapchain Swapchain;
    FxSizedArray<RxFrameData> Frames;

    VmaAllocator GpuAllocator = nullptr;

    // XXX: temporary
    // RxDescriptorPool GPassDescriptorPool;
    RxDescriptorPool CompDescriptorPool;

    RxGpuUploadContext UploadContext;

    bool bInitialized = false;

    RxDeferredCompPass* pCurrentCompPass = nullptr;
    RxDeferredLightingPass* pCurrentLightingPass = nullptr;

    FxRef<RxDeferredRenderer> pDeferredRenderer { nullptr };

    RxUniforms Uniforms;
    RxUniforms BoneBuffer;

    // RxSamplerCache SamplerCache;

    // RxPipelineCache PipelineCache;

    // RxSemaphore OffscreenSemaphore;

private:
    VkInstance mInstance = nullptr;
    VkSurfaceKHR mWindowSurface = nullptr;

    FxRef<FxWindow> mpWindow = nullptr;
    RxGpuDevice mDevice;

    VkDebugUtilsMessengerEXT mDebugMessenger;

    ExtensionList mAvailableExtensions;

    FxSizedArray<RxSemaphore> mSubmitSemaphores;

    uint32 mImageIndex = 0;

protected:
    uint32 mFrameNumber = 0;
    std::atomic_uint32_t mInternalFrameCounter = 0;

    std::mutex mInDeletionQueue;
    std::deque<FxDeletionObject> mDeletionQueue;
};
