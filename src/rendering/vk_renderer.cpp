//
// Created by ethan on 2/5/22.
//

#include "vk_renderer.h"

#include "../system/sdl_system.h"

#include <vector>

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

VkDevice RendererDevice::Initialize(const std::set<uint32_t> &required_queue_families, const std::vector<const char *> &required_extensions) {
    std::vector<VkDeviceQueueCreateInfo> queue_create_info_vec;
    const float priorities[] = {1.0f};
    // for each queue family(for separate threads) we add them to
    // our device initialization data
    for (uint32_t family: required_queue_families) {
        VkDeviceQueueCreateInfo queue_info{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue_info.pQueuePriorities = priorities;
        queue_info.queueCount = 1;
        queue_info.queueFamilyIndex = family;

        queue_create_info_vec.push_back(queue_info);
    }

    VkDeviceCreateInfo create_info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create_info.pQueueCreateInfos = queue_create_info_vec.data();
    create_info.queueCreateInfoCount = (uint32_t) (queue_create_info_vec.size());
    // Setup Device extensions
    create_info.enabledExtensionCount = (uint32_t) (required_extensions.size());
    create_info.ppEnabledExtensionNames = required_extensions.data();

    VkDevice _device;
    VkResult result = vkCreateDevice(this->physical, &create_info, nullptr, &_device);
    if (result != VK_SUCCESS)
        throw std::runtime_error("Could not create RendererDevice!");
    this->device = _device;
    return this->device;
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

void VkRenderer::SetCurrentWindow(const SystemWindow &_window) {
    this->window = _window;
}

SystemWindow VkRenderer::GetCurrentWindow() {
    return this->window;
}

VkRenderer::VkRenderer(SystemSDL &_system, const char *app_name, const char *engine_name) {
    // Application names/versions
    this->SetCurrentWindow(_system.GetCurrentWindow());

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
    extension_names = _system.GetVulkanExtensionNames();

    create_info.enabledExtensionCount = extension_names.size();
    create_info.ppEnabledExtensionNames = extension_names.data();

    VkResult result = vkCreateInstance(&create_info, nullptr, &this->instance);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Vulkan Instance!");
    }
}

uint32_t VkRenderer::FindQueueFamily(VkPhysicalDevice _device) {
    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(_device, &queue_family_count, nullptr);
    std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(_device, &queue_family_count, queue_families.data());

    std::cout << "VK Queue Families supported: " << queue_family_count << "\n";

    int index = 0;
    for (const auto &family : queue_families) {
        VkBool32 supports_presentation = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(_device, index, this->surface, &supports_presentation);

        if ((family.queueFlags & VK_QUEUE_GRAPHICS_BIT) && supports_presentation)
            return i;

        index++;
    }
    return 0;
}

VkDevice VkRenderer::InitDevice(const VkPhysicalDevice &physical,
                                std::set<uint32_t> unique_queue_families,
                                const std::vector<const char *> &required_extensions)
{
    s
}

void VkRenderer::SetRendererDevice(const VkDevice &_device) {
    this->

}

VkPhysicalDevice VkRenderer::PickPhysicalDevice(std::vector<VkPhysicalDevice> _devices) {
    VkPhysicalDeviceProperties properties;
    VkPhysicalDeviceFeatures   features;

    // Check for a Discrete GPU with geometry shaders
    for (const auto &device : _devices) {
        VkGetPhysicalDeviceProperties(deivce, &properties);
        VkGetPhysicalDeviceFeatures(device, &features);

        // Check for discrete GPU
        if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && features.geometryShader) {
            return device;
        }
    }
    // No discrete GPU, but look for one with  at least geometry shaders
    for (const auto &device : _devices) {
        VkGetPhysicalDeviceFeatures(device, &features);
        // Check for geometry shader
        if (features.geometryShader) {
            return device;
        }
    }
    // well shit
    return _devices[0];
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
