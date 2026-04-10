#pragma once

#include <vulkan/vulkan.h>

#include <Renderer/Backend/RxShader.hpp>
#include <vector>

class RxDsLayoutBuilder
{
public:
    RxDsLayoutBuilder() = default;

    RxDsLayoutBuilder& AddBinding(int binding, VkDescriptorType type, RxShaderType stage, int count = 1);

    VkDescriptorSetLayout Build();

private:
    // TODO: Replace usage of std::vector with custom dynamic array. FxPagedArray does not resize into a contiguous
    // buffer, so that does not work here.
    std::vector<VkDescriptorSetLayoutBinding> mLayoutBindings {};
    VkDescriptorSetLayout mpDsLayout = nullptr;
};
