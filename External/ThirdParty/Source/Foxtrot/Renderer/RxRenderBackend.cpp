#include "RxRenderBackend.hpp"

#include "Backend/RxCommands.hpp"
#include "Backend/RxPipeline.hpp"
#include "Backend/RxSynchro.hpp"
#include "Backend/RxUtil.hpp"
#include "FxEngine.hpp"
#include "FxObjectManager.hpp"
#include "RxConstants.hpp"
#include "RxDeferred.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#include <Core/FxDefines.hpp>
#include <Core/FxPanic.hpp>
#include <Core/FxRefUtil.hpp>
#include <Core/FxTypes.hpp>
#include <Renderer/Backend/RxExtensionHandles.hpp>
#include <Renderer/FxCamera.hpp>
#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxState.hpp>
#include <thread>
#include <vector>

#define FX_VULKAN_DEBUG 1

using ExtensionNames = RxRenderBackend::ExtensionNames;
using ExtensionList = RxRenderBackend::ExtensionList;

FX_SET_MODULE_NAME("RenderBackend")

ExtensionNames RxRenderBackend::CheckExtensionsAvailable(ExtensionNames& requested_extensions)
{
    if (mAvailableExtensions.IsEmpty()) {
        QueryInstanceExtensions();
    }

    std::vector<const char*> missing_extensions;

    for (const char* requested_name : requested_extensions) {
        bool found_extension = false;
        for (const auto& extension : mAvailableExtensions) {
            if (!strncmp(extension.extensionName, requested_name, 256)) {
                found_extension = true;
                break;
            }
        }

        if (!found_extension) {
            missing_extensions.push_back(requested_name);
        }
    }

    return missing_extensions;
}

FxSizedArray<VkLayerProperties> RxRenderBackend::GetAvailableValidationLayers()
{
    uint32 layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

    FxSizedArray<VkLayerProperties> validation_layers;
    validation_layers.InitSize(layer_count);

    vkEnumerateInstanceLayerProperties(&layer_count, validation_layers.pData);

    return validation_layers;
}

VkDebugUtilsMessengerEXT CreateDebugMessenger(VkInstance instance);

void RxRenderBackend::Init(FxVec2u window_size)
{
    InitVulkan();
    CreateSurfaceFromWindow();

    mDevice.Create(mInstance, mWindowSurface);

    InitGPUAllocator();
    Swapchain.Init(window_size, mWindowSurface, &mDevice);

    InitFrames();
    InitUploadContext();

    // Create final submission semaphores. Note that there is one submission semaphore
    // per Swapchain image, not frame in flight.
    mSubmitSemaphores.InitSize(Swapchain.OutputImages.Size);
    for (RxSemaphore& sem : mSubmitSemaphores) {
        sem.Create();
    }

    gObjectManager->Create();

    Uniforms.Create(scDefaultUniformSize);
    BoneBuffer.Create(scNumBones * sizeof(FxMat4f));

    pDeferredRenderer = FxMakeRef<RxDeferredRenderer>();
    pDeferredRenderer->Create(Swapchain.Extent);

    bInitialized = true;
}

void RxRenderBackend::InitUploadContext()
{
    UploadContext.CommandPool.Create(GetDevice(), GetDevice()->mQueueFamilies.GetTransferFamily());
    UploadContext.CommandBuffer.Create(&UploadContext.CommandPool);

    UploadContext.UploadFence.Create();
    UploadContext.UploadFence.Reset();
}

void RxRenderBackend::DestroyUploadContext()
{
    UploadContext.UploadFence.Destroy();
    UploadContext.CommandBuffer.Destroy();
    UploadContext.CommandPool.Destroy();
}

void RxRenderBackend::InitFrames()
{
    Frames.InitSize(RxFramesInFlight);

    const uint32 graphics_family = GetDevice()->mQueueFamilies.GetGraphicsFamily();

    RxGpuDevice* device = GetDevice();

    for (int i = 0; i < Frames.Size; i++) {
        RxFrameData& frame = Frames.pData[i];
        frame.CommandPool.Create(device, graphics_family);
        frame.CommandBuffer.Create(&frame.CommandPool);
        /*frame.ShadowCommandBuffer.Create(&frame.CommandPool);
        frame.CompCommandBuffer.Create(&frame.CommandPool);
        frame.LightCommandBuffer.Create(&frame.CommandPool);*/

        frame.Create(device);

        auto synchro_label = std::format("Frame {} I.A.", i);
        RxUtil::SetDebugLabel(synchro_label.c_str(), VK_OBJECT_TYPE_SEMAPHORE, frame.ImageAvailable.Semaphore);

        synchro_label = std::format("Frame {} G.P", i);
        RxUtil::SetDebugLabel(synchro_label.c_str(), VK_OBJECT_TYPE_SEMAPHORE, frame.OffscreenSem.Semaphore);

        synchro_label = std::format("Frame {} R.F", i);
        RxUtil::SetDebugLabel(synchro_label.c_str(), VK_OBJECT_TYPE_SEMAPHORE, frame.RenderFinished.Semaphore);
    }
}

void RxRenderBackend::DestroyFrames()
{
    vkQueueWaitIdle(GetDevice()->GraphicsQueue);

    for (auto& frame : Frames) {
        // frame.DescriptorSet.Destroy();
        frame.CompDescriptorSet.Destroy();

        frame.CommandBuffer.Destroy();

        frame.Destroy();
    }

    Frames.Free();
}

void RxRenderBackend::InitVulkan()
{
    const char* app_name = "Foxtrot";
    VkApplicationInfo app_info = {};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = app_name;
    app_info.pEngineName = app_name;
    app_info.apiVersion = VK_MAKE_VERSION(1, 3, 261);

    ExtensionNames requested_extensions = {
        // VK_EXT_LAYER_SETTINGS_EXTENSION_NAME,
    };

    ExtensionNames all_extensions = MakeInstanceExtensionList(requested_extensions);

#ifdef FX_VULKAN_DEBUG
    all_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    all_extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
#endif

    std::cout << "Requested to load " << all_extensions.size() << " extensions...\n";

    for (const auto& extension : all_extensions) {
        FxLogDebug("Ext: {:s}", extension);
    }

    ExtensionNames missing_extensions = CheckExtensionsAvailable(all_extensions);

    if (!missing_extensions.empty()) {
        std::cerr << "MISSING: ";
        for (size_t i = 0; i < missing_extensions.size(); ++i) {
            std::cerr << missing_extensions[i];
            if (i < missing_extensions.size() - 1) {
                std::cerr << ", ";
            }
        }

        FxModulePanic("Missing required instance extensions");
    }

    // auto validation_layers = GetAvailableValidationLayers();
    // for (auto &layer : validation_layers) {
    //     Log::Debug("Layer: %s", layer.layerName);
    // }

    std::vector<const char*> requested_validation_layers = {
        "VK_LAYER_KHRONOS_validation",
        // "VK_LAYER_KHRONOS_shader_object",
    };

    VkInstanceCreateInfo instance_info = {};
    instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instance_info.pApplicationInfo = &app_info;
    instance_info.ppEnabledExtensionNames = all_extensions.data();
    instance_info.enabledExtensionCount = static_cast<uint32_t>(all_extensions.size());
    instance_info.ppEnabledLayerNames = requested_validation_layers.data();
    instance_info.enabledLayerCount = static_cast<uint32_t>(requested_validation_layers.size());
#ifdef FX_PLATFORM_MACOS
    instance_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
#endif

    VkResult result = vkCreateInstance(&instance_info, nullptr, &mInstance);

    if (result != VK_SUCCESS) {
        FxModulePanicVulkan("Could not create vulkan instance!", result);
    }

#ifdef FX_VULKAN_DEBUG
    mDebugMessenger = CreateDebugMessenger(mInstance);
    if (!mDebugMessenger) {
        FxModulePanic("Could not create debug messenger");
    }
#endif

    bInitialized = true;
}

uint32 DebugMessageCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity, uint32 type,
                            const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
    const char* message = callback_data->pMessage;
    const char* fmt = "VkValidator: {:s}";


    if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)) {
        FxLogError(fmt, message);
    }
    else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)) {
        FxLogWarning(fmt, message);
    }
    else if ((message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)) {
        // Log::Info(fmt, message);
    }
    else {
        FxLogDebug(fmt, message);
    }

    return 0;
}

// template<typename FuncProt>
// FuncProt GetExtensionFuncVk(VkInstance instance, const char *name)
// {
//     PFN_vkVoidFunction raw_ptr = vkGetInstanceProcAddr(instance, name);

//     if (raw_ptr != nullptr) {
//         return (FuncProt)raw_ptr;
//     }

//     Log::Error("Could not load extension function '%s'", name);
//     return nullptr;
// }


VkDebugUtilsMessengerEXT CreateDebugMessenger(VkInstance instance)
{
    const auto severity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;

    const auto message_type = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;


    VkDebugUtilsMessengerCreateInfoEXT create_info = {
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = severity,
        .messageType = message_type,
        .pfnUserCallback = DebugMessageCallback,
    };

    VkDebugUtilsMessengerEXT messenger;

    const auto status = Rx_EXT_CreateDebugUtilsMessenger(instance, &create_info, nullptr, &messenger);
    if (status != VK_SUCCESS) {
        FxLogError("Could not create debug messenger! (err: {:s})", RxUtil::ResultToStr(status));
        return nullptr;
    }

    return messenger;
}

void DestroyDebugMessenger(VkInstance instance, VkDebugUtilsMessengerEXT messenger)
{
    if (messenger == nullptr) {
        return;
    }

    Rx_EXT_DestroyDebugUtilsMessenger(instance, messenger, nullptr);
}

void RxRenderBackend::InitGPUAllocator()
{
    const RxGpuDevice* device = GetDevice();

    const VmaAllocatorCreateInfo create_info = { .physicalDevice = device->Physical,
                                                 .device = device->Device,
                                                 .instance = mInstance };

    const VkResult status = vmaCreateAllocator(&create_info, &GpuAllocator);
    if (status != VK_SUCCESS) {
        FxModulePanicVulkan("Could not create VMA allocator!", status);
    }
}

void RxRenderBackend::DestroyGPUAllocator() { vmaDestroyAllocator(GpuAllocator); }

ExtensionNames RxRenderBackend::MakeInstanceExtensionList(ExtensionNames& user_requested_extensions)
{
    uint32 required_extension_count = 0;
    const char* const* required_extensions = SDL_Vulkan_GetInstanceExtensions(&required_extension_count);

    QueryInstanceExtensions();

    const uint32 total_extensions_size = user_requested_extensions.size() + required_extension_count;
    ExtensionNames total_extensions;
    total_extensions.reserve(total_extensions_size);

    // append the user requested extensions
    total_extensions.insert(total_extensions.begin(), user_requested_extensions.begin(),
                            user_requested_extensions.end());

    for (int32 i = 0; i < required_extension_count; i++) {
        total_extensions.push_back(required_extensions[i]);
    }

    return total_extensions;
}

ExtensionList& RxRenderBackend::QueryInstanceExtensions(bool invalidate_previous)
{
    FxLogDebug("Querying for instance extensions...");


    if (mAvailableExtensions.IsNotEmpty()) {
        if (invalidate_previous) {
            mAvailableExtensions.Free();
        }
        else {
            return mAvailableExtensions;
        }
    }

    // Get the count of the current extensions
    uint32_t extension_count = 0;
    VkResult result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not query instance extensions!");
    }

    std::cout << "Ext count: " << extension_count << "\n";

    // resize to create the items in the vector
    mAvailableExtensions.InitSize(extension_count);

    // Get the available instance extensions
    result = vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, mAvailableExtensions.pData);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Could not query instance extensions!");
    }

    // std::cout << "=== Available Instance Extensions (" << extension_count << ") ===\n";

    // for (uint32_t i = 0; i < extension_count; ++i) {
    //     const VkExtensionProperties& extension = mAvailableExtensions[i];
    //     std::cout << extension.extensionName << " : " << extension.specVersion << "\n";
    // }

    return mAvailableExtensions;
}

void RxRenderBackend::SubmitUploadCmd(RxRenderBackend::SubmitFunc upload_func)
{
    RxCommandBuffer& cmd = UploadContext.CommandBuffer;

    cmd.Record(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    upload_func(cmd);
    cmd.End();

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,

        .commandBufferCount = 1,
        .pCommandBuffers = &cmd.CommandBuffer,
    };

    // Log::Debug(
    //     "cmd thread: %lu",
    //     std::this_thread::get_id()
    // );

    VkTry(vkQueueSubmit(GetDevice()->TransferQueue, 1, &submit_info, UploadContext.UploadFence.Fence),
          "Error submitting upload buffer");

    UploadContext.UploadFence.WaitFor();
    UploadContext.UploadFence.Reset();

    UploadContext.CommandPool.Reset();
}


void RxRenderBackend::SubmitOneTimeCmd(RxRenderBackend::SubmitFunc submit_func)
{
    RxCommandBuffer cmd;
    cmd.Create(&GetFrame()->CommandPool);

    cmd.Record(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
    submit_func(cmd);
    cmd.End();

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,

        .commandBufferCount = 1,
        .pCommandBuffers = &cmd.CommandBuffer,
    };

    VkTry(vkQueueSubmit(GetDevice()->GraphicsQueue, 1, &submit_info, nullptr), "Error submitting upload buffer");

    vkQueueWaitIdle(GetDevice()->GraphicsQueue);

    cmd.Reset();
    cmd.Destroy();
}


RxFrameResult RxRenderBackend::BeginFrame()
{
    RxFrameData* frame = GetFrame();

    Uniforms.Rewind();

    // memcpy(GetUbo().MvpMatrix.RawData, MVPMatrix.RawData, sizeof(Mat4f));

    frame->InFlight.WaitFor();
    frame->InFlight.Reset();


    RxFrameResult result = GetNextSwapchainImage(frame);
    if (result != RxFrameResult::Success) {
        return result;
    }

    return RxFrameResult::Success;
}

void RxRenderBackend::BeginGeometry()
{
    RxFrameData* frame = GetFrame();


    pDeferredRenderer->GPass.Begin(frame->CommandBuffer, *pDeferredRenderer->pGeometryPipeline);
}

void RxRenderBackend::PresentFrame()
{
    RxFrameData* frame = GetFrame();

    const VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT | VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    };

    VkSemaphore* submit_semaphore = &mSubmitSemaphores[mImageIndex].Semaphore;

    const VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,

        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &frame->ImageAvailable.Semaphore,
        .pWaitDstStageMask = wait_stages,

        .commandBufferCount = 1,
        .pCommandBuffers = &frame->CommandBuffer.CommandBuffer,

        .signalSemaphoreCount = 1,
        .pSignalSemaphores = submit_semaphore,
    };

    VkTry(vkQueueSubmit(GetDevice()->GraphicsQueue, 1, &submit_info, frame->InFlight.Fence),
          "Error submitting draw buffer");

    if (Swapchain.bInitialized != true) {
        FxModulePanic("Swapchain not initialized!");
    }

    const VkSwapchainKHR swapchains[] = {
        Swapchain.GetSwapchain(),
    };

    const VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = submit_semaphore,

        .swapchainCount = 1,
        .pSwapchains = swapchains,

        .pImageIndices = &mImageIndex,

        .pResults = nullptr,
    };

    const VkResult status = vkQueuePresentKHR(GetDevice()->PresentQueue, &present_info);

    if (status == VK_SUCCESS) {
    }
    else if (status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_SUBOPTIMAL_KHR) {
        // Swapchain.Rebuild()..
    }
    else {
        FxLogError("Error submitting present queue. Status: {:x}", static_cast<int32>(status));
    }
}

void RxRenderBackend::BeginLighting()
{
    RxFrameData* frame = GetFrame();

    pDeferredRenderer->GPass.End();

    RxTarget* depth_target = pDeferredRenderer->GPass.GetTarget(RxImageFormat::eD32_Float, 0);
    FxAssert(depth_target != nullptr);
    depth_target->Image.TransitionDepthToShaderRO(frame->CommandBuffer);


    pDeferredRenderer->LightPass.Begin(frame->CommandBuffer, pDeferredRenderer->PlLightingDirectional);

    // gState->BufferOffset(RxShaderType::eVertex, gRenderer->Uniforms.GetBaseOffset());
    // gState->Pipeline(&pDeferredRenderer->PlLightingDirectional);
    // gState->Apply(frame->CommandBuffer);
    // gState->Reset();


    pDeferredRenderer->DsLighting.BindWithOffset(0, frame->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                                 pDeferredRenderer->PlLightingDirectional,
                                                 gRenderer->Uniforms.GetBaseOffset());
}


static_assert(RxFramesInFlight == 3);

void RxRenderBackend::BeginUnlit()
{
    RxFrameData* frame = GetFrame();

    pDeferredRenderer->LightPass.End();


    // RxTarget* depth_target = gRenderer->pDeferredRenderer->GPass.GetTarget(RxImageFormat::eD32_Float, 0);
    // FxAssert(depth_target != nullptr);
    // depth_target->Image.TransitionDepthToAttachment(gRenderer->GetFrame()->CommandBuffer);

    pDeferredRenderer->RpForward.Begin(&frame->CommandBuffer, pDeferredRenderer->FbForward.Get(),
                                       FxSlice<VkClearValue>({}, 0));

    pDeferredRenderer->PlUnlit.Bind(frame->CommandBuffer);
}

void RxRenderBackend::DoComposition(FxCamera& render_cam)
{
    RxFrameData* frame = GetFrame();

    pDeferredRenderer->RpForward.End();
    // pDeferredRenderer->LightPass.End();

    pDeferredRenderer->CompPass.Begin(frame->CommandBuffer, pDeferredRenderer->PlComposition);
    pDeferredRenderer->DoCompPass(render_cam);

    {
        /*FxStackArray<RxCommandBuffer, 1> commands;
        commands.Insert(frame->CommandBuffer);

        FxStackArray<RxSemaphore, 1> wait_semaphores;
        wait_semaphores.Insert(frame->ShadowsSem);

        FxStackArray<RxSemaphore, 1> signal_semaphores;
        signal_semaphores.Insert(frame->OffscreenSem);*/


        // pDeferredRenderer->GPass.Submit(FxSlice(commands), FxSlice(wait_semaphores), FxSlice(signal_semaphores));
    }


    PresentFrame();

    ProcessDeletionQueue();

    mInternalFrameCounter++;

    mFrameNumber = (mInternalFrameCounter % RxFramesInFlight);
}

RxFrameResult RxRenderBackend::GetNextSwapchainImage(RxFrameData* frame)
{
    const uint64 timeout = UINT64_MAX; // TODO: change this value and handle AcquireNextImage errors correctly

    const VkResult result = vkAcquireNextImageKHR(GetDevice()->Device, Swapchain.GetSwapchain(), timeout,
                                                  frame->ImageAvailable.Semaphore, nullptr, &mImageIndex);

    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        return RxFrameResult::Success;
    }
    else if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Swapchain.Rebuild()..
        return RxFrameResult::GraphicsOutOfDate;
    }
    else {
        FxLogError("Error getting next swapchain image! Status: {:x}", static_cast<int>(result));
    }

    return RxFrameResult::RenderError;
}

void RxRenderBackend::CreateSurfaceFromWindow()
{
    if (mpWindow == nullptr) {
        FxModulePanic("No window attached! use FxRenderBackend::SelectWindow()");
    }

    bool success = SDL_Vulkan_CreateSurface(mpWindow->GetWindow(), mInstance, nullptr, &mWindowSurface);

    if (!success) {
        FxModulePanic("Could not attach Vulkan instance to window! (SDL err: {})", SDL_GetError());
    }
}


void RxRenderBackend::Destroy()
{
    GetDevice()->WaitForIdle();

    DestroyUploadContext();
    DestroyFrames();

    for (RxSemaphore& sem : mSubmitSemaphores) {
        sem.Destroy();
    }

    Uniforms.Destroy();
    BoneBuffer.Destroy();


    while (!mDeletionQueue.empty()) {
        ProcessDeletionQueue(true);

        // insert a small delay to avoid the processor spinning out while
        // waiting for an object. this allows handing the core off to other threads.
        std::this_thread::sleep_for(std::chrono::nanoseconds(100));
    }


    CompDescriptorPool.Destroy();

    GetDevice()->WaitForIdle();

    Swapchain.Destroy();

    DestroyGPUAllocator();

    if (mWindowSurface) {
        vkDestroySurfaceKHR(mInstance, mWindowSurface, nullptr);
    }

    GetDevice()->Destroy();

    DestroyDebugMessenger(mInstance, mDebugMessenger);
    vkDestroyInstance(mInstance, nullptr);

    bInitialized = false;
}

RxFrameData* RxRenderBackend::GetFrame() { return &Frames[GetFrameNumber()]; }
