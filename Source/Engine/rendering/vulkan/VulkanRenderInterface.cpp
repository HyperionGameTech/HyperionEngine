/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanRenderInterface.hpp>
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
#include <rendering/vulkan/VulkanShaderInstance.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanAsyncCompute.hpp>
#include <rendering/vulkan/VulkanRayTracingPipeline.hpp>
#include <rendering/vulkan/VulkanAccelerationStructure.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <rendering/RenderableAttributes.hpp>
#include <rendering/RenderInterface.hpp>
#include <rendering/FinalPass.hpp>
#include <rendering/Bindless.hpp>
#include <rendering/CrashHandler.hpp>
#include <rendering/ConstantsAllocator.hpp>

#include <Core/containers/SparsePagedArray.hpp>

#include <Core/config/Config.hpp>

#include <rendering/Texture.hpp>

#include <system/AppContext.hpp>

#define CHECK_FRAME_RESULT(result)                  \
    do                                              \
    {                                               \
        auto _result = (result);                    \
                                                    \
        if (!(_result))                             \
        {                                           \
            crashHandler->Dump();                   \
                                                    \
            HYP_UNREACHABLE();                      \
        }                                           \
    }                                               \
    while (0)

namespace Hyperion {

static constexpr bool UseResetDescriptorPool = false;
static constexpr uint32 MaxDescriptorPools = 32;

namespace CoreApi {
extern const GlobalConfig& GetGlobalConfig();
} // namespace CoreApi

enum VulkanDescriptorPoolRequirements : uint8
{
    VDPR_None = 0x0,
    VDPR_BindlessTextures = 0x1,
    VDPR_BindlessBuffers = 0x2,
    VDPR_Bindless = VDPR_BindlessTextures | VDPR_BindlessBuffers,
    VDPR_RayTracing = 0x4
};

#pragma region VulkanRenderConfig

class VulkanRenderConfig final : public IRenderConfig
{
public:
    void Initialize(VulkanRenderInterface* renderBackend)
    {
        Assert(renderBackend != nullptr && renderBackend->GetDevice() != nullptr);

        bindlessTextures = renderBackend->GetDevice()->GetFeatures().SupportsBindlessTextures();
        rayTracing = renderBackend->GetDevice()->GetFeatures().IsRayTracingSupported();
        indirectRendering = CoreApi::GetGlobalConfig().Get("Rendering.IndirectRendering").ToBool(/* defaultValue */ true);
        parallelRendering = CoreApi::GetGlobalConfig().Get("Rendering.ParallelCollection").ToBool(/* defaultValue */ true);
        dynamicDescriptorIndexing = renderBackend->GetDevice()->GetFeatures().SupportsDynamicDescriptorIndexing();
    }
};

#pragma endregion VulkanRenderConfig

#pragma region Vulkan struct wrappers

static VkDescriptorSetLayout CreateVkDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayout& layout)
{
    // flags applied to bindless descriptor sets no matter what
    static constexpr VkDescriptorBindingFlags BindlessFlags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
        | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT
        | VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

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
            descriptorCount = element.IsBuffer()
                ? MaxBindlessResources[BindlessStorage_Buffers]
                : MaxBindlessResources[BindlessStorage_Textures];
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

        VkDescriptorBindingFlags flags = 0;

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

#if HYP_DEBUG_MODE
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

class VulkanDescriptorSetManager
{
public:
    static constexpr uint32 MaxDescriptorSets = 4096;

    VulkanDescriptorSetManager();
    ~VulkanDescriptorSetManager();

    void OnFrameStart();

    RendererResult Create(VulkanDevice* device);
    RendererResult Destroy(VulkanDevice* device);

    RendererResult CreateDescriptorSet(
        VulkanDevice* device,
        VkDescriptorSetLayout layout,
        VulkanDescriptorPoolRequirements reqs,
        VkDescriptorSet& outVkDescriptorSet,
        VkDescriptorPool& outVkDescriptorPool);

    RendererResult DestroyDescriptorSet(
        VulkanDevice* device,
        VkDescriptorSet vkDescriptorSet,
        VkDescriptorPool vkDescriptorPool);

    VkDescriptorSetLayout GetOrCreateVkDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayout& layout);

private:
    VkDescriptorPool GetDescriptorPool(uint32 currentFrameCounter, VulkanDescriptorPoolRequirements reqs, int& outPoolIndex);
    RendererResult CreateDescriptorPool(VulkanDescriptorPoolRequirements reqs, VkDescriptorPool& outDescriptorPool);

    SharedMutex m_mutex;
    HashMap<HashCode, VkDescriptorSetLayout> m_vkDescriptorSetLayouts;

    struct VulkanDescriptorPool
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VulkanDescriptorPoolRequirements reqs = VDPR_None;
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
    const uint32 frameCounter = GetFrameCounter();

    if (!UseResetDescriptorPool)
        return;

    // Reset descriptor pools for this frame
    for (size_t i = 0; i < m_pools.Size(); i++)
    {
        VulkanDescriptorPool& dp = m_pools[i];

        if (dp.frameCounter % RingBufferDepth == 0)
        {
            VkResult result = vkResetDescriptorPool(g_renderInterface->GetDevice()->GetDevice(), dp.pool, 0);
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
    RendererResult result {};

    m_vkDescriptorSetLayouts.Clear();

    for (size_t i = 0; i < m_pools.Size(); i++)
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

VkDescriptorPool VulkanDescriptorSetManager::GetDescriptorPool(
    uint32 currentFrameCounter,
    VulkanDescriptorPoolRequirements reqs,
    int& outPoolIndex)
{
    outPoolIndex = -1;

    // return last descriptor pool, it's most likely for allocations
    // to succeed with it since it would have more free memory
    for (size_t idx = m_pools.Size(); idx != 0; --idx)
    {
        VulkanDescriptorPool& dp = m_pools[idx - 1];

        if (reqs && (dp.reqs & reqs) != reqs)
        {
            continue;
        }

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
    if (RendererResult createDescriptorPoolResult = CreateDescriptorPool(reqs, pool); createDescriptorPoolResult.HasError())
    {
        HYP_FAIL("Failed to create descriptor pool! {}", createDescriptorPoolResult.GetError().GetMessage());
    }

    outPoolIndex = int(m_pools.Size() - 1);

    return pool;
}

RendererResult VulkanDescriptorSetManager::CreateDescriptorPool(VulkanDescriptorPoolRequirements reqs, VkDescriptorPool& outDescriptorPool)
{
    if (m_pools.Size() >= MaxDescriptorPools)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot allocate new descriptor pool: maximum number of descriptor pools has been exceeded ({})", 0, MaxDescriptorPools);
    }

    Array<VkDescriptorPoolSize> descriptorPoolSizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 16 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (reqs & VDPR_BindlessTextures) ? MaxBindlessResources[BindlessStorage_Textures] : 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (reqs & VDPR_BindlessBuffers) ? MaxBindlessResources[BindlessStorage_Buffers] : 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, (reqs & VDPR_BindlessBuffers) ? MaxBindlessResources[BindlessStorage_Buffers] : 1000 }
    };

    // only add acceleration structure descriptor type if rayTracing is supported,
    // otherwise we'll get an error when creating the descriptor pool
    if ((reqs & VDPR_RayTracing) && g_renderInterface->GetDevice()->GetFeatures().IsRayTracingSupported())
    {
        descriptorPoolSizes.PushBack({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 });
    }

    outDescriptorPool = VK_NULL_HANDLE;

    VulkanDescriptorPool& dp = m_pools.EmplaceBack();
    dp.reqs = reqs;
    dp.frameCounter = GetFrameCounter();

    VkDescriptorPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT
        | (!UseResetDescriptorPool ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0);
    poolInfo.maxSets = MaxDescriptorSets;
    poolInfo.poolSizeCount = uint32(descriptorPoolSizes.Size());
    poolInfo.pPoolSizes = descriptorPoolSizes.Data();

    VULKAN_CHECK(vkCreateDescriptorPool(
        g_renderInterface->GetDevice()->GetDevice(),
        &poolInfo,
        nullptr,
        &dp.pool));

    HYP_LOG(RenderingBackend, Verbose, "Created new Vulkan descriptor pool {} ({})", (void*)dp.pool, m_pools.Size());

    outDescriptorPool = dp.pool;

    return {};
}

RendererResult VulkanDescriptorSetManager::CreateDescriptorSet(
    VulkanDevice* device,
    VkDescriptorSetLayout layout,
    VulkanDescriptorPoolRequirements reqs,
    VkDescriptorSet& outVkDescriptorSet,
    VkDescriptorPool& outVkDescriptorPool)
{
    Assert(layout != VK_NULL_HANDLE);

    VkDescriptorSetLayout layouts[] = { layout };

    int poolIndex = -1;

    outVkDescriptorPool = GetDescriptorPool(GetFrameCounter(), reqs, poolIndex);

    const uint32 variableDescriptorCount = (reqs & VDPR_BindlessTextures)
        ? MaxBindlessResources[BindlessStorage_Textures]
        : (reqs & VDPR_BindlessBuffers)
            ? MaxBindlessResources[BindlessStorage_Buffers]
            : 0;

    VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO };
    variableCountInfo.descriptorSetCount = 1;
    variableCountInfo.pDescriptorCounts = &variableDescriptorCount;

    bool shouldRetry = false;

    do
    {
        shouldRetry = false;

        Assert(outVkDescriptorPool != VK_NULL_HANDLE);

        VkDescriptorSetAllocateInfo allocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
        allocInfo.descriptorPool = outVkDescriptorPool;
        allocInfo.descriptorSetCount = ArraySize(layouts);
        allocInfo.pSetLayouts = &layouts[0];

        if (variableDescriptorCount > 0)
        {
            allocInfo.pNext = &variableCountInfo;
        }

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
                    if (RendererResult createDescriptorPoolResult = CreateDescriptorPool(reqs, outVkDescriptorPool); createDescriptorPoolResult.HasError())
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

#pragma region VulkanRenderInterface

VulkanRenderInterface::VulkanRenderInterface()
    : m_instance(nullptr),
      m_renderConfig(MakePimpl<VulkanRenderConfig>()),
      m_descriptorSetManager(MakePimpl<VulkanDescriptorSetManager>()),
      m_currentFrameIndex(0)
{
    m_frames.Resize(NumFramesInFlight);
    m_commandBuffers.Resize(NumFramesInFlight);
}

VulkanRenderInterface::~VulkanRenderInterface()
{
}

const VulkanDeviceRef& VulkanRenderInterface::GetDevice() const
{
    return m_instance->GetDevice();
}

const IRenderConfig& VulkanRenderInterface::GetRenderConfig() const
{
    return *m_renderConfig;
}

RendererResult VulkanRenderInterface::Initialize()
{
#if HYP_DEBUG_MODE
    static const ConfigValue& s_cfgDebugLayers = CoreApi::GetGlobalConfig().Get("Rendering.Vulkan.DebugLayers");

    if (s_cfgDebugLayers.ToBool(false))
    {
        HYP_LOG(RenderingBackend, Info, "Running with Vulkan validation layers enabled; expect lower performance");
    }
    else
    {
        HYP_LOG(RenderingBackend, Info, "Running without Vulkan validation layers");
    }

    const bool enableDebugLayers = s_cfgDebugLayers.ToBool(false);
#else
    const bool enableDebugLayers = false;
#endif

    m_instance = PoolNew<VulkanInstance>(*g_vulkanPool);
    CheckResultOrReturn(m_instance->Initialize(enableDebugLayers));

    VulkanDeviceQueue* deviceQueue = GetDevice()->GetPresentQueue();

    if (!deviceQueue)
    {
        // running in headless mode
        deviceQueue = GetDevice()->GetGraphicsQueue();
    }

    Assert(deviceQueue != nullptr);

    // Create frames
    for (uint32 frameIndex = 0; frameIndex < uint32(m_frames.Size()); frameIndex++)
    {
        VulkanFrameRef& frame = m_frames[frameIndex];
        VulkanCommandBufferRef& commandBuffer = m_commandBuffers[frameIndex];

        VkCommandPool pool = deviceQueue->commandPools[0];
        Assert(pool != VK_NULL_HANDLE);

        commandBuffer = MakeHandle<VulkanCommandBuffer>(VK_COMMAND_BUFFER_LEVEL_PRIMARY);
        frame = MakeHandle<VulkanFrame>(frameIndex);

        CheckResultOrReturn(commandBuffer->Create(pool));
        CheckResultOrReturn(frame->Create());
    }

    VulkanDynamicFunctions::Load(m_instance->GetDevice());

    m_renderConfig->Initialize(this);

    const VulkanFeatures& deviceFeatures = m_instance->GetDevice()->GetFeatures();
    const VkPhysicalDeviceProperties& physicalDeviceProperties = deviceFeatures.GetPhysicalDeviceProperties();

    HYP_LOG(RenderingBackend, Info, "Selected Vulkan physical device: {}", physicalDeviceProperties.deviceName);
    HYP_LOG(RenderingBackend, Info, "Vulkan feature support:\n\tBindless Textures? {}\n\tRay Tracing? {}\n\tDynamic Descriptor Indexing? {}",
        deviceFeatures.SupportsBindlessTextures(),
        deviceFeatures.IsRayTracingSupported(),
        deviceFeatures.SupportsDynamicDescriptorIndexing());

    CheckResultOrReturn(m_descriptorSetManager->Create(m_instance->GetDevice()));

    const VkDeviceSize minUniformBufferOffsetAlignment = g_renderInterface->GetDevice()->GetFeatures()
        .GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;

    constantsAllocator->Initialize(minUniformBufferOffsetAlignment);

    return RenderInterface::Initialize();
}

void VulkanRenderInterface::Shutdown()
{
    CheckResult(m_instance->GetDevice()->WaitIdle());

    for (VulkanFrameRef& frame : m_frames)
    {
        if (!frame)
        {
            continue;
        }

        if (frame->GetFence()->isSubmitted && !frame->GetFence()->CheckStatus())
        {
            frame->GetFence()->Wait();
        }

        frame.Reset();
    }

    m_frames.Clear();
    m_commandBuffers.Clear();

    for (VulkanAsyncCompute* ac : m_asyncComputePool)
    {
        delete ac;
    }

    for (VulkanAsyncCompute* ac : m_submittedAsyncComputes)
    {
        ac->GetFence()->Wait(true);

        delete ac;
    }

    m_asyncComputePool.Clear();
    m_submittedAsyncComputes.Clear();

    RenderInterface::Shutdown();

    m_descriptorSetManager->Destroy(m_instance->GetDevice());
    
    PoolDelete(*g_vulkanPool, m_instance);
    m_instance = nullptr;
    
    DeletionQueue::GetInstance().Flush();
}

VulkanFrame* VulkanRenderInterface::GetCurrentFrame() const
{
    return m_frames[m_currentFrameIndex];
}

VulkanFrame* VulkanRenderInterface::PrepareNextFrame()
{
    VulkanFrame* frame = GetCurrentFrame();
    frame->GetFence()->Wait(true);

    for (auto it = m_submittedAsyncComputes.Begin(); it != m_submittedAsyncComputes.End();)
    {
        VulkanAsyncCompute* elem = *it;

        if (elem->CheckStatus())
        {
            elem->OnCompleted();

            // @NOTE Don't need to lock mutex since we'll only be using CreateAsyncCompute() from main render thread and render task / workers.
            // And workers wouldn't be kicked off at this point in the frame.
            m_asyncComputePool.PushBack(elem);

            it = m_submittedAsyncComputes.Erase(it);

            continue;
        }
        
        ++it;
    }

    // trim async compute pool if > 10 items
    if (m_asyncComputePool.Size() > 10)
    {
        static constexpr uint32 MaxFramesBeforeDiscard = 100;

        const uint32 currFrameIndex = GetFrameCounter();

        for (auto it = m_asyncComputePool.Begin(); it != m_asyncComputePool.End();)
        {
            VulkanAsyncCompute* elem = *it;

            if (currFrameIndex - elem->lastFrame >= MaxFramesBeforeDiscard)
            {
                delete elem;

                it = m_asyncComputePool.Erase(it);

                continue;
            }

            ++it;
        }
    }

    frame->OnFrameStart();

    m_descriptorSetManager->OnFrameStart();

    AssertDebug(frame != nullptr);

    return frame;
}

VulkanSwapchainRef VulkanRenderInterface::CreateSwapchain(ApplicationWindow* window, const Vec2u& extent)
{
    AssertOnThread(g_renderThread);

    VkSurfaceKHR surface = window->GetVkSurface();
    Assert(surface != VK_NULL_HANDLE);

    VulkanSwapchainRef swapchain = MakeHandle<VulkanSwapchain>(surface, extent);
    RendererResult result = swapchain->Create();

    if (!result)
    {
        HYP_FAIL("Failed to create Vulkan swapchain: {}", result.GetError().GetMessage());
    }

    return swapchain;
}

void VulkanRenderInterface::PrepareSwapchain(VulkanSwapchain* swapchain)
{
    swapchain->PrepareForFrame(GetCurrentFrame());
}

void VulkanRenderInterface::SubmitCommandBuffers(VulkanSwapchain* swapchain)
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
}

void VulkanRenderInterface::PresentToSwapchain(VulkanSwapchain* swapchain)
{
    VulkanDeviceQueue* presentQueue = m_instance->GetDevice()->GetPresentQueue();
    AssertDebug(presentQueue != nullptr); // should never be null when presenting, not used in headless mode

    VulkanDevice* vulkanDevice = m_instance->GetDevice();
    VulkanCommandBuffer* vulkanCommandBuffer = GetCurrentCommandBuffer();
    VulkanFrame* vulkanFrame = GetCurrentFrame();

    swapchain->PresentFrame(vulkanFrame, presentQueue);
}

VulkanCommandBuffer* VulkanRenderInterface::GetCurrentCommandBuffer() const
{
    return m_commandBuffers[m_currentFrameIndex];
}

VulkanDescriptorSetRef VulkanRenderInterface::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    DescriptorSetLayout newLayout { layout.GetDeclaration() };
    newLayout.SetIsTemplate(false);
    newLayout.SetIsReference(false);

    VulkanDescriptorSetRef descriptorSet = MakeHandle<VulkanDescriptorSet>(newLayout);
#if HYP_DEBUG_MODE
    descriptorSet->SetDebugName(layout.GetName());
#endif

    return descriptorSet;
}

VulkanDescriptorTableRef VulkanRenderInterface::MakeDescriptorTable(const ShaderInputGroup* decl)
{
    return MakeHandle<VulkanDescriptorTable>(decl);
}

VulkanGraphicsPipelineRef VulkanRenderInterface::MakeGraphicsPipeline(
    const VulkanShaderInstanceRef& shaderInstance,
    const RenderTargetDesc& renderTargetDesc,
    const RenderableAttributeSet& attributes)
{
    VulkanGraphicsPipelineRef graphicsPipeline = MakeHandle<VulkanGraphicsPipeline>();

    if (shaderInstance.IsValid())
    {
        graphicsPipeline->SetShader(shaderInstance);

#if HYP_DEBUG_MODE
        graphicsPipeline->SetDebugName(NAME_FMT("GraphicsPipeline_{}", shaderInstance->GetDebugName().IsValid() ? *shaderInstance->GetDebugName() : "<unnamed shader>"));
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
    graphicsPipeline->SetDepthClamp(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP));

    if (attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
    {
        graphicsPipeline->SetDepthBias(attributes.GetMaterialAttributes().depthBias);
        graphicsPipeline->SetDepthBiasSlope(attributes.GetMaterialAttributes().depthBiasSlope);
    }

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

VulkanComputePipelineRef VulkanRenderInterface::MakeComputePipeline(const VulkanShaderInstanceRef& shaderInstance)
{
    return MakeHandle<VulkanComputePipeline>(shaderInstance);
}

VulkanRayTracingPipelineRef VulkanRenderInterface::MakeRayTracingPipeline(const VulkanShaderInstanceRef& shaderInstance)
{
    return MakeHandle<VulkanRayTracingPipeline>(shaderInstance);
}

VulkanGpuBufferRef VulkanRenderInterface::MakeGpuBuffer(GpuBufferType bufferType, size_t size, size_t alignment)
{
    return MakeHandle<VulkanGpuBuffer>(bufferType, size, alignment);
}

VulkanGpuImageRef VulkanRenderInterface::MakeImage(const TextureDesc& textureDesc)
{
    return MakeHandle<VulkanGpuImage>(textureDesc);
}

VulkanGpuImageViewRef VulkanRenderInterface::MakeImageView(const VulkanGpuImageRef& image)
{
    VulkanGpuImageViewRef ref = MakeHandle<VulkanGpuImageView>(image);
#if HYP_DEBUG_MODE
    ref->SetDebugName(NAME_FMT("{}_IV", image->GetDebugName()));
#endif

    return ref;
}

VulkanGpuImageViewRef VulkanRenderInterface::MakeImageView(const VulkanGpuImageRef& image, uint8 mipIndex, uint8 numMips, uint16 layerIndex, uint16 numLayers)
{
    ImageSubResource subResource {};
    subResource.baseMipLevel = mipIndex;
    subResource.baseArrayLayer = layerIndex;
    subResource.numLevels = numMips;
    subResource.numLayers = numLayers;

    VulkanGpuImageViewRef ref = MakeHandle<VulkanGpuImageView>(image, subResource);
#if HYP_DEBUG_MODE
    ref->SetDebugName(NAME_FMT("{}_IV", image->GetDebugName()));
#endif

    return ref;
}

VulkanSamplerRef VulkanRenderInterface::MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode)
{
    return MakeHandle<VulkanSampler>(filterModeMin, filterModeMag, wrapMode);
}

VulkanFramebufferRef VulkanRenderInterface::MakeFramebuffer(const RenderTargetDesc& renderTargetDesc)
{
    return MakeHandle<VulkanFramebuffer>(renderTargetDesc);
}

VulkanFrameRef VulkanRenderInterface::MakeFrame(uint32 frameIndex)
{
    return MakeHandle<VulkanFrame>(frameIndex);
}

VulkanShaderInstanceRef VulkanRenderInterface::MakeShader(const Shader* shader)
{
    return MakeHandle<VulkanShaderInstance>(shader);
}

VulkanGpuBlasRef VulkanRenderInterface::MakeGpuBlas(
    const VulkanGpuBufferRef& packedVerticesBuffer,
    const VulkanGpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Mat4f& transform)
{
    return MakeHandle<VulkanGpuBlas>(
        VulkanGpuBufferRef(packedVerticesBuffer),
        VulkanGpuBufferRef(packedIndicesBuffer),
        numVertices,
        numIndices,
        material,
        transform);
}

VulkanGpuTlasRef VulkanRenderInterface::MakeTLAS()
{
    return MakeHandle<VulkanGpuTlas>();
}

void VulkanRenderInterface::PopulateIndirectDrawCommandsBuffer(
    const VulkanGpuBufferRef& vertexBuffer,
    const VulkanGpuBufferRef& indexBuffer,
    uint32 instanceOffset,
    TByteBuffer<RenderAllocator>& outByteBuffer)
{
    const size_t requiredSize = (size_t(instanceOffset) + 1) * sizeof(VkDrawIndexedIndirectCommand);

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

TextureFormat VulkanRenderInterface::GetDefaultFormat(DefaultImageFormat type) const
{
    auto it = m_defaultFormats.Find(type);
    if (it != m_defaultFormats.End())
    {
        return it->second;
    }

    return InvalidTextureFormat;
}

bool VulkanRenderInterface::IsSupportedFormat(TextureFormat format, ImageSupport supportType) const
{
    return m_instance->GetDevice()->GetFeatures().IsSupportedFormat(format, supportType);
}

TextureFormat VulkanRenderInterface::FindSupportedFormat(Span<TextureFormat> possibleFormats, ImageSupport supportType) const
{
    return m_instance->GetDevice()->GetFeatures().FindSupportedFormat(possibleFormats, supportType);
}

RendererResult VulkanRenderInterface::CreateDescriptorSet(
    VkDescriptorSetLayout vkDescriptorSetLayout,
    bool isBindlessTextures, bool isBindlessBuffers, bool isRayTracing,
    VkDescriptorSet& outVkDescriptorSet,
    VkDescriptorPool& outVkDescriptorPool)
{
    uint8 reqs = VDPR_None;
    if (isBindlessTextures)
        reqs |= VDPR_BindlessTextures;
    if (isBindlessBuffers)
        reqs |= VDPR_BindlessBuffers;
    if (isRayTracing)
        reqs |= VDPR_RayTracing;

    return m_descriptorSetManager->CreateDescriptorSet(m_instance->GetDevice(),
        vkDescriptorSetLayout, VulkanDescriptorPoolRequirements(reqs), outVkDescriptorSet, outVkDescriptorPool);
}

RendererResult VulkanRenderInterface::DestroyDescriptorSet(VkDescriptorSet vkDescriptorSet, VkDescriptorPool vkDescriptorPool)
{
    return m_descriptorSetManager->DestroyDescriptorSet(m_instance->GetDevice(), vkDescriptorSet, vkDescriptorPool);
}

RendererResult VulkanRenderInterface::GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, VkDescriptorSetLayout& out)
{
    out = m_descriptorSetManager->GetOrCreateVkDescriptorSetLayout(m_instance->GetDevice(), layout);

    if (out != VK_NULL_HANDLE)
    {
        return RendererResult {};
    }

    return HYP_MAKE_ERROR(RendererError, "Failed to get or create Vulkan descriptor set layout");
}

UniquePtr<SingleTimeCommands> VulkanRenderInterface::GetSingleTimeCommands()
{
    return MakeUnique<VulkanSingleTimeCommands>();
}

HYP_NODISCARD VulkanAsyncCompute* VulkanRenderInterface::CreateAsyncCompute()
{
    {
        Mutex::Guard guard(m_asyncComputesMutex);

        if (m_asyncComputePool.Any())
        {
            return m_asyncComputePool.PopBack();
        }
    }

    // create new
    VulkanAsyncCompute* newAsyncCompute = new VulkanAsyncCompute();
    newAsyncCompute->Create();

    return newAsyncCompute;
}

void VulkanRenderInterface::SubmitAsyncCompute(VulkanAsyncCompute* asyncCompute)
{
    Assert(asyncCompute != nullptr);
    
    Mutex::Guard guard(m_asyncComputesMutex);
    Assert(!m_submittedAsyncComputes.Contains(asyncCompute));

    asyncCompute->Submit();

    m_submittedAsyncComputes.PushBack(asyncCompute);
}

void VulkanRenderInterface::ReleaseTransientMemory()
{
    // must happen before arena is reset or it's corruption city!
    GetCurrentFrame()->ResetTransientStates();

    g_vulkanArena->Reset();
}

VkSurfaceKHR VulkanRenderInterface::CreateSurface(ApplicationWindow* window, IDummyVulkanSurfaceContext** ppOutDummySurfaceContext)
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
#elif HYP_ANDROID
    if (!window)
    {
        window = g_appContext->GetMainWindow();
    }

    AndroidApplicationWindow* androidWindow = ObjCast<AndroidApplicationWindow>(window);
    Assert(androidWindow != nullptr);

    return AndroidAppContext::CreateVulkanSurface(androidWindow, ppOutDummySurfaceContext);
#else
    HYP_NOT_IMPLEMENTED();
    return VK_NULL_HANDLE;
#endif
}

RendererResult VulkanRenderInterface::GetVkExtensions(Array<const char*>& outExtensions)
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

#if HYP_ANDROID
    if (const AndroidAppContext* androidAppContext = ObjCast<AndroidAppContext>(g_appContext))
    {
        static const char* RequiredExtensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
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
                return HYP_MAKE_ERROR(RendererError, "Required Vulkan extension '{}' is not supported by the system", 0, requiredExtension);
            }

            outExtensions.PushBack(requiredExtension);
        }

        return {};
    }
#endif

    return HYP_MAKE_ERROR(RendererError, "Failed to get Vulkan extensions: Unsupported application context type");
}

#pragma endregion VulkanRenderInterface

} // namespace Hyperion

#undef CHECK_FRAME_RESULT
