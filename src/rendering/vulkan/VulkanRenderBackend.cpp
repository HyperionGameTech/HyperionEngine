/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanMemory.hpp>
#include <rendering/vulkan/VulkanSwapchain.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanFence.hpp>
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#include <rendering/vulkan/VulkanComputePipeline.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanShader.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanAsyncCompute.hpp>
#include <rendering/vulkan/VulkanRayTracingPipeline.hpp>
#include <rendering/vulkan/VulkanAccelerationStructure.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/FinalPass.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/config/Config.hpp>

#include <rendering/Texture.hpp>

#include <system/AppContext.hpp>

#define CHECK_FRAME_RESULT(result)                  \
    do                                              \
    {                                               \
        auto _result = (result);                    \
                                                    \
        if (!(_result))                             \
        {                                           \
            m_crashHandler.HandleGPUCrash(_result); \
                                                    \
            HYP_UNREACHABLE();                      \
        }                                           \
    }                                               \
    while (0)

namespace Hyperion {

static constexpr SizeType VulkanArenaSize = 4 * 1024 * 1024; // 4 MB for general transient allocations
TArena<RenderAllocator>* g_vulkanArena;

static constexpr bool UseResetDescriptorPool = false;

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

#pragma region VulkanRenderConfig

class VulkanRenderConfig final : public IRenderConfig
{
public:
    void Initialize(VulkanRenderBackend* renderBackend)
    {
        Assert(renderBackend != nullptr && renderBackend->GetDevice() != nullptr);

        bindlessTextures = renderBackend->GetDevice()->GetFeatures().SupportsBindlessTextures();
        rayTracing = renderBackend->GetDevice()->GetFeatures().IsRayTracingSupported();
        indirectRendering = CoreApi::GetGlobalConfig().Get("Rendering.IndirectRendering").ToBool(/* defaultValue */ true);
        parallelRendering = CoreApi::GetGlobalConfig().Get("Rendering.ParallelCollection").ToBool(/* defaultValue */ true);
        dynamicDescriptorIndexing = false; // renderBackend->GetDevice()->GetFeatures().SupportsDynamicDescriptorIndexing();
    }
};

#pragma endregion VulkanRenderConfig

#pragma region Vulkan struct wrappers

static VkDescriptorSetLayout CreateVkDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayout& layout)
{
    static constexpr VkDescriptorBindingFlags BindlessFlags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

    Array<VkDescriptorSetLayoutBinding> bindings;
    bindings.Reserve(layout.GetElements().Size());

    Array<VkDescriptorBindingFlags> bindingFlags;
    bindingFlags.Reserve(layout.GetElements().Size());

    for (const auto& it : layout.GetElements())
    {
        const Name name = it.first;
        const DescriptorSetLayoutElement& element = it.second;

        uint32 descriptorCount = element.count;

        if (element.IsBindless())
        {
            descriptorCount = MaxBindlessResources;
        }

        // if (descriptorCount > 1 && !m_device->GetFeatures().SupportsDynamicDescriptorIndexing()) {
        //     return HYP_MAKE_ERROR(RendererError, "Device does not support descriptor indexing, cannot create descriptor set with element {} that uses an array of elements", 0, name);
        // }

        VkDescriptorSetLayoutBinding binding {};
        binding.descriptorCount = descriptorCount;
        binding.descriptorType = ToVkDescriptorType(element.type);
        binding.pImmutableSamplers = nullptr;
        binding.stageFlags = VK_SHADER_STAGE_ALL;
        binding.binding = element.binding;

        bindings.PushBack(binding);

        VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

        if (element.IsBindless())
        {
            flags |= BindlessFlags;
        }

        bindingFlags.PushBack(flags);
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
    extendedInfo.bindingCount = uint32(bindingFlags.Size());
    extendedInfo.pBindingFlags = bindingFlags.Data();

    VkDescriptorSetLayoutCreateInfo layoutInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
    layoutInfo.pBindings = bindings.Data();
    layoutInfo.bindingCount = uint32(bindings.Size());
    layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    layoutInfo.pNext = &extendedInfo;

    VkDescriptorSetLayout handle = VK_NULL_HANDLE;

    VkResult result = vkCreateDescriptorSetLayout(
        device->GetDevice(),
        &layoutInfo,
        nullptr,
        &handle);

    Assert(result == VK_SUCCESS && handle != VK_NULL_HANDLE);

    return handle;
}

static void DestroyVkDescriptorSetLayout(VulkanDevice* device, VkDescriptorSetLayout layout)
{
    if (layout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(
            device->GetDevice(),
            layout,
            nullptr);
    }
}

#pragma endregion Vulkan struct wrappers

#pragma region VulkanDynamicFunctions

HYP_API VulkanDynamicFunctions* g_vulkanDynamicFunctions = nullptr;

void VulkanDynamicFunctions::Load(VulkanDevice* device)
{
    static VulkanDynamicFunctions s_instance;
    g_vulkanDynamicFunctions = &s_instance;

#define HYP_LOAD_FN(function)                                                                                        \
    do                                                                                                               \
    {                                                                                                                \
        s_instance.function = reinterpret_cast<PFN_##function>(vkGetDeviceProcAddr(device->GetDevice(), #function)); \
        if (!s_instance.function)                                                                                    \
        {                                                                                                            \
            HYP_LOG(RenderingBackend, Warning, "Failed to load Vulkan function {}", #function);                      \
        }                                                                                                            \
    }                                                                                                                \
    while (0)

#if defined(HYP_FEATURES_ENABLE_RAY_TRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
    HYP_LOAD_FN(vkGetBufferDeviceAddressKHR); // currently only used for RT

    HYP_LOAD_FN(vkCmdBuildAccelerationStructuresKHR);
    HYP_LOAD_FN(vkBuildAccelerationStructuresKHR);
    HYP_LOAD_FN(vkCreateAccelerationStructureKHR);
    HYP_LOAD_FN(vkDestroyAccelerationStructureKHR);
    HYP_LOAD_FN(vkGetAccelerationStructureBuildSizesKHR);
    HYP_LOAD_FN(vkGetAccelerationStructureDeviceAddressKHR);
    HYP_LOAD_FN(vkCmdTraceRaysKHR);
    HYP_LOAD_FN(vkGetRayTracingShaderGroupHandlesKHR);
    HYP_LOAD_FN(vkCreateRayTracingPipelinesKHR);
#endif

#ifdef HYP_DEBUG_MODE
    // HYP_LOAD_FN(vkCmdDebugMarkerBeginEXT);
    // HYP_LOAD_FN(vkCmdDebugMarkerEndEXT);
    // HYP_LOAD_FN(vkCmdDebugMarkerInsertEXT);
    // HYP_LOAD_FN(vkDebugMarkerSetNameEXT);
    HYP_LOAD_FN(vkSetDebugUtilsObjectNameEXT);
#endif

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
    HYP_LOAD_FN(vkGetMoltenVKConfigurationMVK);
    HYP_LOAD_FN(vkSetMoltenVKConfigurationMVK);
#endif

#ifdef HYP_WINDOWS
    HYP_LOAD_FN(vkGetMemoryWin32HandleKHR);
#else
    HYP_LOAD_FN(vkGetMemoryFdKHR);
#endif

#undef HYP_LOAD_FN
}

#pragma endregion VulkanDynamicFunctions

#pragma region VulkanDescriptorSetManager

class VulkanDescriptorSetManager final : public IDescriptorSetManager
{
public:
    static constexpr uint32 maxDescriptorSets = 4096;

    VulkanDescriptorSetManager();
    virtual ~VulkanDescriptorSetManager() override;

    void OnFrameStart();

    RendererResult Create(VulkanDevice* device);
    RendererResult Destroy(VulkanDevice* device);

    RendererResult CreateDescriptorSet(VulkanDevice* device,
        VkDescriptorSetLayout layout,
        VkDescriptorSet& outVkDescriptorSet,
        VkDescriptorPool& outVkDescriptorPool);

    RendererResult DestroyDescriptorSet(VulkanDevice* device,
        VkDescriptorSet vkDescriptorSet,
        VkDescriptorPool vkDescriptorPool);

    VkDescriptorSetLayout GetOrCreateVkDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayout& layout);

private:
    VkDescriptorPool GetDescriptorPool(uint32 currentFrameCounter, int& outPoolIndex);
    RendererResult CreateDescriptorPool(VkDescriptorPool& outDescriptorPool);

    SharedMutex m_mutex;
    HashMap<HashCode, VkDescriptorSetLayout> m_vkDescriptorSetLayouts;

    struct VulkanDescriptorPool
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        uint32 useCount = 0;
        uint32 frameCounter = 0; // last used or created
    };

    Array<VulkanDescriptorPool> m_pools;
};

VulkanDescriptorSetManager::VulkanDescriptorSetManager()
{
}

VulkanDescriptorSetManager::~VulkanDescriptorSetManager() = default;

void VulkanDescriptorSetManager::OnFrameStart()
{
    const uint32 frameCounter = RenderApi::GetFrameCounter();

    if (!UseResetDescriptorPool)
        return;

    // Reset descriptor pools for this frame
    for (SizeType i = 0; i < m_pools.Size(); i++)
    {
        VulkanDescriptorPool& dp = m_pools[i];

        if (dp.frameCounter % RingBufferDepth == 0)
        {
            VkResult result = vkResetDescriptorPool(g_renderBackend->GetDevice()->GetDevice(), dp.pool, 0);
            Assert(result == VK_SUCCESS, "Failed to reset descriptor pool! {}", result);
        }
    }
}

RendererResult VulkanDescriptorSetManager::Create(VulkanDevice* device)
{
    return {};
}

RendererResult VulkanDescriptorSetManager::Destroy(VulkanDevice* device)
{
    RendererResult result = RendererResult {};

    m_vkDescriptorSetLayouts.Clear();

    for (SizeType i = 0; i < m_pools.Size(); i++)
    {
        VkDescriptorPool& descriptorPool = m_pools[i].pool;
        AssertDebug(descriptorPool != VK_NULL_HANDLE);

        const uint32 usageCount = m_pools[i].useCount;

        if (usageCount > 0)
        {
            HYP_LOG(RenderingBackend, Warning, "Descriptor pool {} ({}) is still in use by {} descriptor sets", (void*)descriptorPool, i, usageCount);
        }

        vkDestroyDescriptorPool(device->GetDevice(), descriptorPool, nullptr);
    }

    m_pools.Clear();

    return result;
}

VkDescriptorPool VulkanDescriptorSetManager::GetDescriptorPool(uint32 currentFrameCounter, int& outPoolIndex)
{
    outPoolIndex = -1;

    // return last descriptor pool, it's most likely for allocations
    // to succeed with it since it would have more free memory
    for (SizeType idx = m_pools.Size(); idx != 0; --idx)
    {
        VulkanDescriptorPool& dp = m_pools[idx - 1];

        const uint32 delta = currentFrameCounter - dp.frameCounter;

        // we can use it if this many frames have passed OR it is from the same frame
        if (dp.frameCounter == currentFrameCounter || delta >= NumFramesInFlight)
        {
            VkDescriptorPool pool = dp.pool;
            dp.frameCounter = currentFrameCounter;
            
            outPoolIndex = idx - 1;

            return pool;
        }
    }

    // no pool (for this frame); create a new one
    
    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (RendererResult createDescriptorPoolResult = CreateDescriptorPool(pool); createDescriptorPoolResult.HasError())
    {
        HYP_FAIL("Failed to create descriptor pool! {}", createDescriptorPoolResult.GetError().GetMessage());
    }

    outPoolIndex = int(m_pools.Size() - 1);

    return pool;
}

RendererResult VulkanDescriptorSetManager::CreateDescriptorPool(VkDescriptorPool& outDescriptorPool)
{
    Array<VkDescriptorPoolSize> descriptorPoolSizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 16 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 }
    };

    // only add acceleration structure descriptor type if rayTracing is supported,
    // otherwise we'll get an error when creating the descriptor pool
    if (g_renderBackend->GetDevice()->GetFeatures().IsRayTracingSupported())
    {
        descriptorPoolSizes.PushBack({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 });
    }

    outDescriptorPool = VK_NULL_HANDLE;

    VulkanDescriptorPool& dp = m_pools.EmplaceBack();
    dp.frameCounter = RenderApi::GetFrameCounter();

    VkDescriptorPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
        | (!UseResetDescriptorPool ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0);
    poolInfo.maxSets = maxDescriptorSets;
    poolInfo.poolSizeCount = uint32(descriptorPoolSizes.Size());
    poolInfo.pPoolSizes = descriptorPoolSizes.Data();

    VULKAN_CHECK(vkCreateDescriptorPool(
        g_renderBackend->GetDevice()->GetDevice(),
        &poolInfo,
        nullptr,
        &dp.pool));

    HYP_LOG(RenderingBackend, Debug, "Created new Vulkan descriptor pool {} ({})", (void*)dp.pool, m_pools.Size());

    outDescriptorPool = dp.pool;

    return {};
}

RendererResult VulkanDescriptorSetManager::CreateDescriptorSet(
    VulkanDevice* device,
    VkDescriptorSetLayout layout,
    VkDescriptorSet& outVkDescriptorSet,
    VkDescriptorPool& outVkDescriptorPool)
{
    Assert(layout != VK_NULL_HANDLE);

    VkDescriptorSetLayout layouts[] = { layout };

    int poolIndex = -1;

    outVkDescriptorPool = GetDescriptorPool(RenderApi::GetFrameCounter(), poolIndex);

    bool shouldRetry = false;

    do
    {
        shouldRetry = false;

        Assert(outVkDescriptorPool != VK_NULL_HANDLE);

        VkDescriptorSetAllocateInfo allocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = outVkDescriptorPool;
        allocInfo.descriptorSetCount = ArraySize(layouts);
        allocInfo.pSetLayouts = &layouts[0];

        const VkResult vkResult = vkAllocateDescriptorSets(
            device->GetDevice(),
            &allocInfo,
            &outVkDescriptorSet);

        if (vkResult != VK_SUCCESS)
        {
            if (vkResult == VK_ERROR_OUT_OF_POOL_MEMORY)
            {
                // descend down the list of existing descriptor pools. we start trying to allocate from the last descriptor pool (see GetDescriptorPool())
                --poolIndex;

                // create a new descriptor pool if we're out of existing pools to try with
                if (poolIndex >= 0)
                {
                    outVkDescriptorPool = m_pools[poolIndex].pool;
                }
                else
                {
                    if (RendererResult createDescriptorPoolResult = CreateDescriptorPool(outVkDescriptorPool); createDescriptorPoolResult.HasError())
                    {
                        // failed to allocate new descriptor pool
                        return createDescriptorPoolResult;
                    }
                }

                shouldRetry = true;

                // try again with new descriptor pool
                continue;
            }

            return HYP_MAKE_ERROR(RendererError, "Failed to allocate descriptor set!", int(vkResult));
        }
    }
    while (shouldRetry);

    poolIndex = int(m_pools.IndexOf(m_pools.FindIf([&outVkDescriptorPool](const auto& elem)
    {
        return elem.pool == outVkDescriptorPool;
    })));

    Assert(poolIndex >= 0 && poolIndex < int(m_pools.Size()));

    ++m_pools[poolIndex].useCount;

    return {};
}

RendererResult VulkanDescriptorSetManager::DestroyDescriptorSet(VulkanDevice* device, VkDescriptorSet vkDescriptorSet, VkDescriptorPool vkDescriptorPool)
{
    Assert(vkDescriptorSet != VK_NULL_HANDLE);
    Assert(vkDescriptorPool != VK_NULL_HANDLE);

    int poolIndex = int(m_pools.IndexOf(m_pools.FindIf([&vkDescriptorPool](const auto& elem)
    {
        return elem.pool == vkDescriptorPool;
    })));

    Assert(poolIndex >= 0 && poolIndex < int(m_pools.Size()));

    if (!UseResetDescriptorPool)
    {
        vkFreeDescriptorSets(
            device->GetDevice(),
            vkDescriptorPool,
            1,
            &vkDescriptorSet);
    }

    Assert(m_pools[poolIndex].useCount > 0, "miscount of descriptor pool usage counts; should never be less than 0");

    --m_pools[poolIndex].useCount;

    return {};
}

VkDescriptorSetLayout VulkanDescriptorSetManager::GetOrCreateVkDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayout& layout)
{
    const HashCode hashCode = layout.GetHashCode();

    VkDescriptorSetLayout handle = VK_NULL_HANDLE;

    TSharedLock lock(m_mutex);

    auto it = m_vkDescriptorSetLayouts.Find(hashCode);

    if (it != m_vkDescriptorSetLayouts.End())
    {
        handle = it->second;
    }

    if (handle != VK_NULL_HANDLE)
    {
        return handle;
    }

    lock.Reset();

    handle = CreateVkDescriptorSetLayout(device, layout);
    AssertDebug(handle != VK_NULL_HANDLE);

    TUniqueLock lock2(m_mutex);
    
    // make sure it hasnt changed
    auto insertResult = m_vkDescriptorSetLayouts.Set(hashCode, handle);

    if (!insertResult.second)
    {
        DestroyVkDescriptorSetLayout(device, handle);

        handle = m_vkDescriptorSetLayouts.At(hashCode);
        Assert(handle != nullptr);
    }

    return handle;
}

#pragma endregion VulkanDescriptorSetManager

#pragma region VulkanRenderBackend

VulkanRenderBackend::VulkanRenderBackend()
    : m_instance(nullptr),
      m_renderConfig(MakePimpl<VulkanRenderConfig>()),
      m_descriptorSetManager(MakePimpl<VulkanDescriptorSetManager>()),
      m_asyncCompute(new VulkanAsyncCompute()),
      m_currentFrameIndex(0)
{
}

VulkanRenderBackend::~VulkanRenderBackend()
{
}

const VulkanDeviceRef& VulkanRenderBackend::GetDevice() const
{
    return m_instance->GetDevice();
}

const IRenderConfig& VulkanRenderBackend::GetRenderConfig() const
{
    return *m_renderConfig;
}

AsyncComputeBase* VulkanRenderBackend::GetAsyncCompute() const
{
    return m_asyncCompute;
}

RendererResult VulkanRenderBackend::Initialize()
{
#ifdef HYP_DEBUG_MODE
    static const ConfigurationValue& s_cfgDebugLayers = CoreApi::GetGlobalConfig().Get("Rendering.Vulkan.DebugLayers");

    if (s_cfgDebugLayers.ToBool(false))
    {
        HYP_LOG(RenderingBackend, Debug, "Vulkan debug layers enabled");
    }

    const bool enableDebugLayers = s_cfgDebugLayers.ToBool(false);
#else
    const bool enableDebugLayers = false;
#endif

    g_vulkanArena = PoolNew<TArena<RenderAllocator>>(*g_renderPool, VulkanArenaSize);

    m_instance = PoolNew<VulkanInstance>(*g_renderPool);
    Assert(m_instance->Initialize(enableDebugLayers));

    VulkanDeviceQueue* deviceQueue = GetDevice()->GetPresentQueue();

    if (!deviceQueue)
    {
        // running in headless mode
        deviceQueue = GetDevice()->GetGraphicsQueue();
    }

    AssertDebug(deviceQueue != nullptr);

    // Create frames
    for (uint32 frameIndex = 0; frameIndex < uint32(m_frames.Size()); frameIndex++)
    {
        VulkanFrameRef& frame = m_frames[frameIndex];
        VulkanCommandBufferRef& commandBuffer = m_commandBuffers[frameIndex];

        VkCommandPool pool = deviceQueue->commandPools[0];
        Assert(pool != VK_NULL_HANDLE);

        commandBuffer = CreateObject<VulkanCommandBuffer>(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        frame = CreateObject<VulkanFrame>(frameIndex);

        CheckResultOrReturn(commandBuffer->Create(pool));
        CheckResultOrReturn(frame->Create());
    }

    VulkanDynamicFunctions::Load(m_instance->GetDevice());

    m_renderConfig->Initialize(this);

    m_crashHandler.Initialize();

    Assert(m_descriptorSetManager->Create(m_instance->GetDevice()));
    Assert(m_asyncCompute->Create());

    m_defaultFormats.Set(
        DIF_COLOR,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_RGBA8, TF_RGBA16F } },
            IS_SRV));

    m_defaultFormats.Set(
        DIF_NORMALS,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_RGBA16F, TF_RGBA32F, TF_RGBA8 } },
            IS_SRV));

    m_defaultFormats.Set(
        DIF_STORAGE,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_RGBA16F } },
            IS_UAV));

    return {};
}

RendererResult VulkanRenderBackend::Destroy()
{
    SafeDelete(std::move(m_frames));
    SafeDelete(std::move(m_commandBuffers));

    m_descriptorSetManager->Destroy(m_instance->GetDevice());

    delete m_asyncCompute;
    m_asyncCompute = nullptr;

    CheckResultOrReturn(m_instance->GetDevice()->WaitIdle());

    PoolDelete(*g_renderPool, m_instance);
    m_instance = nullptr;

    PoolDelete(*g_renderPool, g_vulkanArena);
    g_vulkanArena = nullptr;

    return {};
}

VulkanFrame* VulkanRenderBackend::GetCurrentFrame() const
{
    return m_frames[m_currentFrameIndex];
}

VulkanFrame* VulkanRenderBackend::PrepareNextFrame()
{
    VulkanFrame* frame = GetCurrentFrame();

    RendererResult res;

    res = frame->GetFence()->Wait(true);
    if (!res)
    {
        HYP_FAIL("Failed to wait on frame fence! VkResult: {}", frame->GetFence()->GetLastFrameResult());
    }

    frame->OnFrameStart();

    m_descriptorSetManager->OnFrameStart();

    // if (m_shouldRecreateSwapchain)
    //{
    //     CHECK_FRAME_RESULT(m_instance->GetDevice()->WaitIdle());

    //    VulkanSwapchainRef newSwapchain = m_instance->GetSwapchain(->Recreate());
    //    SafeDelete()
    //    CHECK_FRAME_RESULT(m_instance->GetSwapchain()());

    //    CHECK_FRAME_RESULT(m_instance->GetSwapchain()->GetCurrentFrame()->RecreateFence());

    //    // Need to prepare frame again now that swapchain has been recreated.
    //    CHECK_FRAME_RESULT(m_instance->GetSwapchain()->PrepareFrame(m_shouldRecreateSwapchain));

    //    frame = m_instance->GetSwapchain()->GetCurrentFrame();

    //    // Recreate FinalPass
    //    PoolDelete(*g_renderPool, g_renderInterface->finalPass);

    //    g_renderInterface->finalPass = PoolNew<FinalPass>(*g_renderPool, m_instance->GetSwapchain());
    //    g_renderInterface->finalPass->Create();

    //    OnSwapchainRecreated(m_instance->GetSwapchain());
    //}

    AssertDebug(frame != nullptr);

    if (m_asyncCompute->IsSupported())
    {
        CHECK_FRAME_RESULT(m_asyncCompute->PrepareForFrame(frame));
    }

    return frame;
}

VulkanSwapchainRef VulkanRenderBackend::CreateSwapchain(ApplicationWindow* window)
{
    AssertOnThread(g_renderThread);

    VkSurfaceKHR surface = window->GetVkSurface();
    Assert(surface != VK_NULL_HANDLE);

    VulkanSwapchainRef swapchain = CreateObject<VulkanSwapchain>(surface, Vec2u(window->GetSize()));
    RendererResult result = swapchain->Create();

    if (!result)
    {
        HYP_FAIL("Failed to create Vulkan swapchain: {}", result.GetError().GetMessage());
    }

    return swapchain;
}

void VulkanRenderBackend::PrepareSwapchain(VulkanSwapchain* swapchain)
{
    swapchain->PrepareForFrame(GetCurrentFrame());
}

void VulkanRenderBackend::SubmitCommandBuffers(VulkanSwapchain* swapchain)
{
    VulkanDeviceQueue* presentQueue = m_instance->GetDevice()->GetPresentQueue();

    if (!presentQueue)
    {
        // running in headless mode
        presentQueue = m_instance->GetDevice()->GetGraphicsQueue();
    }

    VulkanDevice* vulkanDevice = m_instance->GetDevice();
    VulkanFrame* vulkanFrame = GetCurrentFrame();
    VulkanCommandBuffer* vulkanCommandBuffer = GetCurrentCommandBuffer();

    CHECK_FRAME_RESULT(vulkanFrame->Submit(presentQueue, vulkanCommandBuffer, swapchain));

    if (m_asyncCompute->IsSupported())
    {
        CHECK_FRAME_RESULT(m_asyncCompute->Submit(vulkanFrame));
    }
#ifdef HYP_DEBUG_MODE
    else if (!m_asyncCompute->renderQueue.IsEmpty())
    {
        HYP_LOG(RenderingBackend, Fatal, "Cannot write to async compute render queue, this device does not support async compute!");
    }
#endif
}

void VulkanRenderBackend::PresentToSwapchain(VulkanSwapchain* swapchain)
{
    VulkanDeviceQueue* presentQueue = m_instance->GetDevice()->GetPresentQueue();
    AssertDebug(presentQueue != nullptr); // should never be null when presenting, not used in headless mode

    VulkanDevice* vulkanDevice = m_instance->GetDevice();
    VulkanCommandBuffer* vulkanCommandBuffer = GetCurrentCommandBuffer();
    VulkanFrame* vulkanFrame = GetCurrentFrame();

    swapchain->PresentFrame(vulkanFrame, presentQueue);
}

VulkanCommandBuffer* VulkanRenderBackend::GetCurrentCommandBuffer() const
{
    return m_commandBuffers[m_currentFrameIndex];
}

VulkanDescriptorSetRef VulkanRenderBackend::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    DescriptorSetLayout newLayout { layout.GetDeclaration() };
    newLayout.SetIsTemplate(false);
    newLayout.SetIsReference(false);

    VulkanDescriptorSetRef descriptorSet = CreateObject<VulkanDescriptorSet>(newLayout);
    descriptorSet->SetDebugName(layout.GetName());

    return descriptorSet;
}

VulkanDescriptorTableRef VulkanRenderBackend::MakeDescriptorTable(const DescriptorTableDeclaration* decl)
{
    return CreateObject<VulkanDescriptorTable>(decl);
}

VulkanGraphicsPipelineRef VulkanRenderBackend::MakeGraphicsPipeline(
    const VulkanShaderRef& shader,
    const RenderTargetDesc& renderTargetDesc,
    const RenderableAttributeSet& attributes)
{
    VulkanGraphicsPipelineRef graphicsPipeline = CreateObject<VulkanGraphicsPipeline>();

    if (shader.IsValid())
    {
        graphicsPipeline->SetShader(shader);

#ifdef HYP_DEBUG_MODE
        graphicsPipeline->SetDebugName(NAME_FMT("GraphicsPipeline_{}", shader->GetDebugName().IsValid() ? *shader->GetDebugName() : "<unnamed shader>"));
#endif
    }

    graphicsPipeline->SetRenderTargetDesc(renderTargetDesc);

    graphicsPipeline->SetVertexAttributes(attributes.GetMeshAttributes().vertexAttributes);
    graphicsPipeline->SetTopology(attributes.GetMeshAttributes().topology);
    graphicsPipeline->SetCullMode(attributes.GetMaterialAttributes().cullFaces);
    graphicsPipeline->SetFillMode(attributes.GetMaterialAttributes().fillMode);
    graphicsPipeline->SetBlendFunction(attributes.GetMaterialAttributes().blendFunction);
    graphicsPipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
    graphicsPipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));

    if (attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST)  
    {
        graphicsPipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencilFunction);
    }

    // for materials that write a stencil reference value
    if (attributes.GetMaterialAttributes().stencilReference != 0)
    {
        graphicsPipeline->SetStencilWrite(true);
    }

    return graphicsPipeline;
}

VulkanComputePipelineRef VulkanRenderBackend::MakeComputePipeline(
    const VulkanShaderRef& shader,
    const VulkanDescriptorTableRef& descriptorTable)
{
    return CreateObject<VulkanComputePipeline>(VulkanShaderRef(shader), VulkanDescriptorTableRef(descriptorTable));
}

VulkanRayTracingPipelineRef VulkanRenderBackend::MakeRayTracingPipeline(
    const VulkanShaderRef& shader,
    const VulkanDescriptorTableRef& descriptorTable)
{
    return CreateObject<VulkanRayTracingPipeline>(VulkanShaderRef(shader), VulkanDescriptorTableRef(descriptorTable));
}

VulkanGpuBufferRef VulkanRenderBackend::MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment)
{
    return CreateObject<VulkanGpuBuffer>(bufferType, size, alignment);
}

VulkanGpuImageRef VulkanRenderBackend::MakeImage(const TextureDesc& textureDesc)
{
    return CreateObject<VulkanGpuImage>(textureDesc);
}

VulkanGpuImageViewRef VulkanRenderBackend::MakeImageView(const VulkanGpuImageRef& image)
{
    return CreateObject<VulkanGpuImageView>(VulkanGpuImageRef(image));
}

VulkanGpuImageViewRef VulkanRenderBackend::MakeImageView(const VulkanGpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers)
{
    return CreateObject<VulkanGpuImageView>(VulkanGpuImageRef(image), mipIndex, numMips, layerIndex, numLayers);
}

VulkanSamplerRef VulkanRenderBackend::MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode)
{
    return CreateObject<VulkanSampler>(filterModeMin, filterModeMag, wrapMode);
}

VulkanFramebufferRef VulkanRenderBackend::MakeFramebuffer(const RenderTargetDesc& renderTargetDesc)
{
    return CreateObject<VulkanFramebuffer>(renderTargetDesc, VulkanRenderPassMode::RenderTarget);
}

VulkanFrameRef VulkanRenderBackend::MakeFrame(uint32 frameIndex)
{
    return CreateObject<VulkanFrame>(frameIndex);
}

VulkanShaderRef VulkanRenderBackend::MakeShader(const CompiledShader* compiledShader)
{
    return CreateObject<VulkanShader>(compiledShader);
}

VulkanGpuBlasRef VulkanRenderBackend::MakeGpuBlas(
    const VulkanGpuBufferRef& packedVerticesBuffer,
    const VulkanGpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Mat4f& transform)
{
    return CreateObject<VulkanGpuBlas>(
        VulkanGpuBufferRef(packedVerticesBuffer),
        VulkanGpuBufferRef(packedIndicesBuffer),
        numVertices,
        numIndices,
        material,
        transform);
}

VulkanGpuTlasRef VulkanRenderBackend::MakeTLAS()
{
    return CreateObject<VulkanGpuTlas>();
}

void VulkanRenderBackend::PopulateIndirectDrawCommandsBuffer(
    const VulkanGpuBufferRef& vertexBuffer,
    const VulkanGpuBufferRef& indexBuffer,
    uint32 instanceOffset,
    TByteBuffer<RenderAllocator>& outByteBuffer)
{
    const SizeType requiredSize = (SizeType(instanceOffset) + 1) * sizeof(VkDrawIndexedIndirectCommand);

    if (outByteBuffer.Size() < requiredSize)
    {
        outByteBuffer.SetSize(requiredSize);
    }

    uint32 numIndices = 0;

    if (indexBuffer.IsValid())
    {
        numIndices = indexBuffer->Size() / sizeof(uint32);
    }

    VkDrawIndexedIndirectCommand* commandPtr = reinterpret_cast<VkDrawIndexedIndirectCommand*>(outByteBuffer.Data()) + instanceOffset;
    *commandPtr = VkDrawIndexedIndirectCommand {};
    commandPtr->indexCount = numIndices;
}

TextureFormat VulkanRenderBackend::GetDefaultFormat(DefaultImageFormat type) const
{
    auto it = m_defaultFormats.Find(type);
    if (it != m_defaultFormats.End())
    {
        return it->second;
    }

    return TF_NONE;
}

bool VulkanRenderBackend::IsSupportedFormat(TextureFormat format, ImageSupport supportType) const
{
    return m_instance->GetDevice()->GetFeatures().IsSupportedFormat(format, supportType);
}

TextureFormat VulkanRenderBackend::FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const
{
    return m_instance->GetDevice()->GetFeatures().FindSupportedFormat(possibleFormats, supportType);
}

QueryImageCapabilitiesResult VulkanRenderBackend::QueryImageCapabilities(const TextureDesc& textureDesc) const
{
    QueryImageCapabilitiesResult result;

    const TextureFormat format = textureDesc.format;
    const TextureType type = textureDesc.type;

    const bool isAttachmentTexture = textureDesc.imageUsage[IU_ATTACHMENT];
    const bool isRwTexture = textureDesc.imageUsage[IU_STORAGE];

    const bool isDepthStencil = textureDesc.IsDepthStencil();
    const bool isSrgb = textureDesc.IsSrgb();
    const bool isBlended = textureDesc.imageUsage[IU_BLENDED];

    const bool hasMipmaps = textureDesc.HasMipMaps();
    const uint32 numMipmaps = textureDesc.NumMips();
    const uint32 numLayers = textureDesc.NumArrayLayers();

    VkFormat vkFormat = ToVkFormat(format);
    VkImageType vkImageType = ToVkImageType(type);
    VkImageCreateFlags vkImageCreateFlags = 0;
    VkFormatFeatureFlags vkFormatFeatures = 0;
    VkImageFormatProperties vkImageFormatProperties {};

    VkImageTiling vkTiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags vkUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT;

    if (isAttachmentTexture)
    {
        vkUsageFlags |= (isDepthStencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /* for mip chain */
    }

    if (isRwTexture)
    {
        vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
            | VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else
    {
        vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (hasMipmaps)
    {
        /* Mipmapped image needs linear blitting. */
        vkFormatFeatures |= VK_FORMAT_FEATURE_BLIT_DST_BIT
            | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        vkUsageFlags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        switch (textureDesc.filterModeMin)
        {
        case TFM_LINEAR: // fallthrough
        case TFM_LINEAR_MIPMAP:
            vkFormatFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            break;
        case TFM_MINMAX_MIPMAP:
            vkFormatFeatures |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
            break;
        default:
            break;
        }
    }

    if (isBlended)
    {
        vkFormatFeatures |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (textureDesc.IsTextureCube() || textureDesc.IsTextureCubeArray())
    {
        vkImageCreateFlags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    /// \todo Implement me.

    HYP_NOT_IMPLEMENTED();
}

RendererResult VulkanRenderBackend::CreateDescriptorSet(VkDescriptorSetLayout vkDescriptorSetLayout, VkDescriptorSet& outVkDescriptorSet, VkDescriptorPool& outVkDescriptorPool)
{
    return m_descriptorSetManager->CreateDescriptorSet(m_instance->GetDevice(), vkDescriptorSetLayout, outVkDescriptorSet, outVkDescriptorPool);
}

RendererResult VulkanRenderBackend::DestroyDescriptorSet(VkDescriptorSet vkDescriptorSet, VkDescriptorPool vkDescriptorPool)
{
    return m_descriptorSetManager->DestroyDescriptorSet(m_instance->GetDevice(), vkDescriptorSet, vkDescriptorPool);
}

RendererResult VulkanRenderBackend::GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, VkDescriptorSetLayout& out)
{
    out = m_descriptorSetManager->GetOrCreateVkDescriptorSetLayout(m_instance->GetDevice(), layout);

    if (out != VK_NULL_HANDLE)
    {
        return RendererResult {};
    }

    return HYP_MAKE_ERROR(RendererError, "Failed to get or create Vulkan descriptor set layout");
}

UniquePtr<SingleTimeCommands> VulkanRenderBackend::GetSingleTimeCommands()
{
    return MakeUnique<VulkanSingleTimeCommands>();
}

void VulkanRenderBackend::ReleaseTransientMemory()
{
    // must happen before arena is reset or it's corruption city!
    GetCurrentFrame()->ResetTransientStates();

    g_vulkanArena->Reset();
}

VkSurfaceKHR VulkanRenderBackend::CreateSurface(ApplicationWindow* window, IDummyVulkanSurfaceContext** ppOutDummySurfaceContext)
{
    // may be created on main thread, which may not be the render thread if -RenderOnMainThread is not set.
    AssertOnThread(g_mainThread | g_renderThread);

#if HYP_WINDOWS
    Win32ApplicationWindow* win32Window = nullptr;
    if (window != nullptr)
    {
        win32Window = ObjCast<Win32ApplicationWindow>(window);
        Assert(win32Window != nullptr);
    }

    return Win32AppContext::CreateVulkanSurface(win32Window, ppOutDummySurfaceContext);
#elif HYP_MACOS
    CocoaApplicationWindow* cocoaWindow = nullptr;
    if (window != nullptr)
    {
        cocoaWindow = ObjCast<CocoaApplicationWindow>(window);
        Assert(cocoaWindow != nullptr);
    }

    return CocoaAppContext::CreateVulkanSurface(cocoaWindow, ppOutDummySurfaceContext);
#elif HYP_SDL
    SDLApplicationWindow* sdlWindow = nullptr;
    if (window != nullptr)
    {
        sdlWindow = ObjCast<SDLApplicationWindow>(window);
        Assert(sdlWindow != nullptr);
    }

    return SDLAppContext::CreateVulkanSurface(sdlWindow, ppOutDummySurfaceContext);
#else
    HYP_NOT_IMPLEMENTED();
    return VK_NULL_HANDLE;
#endif
}

RendererResult VulkanRenderBackend::GetVkExtensions(Array<const char*>& outExtensions)
{
#if HYP_MACOS
    if (const CocoaAppContext* cocoaAppContext = ObjCast<CocoaAppContext>(g_appContext))
    {
        static constexpr const char* RequiredExtensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_EXT_METAL_SURFACE_EXTENSION_NAME,
            VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
        };

        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

        Array<VkExtensionProperties> vkProperties(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, vkProperties.Data());

        for (const char* ext : RequiredExtensions)
        {
            bool found = false;

            for (VkExtensionProperties& it : vkProperties)
            {
                if (!std::strcmp(it.extensionName, ext))
                {
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                // required extension missing.
                return HYP_MAKE_ERROR(RendererError, "Required Vulkan extension '{}' is not supported by the system", 0, ext);
            }

            outExtensions.PushBack(ext);
        }

        return {};
    }
#endif

#if HYP_SDL
    if (const SDLAppContext* sdlAppContext = ObjCast<SDLAppContext>(g_appContext))
    {
        uint32 numExtensions = 0;
        SDL_Window* sdlWindow = static_cast<SDL_Window*>(static_cast<SDLApplicationWindow*>(sdlAppContext->GetMainWindow())->GetHWND());

        if (!SDL_Vulkan_GetInstanceExtensions(sdlWindow, &numExtensions, nullptr))
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to get Vulkan instance extensions from SDL: {}", 0, SDL_GetError());
        }

        outExtensions.Resize(numExtensions);

        if (!SDL_Vulkan_GetInstanceExtensions(sdlWindow, &numExtensions, outExtensions.Data()))
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to get Vulkan instance extensions from SDL: {}", 0, SDL_GetError());
        }

        return {};
    }
#endif

#if HYP_WINDOWS
    if (const Win32AppContext* win32AppContext = ObjCast<Win32AppContext>(g_appContext))
    {
        // extensions required for Win32 surface support
        static const char* RequiredExtensions[] = {
            "VK_KHR_surface",
            "VK_KHR_win32_surface"
        };

        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

        Array<VkExtensionProperties> vkProperties(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, vkProperties.Data());

        for (const char* requiredExtension : RequiredExtensions)
        {
            bool found = false;

            for (VkExtensionProperties& it : vkProperties)
            {
                if (!std::strcmp(it.extensionName, requiredExtension))
                {
                    found = true;

                    break;
                }
            }

            if (!found)
            {
                // required extension missing.
                return HYP_MAKE_ERROR(RendererError, "Required Vulkan extension '{}' is not supported by the system", 0, requiredExtension);
            }

            outExtensions.PushBack(requiredExtension);
        }

        return {};
    }
#endif

    return HYP_MAKE_ERROR(RendererError, "Failed to get Vulkan extensions: Unsupported application context type");
}

#pragma endregion VulkanRenderBackend

} // namespace Hyperion

#undef CHECK_FRAME_RESULT
