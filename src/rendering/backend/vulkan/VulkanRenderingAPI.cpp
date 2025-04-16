/* Copyright (c) 2024-2025 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererHelpers.hpp>
#include <rendering/backend/AsyncCompute.hpp>
#include <rendering/backend/RendererInstance.hpp>
#include <rendering/backend/RendererFrameHandler.hpp>
#include <rendering/backend/RendererFeatures.hpp>

#include <system/AppContext.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {

#ifdef HYP_DEBUG_MODE
static constexpr bool g_use_debug_layers = true;
#else
static constexpr bool g_use_debug_layers = false;
#endif

#pragma region VulkanRenderConfig

class VulkanRenderConfig final : public IRenderConfig
{
public:
    VulkanRenderConfig(VulkanRenderingAPI *rendering_api)
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

private:
    VulkanRenderingAPI  *m_rendering_api;
};

#pragma endregion VulkanRenderConfig

#pragma region Vulkan struct wrappers

struct VulkanDescriptorSetLayoutWrapper
{
    VkDescriptorSetLayout   vk_layout = VK_NULL_HANDLE;

    RendererResult Create(platform::Device<Platform::VULKAN> *device, const platform::DescriptorSetLayout<Platform::VULKAN> &layout)
    {
        AssertThrow(vk_layout == VK_NULL_HANDLE);

        static constexpr VkDescriptorBindingFlags bindless_flags = VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;

        Array<VkDescriptorSetLayoutBinding> bindings;
        bindings.Reserve(layout.GetElements().Size());

        Array<VkDescriptorBindingFlags> binding_flags;
        binding_flags.Reserve(layout.GetElements().Size());

        for (const auto &it : layout.GetElements()) {
            const Name name = it.first;
            const DescriptorSetLayoutElement &element = it.second;

            uint32 descriptor_count = element.count;

            if (element.IsBindless()) {
                descriptor_count = max_bindless_resources;
            }

            VkDescriptorSetLayoutBinding binding { };
            binding.descriptorCount = descriptor_count;
            binding.descriptorType = helpers::ToVkDescriptorType(element.type);
            binding.pImmutableSamplers = nullptr;
            binding.stageFlags = VK_SHADER_STAGE_ALL;
            binding.binding = element.binding;

            bindings.PushBack(binding);

            VkDescriptorBindingFlags flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;

            if (element.IsBindless()) {
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
            &vk_layout
        ));

        return RendererResult { };
    }

    RendererResult Destroy(platform::Device<Platform::VULKAN> *device)
    {
        AssertThrow(vk_layout != VK_NULL_HANDLE);

        vkDestroyDescriptorSetLayout(
            device->GetDevice(),
            vk_layout,
            nullptr
        );

        vk_layout = VK_NULL_HANDLE;

        return RendererResult { };
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

    RendererResult Create(platform::Device<Platform::VULKAN> *device);
    RendererResult Destroy(platform::Device<Platform::VULKAN> *device);

    RendererResult CreateDescriptorSet(platform::Device<Platform::VULKAN> *device, const RC<VulkanDescriptorSetLayoutWrapper> &layout, VkDescriptorSet &out_vk_descriptor_set);
    RendererResult DestroyDescriptorSet(platform::Device<Platform::VULKAN> *device, VkDescriptorSet vk_descriptor_set);

    RC<VulkanDescriptorSetLayoutWrapper> GetOrCreateVkDescriptorSetLayout(platform::Device<Platform::VULKAN> *device, const platform::DescriptorSetLayout<Platform::VULKAN> &layout);

private:
    HashMap<HashCode, Weak<VulkanDescriptorSetLayoutWrapper>>   m_vk_descriptor_set_layouts;

    VkDescriptorPool                                            m_vk_descriptor_pool;
};

VulkanDescriptorSetManager::VulkanDescriptorSetManager()
    : m_vk_descriptor_pool(VK_NULL_HANDLE)
{
}

VulkanDescriptorSetManager::~VulkanDescriptorSetManager() = default;

RendererResult VulkanDescriptorSetManager::Create(platform::Device<Platform::VULKAN> *device)
{
    static const Array<VkDescriptorPoolSize> pool_sizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                    256 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,     8 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,              32000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,              32000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,             64000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,     64000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,             32000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC,     32000 }
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
        &m_vk_descriptor_pool
    ));

    return RendererResult { };
}

RendererResult VulkanDescriptorSetManager::Destroy(platform::Device<Platform::VULKAN> *device)
{
    RendererResult result = RendererResult { };

    for (auto &it : m_vk_descriptor_set_layouts) {
        if (auto rc = it.second.Lock()) {
            HYPERION_PASS_ERRORS(
                rc->Destroy(device),
                result
            );
        }
    }

    m_vk_descriptor_set_layouts.Clear();

    if (m_vk_descriptor_pool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(
            device->GetDevice(),
            m_vk_descriptor_pool,
            nullptr
        );

        m_vk_descriptor_pool = VK_NULL_HANDLE;
    }

    return result;
}

RendererResult VulkanDescriptorSetManager::CreateDescriptorSet(platform::Device<Platform::VULKAN> *device, const RC<VulkanDescriptorSetLayoutWrapper> &layout, VkDescriptorSet &out_vk_descriptor_set)
{
    AssertThrow(m_vk_descriptor_pool != VK_NULL_HANDLE);

    AssertThrow(layout != nullptr);
    AssertThrow(layout->vk_layout != VK_NULL_HANDLE);

    VkDescriptorSetAllocateInfo alloc_info { VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
    alloc_info.descriptorPool = m_vk_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &layout->vk_layout;

    const VkResult vk_result = vkAllocateDescriptorSets(
        device->GetDevice(),
        &alloc_info,
        &out_vk_descriptor_set
    );

    if (vk_result != VK_SUCCESS) {
        return HYP_MAKE_ERROR(RendererError, "Failed to allocate descriptor set", int(vk_result));
    }

    return RendererResult { };
}

RendererResult VulkanDescriptorSetManager::DestroyDescriptorSet(platform::Device<Platform::VULKAN> *device, VkDescriptorSet vk_descriptor_set)
{
    AssertThrow(m_vk_descriptor_pool != VK_NULL_HANDLE);

    AssertThrow(vk_descriptor_set != VK_NULL_HANDLE);

    vkFreeDescriptorSets(
        device->GetDevice(),
        m_vk_descriptor_pool,
        1,
        &vk_descriptor_set
    );

    return RendererResult { };
}

RC<VulkanDescriptorSetLayoutWrapper> VulkanDescriptorSetManager::GetOrCreateVkDescriptorSetLayout(platform::Device<Platform::VULKAN> *device, const platform::DescriptorSetLayout<Platform::VULKAN> &layout)
{
    const HashCode hash_code = layout.GetHashCode();

    RC<VulkanDescriptorSetLayoutWrapper> vk_descriptor_set_layout;

    auto it = m_vk_descriptor_set_layouts.Find(hash_code);

    if (it != m_vk_descriptor_set_layouts.End()) {
        vk_descriptor_set_layout = it->second.Lock();
    }

    if (vk_descriptor_set_layout != nullptr) {
        return vk_descriptor_set_layout;
    }

    vk_descriptor_set_layout = MakeRefCountedPtr<VulkanDescriptorSetLayoutWrapper>();
    
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

platform::Device<Platform::VULKAN> *VulkanRenderingAPI::GetDevice() const
{
    return m_instance->GetDevice();
}

ISwapchain *VulkanRenderingAPI::GetSwapchain() const
{
    return m_instance->GetSwapchain();
}

RendererResult VulkanRenderingAPI::Initialize(AppContext &app_context)
{
    m_instance = new platform::Instance<Platform::VULKAN>();
    HYPERION_BUBBLE_ERRORS(m_instance->Initialize(app_context, g_use_debug_layers));

    m_crash_handler.Initialize();

    HYPERION_BUBBLE_ERRORS(m_descriptor_set_manager->Create(m_instance->GetDevice()));

    platform::AsyncCompute<Platform::VULKAN> *async_compute = new platform::AsyncCompute<Platform::VULKAN>();
    HYPERION_BUBBLE_ERRORS(async_compute->Create());
    m_async_compute = async_compute;

    m_default_formats.Set(
        DefaultImageFormatType::COLOR,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { InternalFormat::RGBA8, InternalFormat::R10G10B10A2, InternalFormat::RGBA16F } },
            ImageSupportType::SRV
        )
    );

    m_default_formats.Set(
        DefaultImageFormatType::DEPTH,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { InternalFormat::DEPTH_32F, InternalFormat::DEPTH_24, InternalFormat::DEPTH_16 } },
            ImageSupportType::DEPTH
        )
    );

    m_default_formats.Set(
        DefaultImageFormatType::NORMALS,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { InternalFormat::RGBA16F, InternalFormat::RGBA32F, InternalFormat::RGBA8 } },
            ImageSupportType::SRV
        )
    );

    m_default_formats.Set(
        DefaultImageFormatType::STORAGE,
        m_instance->GetDevice()->GetFeatures().FindSupportedFormat(
            { { InternalFormat::RGBA16F } },
            ImageSupportType::UAV
        )
    );

    return { };
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

    return { };
}

IFrame *VulkanRenderingAPI::GetCurrentFrame() const
{
    return m_instance->GetSwapchain()->GetFrameHandler()->GetCurrentFrame();
}

IFrame *VulkanRenderingAPI::PrepareNextFrame()
{
    RendererResult frame_result = m_instance->GetSwapchain()->GetFrameHandler()->PrepareFrame(
        m_instance->GetSwapchain(),
        m_should_recreate_swapchain
    );

    Frame *frame = m_instance->GetSwapchain()->GetFrameHandler()->GetCurrentFrame();

    if (m_should_recreate_swapchain) {
        m_should_recreate_swapchain = false;

        HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());
        HYPERION_ASSERT_RESULT(m_instance->RecreateSwapchain());
        HYPERION_ASSERT_RESULT(m_instance->GetDevice()->Wait());

        HYPERION_ASSERT_RESULT(m_instance->GetSwapchain()->GetFrameHandler()->GetCurrentFrame()->RecreateFence());

        // Need to prepare frame again now that swapchain has been recreated.
        HYPERION_ASSERT_RESULT(m_instance->GetSwapchain()->GetFrameHandler()->PrepareFrame(
            m_instance->GetSwapchain(),
            m_should_recreate_swapchain
        ));

        AssertThrow(!m_should_recreate_swapchain);

        frame = m_instance->GetSwapchain()->GetFrameHandler()->GetCurrentFrame();

        OnSwapchainRecreated(m_instance->GetSwapchain());
    }

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);

        return nullptr;
    }

    platform::AsyncCompute<Platform::VULKAN> *async_compute = static_cast<platform::AsyncCompute<Platform::VULKAN> *>(m_async_compute);

    HYPERION_ASSERT_RESULT(async_compute->PrepareForFrame(frame));

    return frame;
}

void VulkanRenderingAPI::PresentFrame(IFrame *frame)
{
    Frame *vulkan_frame = static_cast<Frame *>(frame);

    platform::AsyncCompute<Platform::VULKAN> *async_compute = static_cast<platform::AsyncCompute<Platform::VULKAN> *>(m_async_compute);

    // Add commands from the frame's command list to the command buffer
    const CommandBufferRef &command_buffer = m_instance->GetSwapchain()->GetFrameHandler()->GetCurrentCommandBuffer();
    AssertThrow(command_buffer != nullptr);

    command_buffer->Begin();
    vulkan_frame->GetCommandList().Execute(command_buffer);
    command_buffer->End();

    RendererResult frame_result = command_buffer->SubmitPrimary(&m_instance->GetDevice()->GetGraphicsQueue(), vulkan_frame->GetFence(), &vulkan_frame->GetPresentSemaphores());

    if (!frame_result) {
        m_crash_handler.HandleGPUCrash(frame_result);

        return;
    }

    HYPERION_ASSERT_RESULT(async_compute->Submit(vulkan_frame));

    m_instance->GetSwapchain()->GetFrameHandler()->PresentFrame(&m_instance->GetDevice()->GetGraphicsQueue(), m_instance->GetSwapchain());
    m_instance->GetSwapchain()->GetFrameHandler()->NextFrame();
}

InternalFormat VulkanRenderingAPI::GetDefaultFormat(DefaultImageFormatType type) const
{
    auto it = m_default_formats.Find(type);
    if (it != m_default_formats.End()) {
        return it->second;
    }

    return InternalFormat::NONE;
}

bool VulkanRenderingAPI::IsSupportedFormat(InternalFormat format, ImageSupportType support_type) const
{
    return m_instance->GetDevice()->GetFeatures().IsSupportedFormat(format, support_type);
}

InternalFormat VulkanRenderingAPI::FindSupportedFormat(Span<InternalFormat> possible_formats, ImageSupportType support_type) const
{
    return m_instance->GetDevice()->GetFeatures().FindSupportedFormat(possible_formats, support_type);
}

QueryImageCapabilitiesResult VulkanRenderingAPI::QueryImageCapabilities(const TextureDesc &texture_desc) const
{
    QueryImageCapabilitiesResult result;

    const InternalFormat format = texture_desc.format;
    const ImageType type = texture_desc.type;

    const bool is_attachment_texture = texture_desc.image_format_capabilities[ImageFormatCapabilities::ATTACHMENT];
    const bool is_rw_texture = texture_desc.image_format_capabilities[ImageFormatCapabilities::STORAGE];

    const bool is_depth_stencil = texture_desc.IsDepthStencil();
    const bool is_srgb = texture_desc.IsSRGB();
    const bool is_blended = texture_desc.image_format_capabilities[ImageFormatCapabilities::BLENDED];

    const bool is_cubemap = texture_desc.IsTextureCube();

    const bool has_mipmaps = texture_desc.HasMipmaps();
    const uint32 num_mipmaps = texture_desc.NumMipmaps();
    const uint32 num_faces = texture_desc.NumFaces();

    VkFormat vk_format = helpers::ToVkFormat(format);
    VkImageType vk_image_type = helpers::ToVkType(type);
    VkImageCreateFlags vk_image_create_flags = 0;
    VkFormatFeatureFlags vk_format_features = 0;
    VkImageFormatProperties vk_image_format_properties { };

    VkImageTiling vk_tiling = VK_IMAGE_TILING_OPTIMAL;
    VkImageUsageFlags vk_usage_flags = VK_IMAGE_USAGE_SAMPLED_BIT;

    if (is_attachment_texture) {
        vk_usage_flags |= (is_depth_stencil ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
            | VK_IMAGE_USAGE_TRANSFER_SRC_BIT; /* for mip chain */
    }

    if (is_rw_texture) {
        vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT /* allow readback */
            | VK_IMAGE_USAGE_STORAGE_BIT;
    } else {
        vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    if (has_mipmaps) {
        /* Mipmapped image needs linear blitting. */
        vk_format_features |= VK_FORMAT_FEATURE_BLIT_DST_BIT
            | VK_FORMAT_FEATURE_BLIT_SRC_BIT;

        vk_usage_flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

        switch (texture_desc.filter_mode_min) {
        case FilterMode::TEXTURE_FILTER_LINEAR: // fallthrough
        case FilterMode::TEXTURE_FILTER_LINEAR_MIPMAP:
            vk_format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
            break;
        case FilterMode::TEXTURE_FILTER_MINMAX_MIPMAP:
            vk_format_features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_MINMAX_BIT;
            break;
        default:
            break;
        }
    }

    if (is_blended) {
        vk_format_features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    }

    if (is_cubemap) {
        vk_image_create_flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }

    // @TODO Implement me. 

    HYP_NOT_IMPLEMENTED();
}

RendererResult VulkanRenderingAPI::CreateDescriptorSet(const RC<VulkanDescriptorSetLayoutWrapper> &layout, VkDescriptorSet &out_vk_descriptor_set)
{
    return m_descriptor_set_manager->CreateDescriptorSet(m_instance->GetDevice(), layout, out_vk_descriptor_set);
}

RendererResult VulkanRenderingAPI::DestroyDescriptorSet(VkDescriptorSet vk_descriptor_set)
{
    return m_descriptor_set_manager->DestroyDescriptorSet(m_instance->GetDevice(), vk_descriptor_set);
}

RC<VulkanDescriptorSetLayoutWrapper> VulkanRenderingAPI::GetOrCreateVkDescriptorSetLayout(const platform::DescriptorSetLayout<Platform::VULKAN> &layout)
{
    return m_descriptor_set_manager->GetOrCreateVkDescriptorSetLayout(m_instance->GetDevice(), layout);
}

#pragma endregion VulkanRenderingAPI

VkDescriptorSetLayout GetVkDescriptorSetLayout(const VulkanDescriptorSetLayoutWrapper &layout)
{
    return layout.vk_layout;
}

} // namespace renderer
} // namespace hyperion