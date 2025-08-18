/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanSwapchain.hpp>
#include <rendering/vulkan/VulkanFrame.hpp>
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#include <rendering/vulkan/VulkanComputePipeline.hpp>
#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanInstance.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanShader.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanAsyncCompute.hpp>
#include <rendering/vulkan/rt/VulkanRaytracingPipeline.hpp>
#include <rendering/vulkan/rt/VulkanAccelerationStructure.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <rendering/RenderableAttributes.hpp>

#include <core/containers/SparsePagedArray.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <rendering/Texture.hpp>

#include <system/AppContext.hpp>

#include <engine/EngineGlobals.hpp>

#include <vulkan/vulkan.h>

#ifdef HYP_WINDOWS
#include <vulkan/vulkan_win32.h>
#endif

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

namespace hyperion {

static constexpr bool g_useDebugLayers = true;

#pragma region VulkanRenderConfig

class VulkanRenderConfig final : public IRenderConfig
{
public:
    VulkanRenderConfig(VulkanRenderBackend* renderingApi)
        : m_renderBackend(renderingApi)
    {
    }

    virtual ~VulkanRenderConfig() override = default;

    virtual bool ShouldCollectUniqueDrawCallPerMaterial() const override
    {
        return true;
    }

    virtual bool IsBindlessSupported() const override
    {
        return m_renderBackend->GetDevice()->GetFeatures().SupportsBindlessTextures();
    }

    virtual bool IsRaytracingSupported() const override
    {
        return m_renderBackend->GetDevice()->GetFeatures().IsRaytracingSupported();
    }

    virtual bool IsIndirectRenderingEnabled() const override
    {
        return true;
    }

    virtual bool IsParallelRenderingEnabled() const override
    {
        return true;
    }

    virtual bool IsDynamicDescriptorIndexingSupported() const override
    {
        return false; // m_renderBackend->GetDevice()->GetFeatures().SupportsDynamicDescriptorIndexing();
    }

private:
    VulkanRenderBackend* m_renderBackend;
};

#pragma endregion VulkanRenderConfig

#pragma region Vulkan struct wrappers

class VulkanDescriptorSetLayoutWrapper final
{
    VkDescriptorSetLayout m_handle;
    VulkanDevice* m_device;

public:
    VulkanDescriptorSetLayoutWrapper(VulkanDevice* device)
        : m_handle(VK_NULL_HANDLE),
          m_device(device)
    {
    }

    ~VulkanDescriptorSetLayoutWrapper()
    {
        if (m_handle != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(
                m_device->GetDevice(),
                m_handle,
                nullptr);

            m_handle = VK_NULL_HANDLE;
        }
    }

    HYP_FORCE_INLINE VkDescriptorSetLayout GetVulkanHandle() const
    {
        return m_handle;
    }

    RendererResult Create(VulkanDevice* device, const DescriptorSetLayout& layout)
    {
        HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE);

        static constexpr VkDescriptorBindingFlags bindlessFlags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

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
                descriptorCount = g_maxBindlessResources;
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
                flags |= bindlessFlags;
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

        VULKAN_CHECK(vkCreateDescriptorSetLayout(
            device->GetDevice(),
            &layoutInfo,
            nullptr,
            &m_handle));

        return RendererResult {};
    }
};

#pragma endregion Vulkan struct wrappers

#pragma region VulkanDynamicFunctions

HYP_API VulkanDynamicFunctions* g_vulkanDynamicFunctions = nullptr;

void VulkanDynamicFunctions::Load(VulkanDevice* device)
{
    static VulkanDynamicFunctions instance;
    g_vulkanDynamicFunctions = &instance;

#define HYP_LOAD_FN(function) instance.function = reinterpret_cast<PFN_##function>(vkGetDeviceProcAddr(device->GetDevice(), #function))

#if defined(HYP_FEATURES_ENABLE_RAYTRACING) && defined(HYP_FEATURES_BINDLESS_TEXTURES)
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

    RendererResult Create(VulkanDevice* device);
    RendererResult Destroy(VulkanDevice* device);

    RendererResult CreateDescriptorSet(VulkanDevice* device, const RC<VulkanDescriptorSetLayoutWrapper>& layout, VkDescriptorSet& outVkDescriptorSet);
    RendererResult DestroyDescriptorSet(VulkanDevice* device, VkDescriptorSet vkDescriptorSet);

    RC<VulkanDescriptorSetLayoutWrapper> GetOrCreateVkDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayout& layout);

private:
    Mutex m_mutex;
    HashMap<HashCode, Weak<VulkanDescriptorSetLayoutWrapper>> m_vkDescriptorSetLayouts;

    VkDescriptorPool m_vkDescriptorPool;
};

VulkanDescriptorSetManager::VulkanDescriptorSetManager()
    : m_vkDescriptorPool(VK_NULL_HANDLE)
{
}

VulkanDescriptorSetManager::~VulkanDescriptorSetManager() = default;

RendererResult VulkanDescriptorSetManager::Create(VulkanDevice* device)
{
    static const Array<VkDescriptorPoolSize> poolSizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_SAMPLER, 256 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 8 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 32000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 64000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 64000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 32000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 32000 }
    };

    HYP_GFX_ASSERT(m_vkDescriptorPool == VK_NULL_HANDLE);

    VkDescriptorPoolCreateInfo poolInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    poolInfo.maxSets = maxDescriptorSets;
    poolInfo.poolSizeCount = uint32(poolSizes.Size());
    poolInfo.pPoolSizes = poolSizes.Data();

    VULKAN_CHECK(vkCreateDescriptorPool(
        device->GetDevice(),
        &poolInfo,
        nullptr,
        &m_vkDescriptorPool));

    return RendererResult {};
}

RendererResult VulkanDescriptorSetManager::Destroy(VulkanDevice* device)
{
    RendererResult result = RendererResult {};

    m_vkDescriptorSetLayouts.Clear();

    if (m_vkDescriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(
            device->GetDevice(),
            m_vkDescriptorPool,
            nullptr);

        m_vkDescriptorPool = VK_NULL_HANDLE;
    }

    return result;
}

RendererResult VulkanDescriptorSetManager::CreateDescriptorSet(VulkanDevice* device, const RC<VulkanDescriptorSetLayoutWrapper>& layout, VkDescriptorSet& outVkDescriptorSet)
{
    HYP_GFX_ASSERT(m_vkDescriptorPool != VK_NULL_HANDLE);

    HYP_GFX_ASSERT(layout != nullptr);
    HYP_GFX_ASSERT(layout->GetVulkanHandle() != VK_NULL_HANDLE);

    VkDescriptorSetLayout layouts[] = { layout->GetVulkanHandle() };

    VkDescriptorSetAllocateInfo allocInfo { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    allocInfo.descriptorPool = m_vkDescriptorPool;
    allocInfo.descriptorSetCount = ArraySize(layouts);
    allocInfo.pSetLayouts = &layouts[0];

    const VkResult vkResult = vkAllocateDescriptorSets(
        device->GetDevice(),
        &allocInfo,
        &outVkDescriptorSet);

    if (vkResult != VK_SUCCESS)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to allocate descriptor set", int(vkResult));
    }

    return RendererResult {};
}

RendererResult VulkanDescriptorSetManager::DestroyDescriptorSet(VulkanDevice* device, VkDescriptorSet vkDescriptorSet)
{
    HYP_GFX_ASSERT(m_vkDescriptorPool != VK_NULL_HANDLE);

    HYP_GFX_ASSERT(vkDescriptorSet != VK_NULL_HANDLE);

    vkFreeDescriptorSets(
        device->GetDevice(),
        m_vkDescriptorPool,
        1,
        &vkDescriptorSet);

    return RendererResult {};
}

RC<VulkanDescriptorSetLayoutWrapper> VulkanDescriptorSetManager::GetOrCreateVkDescriptorSetLayout(VulkanDevice* device, const DescriptorSetLayout& layout)
{
    const HashCode hashCode = layout.GetHashCode();

    RC<VulkanDescriptorSetLayoutWrapper> vkDescriptorSetLayout;

    Mutex::Guard guard(m_mutex);

    auto it = m_vkDescriptorSetLayouts.Find(hashCode);

    if (it != m_vkDescriptorSetLayouts.End())
    {
        vkDescriptorSetLayout = it->second.Lock();
    }

    if (vkDescriptorSetLayout != nullptr)
    {
        return vkDescriptorSetLayout;
    }

    vkDescriptorSetLayout = MakeRefCountedPtr<VulkanDescriptorSetLayoutWrapper>(device);

    HYP_GFX_ASSERT(vkDescriptorSetLayout->Create(device, layout));

    m_vkDescriptorSetLayouts.Set(hashCode, vkDescriptorSetLayout);

    return vkDescriptorSetLayout;
}

#pragma endregion VulkanDescriptorSetManager

#pragma region VulkanTextureCache

class VulkanTextureCache
{
public:
    // map texture ID -> image views
    SparsePagedArray<HashMap<ImageSubResource, GpuImageViewRef>, 1024> imageViews;
    // to keep texture IDs as valid
    SparsePagedArray<WeakHandle<Texture>, 1024> weakTextureHandles;

    typename decltype(weakTextureHandles)::Iterator cleanupIterator;

    VulkanTextureCache()
    {
        cleanupIterator = weakTextureHandles.End();
    }

    const GpuImageViewRef& GetOrCreate(const Handle<Texture>& texture, const ImageSubResource& subResource)
    {
        Threads::AssertOnThread(g_renderThread);

        HYP_GFX_ASSERT(texture.IsValid());

        const SizeType idx = texture.Id().ToIndex();

        if (!imageViews.HasIndex(idx))
        {
            imageViews.Emplace(idx);
            weakTextureHandles.Emplace(idx, texture.ToWeak());
        }

        auto& textureImageViews = imageViews.Get(idx);

        auto it = textureImageViews.Find(subResource);

        if (it == textureImageViews.End())
        {

            VulkanGpuImageViewRef imageView = CreateObject<VulkanGpuImageView>(
                VulkanGpuImageRef(texture->GetGpuImage()),
                subResource.baseMipLevel,
                subResource.numLevels,
                subResource.baseArrayLayer,
                subResource.numLayers);

            HYP_GFX_ASSERT(imageView->Create());

            it = textureImageViews.Set(subResource, imageView).first;
        }

        HYP_GFX_ASSERT(it->second.IsValid());

        return it->second;
    }

    void RemoveTexture(const Handle<Texture>& texture)
    {
        Threads::AssertOnThread(g_renderThread);

        if (!texture.IsValid())
        {
            return;
        }

        const SizeType idx = texture.Id().ToIndex();

        if (imageViews.HasIndex(idx))
        {
            for (auto& it : imageViews.Get(idx))
            {
                SafeDelete(std::move(it.second));
            }

            imageViews.EraseAt(idx);
            weakTextureHandles.EraseAt(idx);
        }
    }

    void CleanupUnusedTextures()
    {
        Threads::AssertOnThread(g_renderThread);

        constexpr uint32 maxCycles = 32;

        cleanupIterator = typename decltype(weakTextureHandles)::Iterator {
            &weakTextureHandles,
            cleanupIterator.page,
            cleanupIterator.elem
        };

        if (cleanupIterator == weakTextureHandles.End())
        {
            cleanupIterator = weakTextureHandles.Begin();
        }

        uint32 numRemoved = 0;

        for (uint32 i = 0; cleanupIterator != weakTextureHandles.End() && i < maxCycles; i++)
        {
            auto& entry = *cleanupIterator;

            if (!entry.Lock())
            {
                const SizeType idx = weakTextureHandles.IndexOf(cleanupIterator);

                HYP_GFX_ASSERT(imageViews.HasIndex(idx));
                HYP_GFX_ASSERT(weakTextureHandles.HasIndex(idx));

                for (auto& it : imageViews.Get(idx))
                {
                    SafeDelete(std::move(it.second));
                }

                imageViews.EraseAt(idx);

                cleanupIterator = weakTextureHandles.Erase(cleanupIterator);

                ++numRemoved;

                continue;
            }

            ++cleanupIterator;
        }

        if (numRemoved != 0)
        {
            HYP_LOG(RenderingBackend, Debug, "VulkanTextureCache: Cleaned up {} unused textures", numRemoved);
        }
    }
};

#pragma endregion VulkanTextureCache

#pragma region VulkanRenderBackend

VulkanRenderBackend::VulkanRenderBackend()
    : m_instance(nullptr),
      m_renderConfig(MakePimpl<VulkanRenderConfig>(this)),
      m_descriptorSetManager(MakePimpl<VulkanDescriptorSetManager>()),
      m_textureCache(MakePimpl<VulkanTextureCache>()),
      m_asyncCompute(MakePimpl<VulkanAsyncCompute>()),
      m_shouldRecreateSwapchain(false)
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

SwapchainBase* VulkanRenderBackend::GetSwapchain() const
{
    return m_instance->GetSwapchain();
}

AsyncComputeBase* VulkanRenderBackend::GetAsyncCompute() const
{
    return m_asyncCompute.Get();
}

RendererResult VulkanRenderBackend::Initialize()
{
    m_instance = new VulkanInstance();
    HYP_GFX_CHECK(m_instance->Initialize(g_useDebugLayers));

    VulkanDynamicFunctions::Load(m_instance->GetDevice());

    m_crashHandler.Initialize();

    HYP_GFX_CHECK(m_descriptorSetManager->Create(m_instance->GetDevice()));
    HYP_GFX_CHECK(m_asyncCompute->Create());

    m_defaultFormats.Set(
        DIF_COLOR,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_RGBA8, TF_RGBA16F } },
            IS_SRV));

    m_defaultFormats.Set(
        DIF_DEPTH,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_DEPTH_24, TF_DEPTH_16, TF_DEPTH_32F } },
            IS_DEPTH));

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
    m_descriptorSetManager->Destroy(m_instance->GetDevice());

    m_asyncCompute.Reset();

    HYP_GFX_CHECK(m_instance->GetDevice()->Wait());

    delete m_instance;
    m_instance = nullptr;

    return {};
}

FrameBase* VulkanRenderBackend::GetCurrentFrame() const
{
    return m_instance->GetSwapchain()->GetCurrentFrame().Get();
}

FrameBase* VulkanRenderBackend::PrepareNextFrame()
{
    CHECK_FRAME_RESULT(m_instance->GetSwapchain()->PrepareFrame(m_shouldRecreateSwapchain));

    VulkanFrame* frame = m_instance->GetSwapchain()->GetCurrentFrame();

    if (m_shouldRecreateSwapchain)
    {
        m_shouldRecreateSwapchain = false;

        CHECK_FRAME_RESULT(m_instance->GetDevice()->Wait());
        CHECK_FRAME_RESULT(m_instance->RecreateSwapchain());

        CHECK_FRAME_RESULT(m_instance->GetSwapchain()->GetCurrentFrame()->RecreateFence());

        // Need to prepare frame again now that swapchain has been recreated.
        CHECK_FRAME_RESULT(m_instance->GetSwapchain()->PrepareFrame(m_shouldRecreateSwapchain));

        frame = m_instance->GetSwapchain()->GetCurrentFrame();

        OnSwapchainRecreated(m_instance->GetSwapchain());
    }

    AssertDebug(frame != nullptr);

    if (m_asyncCompute->IsSupported())
    {
        CHECK_FRAME_RESULT(m_asyncCompute->PrepareForFrame(frame));
    }

    return frame;
}

void VulkanRenderBackend::PresentFrame(FrameBase* frame)
{
    const CommandBufferRef& commandBuffer = m_instance->GetSwapchain()->GetCurrentCommandBuffer();

    VulkanFrame* vulkanFrame = VULKAN_CAST(frame);
    VulkanCommandBuffer* vulkanCommandBuffer = VULKAN_CAST(commandBuffer.Get());
    VulkanAsyncCompute* vulkanAsyncCompute = m_asyncCompute.Get();

    CHECK_FRAME_RESULT(vulkanFrame->Submit(&m_instance->GetDevice()->GetGraphicsQueue(), vulkanCommandBuffer));

    if (vulkanAsyncCompute->IsSupported())
    {
        CHECK_FRAME_RESULT(vulkanAsyncCompute->Submit(vulkanFrame));
    }
#ifdef HYP_DEBUG_MODE
    else if (!vulkanAsyncCompute->renderQueue.IsEmpty())
    {
        HYP_LOG(RenderingBackend, Fatal, "Cannot write to async compute render queue, this device does not support async compute!");
    }
#endif

    m_textureCache->CleanupUnusedTextures();

    CHECK_FRAME_RESULT(m_instance->GetSwapchain()->PresentFrame(&m_instance->GetDevice()->GetGraphicsQueue()));

    m_instance->GetSwapchain()->NextFrame();
}

DescriptorSetRef VulkanRenderBackend::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    DescriptorSetLayout newLayout { layout.GetDeclaration() };
    newLayout.SetIsTemplate(false);
    newLayout.SetIsReference(false);

    DescriptorSetRef descriptorSet = CreateObject<VulkanDescriptorSet>(newLayout);
    descriptorSet->SetDebugName(layout.GetName());

    return descriptorSet;
}

DescriptorTableRef VulkanRenderBackend::MakeDescriptorTable(const DescriptorTableDeclaration* decl)
{
    return CreateObject<VulkanDescriptorTable>(decl);
}

GraphicsPipelineRef VulkanRenderBackend::MakeGraphicsPipeline(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable,
    Span<const FramebufferRef> framebuffers,
    const RenderableAttributeSet& attributes)
{
    VulkanRenderPassRef renderPass;

    for (const FramebufferRef& framebuffer : framebuffers)
    {
        HYP_GFX_ASSERT(framebuffer.IsValid());

        VulkanFramebuffer* vulkanFramebuffer = VULKAN_CAST(framebuffer.Get());

        if (vulkanFramebuffer->GetRenderPass() != nullptr)
        {
            renderPass = vulkanFramebuffer->GetRenderPass();

            break;
        }
    }

    VulkanGraphicsPipelineRef graphicsPipeline;

    if (descriptorTable.IsValid())
    {
        graphicsPipeline = CreateObject<VulkanGraphicsPipeline>(VulkanShaderRef::empty, VulkanDescriptorTableRef(descriptorTable));
    }
    else
    {
        graphicsPipeline = CreateObject<VulkanGraphicsPipeline>();
    }

    if (shader.IsValid())
    {
        graphicsPipeline->SetShader(shader);

#ifdef HYP_DEBUG_MODE
        graphicsPipeline->SetDebugName(NAME_FMT("GraphicsPipeline_{}", shader->GetDebugName().IsValid() ? *shader->GetDebugName() : "<unnamed shader>"));
#endif
    }

    HYP_GFX_ASSERT(graphicsPipeline->GetDescriptorTable().IsValid());

    graphicsPipeline->SetVertexAttributes(attributes.GetMeshAttributes().vertexAttributes);
    graphicsPipeline->SetTopology(attributes.GetMeshAttributes().topology);
    graphicsPipeline->SetCullMode(attributes.GetMaterialAttributes().cullFaces);
    graphicsPipeline->SetFillMode(attributes.GetMaterialAttributes().fillMode);
    graphicsPipeline->SetBlendFunction(attributes.GetMaterialAttributes().blendFunction);
    graphicsPipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencilFunction);
    graphicsPipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_TEST));
    graphicsPipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MAF_DEPTH_WRITE));
    graphicsPipeline->SetRenderPass(renderPass);
    graphicsPipeline->SetFramebuffers(framebuffers);

    return graphicsPipeline;
}

ComputePipelineRef VulkanRenderBackend::MakeComputePipeline(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable)
{
    return CreateObject<VulkanComputePipeline>(VulkanShaderRef(shader), VulkanDescriptorTableRef(descriptorTable));
}

RaytracingPipelineRef VulkanRenderBackend::MakeRaytracingPipeline(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptorTable)
{
    return CreateObject<VulkanRaytracingPipeline>(VulkanShaderRef(shader), VulkanDescriptorTableRef(descriptorTable));
}

GpuBufferRef VulkanRenderBackend::MakeGpuBuffer(GpuBufferType bufferType, SizeType size, SizeType alignment)
{
    return CreateObject<VulkanGpuBuffer>(bufferType, size, alignment);
}

GpuImageRef VulkanRenderBackend::MakeImage(const TextureDesc& textureDesc)
{
    return CreateObject<VulkanGpuImage>(textureDesc);
}

GpuImageViewRef VulkanRenderBackend::MakeImageView(const GpuImageRef& image)
{
    return CreateObject<VulkanGpuImageView>(VulkanGpuImageRef(image));
}

GpuImageViewRef VulkanRenderBackend::MakeImageView(const GpuImageRef& image, uint32 mipIndex, uint32 numMips, uint32 faceIndex, uint32 numFaces)
{
    return CreateObject<VulkanGpuImageView>(VulkanGpuImageRef(image), mipIndex, numMips, faceIndex, numFaces);
}

SamplerRef VulkanRenderBackend::MakeSampler(TextureFilterMode filterModeMin, TextureFilterMode filterModeMag, TextureWrapMode wrapMode)
{
    return CreateObject<VulkanSampler>(filterModeMin, filterModeMag, wrapMode);
}

FramebufferRef VulkanRenderBackend::MakeFramebuffer(Vec2u extent, uint32 numViews)
{
    return CreateObject<VulkanFramebuffer>(
        extent,
        RenderPassStage::SHADER,
        numViews);
}

FramebufferRef VulkanRenderBackend::MakeFramebuffer(Vec2u extent, RenderPassStage stage, uint32 numViews)
{
    return CreateObject<VulkanFramebuffer>(
        extent,
        stage,
        numViews);
}

FrameRef VulkanRenderBackend::MakeFrame(uint32 frameIndex)
{
    return CreateObject<VulkanFrame>(frameIndex);
}

ShaderRef VulkanRenderBackend::MakeShader(const RC<CompiledShader>& compiledShader)
{
    return CreateObject<VulkanShader>(compiledShader);
}

BLASRef VulkanRenderBackend::MakeBLAS(
    const GpuBufferRef& packedVerticesBuffer,
    const GpuBufferRef& packedIndicesBuffer,
    uint32 numVertices,
    uint32 numIndices,
    const Handle<Material>& material,
    const Matrix4& transform)
{
    return CreateObject<VulkanBLAS>(
        VulkanGpuBufferRef(packedVerticesBuffer),
        VulkanGpuBufferRef(packedIndicesBuffer),
        numVertices,
        numIndices,
        material,
        transform);
}

TLASRef VulkanRenderBackend::MakeTLAS()
{
    return CreateObject<VulkanTLAS>();
}

const GpuImageViewRef& VulkanRenderBackend::GetTextureImageView(const Handle<Texture>& texture, uint32 mipIndex, uint32 numMips, uint32 faceIndex, uint32 numFaces)
{
    if (!texture.IsValid())
    {
        return GpuImageViewRef::empty;
    }

    ImageSubResource subResource {};
    subResource.numLevels = MathUtil::Min(numMips, texture->GetTextureDesc().NumMipmaps());
    subResource.baseMipLevel = MathUtil::Min(mipIndex, numMips - 1);
    subResource.numLayers = MathUtil::Min(numFaces, texture->GetTextureDesc().NumFaces());
    subResource.baseArrayLayer = MathUtil::Min(faceIndex, numFaces - 1);

    const GpuImageViewRef& imageView = m_textureCache->GetOrCreate(texture, subResource);
    HYP_GFX_ASSERT(imageView.IsValid());

    return imageView;
}

void VulkanRenderBackend::PopulateIndirectDrawCommandsBuffer(const GpuBufferRef& vertexBuffer, const GpuBufferRef& indexBuffer, uint32 instanceOffset, ByteBuffer& outByteBuffer)
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

    const bool hasMipmaps = textureDesc.HasMipmaps();
    const uint32 numMipmaps = textureDesc.NumMipmaps();
    const uint32 numFaces = textureDesc.NumFaces();

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

    // @TODO Implement me.

    HYP_NOT_IMPLEMENTED();
}

RendererResult VulkanRenderBackend::CreateDescriptorSet(const RC<VulkanDescriptorSetLayoutWrapper>& layout, VkDescriptorSet& outVkDescriptorSet)
{
    return m_descriptorSetManager->CreateDescriptorSet(m_instance->GetDevice(), layout, outVkDescriptorSet);
}

RendererResult VulkanRenderBackend::DestroyDescriptorSet(VkDescriptorSet vkDescriptorSet)
{
    return m_descriptorSetManager->DestroyDescriptorSet(m_instance->GetDevice(), vkDescriptorSet);
}

RendererResult VulkanRenderBackend::GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, RC<VulkanDescriptorSetLayoutWrapper>& outRef)
{
    outRef = m_descriptorSetManager->GetOrCreateVkDescriptorSetLayout(m_instance->GetDevice(), layout);

    if (outRef.IsValid())
    {
        return RendererResult {};
    }

    return HYP_MAKE_ERROR(RendererError, "Failed to get or create Vulkan descriptor set layout");
}

UniquePtr<SingleTimeCommands> VulkanRenderBackend::GetSingleTimeCommands()
{
    return MakeUnique<VulkanSingleTimeCommands>();
}

VkSurfaceKHR VulkanRenderBackend::CreateVkSurface(ApplicationWindow* window, VulkanInstance* instance)
{
    HYP_GFX_ASSERT(window && instance);

#ifdef HYP_SDL
    if (SDLApplicationWindow* sdlWindow = ObjCast<SDLApplicationWindow>(window))
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        SDL_bool result = SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(sdlWindow->GetInternalWindowHandle()), instance->GetInstance(), &surface);

        HYP_GFX_ASSERT(result == SDL_TRUE, "Failed to create Vulkan surface: %s", SDL_GetError());

        return surface;
    }
#endif

#ifdef HYP_WINDOWS
    if (Win32ApplicationWindow* win32Window = ObjCast<Win32ApplicationWindow>(window))
    {
        VkSurfaceKHR surface = VK_NULL_HANDLE;

        VkWin32SurfaceCreateInfoKHR createInfo { VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR };
        createInfo.hinstance = win32Window->GetHINSTANCE();
        createInfo.hwnd = win32Window->GetHWND();

        VkResult vkResult = vkCreateWin32SurfaceKHR(
            instance->GetInstance(),
            &createInfo,
            nullptr,
            &surface);

        HYP_GFX_ASSERT(vkResult == VK_SUCCESS, "Failed to create Win32 Vulkan surface: %d", int(vkResult));

        return surface;
    }
#endif

    HYP_NOT_IMPLEMENTED();
}

RendererResult VulkanRenderBackend::GetVkExtensions(Array<const char*>& outExtensions)
{
#ifdef HYP_SDL
    if (const SDLAppContext* sdlAppContext = ObjCast<SDLAppContext>(g_appContext))
    {
        uint32 numExtensions = 0;
        SDL_Window* sdlWindow = static_cast<SDL_Window*>(static_cast<SDLApplicationWindow*>(sdlAppContext->GetMainWindow())->GetInternalWindowHandle());

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

#ifdef HYP_WINDOWS
    if (const Win32AppContext* win32AppContext = ObjCast<Win32AppContext>(g_appContext))
    {
        // extensions required for Win32 surface support
        static const char* requiredExtensions[] = {
            "VK_KHR_surface",
            "VK_KHR_win32_surface"
        };

        uint32_t count = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

        Array<VkExtensionProperties> vkProperties(count);
        vkEnumerateInstanceExtensionProperties(nullptr, &count, vkProperties.Data());

        for (const char* requiredExtension : requiredExtensions)
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

VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper& layout)
{
    return layout.GetVulkanHandle();
}

} // namespace hyperion

#undef CHECK_FRAME_RESULT
