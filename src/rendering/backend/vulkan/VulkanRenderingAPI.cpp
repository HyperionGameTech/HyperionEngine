/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>
#include <rendering/backend/vulkan/RendererSwapchain.hpp>
#include <rendering/backend/vulkan/RendererFrame.hpp>
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#include <rendering/backend/vulkan/RendererComputePipeline.hpp>
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#include <rendering/backend/vulkan/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/vulkan/rt/RendererAccelerationStructure.hpp>
#include <rendering/backend/vulkan/RendererShader.hpp>
#include <rendering/backend/vulkan/AsyncCompute.hpp>

#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <rendering/RenderableAttributes.hpp>

#include <core/algorithm/Map.hpp>

#include <system/AppContext.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
#ifdef HYP_DEBUG_MODE
static constexpr bool g_use_debug_layers = true;
#else
static constexpr bool g_use_debug_layers = false;
#endif

#pragma region VulkanRenderConfig

class VulkanRenderConfig final : public IRenderConfig
{
public:
    VulkanRenderConfig(VulkanRenderingAPI* rendering_api)
        : m_rendering_api(rendering_api)
    {
    }

    virtual ~VulkanRenderConfig() override = default;

    virtual bool ShouldCollectUniqueDrawCallPerMaterial() const override
    {
        return true;
    }

    virtual bool IsBindlessSupported() const override
    {
        return m_rendering_api->GetDevice()->GetFeatures().SupportsBindlessTextures();
    }

    virtual bool IsRaytracingSupported() const override
    {
        return m_rendering_api->GetDevice()->GetFeatures().IsRaytracingSupported();
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
        return false; // m_rendering_api->GetDevice()->GetFeatures().SupportsDynamicDescriptorIndexing();
    }

private:
    VulkanRenderingAPI* m_rendering_api;
};

#pragma endregion VulkanRenderConfig

#pragma region Vulkan struct wrappers

class VulkanDescriptorSetLayoutWrapper : public RenderObject<VulkanDescriptorSetLayoutWrapper>
{
    VkDescriptorSetLayout m_handle;
    platform::Device<Platform::vulkan>* m_device;
    RendererResult (*m_delete_fn)(platform::Device<Platform::vulkan>* device, VulkanDescriptorSetLayoutWrapper*);

public:
    VulkanDescriptorSetLayoutWrapper(platform::Device<Platform::vulkan>* device, RendererResult (*delete_fn)(platform::Device<Platform::vulkan>* device, VulkanDescriptorSetLayoutWrapper*))
        : RenderObject<VulkanDescriptorSetLayoutWrapper>(),
          m_handle(VK_NULL_HANDLE),
          m_device(device),
          m_delete_fn(delete_fn)
    {
        AssertThrow(m_delete_fn != nullptr);
    }

    virtual ~VulkanDescriptorSetLayoutWrapper() override
    {
        if (m_handle != VK_NULL_HANDLE)
        {
            AssertThrow(m_delete_fn != nullptr);
            HYPERION_ASSERT_RESULT(m_delete_fn(m_device, this));
        }
    }

    HYP_FORCE_INLINE VkDescriptorSetLayout GetVulkanHandle() const
    {
        return m_handle;
    }

    RendererResult Create(platform::Device<Platform::vulkan>* device, const DescriptorSetLayout& layout)
    {
        AssertThrow(m_handle == VK_NULL_HANDLE);

        static constexpr VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        Array<VkDescriptorSetLayoutBinding> bindings;
        bindings.Reserve(layout.GetElements().Size());

        Array<VkDescriptorBindingFlags> binding_flags;
        binding_flags.Reserve(layout.GetElements().Size());

        for (const auto& it : layout.GetElements())
        {
            const Name name = it.first;
            const DescriptorSetLayoutElement& element = it.second;

            uint32 descriptor_count = element.count;

            if (element.IsBindless())
            {
                descriptor_count = max_bindless_resources;
            }

            // if (descriptor_count > 1 && !m_device->GetFeatures().SupportsDynamicDescriptorIndexing()) {
            //     return HYP_MAKE_ERROR(RendererError, "Device does not support descriptor indexing, cannot create descriptor set with element {} that uses an array of elements", 0, name);
            // }

            VkDescriptorSetLayoutBinding binding {};
            binding.descriptorCount = descriptor_count;
            binding.descriptorType = helpers::ToVkDescriptorType(element.type);
            binding.pImmutableSamplers = nullptr;
            binding.stageFlags = VK_SHADER_STAGE_ALL;
            binding.binding = element.binding;

            bindings.PushBack(binding);

            VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

            if (element.IsBindless())
            {
                flags |= bindless_flags;
            }

            binding_flags.PushBack(flags);
        }

        VkDescriptorSetLayoutBindingFlagsCreateInfo extended_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO };
        extended_info.bindingCount = uint32(binding_flags.Size());
        extended_info.pBindingFlags = binding_flags.Data();

        VkDescriptorSetLayoutCreateInfo layout_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
        layout_info.pBindings = bindings.Data();
        layout_info.bindingCount = uint32(bindings.Size());
        layout_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
        layout_info.pNext = &extended_info;

        HYPERION_VK_CHECK(vkCreateDescriptorSetLayout(
            device->GetDevice(),
            &layout_info,
            nullptr,
            &m_handle));

        return RendererResult {};
    }

    RendererResult Destroy(platform::Device<Platform::vulkan>* device)
    {
        AssertThrow(m_handle != VK_NULL_HANDLE);

        vkDestroyDescriptorSetLayout(
            device->GetDevice(),
            m_handle,
            nullptr);

        m_handle = VK_NULL_HANDLE;

        return RendererResult {};
    }
};

#pragma endregion Vulkan struct wrappers

#pragma region VulkanDescriptorSetManager

class VulkanDescriptorSetManager final : public IDescriptorSetManager
{
public:
    static constexpr uint32 max_descriptor_sets = 4096;

    VulkanDescriptorSetManager();
    virtual ~VulkanDescriptorSetManager() override;

    RendererResult Create(platform::Device<Platform::vulkan>* device);
    RendererResult Destroy(platform::Device<Platform::vulkan>* device);

    RendererResult CreateDescriptorSet(platform::Device<Platform::vulkan>* device, const VulkanDescriptorSetLayoutWrapperRef& layout, VkDescriptorSet& out_vk_descriptor_set);
    RendererResult DestroyDescriptorSet(platform::Device<Platform::vulkan>* device, VkDescriptorSet vk_descriptor_set);

    VulkanDescriptorSetLayoutWrapperRef GetOrCreateVkDescriptorSetLayout(platform::Device<Platform::vulkan>* device, const DescriptorSetLayout& layout);

private:
    Mutex m_mutex;
    HashMap<HashCode, VulkanDescriptorSetLayoutWrapperWeakRef> m_vk_descriptor_set_layouts;

    VkDescriptorPool m_vk_descriptor_pool;
};

VulkanDescriptorSetManager::VulkanDescriptorSetManager()
    : m_vk_descriptor_pool(VK_NULL_HANDLE)
{
}

VulkanDescriptorSetManager::~VulkanDescriptorSetManager() = default;

RendererResult VulkanDescriptorSetManager::Create(platform::Device<Platform::vulkan>* device)
{
    static const Array<VkDescriptorPoolSize> pool_sizes = {
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

    AssertThrow(m_vk_descriptor_pool == VK_NULL_HANDLE);

    VkDescriptorPoolCreateInfo pool_info { VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
    pool_info.maxSets = max_descriptor_sets;
    pool_info.poolSizeCount = uint32(pool_sizes.Size());
    pool_info.pPoolSizes = pool_sizes.Data();

    HYPERION_VK_CHECK(vkCreateDescriptorPool(
        device->GetDevice(),
        &pool_info,
        nullptr,
        &m_vk_descriptor_pool));

    return RendererResult {};
}

RendererResult VulkanDescriptorSetManager::Destroy(platform::Device<Platform::vulkan>* device)
{
    RendererResult result = RendererResult {};

    for (auto& it : m_vk_descriptor_set_layouts)
    {
        if (auto rc = it.second.Lock())
        {
            HYPERION_PASS_ERRORS(
                rc->Destroy(device),
                result);
        }
    }

    m_vk_descriptor_set_layouts.Clear();

    if (m_vk_descriptor_pool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(
            device->GetDevice(),
            m_vk_descriptor_pool,
            nullptr);

        m_vk_descriptor_pool = VK_NULL_HANDLE;
    }

    return result;
}

RendererResult VulkanDescriptorSetManager::CreateDescriptorSet(platform::Device<Platform::vulkan>* device, const VulkanDescriptorSetLayoutWrapperRef& layout, VkDescriptorSet& out_vk_descriptor_set)
{
    AssertThrow(m_vk_descriptor_pool != VK_NULL_HANDLE);

    AssertThrow(layout != nullptr);
    AssertThrow(layout->GetVulkanHandle() != VK_NULL_HANDLE);

    VkDescriptorSetLayout layouts[] = { layout->GetVulkanHandle() };

    VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    alloc_info.descriptorPool = m_vk_descriptor_pool;
    alloc_info.descriptorSetCount = ArraySize(layouts);
    alloc_info.pSetLayouts = &layouts[0];

    const VkResult vk_result = vkAllocateDescriptorSets(
        device->GetDevice(),
        &alloc_info,
        &out_vk_descriptor_set);

    if (vk_result != VK_SUCCESS)
    {
        return HYP_MAKE_ERROR(RendererError, "Failed to allocate descriptor set", int(vk_result));
    }

    return RendererResult {};
}

RendererResult VulkanDescriptorSetManager::DestroyDescriptorSet(platform::Device<Platform::vulkan>* device, VkDescriptorSet vk_descriptor_set)
{
    AssertThrow(m_vk_descriptor_pool != VK_NULL_HANDLE);

    AssertThrow(vk_descriptor_set != VK_NULL_HANDLE);

    vkFreeDescriptorSets(
        device->GetDevice(),
        m_vk_descriptor_pool,
        1,
        &vk_descriptor_set);

    return RendererResult {};
}

VulkanDescriptorSetLayoutWrapperRef VulkanDescriptorSetManager::GetOrCreateVkDescriptorSetLayout(platform::Device<Platform::vulkan>* device, const DescriptorSetLayout& layout)
{
    const HashCode hash_code = layout.GetHashCode();

    VulkanDescriptorSetLayoutWrapperRef vk_descriptor_set_layout;

    Mutex::Guard guard(m_mutex);

    auto it = m_vk_descriptor_set_layouts.Find(hash_code);

    if (it != m_vk_descriptor_set_layouts.End())
    {
        vk_descriptor_set_layout = it->second.Lock();
    }

    if (vk_descriptor_set_layout != nullptr)
    {
        return vk_descriptor_set_layout;
    }

    vk_descriptor_set_layout = MakeRenderObject<VulkanDescriptorSetLayoutWrapper>(device, [](platform::Device<Platform::vulkan>* device, VulkanDescriptorSetLayoutWrapper* wrapper)
        {
            return wrapper->Destroy(device);
        });

    HYPERION_ASSERT_RESULT(vk_descriptor_set_layout->Create(device, layout));

    m_vk_descriptor_set_layouts.Set(hash_code, vk_descriptor_set_layout);

    return vk_descriptor_set_layout;
}

#pragma endregion VulkanDescriptorSetManager

#pragma region VulkanRenderingAPI

VulkanRenderingAPI::VulkanRenderingAPI()
    : m_instance(nullptr),
      m_render_config(new VulkanRenderConfig(this)),
      m_descriptor_set_manager(new VulkanDescriptorSetManager()),
      m_async_compute(nullptr),
      m_should_recreate_swapchain(false)
{
}

VulkanRenderingAPI::~VulkanRenderingAPI()
{
    delete m_render_config;
    delete m_descriptor_set_manager;
}

platform::Device<Platform::vulkan>* VulkanRenderingAPI::GetDevice() const
{
    return m_instance->GetDevice();
}

SwapchainBase* VulkanRenderingAPI::GetSwapchain() const
{
    return m_instance->GetSwapchain();
}

RendererResult VulkanRenderingAPI::Initialize(AppContextBase& app_context)
{
    m_instance = new platform::Instance<Platform::vulkan>();
    HYPERION_BUBBLE_ERRORS(m_instance->Initialize(app_context, g_use_debug_layers));

    m_crash_handler.Initialize();

    HYPERION_BUBBLE_ERRORS(m_descriptor_set_manager->Create(m_instance->GetDevice()));

    VulkanAsyncCompute* async_compute = new VulkanAsyncCompute();
    HYPERION_BUBBLE_ERRORS(async_compute->Create());
    m_async_compute = async_compute;

    m_default_formats.Set(
        DIF_COLOR,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_RGBA8, TF_R10G10B10A2, TF_RGBA16F } },
            IS_SRV));

    m_default_formats.Set(
        DIF_DEPTH,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_DEPTH_24, TF_DEPTH_32F, TF_DEPTH_16 } },
            IS_DEPTH));

    m_default_formats.Set(
        DIF_NORMALS,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_RGBA16F, TF_RGBA32F, TF_RGBA8 } },
            IS_SRV));

    m_default_formats.Set(
        DIF_STORAGE,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { TF_RGBA16F } },
            IS_UAV));

    return {};
}

RendererResult VulkanRenderingAPI::Destroy()
{
    m_descriptor_set_manager->Destroy(m_instance->GetDevice());

    delete m_async_compute;
    m_async_compute = nullptr;

    HYPERION_BUBBLE_ERRORS(m_instance->GetDevice()->Wait());
    HYPERION_BUBBLE_ERRORS(m_instance->Destroy());

    delete m_instance;
    m_instance = nullptr;

    return {};
}

FrameBase* VulkanRenderingAPI::GetCurrentFrame() const
{
    return m_instance->GetSwapchain()->GetCurrentFrame().Get();
}

FrameBase* VulkanRenderingAPI::PrepareNextFrame()
{
    RendererResult frame_result = m_instance->GetSwapchain()->PrepareFrame(m_should_recreate_swapchain);

    VulkanFrame* frame = m_instance->GetSwapchain()->GetCurrentFrame();

    if (m_should_recreate_swapchain)
    {
        m_should_recreate_swapchain = false;

        HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());
        HYPERION_ASSERT_RESULT(m_instance->RecreateSwapchain());
        HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());

        HYPERION_ASSERT_RESULT(m_instance->GetSwapchain()->GetCurrentFrame()->RecreateFence());

        // Need to prepare frame again now that swapchain has been recreated.
        HYPERION_ASSERT_RESULT(m_instance->GetSwapchain()->PrepareFrame(m_should_recreate_swapchain));

        AssertThrow(!m_should_recreate_swapchain);

        frame = m_instance->GetSwapchain()->GetCurrentFrame();

        OnSwapchainRecreated(m_instance->GetSwapchain());
    }

    if (!frame_result)
    {
        m_crash_handler.HandleGPUCrash(frame_result);

        return nullptr;
    }

    VulkanAsyncCompute* async_compute = static_cast<VulkanAsyncCompute*>(m_async_compute);

    HYPERION_ASSERT_RESULT(async_compute->PrepareForFrame(frame));

    return frame;
}

void VulkanRenderingAPI::PresentFrame(FrameBase* frame)
{
    const CommandBufferRef& command_buffer = m_instance->GetSwapchain()->GetCurrentCommandBuffer();

    VulkanFrame* vulkan_frame = static_cast<VulkanFrame*>(frame);
    VulkanCommandBufferRef vulkan_command_buffer = VulkanCommandBufferRef(command_buffer);
    VulkanAsyncCompute* vulkan_async_compute = static_cast<VulkanAsyncCompute*>(m_async_compute);

    RendererResult submit_result = vulkan_frame->Submit(&m_instance->GetDevice()->GetGraphicsQueue(), vulkan_command_buffer);

    if (!submit_result)
    {
        m_crash_handler.HandleGPUCrash(submit_result);

        return;
    }

    HYPERION_ASSERT_RESULT(vulkan_async_compute->Submit(vulkan_frame));

    m_instance->GetSwapchain()->PresentFrame(&m_instance->GetDevice()->GetGraphicsQueue());
    m_instance->GetSwapchain()->NextFrame();
}

DescriptorSetRef VulkanRenderingAPI::MakeDescriptorSet(const DescriptorSetLayout& layout)
{
    DescriptorSetLayout new_layout { layout.GetDeclaration() };
    new_layout.SetIsTemplate(false);
    new_layout.SetIsReference(false);

    DescriptorSetRef descriptor_set = MakeRenderObject<VulkanDescriptorSet>(new_layout);
    descriptor_set->SetDebugName(layout.GetName());

    return descriptor_set;
}

DescriptorTableRef VulkanRenderingAPI::MakeDescriptorTable(const DescriptorTableDeclaration* decl)
{
    return MakeRenderObject<VulkanDescriptorTable>(decl);
}

GraphicsPipelineRef VulkanRenderingAPI::MakeGraphicsPipeline(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptor_table,
    const Array<FramebufferRef>& framebuffers,
    const RenderableAttributeSet& attributes)
{
    VulkanRenderPassRef render_pass;
    Array<VulkanFramebufferRef> vulkan_framebuffers = Map(framebuffers, [](const FramebufferRef& framebuffer)
        {
            return VulkanFramebufferRef(framebuffer);
        });

    for (const auto& framebuffer : vulkan_framebuffers)
    {
        if (framebuffer->GetRenderPass() != nullptr)
        {
            render_pass = framebuffer->GetRenderPass();
            break;
        }
    }

    VulkanGraphicsPipelineRef graphics_pipeline;

    if (descriptor_table.IsValid())
    {
        graphics_pipeline = MakeRenderObject<VulkanGraphicsPipeline>(VulkanShaderRef::unset, VulkanDescriptorTableRef(descriptor_table));
    }
    else
    {
        graphics_pipeline = MakeRenderObject<VulkanGraphicsPipeline>();
    }

    if (shader.IsValid())
    {
        graphics_pipeline->SetShader(shader);
    }

    AssertThrow(graphics_pipeline->GetDescriptorTable().IsValid());

    graphics_pipeline->SetVertexAttributes(attributes.GetMeshAttributes().vertex_attributes);
    graphics_pipeline->SetTopology(attributes.GetMeshAttributes().topology);
    graphics_pipeline->SetCullMode(attributes.GetMaterialAttributes().cull_faces);
    graphics_pipeline->SetFillMode(attributes.GetMaterialAttributes().fill_mode);
    graphics_pipeline->SetBlendFunction(attributes.GetMaterialAttributes().blend_function);
    graphics_pipeline->SetStencilFunction(attributes.GetMaterialAttributes().stencil_function);
    graphics_pipeline->SetDepthTest(bool(attributes.GetMaterialAttributes().flags & MaterialAttributeFlags::DEPTH_TEST));
    graphics_pipeline->SetDepthWrite(bool(attributes.GetMaterialAttributes().flags & MaterialAttributeFlags::DEPTH_WRITE));
    graphics_pipeline->SetRenderPass(render_pass);
    graphics_pipeline->SetFramebuffers(vulkan_framebuffers);

    return graphics_pipeline;
}

ComputePipelineRef VulkanRenderingAPI::MakeComputePipeline(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptor_table)
{
    return MakeRenderObject<VulkanComputePipeline>(VulkanShaderRef(shader), VulkanDescriptorTableRef(descriptor_table));
}

RaytracingPipelineRef VulkanRenderingAPI::MakeRaytracingPipeline(
    const ShaderRef& shader,
    const DescriptorTableRef& descriptor_table)
{
    return MakeRenderObject<VulkanRaytracingPipeline>(VulkanShaderRef(shader), VulkanDescriptorTableRef(descriptor_table));
}

GPUBufferRef VulkanRenderingAPI::MakeGPUBuffer(GPUBufferType buffer_type, SizeType size, SizeType alignment)
{
    return MakeRenderObject<VulkanGPUBuffer>(buffer_type, size, alignment);
}

ImageRef VulkanRenderingAPI::MakeImage(const TextureDesc& texture_desc)
{
    return MakeRenderObject<VulkanImage>(texture_desc);
}

ImageViewRef VulkanRenderingAPI::MakeImageView(const ImageRef& image)
{
    return MakeRenderObject<VulkanImageView>(VulkanImageRef(image));
}

ImageViewRef VulkanRenderingAPI::MakeImageView(const ImageRef& image, uint32 mip_index, uint32 num_mips, uint32 face_index, uint32 num_faces)
{
    return MakeRenderObject<VulkanImageView>(VulkanImageRef(image), mip_index, num_mips, face_index, num_faces);
}

SamplerRef VulkanRenderingAPI::MakeSampler(TextureFilterMode filter_mode_min, TextureFilterMode filter_mode_mag, TextureWrapMode wrap_mode)
{
    return MakeRenderObject<VulkanSampler>(filter_mode_min, filter_mode_mag, wrap_mode);
}

FramebufferRef VulkanRenderingAPI::MakeFramebuffer(Vec2u extent, uint32 num_views)
{
    return MakeRenderObject<VulkanFramebuffer>(
        extent,
        RenderPassStage::SHADER,
        num_views);
}

FramebufferRef VulkanRenderingAPI::MakeFramebuffer(Vec2u extent, RenderPassStage stage, uint32 num_views)
{
    return MakeRenderObject<VulkanFramebuffer>(
        extent,
        stage,
        num_views);
}

FrameRef VulkanRenderingAPI::MakeFrame(uint32 frame_index)
{
    return MakeRenderObject<VulkanFrame>(frame_index);
}

ShaderRef VulkanRenderingAPI::MakeShader(const RC<CompiledShader>& compiled_shader)
{
    return MakeRenderObject<VulkanShader>(compiled_shader);
}

BLASRef VulkanRenderingAPI::MakeBLAS(
    const GPUBufferRef& packed_vertices_buffer,
    const GPUBufferRef& packed_indices_buffer,
    const Handle<Material>& material,
    const Matrix4& transform)
{
    return MakeRenderObject<VulkanBLAS>(
        VulkanGPUBufferRef(packed_vertices_buffer),
        VulkanGPUBufferRef(packed_indices_buffer),
        material,
        transform);
}

TLASRef VulkanRenderingAPI::MakeTLAS()
{
    return MakeRenderObject<VulkanTLAS>();
}

TextureFormat VulkanRenderingAPI::GetDefaultFormat(DefaultImageFormat type) const
{
    auto it = m_default_formats.Find(type);
    if (it != m_default_formats.End())
    {
        return it->second;
    }

    return TF_NONE;
}

bool VulkanRenderingAPI::IsSupportedFormat(TextureFormat format, ImageSupport support_type) const
{
    return m_instance->GetDevice()->GetFeatures().IsSupportedFormat(format, support_type);
}

TextureFormat VulkanRenderingAPI::FindSupportedFormat(Span<TextureFormat> possible_formats, ImageSupport support_type) const
{
    return m_instance->GetDevice()->GetFeatures().FindSupportedFormat(possible_formats, support_type);
}

QueryImageCapabilitiesResult VulkanRenderingAPI::QueryImageCapabilities(const TextureDesc& texture_desc) const
{
    QueryImageCapabilitiesResult result;

    const TextureFormat format = texture_desc.format;
    const TextureType type = texture_desc.type;

    const bool is_attachment_texture = texture_desc.image_usage[IU_ATTACHMENT];
    const bool is_rw_texture = texture_desc.image_usage[IU_STORAGE];

    const bool is_depth_stencil = texture_desc.IsDepthStencil();
    const bool is_srgb = texture_desc.IsSRGB();
    const bool is_blended = texture_desc.image_usage[IU_BLENDED];

    const bool has_mipmaps = texture_desc.HasMipmaps();
    const uint32 num_mipmaps = texture_desc.NumMipmaps();
    const uint32 num_faces = texture_desc.NumFaces();

    VkFormat vk_format = helpers::ToVkFormat(format);
    VkImageType vk_image_type = helpers::ToVkImageType(type);
    VkImageCreateFlags vk_image_create_flags = 0;
    VkFormatFeatureFlags vk_format_features = 0;
    VkImageFormatProperties vk_image_format_properties {};

    VkImageTiling vk_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags vk_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT;

    if (is_attachment_texture)
    {
        vk_usage_flags |= (is_depth_stencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /* for mip chain */
    }

    if (is_rw_texture)
    {
        vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
            | VK_IMAGE_USAGE_STORAGE_BIT;
    }
    else
    {
        vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (has_mipmaps)
    {
        /* Mipmapped image needs linear blitting. */
        vk_format_features |= VK_FORMAT_FEATURE_BLIT_DST_BIT
            | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        switch (texture_desc.filter_mode_min)
        {
        case TFM_LINEAR: // fallthrough
        case TFM_LINEAR_MIPMAP:
            vk_format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            break;
        case TFM_MINMAX_MIPMAP:
            vk_format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
            break;
        default:
            break;
        }
    }

    if (is_blended)
    {
        vk_format_features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (texture_desc.IsTextureCube() || texture_desc.IsTextureCubeArray())
    {
        vk_image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    // @TODO Implement me.

    HYP_NOT_IMPLEMENTED();
}

RendererResult VulkanRenderingAPI::CreateDescriptorSet(const VulkanDescriptorSetLayoutWrapperRef& layout, VkDescriptorSet& out_vk_descriptor_set)
{
    return m_descriptor_set_manager->CreateDescriptorSet(m_instance->GetDevice(), layout, out_vk_descriptor_set);
}

RendererResult VulkanRenderingAPI::DestroyDescriptorSet(VkDescriptorSet vk_descriptor_set)
{
    return m_descriptor_set_manager->DestroyDescriptorSet(m_instance->GetDevice(), vk_descriptor_set);
}

RendererResult VulkanRenderingAPI::GetOrCreateVkDescriptorSetLayout(const DescriptorSetLayout& layout, VulkanDescriptorSetLayoutWrapperRef& out_ref)
{
    out_ref = m_descriptor_set_manager->GetOrCreateVkDescriptorSetLayout(m_instance->GetDevice(), layout);

    if (out_ref.IsValid())
    {
        return RendererResult {};
    }

    return HYP_MAKE_ERROR(RendererError, "Failed to get or create Vulkan descriptor set layout");
}

#pragma endregion VulkanRenderingAPI

HYP_API VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper& layout)
{
    return layout.GetVulkanHandle();
}

} // namespace hyperion
