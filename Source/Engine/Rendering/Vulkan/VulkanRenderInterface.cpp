/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 *  @note Created by ethan on 2/5/22.
 */

#include <VulkanPch.hpp>

#include <Rendering/Vulkan/VulkanRenderInterface.hpp>
#include <Rendering/Vulkan/VulkanMemory.hpp>
#include <Rendering/Vulkan/VulkanSwapchain.hpp>
#include <Rendering/Vulkan/VulkanFrame.hpp>
#include <Rendering/Vulkan/VulkanFence.hpp>
#include <Rendering/Vulkan/VulkanGraphicsPipeline.hpp>
#include <Rendering/Vulkan/VulkanComputePipeline.hpp>
#include <Rendering/Vulkan/VulkanDescriptorSet.hpp>
#include <Rendering/Vulkan/VulkanInstance.hpp>
#include <Rendering/Vulkan/VulkanDevice.hpp>
#include <Rendering/Vulkan/VulkanHelpers.hpp>
#include <Rendering/Vulkan/VulkanShaderInstance.hpp>
#include <Rendering/Vulkan/VulkanFeatures.hpp>
#include <Rendering/Vulkan/VulkanAsyncCompute.hpp>
#include <Rendering/Vulkan/VulkanRayTracingPipeline.hpp>
#include <Rendering/Vulkan/VulkanAccelerationStructure.hpp>
#include <Rendering/Vulkan/VulkanGpuTimerBackend.hpp>

#include <Rendering/Util/DeletionQueue.hpp>

#include <Rendering/RenderableAttributes.hpp>
#include <Rendering/RenderInterface.hpp>
#include <Rendering/FinalPass.hpp>
#include <Rendering/Bindless.hpp>
#include <Rendering/CrashHandler.hpp>
#include <Rendering/CBufferAllocator.hpp>
#include <Rendering/BLASCache.hpp>

#include <Framework/Config/EngineConfig.hpp>

#include <Framework/EngineStats.hpp>

#include <Core/Containers/SparsePagedArray.hpp>

#include <Rendering/Texture.hpp>

#include <System/AppContext.hpp>

#define CHECK_FRAME_RESULT(result) \
    do                             \
    {                              \
        auto _result = (result);   \
                                   \
        if (!(_result))            \
        {                          \
            CrashHandler::Dump();  \
                                   \
            HYP_UNREACHABLE();     \
        }                          \
    }                              \
    while (0)

namespace Hyperion {

static constexpr bool UseResetDescriptorPool = false;
static constexpr uint32 MaxDescriptorPools = 32;

static EngineStatTimer s_statVulkanFrameSync("Rendering/Vulkan/FrameSync");

extern EngineStatGpuTimer g_statGpuFrameTime;
extern EngineStatTimer g_statTotalStallTime;

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

        EngineConfig cfg;
        cfg.Load();

        bindlessTextures = renderBackend->GetDevice()->GetFeatures().SupportsBindlessTextures();
        rayTracing = renderBackend->GetDevice()->GetFeatures().IsRayTracingSupported();
#ifndef HYP_ANDROID
        indirectRendering = cfg.Get("Rendering.IndirectRendering").ToBool(/* defaultValue */ true);
#endif
        parallelRendering = cfg.Get("Rendering.ParallelCollection").ToBool(/* defaultValue */ true);
        timelineSemaphores = cfg.Get("Rendering.Vulkan.TimelineSemaphores").ToBool(/* defaultValue */ false);
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

    Array<VkDescriptorSetLayoutBinding, VulkanTempAllocator> bindings;
    bindings.Reserve(layout.GetElements().Size());

    Array<VkDescriptorBindingFlags, VulkanTempAllocator> bindingFlags;
    bindingFlags.Reserve(layout.GetElements().Size());

    bool isBindlessTextures = false;
    bool isBindlessBuffers = false;

    for (const ShaderInputWithBinding& shaderInput : layout.GetElements())
    {
        uint32 descriptorCount = shaderInput.count;

        if (descriptorCount == ~0u)
        {
            if (shaderInput.category == ShaderResourceCategory::Buffer)
            {
                isBindlessBuffers = true;

                descriptorCount = MaxBindlessResources[BindlessStorage_Buffers];
            }
            else
            {
                isBindlessTextures = true;

                descriptorCount = MaxBindlessResources[BindlessStorage_Textures];
            }
        }

        // if (descriptorCount > 1 && !m_device->GetFeatures().SupportsDynamicDescriptorIndexing()) {
        //     return HYP_MAKE_ERROR(RendererError, "Device does not support descriptor indexing, cannot create descriptor set with element {} that uses an array of elements", 0, name);
        // }

        VkDescriptorSetLayoutBinding binding {};
        binding.descriptorCount = descriptorCount;
        binding.descriptorType = ToVkDescriptorType(shaderInput.type, shaderInput.category);
        binding.pImmutableSamplers = nullptr;
        binding.stageFlags = VK_SHADER_STAGE_ALL;
        binding.binding = shaderInput.binding;

        bindings.PushBack(binding);

        VkDescriptorBindingFlags flags = 0;

        if (shaderInput.count == ~0u)
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
    layoutInfo.pNext = &extendedInfo;

    if (isBindlessTextures || isBindlessBuffers)
    {
        layoutInfo.flags |= VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
    }

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

void VulkanDynamicFunctions::Load(VulkanDevice* device)
{
#define HYP_LOAD_FN(function)                                                                             \
    do                                                                                                    \
    {                                                                                                     \
        function = reinterpret_cast<PFN_##function>(vkGetDeviceProcAddr(device->GetDevice(), #function)); \
        if (!function)                                                                                    \
        {                                                                                                 \
            HYP_LOG(RenderingBackend, Warning, "Failed to load Vulkan function {}", #function);           \
        }                                                                                                 \
    }                                                                                                     \
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

    HYP_LOAD_FN(vkSignalSemaphore);
    HYP_LOAD_FN(vkWaitSemaphores);
    HYP_LOAD_FN(vkGetSemaphoreCounterValue);

#if HYP_DEBUG_MODE
    HYP_LOAD_FN(vkCmdDebugMarkerBeginEXT);
    HYP_LOAD_FN(vkCmdDebugMarkerEndEXT);
    HYP_LOAD_FN(vkCmdDebugMarkerInsertEXT);
#endif

#ifdef HYP_RHI_DEBUG_NAMES
    HYP_LOAD_FN(vkDebugMarkerSetObjectNameEXT);
    HYP_LOAD_FN(vkSetDebugUtilsObjectNameEXT);
    HYP_LOAD_FN(vkSetDebugUtilsObjectTagEXT);
#endif

#if defined(HYP_MOLTENVK) && HYP_MOLTENVK && HYP_MOLTENVK_LINKED
    HYP_LOAD_FN(vkGetMoltenVKConfigurationMVK);
    HYP_LOAD_FN(vkSetMoltenVKConfigurationMVK);
#endif

    // extended dynamic state (VK_EXT_extended_dynamic_state)
    HYP_LOAD_FN(vkCmdSetDepthWriteEnableEXT);
    HYP_LOAD_FN(vkCmdSetDepthTestEnableEXT);
    HYP_LOAD_FN(vkCmdSetDepthCompareOpEXT);

#undef HYP_LOAD_FN
}

#pragma endregion VulkanDynamicFunctions

#pragma region VulkanDescriptorSetManager

class VulkanDescriptorSetManager
{
public:
    HYP_DEF_POOL_NEW_DELETE(g_vulkanPool);

    static constexpr uint32 MaxDescriptorSets = 4096;

    VulkanDescriptorSetManager();
    ~VulkanDescriptorSetManager();

    void OnFrameStart();

    void Initialize(VulkanDevice* device);
    void Shutdown(VulkanDevice* device);

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
    Map<uint64, VkDescriptorSetLayout, VulkanAllocator> m_vkDescriptorSetLayouts;

    struct VulkanDescriptorPool
    {
        VkDescriptorPool pool = VK_NULL_HANDLE;
        VulkanDescriptorPoolRequirements reqs = VDPR_None;
        uint32 useCount = 0;
        uint32 frameCounter = 0; // last used or created
    };

    Array<VulkanDescriptorPool, VulkanAllocator> m_pools;
};

VulkanDescriptorSetManager::VulkanDescriptorSetManager()
{
}

VulkanDescriptorSetManager::~VulkanDescriptorSetManager() = default;

void VulkanDescriptorSetManager::OnFrameStart()
{
    if (!UseResetDescriptorPool)
    {
        return;
    }

    // Reset descriptor pools for this frame
    for (size_t i = 0; i < m_pools.Size(); i++)
    {
        VulkanDescriptorPool& dp = m_pools[i];

        if (dp.frameCounter % RingBufferDepth == 0)
        {
            VkResult result = vkResetDescriptorPool(RI.GetDevice()->GetDevice(), dp.pool, 0);
            Assert(result == VK_SUCCESS, "Failed to reset descriptor pool! {}", result);
        }
    }
}

void VulkanDescriptorSetManager::Initialize(VulkanDevice* device)
{
}

void VulkanDescriptorSetManager::Shutdown(VulkanDevice* device)
{
    for (auto& pair : m_vkDescriptorSetLayouts)
    {
        DestroyVkDescriptorSetLayout(device, pair.second);
    }

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

    Array<VkDescriptorPoolSize, VulkanTempAllocator> descriptorPoolSizes = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000, },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, (reqs & VDPR_BindlessTextures) ? MaxBindlessResources[BindlessStorage_Textures] : 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, (reqs & VDPR_BindlessBuffers) ? MaxBindlessResources[BindlessStorage_Buffers] : 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 }
    };

    // only add acceleration structure descriptor type if rayTracing is supported,
    // otherwise we'll get an error when creating the descriptor pool
    if ((reqs & VDPR_RayTracing) && RI.GetDevice()->GetFeatures().IsRayTracingSupported())
    {
        descriptorPoolSizes.PushBack({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 });
    }

    outDescriptorPool = VK_NULL_HANDLE;

    VulkanDescriptorPool& dp = m_pools.EmplaceBack();
    dp.reqs = reqs;
    dp.frameCounter = GetFrameCounter();

    VkDescriptorPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.flags = (!UseResetDescriptorPool ? VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT : 0);
    poolInfo.maxSets = MaxDescriptorSets;
    poolInfo.poolSizeCount = uint32(descriptorPoolSizes.Size());
    poolInfo.pPoolSizes = descriptorPoolSizes.Data();

    if (reqs & VDPR_Bindless)
    {
        poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    }

    VULKAN_CHECK(vkCreateDescriptorPool(
        RI.GetDevice()->GetDevice(),
        &poolInfo,
        nullptr,
        &dp.pool));

    HYP_LOG(RenderingBackend, Verbose, "Created new Vulkan descriptor pool {}, reqs = {}", (void*)dp.pool, reqs);

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

    bool shouldRetry = true;

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
                if (poolIndex < 0)
                {
                    // Failed to allocate from newly allocated pool - fail instead of trying infinitely until we run out of pools.
                    return HYP_MAKE_ERROR(RendererError, "Failed to allocate descriptor set!", int(vkResult));
                }

                // descend down the list of existing descriptor pools. we start trying to allocate from the last descriptor pool (see GetDescriptorPool())
                --poolIndex;

                // we need a pool that matches our requirements.
                if (reqs)
                {
                    while (poolIndex >= 0 && (m_pools[poolIndex].reqs & reqs) != reqs)
                    {
                        --poolIndex;
                    }
                }

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
    const uint64 layoutHash = layout.GetHashCode().Value();

    VkDescriptorSetLayout handle = VK_NULL_HANDLE;

    TSharedLock lock(m_mutex);

    auto it = m_vkDescriptorSetLayouts.Find(layoutHash);

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
    auto insertResult = m_vkDescriptorSetLayouts.Insert(layoutHash, handle);

    if (!insertResult.second)
    {
        DestroyVkDescriptorSetLayout(device, handle);

        handle = insertResult.first->second;
        AssertDebug(handle != VK_NULL_HANDLE);
    }

    return handle;
}

#pragma endregion VulkanDescriptorSetManager

#pragma region VulkanRenderInterface

VulkanRenderInterface::VulkanRenderInterface()
    : m_instance(nullptr),
      m_renderConfig(nullptr),
      m_currentFrameIndex(0)
{
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
    m_renderConfig = new VulkanRenderConfig;
    m_descriptorSetManager = new VulkanDescriptorSetManager;

    // CrashHandler must be initialized before we create the Vulkan instance
    CrashHandler::Initialize();

    m_frames.Resize(NumFramesInFlight);
    m_commandBuffers.Resize(NumFramesInFlight);
#ifdef HYP_RHI_DEBUG_NAMES
    EngineConfig cfg;
    cfg.Load();

    const ConfigValue& cfgDebugLayers = cfg.Get("Rendering.Vulkan.DebugLayers");

    if (cfgDebugLayers.ToBool(false))
    {
        HYP_LOG(RenderingBackend, Info, "Running with Vulkan validation layers enabled; expect lower performance");
    }
    else
    {
        HYP_LOG(RenderingBackend, Info, "Running without Vulkan validation layers");
    }

    const bool enableDebugLayers = cfgDebugLayers.ToBool(false);
#else
    const bool enableDebugLayers = false;
#endif

    m_instance = new VulkanInstance;
    CheckResultOrReturn(m_instance->Initialize(enableDebugLayers));

    m_renderConfig->Initialize(this);

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

        commandBuffer = MakeHandle<VulkanCommandBuffer>();
        frame = MakeHandle<VulkanFrame>(frameIndex);

        CheckResultOrReturn(commandBuffer->Create(pool));
        CheckResultOrReturn(frame->Create());
    }

    m_gpuTimerBackend = new VulkanGpuTimerBackend;
    if (!m_gpuTimerBackend->Initialize(m_instance->GetDevice()))
    {
        HYP_LOG(RenderingBackend, Info, "GPU timestamp queries not supported on this device");
    }

    dynamicFunctions.Load(m_instance->GetDevice());

    const VulkanFeatures& deviceFeatures = m_instance->GetDevice()->GetFeatures();
    const VkPhysicalDeviceProperties& physicalDeviceProperties = deviceFeatures.GetPhysicalDeviceProperties();

    HYP_LOG(RenderingBackend, Info, "Selected Vulkan physical device: {}", physicalDeviceProperties.deviceName);
    HYP_LOG(RenderingBackend, Info, "Vulkan feature support:\n\tBindless Textures? {}\n\tRay Tracing? {}\n\tDynamic Descriptor Indexing? {}",
            deviceFeatures.SupportsBindlessTextures(),
            deviceFeatures.IsRayTracingSupported(),
            deviceFeatures.SupportsDynamicDescriptorIndexing());

    m_descriptorSetManager->Initialize(m_instance->GetDevice());

    const VkDeviceSize minUniformBufferOffsetAlignment = RI.GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits.minUniformBufferOffsetAlignment;

    CheckResultOrReturn(RenderInterface::Initialize());

    cbufferAllocator->Initialize(minUniformBufferOffsetAlignment);

    return {};
}

void VulkanRenderInterface::Shutdown()
{
    CheckResult(m_instance->GetDevice()->WaitIdle());

    const uint32 frameCounter = GetFrameCounter();

    for (VulkanFrameRef& frame : m_frames)
    {
        if (!frame)
        {
            continue;
        }

        VulkanFence* fence = frame->GetFence();

        if (fence && fence->isSubmitted && !fence->CheckStatus())
        {
            fence->Wait();
        }

        frame.Reset();
    }

    m_frames.Clear();
    m_commandBuffers.Clear();

    { // Flush transient submits
        commandRecorderAllocator.Flush(true);

        auto& fences = m_transientCommandBufferFences[frameCounter % NumFramesInFlight];
        for (auto it = fences.Begin(); it != fences.End(); ++it)
        {
            VulkanFence& fence = *it;

            if (fence.isSubmitted)
            {
                fence.Wait(true);
                fence.Reset();
            }
        }

        fences.Clear();

        m_recycledTransientCommandBufferFences.Clear();
        m_recycledTransientCommandBufferSemaphores.Clear();

        for (uint32 frameIndex = 0; frameIndex < NumFramesInFlight; frameIndex++)
        {
            m_transientCommandBufferFences[frameIndex].Clear();
            m_transientCommandBufferSemaphores[frameIndex].Clear();

            for (uint32 threadIndex = 0; threadIndex < NumRendererWorkerThreads + 1; threadIndex++)
            {
                m_transientCommandBuffers[threadIndex][frameIndex].Clear();
                m_pendingTransientCommandBuffers[threadIndex][frameIndex].Clear();
            }
        }
    }

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

    m_gpuTimerBackend->Shutdown();

    delete m_gpuTimerBackend;
    m_gpuTimerBackend = nullptr;

    RenderInterface::Shutdown();

    DeletionQueue::GetInstance().Shutdown();

    m_descriptorSetManager->Shutdown(m_instance->GetDevice());

    delete m_descriptorSetManager;
    m_descriptorSetManager = nullptr;

    delete m_instance;
    m_instance = nullptr;
}

void VulkanRenderInterface::BeginFrame(AtomicFlag* pCancelFlag)
{
    RenderInterface::BeginFrame(pCancelFlag);
}

void VulkanRenderInterface::EndFrame()
{
    RenderInterface::EndFrame();
}

VulkanFrame* VulkanRenderInterface::GetCurrentFrame() const
{
    if (m_frames.Empty())
        return nullptr;

    return m_frames[m_currentFrameIndex];
}

void VulkanRenderInterface::PrepareFrame(VulkanFrame* frame)
{
    const uint32 frameCounter = GetFrameCounter();

    {
        ENGINE_STAT_SCOPE(&s_statVulkanFrameSync);
        ENGINE_STAT_SCOPE(&g_statTotalStallTime);

        if (frame->IsUsingTimelineSemaphore())
        {
            VulkanSemaphoreRef timelineSemaphore = frame->GetFrameCompleteSemaphore();
            const uint64 waitValue = frame->GetFrameCompleteValue();

            if (waitValue > 0)
            {
                const uint64 currentValue = timelineSemaphore->GetCounterValue();

                if (currentValue < waitValue)
                {
                    timelineSemaphore->WaitForValue(waitValue, UINT64_MAX);
                }
            }
        }
        else
        {
            frame->GetFence()->Wait(true);
        }
    }

    // Read back GPU timestamps from the completed frame
    ResolveGpuFrameResults(frameCounter % NumFramesInFlight);

    // call frame callbacks after GPU sync is complete
    if (frame->OnFrameEnd.AnyBound())
    {
        frame->OnFrameEnd(frame);
        frame->OnFrameEnd.RemoveAllDetached();
    }

    for (auto it = m_submittedAsyncComputes.Begin(); it != m_submittedAsyncComputes.End();)
    {
        VulkanAsyncCompute* elem = *it;

        if (elem->CheckStatus())
        {
            elem->OnCompleted();
            elem->OnCompleted.RemoveAllDetached();

            AssertDebug(!elem->OnCompleted.AnyBound());

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

    auto& fences = m_transientCommandBufferFences[frameCounter % NumFramesInFlight];
    for (auto it = fences.Begin(); it != fences.End();)
    {
        VulkanFence& fence = *it;

        if (fence.isSubmitted)
        {
            ENGINE_STAT_SCOPE(&s_statVulkanFrameSync);
            ENGINE_STAT_SCOPE(&g_statTotalStallTime);

            fence.Wait(true);

            fence.Reset();
        }

        m_recycledTransientCommandBufferFences.PushBack(std::move(fence));

        it = fences.Erase(it);
    }

    // Semaphores are destroyed when cleared (destructor enqueues vkDestroySemaphore).
    // No recycling needed: each transient submit creates a fresh semaphore.
    m_transientCommandBufferSemaphores[frameCounter % NumFramesInFlight].Clear();

    for (uint32 threadIndex = 0; threadIndex < NumRendererWorkerThreads + 1; threadIndex++)
    {
        // reset our transient command buffers
        List<VulkanCommandBuffer, VulkanAllocator>& freeList = m_transientCommandBuffers[threadIndex][frameCounter % NumFramesInFlight];
        List<VulkanCommandBuffer, VulkanAllocator>& pendingList = m_pendingTransientCommandBuffers[threadIndex][frameCounter % NumFramesInFlight];

        for (auto it = pendingList.Begin(); it != pendingList.End();)
        {
            VulkanCommandBuffer& commandBuffer = *it;
            Assert(!commandBuffer.IsRecording());

            freeList.EmplaceBack(std::move(*it));

            it = pendingList.Erase(it);
        }
    }

    frame->OnFrameStart();

    m_descriptorSetManager->OnFrameStart();
}

VulkanSwapchainRef VulkanRenderInterface::CreateSwapchain(ApplicationWindow* window, const Vec2u& extent)
{
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

void VulkanRenderInterface::PresentToSwapchain(VulkanSwapchain* swapchain)
{
    VulkanDeviceQueue* presentQueue = m_instance->GetDevice()->GetPresentQueue();

    if (swapchain != nullptr)
    {
        AssertDebug(presentQueue != nullptr); // should never be null when presenting, not used in headless mode
    }
    else
    {
        if (!presentQueue)
        {
            VulkanDeviceQueue* graphicsQueue = m_instance->GetDevice()->GetGraphicsQueue();
            Assert(graphicsQueue != nullptr);

            presentQueue = graphicsQueue;
        }
    }

    VulkanCommandBuffer* commandBuffer = GetCurrentCommandBuffer();
    AssertDebug(!commandBuffer->IsRecording());

    VulkanFrame* frame = GetCurrentFrame();

    VulkanSemaphore* waitSemaphore = nullptr;
    VulkanSemaphore* signalSemaphore = nullptr;

    if (swapchain != nullptr)
    {
        waitSemaphore = frame->GetImageAvailableSemaphore(swapchain, /* createIfNotExist */ true);
        signalSemaphore = swapchain->GetCurrentPresentSemaphore();

        AssertDebug(waitSemaphore != nullptr && signalSemaphore != nullptr);
    }

    const bool useTimeline = frame->IsUsingTimelineSemaphore();
    VulkanFence* submitFence = useTimeline ? nullptr : frame->GetFence();

    if (useTimeline && frame->GetFrameCompleteValue() > 0)
    {
        VulkanSemaphore* signalSemaphores[2] = { nullptr, nullptr };
        uint64 signalValues[2] = { 0, 0 };
        uint32 signalCount = 0;

        if (signalSemaphore != nullptr)
        {
            signalSemaphores[signalCount] = signalSemaphore;
            signalValues[signalCount] = 0;
            signalCount++;
        }

        signalSemaphores[signalCount] = frame->GetFrameCompleteSemaphore().Get();
        signalValues[signalCount] = frame->GetFrameCompleteValue();
        signalCount++;

        commandBuffer->Submit(
            presentQueue,
            submitFence,
            Span<VulkanSemaphore*>(&waitSemaphore, waitSemaphore ? 1 : 0),
            Span<VulkanSemaphore*>(signalSemaphores, signalCount),
            nullptr,
            signalValues);
    }
    else
    {
        commandBuffer->Submit(
            presentQueue,
            submitFence,
            Span<VulkanSemaphore*>(&waitSemaphore, waitSemaphore ? 1 : 0),
            Span<VulkanSemaphore*>(&signalSemaphore, signalSemaphore ? 1 : 0));
    }

    if (swapchain != nullptr)
    {
        swapchain->PresentFrame(frame, presentQueue);
    }
}

VulkanCommandBuffer* VulkanRenderInterface::GetCurrentCommandBuffer() const
{
    return m_commandBuffers[m_currentFrameIndex];
}

VulkanCommandBuffer& VulkanRenderInterface::GetTransientCommandBuffer()
{
    // usable from main render thread or renderer worker threads.
    AssertOnThread(g_renderThread | ThreadCategory::THREAD_CATEGORY_TASK);

    const uint32 frameCounter = GetFrameCounter();
    const uint32 renderThreadIndex = CurrentRenderThreadIndex();

    List<VulkanCommandBuffer, VulkanAllocator>& freeList = m_transientCommandBuffers[renderThreadIndex][frameCounter % NumFramesInFlight];
    List<VulkanCommandBuffer, VulkanAllocator>& pendingList = m_pendingTransientCommandBuffers[renderThreadIndex][frameCounter % NumFramesInFlight];

    VulkanDeviceQueue* graphicsQueue = m_instance->GetDevice()->GetGraphicsQueue();
    Assert(graphicsQueue != nullptr);

    VkCommandPool pool = graphicsQueue->commandPools[renderThreadIndex];
    Assert(pool != VK_NULL_HANDLE);

    if (freeList.Empty())
    {
        VulkanCommandBuffer& commandBuffer = pendingList.EmplaceBack();
        CheckResult(commandBuffer.Create(pool));

        commandBuffer.Begin();

        return commandBuffer;
    }

    VulkanCommandBuffer& commandBuffer = pendingList.PushBack(freeList.PopFront());
    commandBuffer.Begin();

    return commandBuffer;
}

void VulkanRenderInterface::SubmitTransientCommandBuffer(VulkanCommandBuffer& commandBuffer)
{
    const uint32 frameCounter = GetFrameCounter();
    const uint32 frameIndex = frameCounter % NumFramesInFlight;

    if (commandBuffer.IsRecording())
    {
        commandBuffer.End();
    }

    VulkanDeviceQueue* graphicsQueue = m_instance->GetDevice()->GetGraphicsQueue();
    Assert(graphicsQueue != nullptr);

    // Previous transient command buffer semaphore, if applicable.
    VulkanSemaphore* pWaitSemaphore = nullptr;
    VulkanSemaphore* pSignalSemaphore = nullptr;

    {
        Mutex::Guard guard(m_transientCommandBuffersMutex);

        if (m_transientCommandBufferSemaphores[frameIndex].Any())
        {
            pWaitSemaphore = &m_transientCommandBufferSemaphores[frameIndex].Back();
        }

        // Add signal semaphore
        VulkanSemaphore& signalSemaphore = m_transientCommandBufferSemaphores[frameIndex].EmplaceBack();
        pSignalSemaphore = &signalSemaphore;

        CheckResult(signalSemaphore.Create());

        VulkanFence& fence = m_transientCommandBufferFences[frameIndex].EmplaceBack();

        if (m_recycledTransientCommandBufferFences.Any())
        {
            fence = m_recycledTransientCommandBufferFences.PopFront();
        }
        else
        {
            fence.Create();
        }

        Span<VulkanSemaphore*> waitSemaphoreSpan {};
        if (pWaitSemaphore != nullptr)
        {
            waitSemaphoreSpan = { &pWaitSemaphore, 1 };
        }

        commandBuffer.Submit(
            graphicsQueue,
            &fence,
            waitSemaphoreSpan,
            Span<VulkanSemaphore*> { &pSignalSemaphore, 1 });
    }
}

VulkanDescriptorSetRef VulkanRenderInterface::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    DescriptorSetLayout newLayout { layout.GetDeclaration() };
    newLayout.SetIsTemplate(false);
    newLayout.SetIsReference(false);

    VulkanDescriptorSetRef descriptorSet = MakeHandle<VulkanDescriptorSet>(newLayout);
#ifdef HYP_RHI_DEBUG_NAMES
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
    const FramebufferDesc& framebufferDesc,
    const RenderableAttributeSet& attributes)
{
    VulkanGraphicsPipelineRef pipeline = MakeHandle<VulkanGraphicsPipeline>();

    if (shaderInstance.IsValid())
    {
        pipeline->SetShader(shaderInstance);
#ifdef HYP_RHI_DEBUG_NAMES
        pipeline->SetDebugName(NAME_FMT("GraphicsPipeline_{}", shaderInstance->GetDebugName().IsValid() ? *shaderInstance->GetDebugName() : "<unnamed shader>"));
#endif
    }

    pipeline->SetFramebufferDesc(framebufferDesc);

    pipeline->SetInputLayout(attributes.GetMeshAttributes().inputLayout);
    pipeline->SetTopology(attributes.GetMeshAttributes().topology);
    pipeline->SetCullMode(attributes.GetMaterialAttributes().cullFaces);
    pipeline->SetFillMode(attributes.GetMaterialAttributes().fillMode);
    pipeline->SetBlendFunction(attributes.GetMaterialAttributes().blendFunction);
    pipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
    pipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
    pipeline->SetDepthCompareOp(attributes.GetMaterialAttributes().depthCompareOp);
    pipeline->SetDepthClamp(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_CLAMP));

    if (attributes.GetMaterialAttributes().flags & MAF_DEPTH_BIAS)
    {
        pipeline->SetDepthBias(attributes.GetMaterialAttributes().depthBias);
        pipeline->SetDepthBiasSlope(attributes.GetMaterialAttributes().depthBiasSlope);
    }

    if (attributes.GetMaterialAttributes().flags & MAF_STENCIL_TEST)
    {
        pipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencilFunction);
    }

    // for materials that write a stencil reference value
    if (attributes.GetMaterialAttributes().stencilReference != 0)
    {
        pipeline->SetStencilWrite(true);
    }

    // // sanity check: newly created pipeline must match or caching will fail.
    // AssertDebug(pipeline->MatchesSignature(attributes, framebufferDesc));

    return pipeline;
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
#ifdef HYP_RHI_DEBUG_NAMES
    ref->SetDebugName(NAME_FMT("{}_IV", image->GetDebugName()));
#endif

    return ref;
}

VulkanGpuImageViewRef VulkanRenderInterface::MakeImageView(
    const VulkanGpuImageRef& image,
    uint8 mipIndex,
    uint8 numMips,
    uint16 layerIndex,
    uint16 numLayers,
    TextureType viewType)
{
    ImageSubResource subResource {};
    subResource.baseMipLevel = mipIndex;
    subResource.baseArrayLayer = layerIndex;
    subResource.numLevels = numMips;
    subResource.numLayers = numLayers;

    VulkanGpuImageViewRef ref = MakeHandle<VulkanGpuImageView>(image, subResource, viewType);
#ifdef HYP_RHI_DEBUG_NAMES
    ref->SetDebugName(NAME_FMT("{}_IV", image->GetDebugName()));
#endif

    return ref;
}

VulkanSamplerRef VulkanRenderInterface::MakeSampler(const SamplerDesc& samplerDesc)
{
    return MakeHandle<VulkanSampler>(samplerDesc);
}

VulkanFramebufferRef VulkanRenderInterface::MakeFramebuffer(const FramebufferDesc& framebufferDesc)
{
    return MakeHandle<VulkanFramebuffer>(framebufferDesc);
}

VulkanFrameRef VulkanRenderInterface::MakeFrame(uint32 frameIndex)
{
    return MakeHandle<VulkanFrame>(frameIndex);
}

VulkanShaderInstanceRef VulkanRenderInterface::MakeShader(const Shader* shader)
{
    return MakeHandle<VulkanShaderInstance>(shader);
}

VulkanBottomLevelASRef VulkanRenderInterface::MakeBottomLevelAS(
    const VulkanGpuBufferRef& packedVerticesBuffer,
    const VulkanGpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Mat4f& transform)
{
    return MakeHandle<VulkanBottomLevelAS>(
        VulkanGpuBufferRef(packedVerticesBuffer),
        VulkanGpuBufferRef(packedIndicesBuffer),
        numVertices,
        numIndices,
        material,
        transform);
}

VulkanTopLevelASRef VulkanRenderInterface::MakeTLAS()
{
    ASResourceCallbacks callbacks {};

    callbacks.setBLASBuffers = [](uint64 key, VulkanGpuBuffer* vb, VulkanGpuBuffer* ib) -> uint32
    {
        Assert(vb != nullptr && ib != nullptr);

        if (!RI.bindlessStorage)
        {
            return BLASCache::InvalidStorageId;
        }

        uint32 storageId = RI.blasCache->AllocateStorageId(key);

        if (storageId == BLASCache::InvalidStorageId)
        {
            return storageId;
        }

        RI.bindlessStorage->AddResource(BindlessStorage_Buffers, storageId * 2 + 0, MakeStrongRef(vb));
        RI.bindlessStorage->AddResource(BindlessStorage_Buffers, storageId * 2 + 1, MakeStrongRef(ib));

        return storageId;
    };

    callbacks.removeBLASBuffers = [](uint64 key) -> bool
    {
        if (!RI.bindlessStorage)
        {
            return false;
        }

        uint32 storageId;
        uint32 refCount;

        if (RI.blasCache->ReleaseStorageIdForBLASKey(key, storageId, refCount))
        {
            if (refCount == 0)
            {
                RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2 + 0);
                RI.bindlessStorage->RemoveResource(BindlessStorage_Buffers, storageId * 2 + 1);
            }
        }

        return true;
    };

    return MakeHandle<VulkanTopLevelAS>(callbacks);
}

void VulkanRenderInterface::PopulateIndirectDrawCommandsBuffer(
    const VulkanGpuBuffer* vertexBuffer,
    const VulkanGpuBuffer* indexBuffer,
    uint32 instanceOffset,
    Array<VkDrawIndexedIndirectCommand, VulkanAllocator>& outBuffer)
{
    const size_t requiredSize = (size_t(instanceOffset) + 1);

    if (outBuffer.Size() < requiredSize)
    {
        outBuffer.ResizeUninitialized(requiredSize);
    }

    uint32 numIndices = 0;

    if (indexBuffer != nullptr)
    {
        numIndices = indexBuffer->Size() / sizeof(uint32);
    }

    VkDrawIndexedIndirectCommand& command = outBuffer[instanceOffset];
    command = VkDrawIndexedIndirectCommand {};
    command.indexCount = numIndices;
    command.vertexOffset = 0;
    command.firstInstance = 0;
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

void VulkanRenderInterface::RecordStartTimestamp(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer)
{
    if (m_gpuTimerBackend)
    {
        m_gpuTimerBackend->WriteStartTimestamp(cmd, timer);
    }
}

void VulkanRenderInterface::RecordStopTimestamp(VulkanCommandBuffer* cmd, EngineStatGpuTimer* timer)
{
    if (m_gpuTimerBackend)
    {
        m_gpuTimerBackend->WriteStopTimestamp(cmd, timer);
    }
}

void VulkanRenderInterface::ResolveGpuFrameResults(uint32 completedFrameIndex)
{
    if (m_gpuTimerBackend)
    {
        m_gpuTimerBackend->ResolveFrameResults(completedFrameIndex);
    }
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
        win32Window = DynamicCast<Win32ApplicationWindow>(window);
        Assert(win32Window != nullptr);
    }

    return Win32AppContext::CreateVulkanSurface(win32Window, ppOutDummySurfaceContext);
#elif HYP_MACOS
    CocoaApplicationWindow* cocoaWindow = nullptr;
    if (window != nullptr)
    {
        cocoaWindow = DynamicCast<CocoaApplicationWindow>(window);
        Assert(cocoaWindow != nullptr);
    }

    return CocoaAppContext::CreateVulkanSurface(cocoaWindow, ppOutDummySurfaceContext);
#elif HYP_IOS
    iOSApplicationWindow* iosWindow = nullptr;
    if (window != nullptr)
    {
        iosWindow = DynamicCast<iOSApplicationWindow>(window);
        Assert(iosWindow != nullptr);
    }

    return iOSAppContext::CreateVulkanSurface(iosWindow, ppOutDummySurfaceContext);
#elif HYP_ANDROID
    if (!window)
    {
        window = g_appContext->GetMainWindow();
    }

    AndroidApplicationWindow* androidWindow = DynamicCast<AndroidApplicationWindow>(window);
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
    if (const CocoaAppContext* cocoaAppContext = DynamicCast<CocoaAppContext>(g_appContext))
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

#if HYP_WINDOWS
    if (g_appContext->IsA(Win32AppContext::StaticClass()))
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
    if (const AndroidAppContext* androidAppContext = DynamicCast<AndroidAppContext>(g_appContext))
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

#if HYP_IOS
    if (g_appContext->IsA(iOSAppContext::StaticClass()))
    {
        static constexpr const char* RequiredExtensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_EXT_METAL_SURFACE_EXTENSION_NAME
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

void VulkanRenderInterface::InitDeviceDetails(DeviceDetails& deviceDetails)
{
    const VulkanFeatures& features = m_instance->GetDevice()->GetFeatures();
    uint32 deviceId = features.GetDeviceId();
    uint32 vendorId = deviceId >> 24;
    uint32 deviceIdLower = deviceId & 0xFFFF;

    GpuInfo info;
    info.gpuType = features.IsDiscreteGpu() ? GpuType::Dedicated : GpuType::Integrated;
    info.vendorId = vendorId;
    info.deviceId = deviceIdLower;
    info.gpuModel = String(features.GetDeviceName());
    info.isDiscrete = features.IsDiscreteGpu();
    info.supportsRayTracing = features.IsRayTracingSupported();
    info.supportsRayQueries = features.SupportsRayQueries();
    info.supportsBindless = features.SupportsBindlessTextures();

    deviceDetails.Set(info);
}

} // namespace Hyperion

#undef CHECK_FRAME_RESULT
