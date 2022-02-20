//
// Created by ethan on 2/5/22.
//

#include "vk_renderer.h"

#include "../system/sdl_system.h"
#include "../system/debug.h"

#include <vector>
#include <iostream>
#include <optional>
#include <cstring>
#include <fstream>

namespace hyperion {

RendererQueue::RendererQueue() {
    this->queue = nullptr;
}

void RendererQueue::GetQueueFromDevice(RendererDevice device, uint32_t queue_family_index,
                                       uint32_t queue_index) {
    vkGetDeviceQueue(device.GetDevice(), queue_family_index, queue_index, &this->queue);
}

void RendererDevice::SetDevice(const VkDevice &_device) {
    this->device = _device;
}
void RendererDevice::SetPhysicalDevice(const VkPhysicalDevice &_physical) {
    this->physical = _physical;
}
void RendererDevice::SetRenderSurface(const VkSurfaceKHR &_surface) {
    this->surface = _surface;
}
void RendererDevice::SetRequiredExtensions(const std::vector<const char *> &_extensions) {
    this->required_extensions = _extensions;
}

VkDevice RendererDevice::GetDevice() {
    return this->device;
}
VkPhysicalDevice RendererDevice::GetPhysicalDevice() {
    return this->physical;
}
VkSurfaceKHR RendererDevice::GetRenderSurface() {
    if (this->surface == nullptr) {
        DebugLog(LogType::Fatal, "Device render surface is null!\n");
        throw std::runtime_error("Device render surface not set");
    }
    return this->surface;
}
std::vector<const char *> RendererDevice::GetRequiredExtensions() {
    return this->required_extensions;
}

RendererDevice::RendererDevice() {
    this->device = nullptr;
    this->physical = nullptr;
    this->surface = nullptr;
}

RendererDevice::~RendererDevice() {
    /* By the time this destructor is called there should never
     * be a running queue, but just in case we will wait until
     * all the queues on our device are stopped. */
    vkDeviceWaitIdle(this->device);
    vkDestroyDevice(this->device, nullptr);
}

QueueFamilyIndices RendererDevice::FindQueueFamilies() {
    VkPhysicalDevice _physical_device = this->GetPhysicalDevice();
    QueueFamilyIndices indices;

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_physical_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(_physical_device, &queue_family_count, families.data());

    int index = 0;
    VkBool32 supports_presentation = false;
    for (const auto &queue_family : families) {
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            indices.graphics_family = index;

        /* Some devices appear only to compute and are not graphical,
         * so we need to make sure it supports presenting to the user. */
        vkGetPhysicalDeviceSurfaceSupportKHR(_physical_device, index, this->GetRenderSurface(), &supports_presentation);
        if (supports_presentation)
            indices.present_family = index;

        /* Everything was found, return the indices */
        if (indices.is_complete())
            break;

        index++;
    }
    return indices;
}

VkPhysicalDeviceFeatures RendererDevice::GetDeviceFeatures() {
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(this->physical, &features);
    return features;
}

std::vector<VkExtensionProperties> RendererDevice::GetSupportedExtensions() {
    uint32_t extension_count = 0;
    vkEnumerateDeviceExtensionProperties(this->physical, nullptr, &extension_count, nullptr);
    std::vector<VkExtensionProperties> supported_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(this->physical, nullptr, &extension_count, supported_extensions.data());
    return supported_extensions;
}

std::vector<const char *> RendererDevice::CheckExtensionSupport(std::vector<const char *> required_extensions) {
    auto extensions_supported = this->GetSupportedExtensions();
    std::vector<const char *> unsupported_extensions = required_extensions;

    for (const char *required_ext_name : required_extensions) {
        /* For each extension required, check */
        for (auto &supported : extensions_supported) {
            if (!strcmp(supported.extensionName, required_ext_name)) {
                unsupported_extensions.erase(std::find(unsupported_extensions.begin(), unsupported_extensions.end(), required_ext_name));
            }
        }
    }
    return unsupported_extensions;
}
std::vector<const char *> RendererDevice::CheckExtensionSupport() {
    return this->CheckExtensionSupport(this->GetRequiredExtensions());
}

SwapchainSupportDetails RendererDevice::QuerySwapchainSupport() {
    SwapchainSupportDetails details;
    VkPhysicalDevice _physical = this->GetPhysicalDevice();
    VkSurfaceKHR     _surface  = this->GetRenderSurface();

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(_physical, _surface, &details.capabilities);
    uint32_t count = 0;
    /* Get device surface formats */
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physical, _surface, &count, nullptr);
    std::vector<VkSurfaceFormatKHR> surface_formats(count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(_physical, _surface, &count, surface_formats.data());
    if (count == 0)
        DebugLog(LogType::Warn, "No surface formats available!\n");

    vkGetPhysicalDeviceSurfacePresentModesKHR(_physical, _surface, &count, nullptr);
    std::vector<VkPresentModeKHR> present_modes(count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(_physical, _surface, &count, present_modes.data());
    if (count == 0)
        DebugLog(LogType::Warn, "No present modes available!\n");

    details.formats = surface_formats;
    details.present_modes = present_modes;

    return details;
}

bool RendererDevice::CheckDeviceSuitable() {
    QueueFamilyIndices indices = this->FindQueueFamilies();
    std::vector<const char *> unsupported_extensions = this->CheckExtensionSupport();
    if (!unsupported_extensions.empty()) {
        DebugLog(LogType::Warn, "--- Unsupported Extensions ---\n");
        for (const auto &extension : unsupported_extensions) {
            DebugLog(LogType::Warn, "\t%s\n", extension);
        }
        DebugLog(LogType::Error, "Vulkan: Device does not support required extensions\n");
        /* TODO: try different device(s) before exploding  */
        throw std::runtime_error("Device does not support required extensions");
    }
    bool extensions_available = unsupported_extensions.empty();

    SwapchainSupportDetails swapchain_support = this->QuerySwapchainSupport();
    bool swapchains_available = (!swapchain_support.formats.empty() && !swapchain_support.present_modes.empty());

    return (indices.is_complete() && extensions_available && swapchains_available);
}


VkDevice RendererDevice::CreateLogicalDevice(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions) {
    this->SetRequiredExtensions(required_extensions);

    std::vector<VkDeviceQueueCreateInfo> queue_create_info_vec;
    const float priorities[] = { 1.0f };
    // for each queue family(for separate threads) we add them to
    // our device initialization data
    for (uint32_t family: required_queue_families) {
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.pQueuePriorities = priorities;
        queue_info.queueCount = 1;
        queue_info.queueFamilyIndex = family;

        queue_create_info_vec.push_back(queue_info);
    }

    if (!this->CheckDeviceSuitable()) {
        DebugLog(LogType::Error, "Device not suitable!\n");
        throw std::runtime_error("Device not suitable");
    }

    VkDeviceCreateInfo create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create_info.pQueueCreateInfos = queue_create_info_vec.data();
    create_info.queueCreateInfoCount = (uint32_t) (queue_create_info_vec.size());
    // Setup Device extensions
    create_info.enabledExtensionCount = (uint32_t) (required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();
    // Setup Device Features
    VkPhysicalDeviceFeatures device_features = this->GetDeviceFeatures();
    create_info.pEnabledFeatures = &device_features;

    VkDevice _device;
    VkResult result = vkCreateDevice(this->physical, &create_info, nullptr, &_device);
    if (result != VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create RendererDevice!\n");
        throw std::runtime_error("Could not create RendererDevice!");
    }

    this->SetDevice(_device);

    return this->device;
}

VkQueue RendererDevice::GetQueue(uint32_t queue_family_index, uint32_t queue_index) {
    VkQueue queue;
    vkGetDeviceQueue(this->GetDevice(), queue_family_index, queue_index, &queue);
    return queue;
}

RendererSwapchain::RendererSwapchain(RendererDevice *_device, const SwapchainSupportDetails &_details) {
    this->swapchain = nullptr;
    this->renderer_device = _device;
    this->support_details = _details;
}

VkSurfaceFormatKHR RendererSwapchain::ChooseSurfaceFormat() {
    for (const auto &format: this->support_details.formats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    DebugLog(LogType::Warn, "Swapchain format sRGB is not supported, going with defaults...\n");
    return this->support_details.formats[0];
}

VkPresentModeKHR RendererSwapchain::ChoosePresentMode() {
    /* TODO: add more presentation modes in the future */
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D RendererSwapchain::ChooseSwapchainExtent() {
    return this->support_details.capabilities.currentExtent;
}

void RendererSwapchain::RetrieveImageHandles() {
    uint32_t image_count = 0;
    /* Query for the size, as we will need to create swap chains with more images
     * in the future for more complex applications. */
    vkGetSwapchainImagesKHR(this->renderer_device->GetDevice(), this->swapchain, &image_count, nullptr);
    this->images.resize(image_count);
    vkGetSwapchainImagesKHR(this->renderer_device->GetDevice(), this->swapchain, &image_count, this->images.data());
    DebugLog(LogType::Info, "Retrieved Swapchain images\n");
}

void RendererSwapchain::CreateImageView(size_t index, VkImage *swapchain_image) {
    VkImageViewCreateInfo create_info{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    create_info.image = (*swapchain_image);
    create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    create_info.format   = this->image_format;
    /* Components; we'll just stick to the default mapping for now. */
    create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    create_info.subresourceRange.baseMipLevel = 0;
    create_info.subresourceRange.levelCount = 1;
    create_info.subresourceRange.baseArrayLayer = 0;
    create_info.subresourceRange.layerCount = 1;

    if (vkCreateImageView(this->renderer_device->GetDevice(), &create_info, nullptr, &this->image_views[index]) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create swapchain image views!\n");
        throw std::runtime_error("Could not create image views");
    }
}

void RendererSwapchain::CreateImageViews() {
    this->image_views.resize(this->images.size());
    for (size_t i = 0; i < this->images.size(); i++) {
        this->CreateImageView(i, &this->images[i]);
    }
}

void RendererSwapchain::DestroyImageViews() {
    for (auto view : this->image_views) {
        vkDestroyImageView(this->renderer_device->GetDevice(), view, nullptr);
    }
}

void RendererSwapchain::CreateFramebuffers(VkRenderPass *renderpass) {
    this->framebuffers.resize(this->image_views.size());
    for (size_t i; i < this->image_views.size(); i++) {
        VkImageView attachments[] = { this->image_views[i] };
        VkFramebufferCreateInfo create_info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        create_info.renderPass = *renderpass;
        create_info.attachmentCount = 1;
        create_info.pAttachments = attachments;
        create_info.width = this->extent.width;
        create_info.height = this->extent.height;
        create_info.layers = 1;

        if (vkCreateFramebuffer(this->renderer_device->GetDevice(), &create_info, nullptr, &this->framebuffers[i]) != VK_SUCCESS) {
            DebugLog(LogType::Error, "Could not create Vulkan framebuffer!\n");
            throw std::runtime_error("could not create framebuffer");
        }
    }
}

void RendererSwapchain::Create(const VkSurfaceKHR &surface, QueueFamilyIndices qf_indices) {
    this->surface_format = this->ChooseSurfaceFormat();
    this->present_mode   = this->ChoosePresentMode();
    this->extent         = this->ChooseSwapchainExtent();
    this->image_format   = this->surface_format.format;

    uint32_t image_count = this->support_details.capabilities.minImageCount + 1;

    VkSwapchainCreateInfoKHR create_info{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create_info.surface = surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent     = extent;
    create_info.imageArrayLayers = 1; /* This is always 1 unless we make a stereoscopic/VR application */
    create_info.imageUsage       = this->image_usage_flags;

    /* Graphics computations and presentation are done on separate hardware */
    if (qf_indices.graphics_family != qf_indices.present_family) {
        DebugLog(LogType::Info, "Swapchain sharing mode set to Concurrent!\n");
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2; /* Two family indices(one for each process) */
        const uint32_t families[] = {qf_indices.graphics_family.value(), qf_indices.present_family.value()};
        create_info.pQueueFamilyIndices = families;
    }
    /* Computations and presentation are done on same hardware(most scenarios) */
    else {
        DebugLog(LogType::Info, "Swapchain sharing mode set to Exclusive!\n");
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;       /* Optional */
        create_info.pQueueFamilyIndices   = nullptr; /* Optional */
    }
    /* For transformations such as rotations, etc. */
    create_info.preTransform = this->support_details.capabilities.currentTransform;
    /* This can be used to blend with other windows in the windowing system, but we
     * simply leave it opaque.*/
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(this->renderer_device->GetDevice(), &create_info, nullptr, &this->swapchain) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Failed to create Vulkan swapchain!\n");
        throw std::runtime_error("Failed to create swapchain");
    }
    DebugLog(LogType::Info, "Created Swapchain!\n");
    this->RetrieveImageHandles();
    this->CreateImageViews();
}

RendererSwapchain::~RendererSwapchain() {
    this->DestroyImageViews();
    if (this->swapchain != nullptr)
        vkDestroySwapchainKHR(this->renderer_device->GetDevice(), this->swapchain, nullptr);
}

void RendererShader::AttachShader(RendererDevice *_device, const SPIRVObject &spirv) {
    this->device = _device;

    VkShaderModuleCreateInfo create_info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    create_info.codeSize = spirv.raw.size();
    create_info.pCode    = spirv.VkCode();
    VkShaderModule module;

    if (vkCreateShaderModule(device->GetDevice(), &create_info, nullptr, &module) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create Vulkan shader module!\n");
        return;
    }

    RendererShaderModule shader_mod = { spirv.type, module };
    this->shader_modules.push_back(shader_mod);
}

VkPipelineShaderStageCreateInfo RendererShader::CreateShaderStage(RendererShaderModule *module, const std::string &name) {
    VkPipelineShaderStageCreateInfo create_info{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
    create_info.module = module->module;
    create_info.pName = name.c_str();
    switch (module->type) {
        case SPIRVObject::Type::Vertex:
            create_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
            break;
        case SPIRVObject::Type::Fragment:
            create_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            break;
        case SPIRVObject::Type::Geometry:
            create_info.stage = VK_SHADER_STAGE_GEOMETRY_BIT;
            break;
        case SPIRVObject::Type::Compute:
            create_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
            break;
        case SPIRVObject::Type::Task:
            create_info.stage = VK_SHADER_STAGE_TASK_BIT_NV;
            break;
        case SPIRVObject::Type::Mesh:
            create_info.stage = VK_SHADER_STAGE_MESH_BIT_NV;
            break;
        case SPIRVObject::Type::TessControl:
            create_info.stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
            break;
        case SPIRVObject::Type::TessEval:
            create_info.stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
            break;
        case SPIRVObject::Type::RayGen:
            create_info.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
            break;
        case SPIRVObject::Type::RayIntersect:
            create_info.stage = VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
            break;
        case SPIRVObject::Type::RayAnyHit:
            create_info.stage = VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
            break;
        case SPIRVObject::Type::RayClosestHit:
            create_info.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
            break;
        case SPIRVObject::Type::RayMiss:
            create_info.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
            break;
        default:
            DebugLog(LogType::Warn, "Shader type %d is currently unimplemented!\n", module->type);
    }
    return create_info;
}

void RendererShader::CreateProgram(const std::string &name) {
    std::vector<VkPipelineShaderStageCreateInfo> stages;
    for (auto module: this->shader_modules) {
        auto stage = this->CreateShaderStage(&module, name);
        this->shader_stages.push_back(stage);
    }
}

void RendererShader::Destroy() {
    for (auto module: this->shader_modules) {
        vkDestroyShaderModule(this->device->GetDevice(), module.module, nullptr);
    }
}

RendererPipeline::RendererPipeline(RendererDevice *_device, RendererSwapchain *_swapchain) {
    this->primitive = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    this->swapchain = _swapchain;
    this->device = _device;

    auto width = (float)swapchain->extent.width;
    auto height = (float)swapchain->extent.height;
    this->SetViewport(0.0f, 0.0f, width, height);
    this->SetScissor(0, 0, _swapchain->extent.width, _swapchain->extent.height);

    std::vector<VkDynamicState> default_dynamic_states = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH,
    };
    this->SetDynamicStates(default_dynamic_states);
}

void RendererPipeline::SetPrimitive(VkPrimitiveTopology _primitive) {
    this->primitive = _primitive;
}
VkPrimitiveTopology RendererPipeline::GetPrimitive() {
    return this->primitive;
}

void RendererPipeline::SetViewport(float x, float y, float width, float height, float min_depth, float max_depth) {
    VkViewport *vp = &this->viewport;
    vp->x = x;
    vp->y = y;
    vp->width = width;
    vp->height = height;
    vp->minDepth = min_depth;
    vp->maxDepth = max_depth;
}

void RendererPipeline::SetScissor(int x, int y, uint32_t width, uint32_t height) {
    this->scissor.offset = { x, y };
    this->scissor.extent = { width, height };
}

void RendererPipeline::SetDynamicStates(const std::vector<VkDynamicState> &_states) {
    this->dynamic_states = _states;
}

std::vector<VkDynamicState> RendererPipeline::GetDynamicStates() {
    return this->dynamic_states;
}

void RendererPipeline::CreateRenderPass(VkSampleCountFlagBits sample_count) {
    VkAttachmentDescription attachment{};
    attachment.format = this->swapchain->image_format;
    attachment.samples = sample_count;
    /* Options to change what happens before and after our render pass */
    attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

    attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    /* Images are presented to the swapchain */
    attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference attachment_ref{};
    attachment_ref.attachment = 0; /* Our attachment array(only one pass, so it will be 0) */
    attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    /* Subpasses for post processing, etc. */
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &attachment_ref;

    const VkAttachmentDescription attachments[] = { attachment };
    const VkSubpassDescription    subpasses[]   = { subpass };

    /* Create render pass */
    VkRenderPassCreateInfo render_pass_info{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = attachments;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses   = subpasses;

    if (vkCreateRenderPass(this->device->GetDevice(), &render_pass_info, nullptr, &this->render_pass) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create render pass!\n");
        throw std::runtime_error("Error creating render pass");
    }
    DebugLog(LogType::Info, "Renderpass created!\n");
}

void RendererPipeline::Rebuild(RendererShader *shader) {
    VkPipelineVertexInputStateCreateInfo vertex_input_info{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vertex_input_info.vertexBindingDescriptionCount = 0;
    vertex_input_info.pVertexBindingDescriptions = nullptr;
    vertex_input_info.vertexAttributeDescriptionCount = 0;
    vertex_input_info.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo input_asm_info{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    input_asm_info.topology = this->GetPrimitive();
    input_asm_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_state{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    VkViewport viewports[] = { this->viewport };
    viewport_state.viewportCount = 1;
    viewport_state.pViewports = viewports;

    VkRect2D scissors[] = { this->scissor };
    viewport_state.scissorCount = 1;
    viewport_state.pScissors = scissors;

    VkPipelineRasterizationStateCreateInfo rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    /* TODO: Revisit this for shadow maps! */
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    /* Backface culling */
    rasterizer.cullMode  = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    /* Also visit for shadow mapping! Along with other optional parameters such as
     * depthBiasClamp, slopeFactor etc. */
    rasterizer.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;

    /* TODO: enable multisampling and the GPU feature required for it.  */

    VkPipelineColorBlendAttachmentState color_blend_attachment{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blend_attachment.colorWriteMask = (
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
        VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT);

    /* TODO: with multiple framebuffers and post processing, we will need to enable colour blending */
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blending{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    color_blending.logicOpEnable = false;
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;

    /* Dynamic states */
    VkPipelineDynamicStateCreateInfo dynamic_state{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic_state.dynamicStateCount = 2;
    VkDynamicState states[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_LINE_WIDTH,
    };
    dynamic_state.pDynamicStates  = states;
    DebugLog(LogType::Info, "Enabling [%d] dynamic states\n", dynamic_state.dynamicStateCount);
    /* Pipeline layout */
    VkPipelineLayoutCreateInfo layout_info{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    layout_info.setLayoutCount = 0;
    layout_info.pSetLayouts = nullptr;
    layout_info.pushConstantRangeCount = 0;
    layout_info.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(this->device->GetDevice(), &layout_info, nullptr, &this->layout) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Error creating pipeline layout!\n");
        throw std::runtime_error("Error creating pipeline layout");
    }

    VkGraphicsPipelineCreateInfo pipeline_info{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    auto stages = shader->shader_stages;
    pipeline_info.stageCount = stages.size();
    pipeline_info.pStages = stages.data();

    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_asm_info;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state;

    pipeline_info.layout = layout;
    pipeline_info.renderPass = render_pass;
    pipeline_info.subpass = 0; /* Index of the subpass */
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    if (vkCreateGraphicsPipelines(this->device->GetDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &this->pipeline) != VK_SUCCESS) {
        DebugLog(LogType::Error, "Could not create graphics pipeline!\n");
        throw std::runtime_error("Could not create pipeline");
    }
    DebugLog(LogType::Info, "Created graphics pipeline!\n");
}

RendererPipeline::~RendererPipeline() {
    VkDevice render_device = this->device->GetDevice();
    vkDestroyPipeline(render_device, this->pipeline, nullptr);
    vkDestroyPipelineLayout(render_device, this->layout, nullptr);
    vkDestroyRenderPass(render_device, this->render_pass, nullptr);
}



bool VkRenderer::CheckValidationLayerSupport(const std::vector<const char *> &requested_layers) {
    uint32_t layers_count;
    vkEnumerateInstanceLayerProperties(&layers_count, nullptr);

    std::vector<VkLayerProperties> available_layers(layers_count);
    vkEnumerateInstanceLayerProperties(&layers_count, available_layers.data());

    for (const char *request : requested_layers) {
        bool layer_found = false;
        for (const auto &available_properties : available_layers) {
            if (!strcmp(available_properties.layerName, request)) {
                layer_found = true;
                break;
            }
        }
        if (!layer_found) {
            // TODO: disable debug mode/validation layers instead of causing a runtime error
            throw std::runtime_error("Validation Layer "+std::string(request)+" is unavailable!");
        }
    }
    return false;
}

void VkRenderer::SetValidationLayers(std::vector<const char *> _layers) {
    this->CheckValidationLayerSupport(_layers);
    this->validation_layers = _layers;
}

void VkRenderer::SetupDebug() {
    const std::vector<const char *> layers = {
        "VK_LAYER_KHRONOS_validation",
    };
    this->SetValidationLayers(layers);
}

void VkRenderer::SetCurrentWindow(SystemWindow *_window) {
    this->window = _window;
}

SystemWindow *VkRenderer::GetCurrentWindow() {
    return this->window;
}

VkRenderer::VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name) {
    this->system = _system;
    this->app_name = app_name;
    this->engine_name = engine_name;
    this->device = nullptr;
}

void VkRenderer::Initialize(bool load_debug_layers) {
    // Application names/versions
    this->SetCurrentWindow(this->system.GetCurrentWindow());

    /* Set up our debug and validation layers */
    if (load_debug_layers)
        this->SetupDebug();

    VkApplicationInfo app_info{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app_info.pApplicationName = app_name;
    app_info.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    app_info.pEngineName = engine_name;
    app_info.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    // Set target api version
    app_info.apiVersion = VK_RENDERER_API_VERSION;

    VkInstanceCreateInfo create_info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create_info.pApplicationInfo = &app_info;

    // Setup validation layers
    create_info.enabledLayerCount = (uint32_t)(this->validation_layers.size());
    create_info.ppEnabledLayerNames = this->validation_layers.data();
    // Setup Vulkan extensions
    std::vector<const char *> extension_names;
    extension_names = this->system.GetVulkanExtensionNames();

    create_info.enabledExtensionCount = extension_names.size();
    create_info.ppEnabledExtensionNames = extension_names.data();

    DebugLog(LogType::Info, "Loading [%d] Instance extensions...\n", extension_names.size());

    VkResult result = vkCreateInstance(&create_info, nullptr, &this->instance);
    if (result != VK_SUCCESS) {
        DebugLog(LogType::Fatal, "Failed to create Vulkan Instance!\n");
        throw std::runtime_error("Failed to create Vulkan Instance!");
    }

    this->surface = nullptr;
    this->requested_device_extensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
}

VkRenderer::~VkRenderer() {
    delete this->pipeline;
    delete this->swapchain;
    delete this->device;
    /* Destroy the surface from SDL */
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
    /* Destroy the Vulkan instance(this should always be last!) */
    vkDestroyInstance(this->instance, nullptr);
}

void VkRenderer::SetQueueFamilies(std::set<uint32_t> _queue_families) {
    this->queue_families = _queue_families;
}
//VkDevice VkRenderer::InitDevice(const VkPhysicalDevice &physical,
//                                std::set<uint32_t> unique_queue_families,
//                          <      const std::vector<const char *> &required_extensions)
//{
//
//}

RendererDevice *VkRenderer::GetRendererDevice() {
    return this->device;
}

void VkRenderer::CreateSurface() {
    this->surface = this->GetCurrentWindow()->CreateVulkanSurface(this->instance);
    std::cout << "Created window surface\n";
}

VkPhysicalDevice VkRenderer::PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures   features;

    /* Check for a discrete/dedicated GPU with geometry shaders */
    for (const auto &_device : _devices) {
        vkGetPhysicalDeviceProperties(_device, &properties);
        vkGetPhysicalDeviceFeatures(_device, &features);

        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && features.geometryShader) {
            std::cout << "select device " << properties.deviceName << "\n";
            return _device;
        }
    }
    /* No discrete gpu found, look for a device with at least geometry shaders */
    for (const auto &_device : _devices) {
        vkGetPhysicalDeviceFeatures(_device, &features);
        if (features.geometryShader) {
            return _device;
        }
    }
    /* well shit, we'll just hope for the best at this point */
    return _devices[0];
}

RendererDevice *VkRenderer::InitializeRendererDevice(VkPhysicalDevice physical_device) {
    if (physical_device == nullptr) {
        std::cout << "Selecting Physical Device\n";
        std::vector<VkPhysicalDevice> physical_devices = this->EnumeratePhysicalDevices();
        physical_device = this->PickPhysicalDevice(physical_devices);
    }

    AssertExit(this->device == nullptr);

    this->device = new RendererDevice();

    this->device->SetRequiredExtensions(this->requested_device_extensions);
    this->device->SetPhysicalDevice(physical_device);
    this->device->SetRenderSurface(this->surface);
    QueueFamilyIndices family_indices = this->device->FindQueueFamilies();

    /* No user specified queue families to create, so we just use the defaults */
    std::cout << "Found queue family indices\n";
    if (this->queue_families.empty()) {
        std::cout << "queue_families is empty! setting to defaults\n";
        this->SetQueueFamilies({
            family_indices.graphics_family.value(),
            family_indices.present_family.value()
        });
    }
    std::cout << "Creating Logical Device\n";
    this->device->CreateLogicalDevice(this->queue_families, this->requested_device_extensions);
    std::cout << "Finding the queues\n";

    /* Get the internal queues from our device */
    this->queue_graphics = device->GetQueue(family_indices.graphics_family.value(), 0);
    this->queue_present  = device->GetQueue(family_indices.present_family.value(), 0);

    return this->device;
}

void VkRenderer::InitializePipeline(RendererShader *render_shader) {
    this->pipeline = new RendererPipeline(this->device, this->swapchain);
    this->pipeline->CreateRenderPass();
    this->pipeline->Rebuild(render_shader);
}

void VkRenderer::InitializeSwapchain() {
    SwapchainSupportDetails sc_support = this->device->QuerySwapchainSupport();
    QueueFamilyIndices      qf_indices = this->device->FindQueueFamilies();

    this->swapchain = new RendererSwapchain(this->device, sc_support);
    this->swapchain->Create(this->surface, qf_indices);
}

std::vector<VkPhysicalDevice> VkRenderer::EnumeratePhysicalDevices() {
    uint32_t device_count = 0;

    vkEnumeratePhysicalDevices(this->instance, &device_count, nullptr);
    if (device_count == 0) {
        DebugLog(LogType::Fatal, "No devices with Vulkan support found! " \
                                 "Please update your graphics drivers or install a Vulkan compatible device.\n");
        throw std::runtime_error("No GPUs with Vulkan support found!");
    }

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(this->instance, &device_count, devices.data());

    return devices;
}

} // namespace hyperion