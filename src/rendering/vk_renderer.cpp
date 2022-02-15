//
// Created by ethan on 2/5/22.
//

#include "vk_renderer.h"

#include "../system/sdl_system.h"
#include "../system/debug.h"

#include <vector>
#include <iostream>
#include <optional>


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

VkDevice RendererDevice::GetDevice() {
    return this->device;
}

VkPhysicalDevice RendererDevice::GetPhysicalDevice() {
    return this->physical;
}

RendererDevice::RendererDevice() {
    this->device = nullptr;
    this->physical = nullptr;
}

RendererDevice::~RendererDevice() {
    /* By the time this destructor is called there should never
     * be a running queue, but just in case we will wait until
     * all the queues on our device are stopped. */
    vkDeviceWaitIdle(this->device);
    vkDestroyDevice(this->device, nullptr);
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

std::vector<VkExtensionProperties> RendererDevice::CheckExtensionSupport(std::vector<const char *> required_extensions) {
    auto supported = this->GetSupportedExtensions();
    std::vector<VkExtensionProperties> unsupported_extensions;

    for (auto &required_ext_name : required_extensions) {
        /* For each extension required, check */
        auto it = std::find_if(
                supported.begin(),
                supported.end(),
                [required_ext_name] (VkExtensionProperties &property) {
                    return (property.extensionName == required_ext_name);
                }
        );
        if (it != supported.end()) {
            unsupported_extensions.push_back(*it);
        }
    }
    return unsupported_extensions;
}

VkDevice RendererDevice::CreateLogicalDevice(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions) {
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

    auto unsupported_extensions = this->CheckExtensionSupport(required_extensions);
    if (!unsupported_extensions.empty()) {
        DebugLog(LogType::Warn, "--- Unsupported Extensions ---\n");
        for (const auto &extension : unsupported_extensions) {
            DebugLog(LogType::Warn, "\t%s (spec-ver: %d)\n", extension.extensionName, extension.specVersion);
        }
        DebugLog(LogType::Error, "Vulkan: Device does not support required extensions\n");
        throw std::runtime_error("Device does not support required extensions");
    }
    else {
        DebugLog(LogType::Info, "Vulkan: loaded %d device extension(s)\n", required_extensions.size());
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
    /* We need to make sure that the current device is destroyed
     * before we free the rest of the renderer */
    this->device.~RendererDevice();
    vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
    vkDestroyInstance(this->instance, nullptr);
}

void VkRenderer::SetQueueFamilies(std::set<uint32_t> _queue_families) {
    this->queue_families = _queue_families;
}

uint32_t VkRenderer::FindQueueFamily(VkPhysicalDevice _device) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(_device, &queue_family_count, families.data());

    std::cout << "VK Queue Families supported: " << queue_family_count << "\n";

    int index = 0;
    for (const auto &family : families) {
        VkBool32 supports_presentation = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(_device, index, this->surface, &supports_presentation);

        if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && supports_presentation)
            return index;

        index++;
    }
    return 0;
}

QueueFamilyIndices VkRenderer::FindQueueFamilies(VkPhysicalDevice _physical_device) {
    if (_physical_device == nullptr) {
        /* No alternate physical device passed in, so
         * we'll just use the currently selected one */
        _physical_device = this->device.GetPhysicalDevice();
    }
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
        vkGetPhysicalDeviceSurfaceSupportKHR(_physical_device, index, this->surface, &supports_presentation);
        if (supports_presentation)
            indices.present_family = index;

        /* Everything was found, return the indices */
        if (indices.is_complete())
            break;

        index++;
    }
    std::cout << "Are all queue families supported? [" << indices.is_complete() << "]\n";
    std::cout << "\tGraphical: [" << indices.graphics_family.has_value() << "] Presentation: [" << indices.present_family.has_value() << "]\n";
    return indices;
}

//VkDevice VkRenderer::InitDevice(const VkPhysicalDevice &physical,
//                                std::set<uint32_t> unique_queue_families,
//                          <      const std::vector<const char *> &required_extensions)
//{
//
//}


void VkRenderer::SetRendererDevice(const RendererDevice &_device) {
    this->device = _device;
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
    QueueFamilyIndices family_indices = this->FindQueueFamilies(physical_device);

    /* No user specified queue families to create, so we just use the defaults */
    std::cout << "Found queue family indices\n";
    if (this->queue_families.empty()) {
        std::cout << "queue_families is empty! setting to defaults\n";
        this->SetQueueFamilies({
            family_indices.graphics_family.value(),
            family_indices.present_family.value()
        });
    }
    this->device.SetPhysicalDevice(physical_device);
    std::cout << "Creating Logical Device\n";
    this->device.CreateLogicalDevice(this->queue_families, this->requested_device_extensions);
    std::cout << "Finding the queues\n";

    /* Get the internal queues from our device */
    this->queue_graphics = device.GetQueue(family_indices.graphics_family.value(), 0);
    this->queue_present  = device.GetQueue(family_indices.present_family.value(), 0);

    return &this->device;
}

std::vector<VkPhysicalDevice> VkRenderer::EnumeratePhysicalDevices() {
    uint32_t device_count = 0;

    vkEnumeratePhysicalDevices(this->instance, &device_count, nullptr);
    if (device_count == 0)
        throw std::runtime_error("No GPUs with Vulkan support found!");

    std::vector<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(this->instance, &device_count, devices.data());

    return devices;
}
