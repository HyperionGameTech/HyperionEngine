#include "RxDsLayoutBuilder.hpp"

#include <Renderer/RxGlobals.hpp>
#include <Renderer/RxRenderBackend.hpp>


RxDsLayoutBuilder& RxDsLayoutBuilder::AddBinding(int binding, VkDescriptorType type, RxShaderType stage, int count)
{
    const VkSampler* pcImmutableSamplers = nullptr;

    mLayoutBindings.emplace_back(binding, type, count, RxShaderUtil::ToUnderlyingType(stage), pcImmutableSamplers);

    return *this;
}


VkDescriptorSetLayout RxDsLayoutBuilder::Build()
{
    VkDescriptorSetLayoutCreateInfo create_info {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = static_cast<uint32>(mLayoutBindings.size()),
        .pBindings = mLayoutBindings.data(),
    };

    VkResult status = vkCreateDescriptorSetLayout(gRenderer->GetDevice()->Device, &create_info, nullptr, &mpDsLayout);

    if (status != VK_SUCCESS) {
        FxLogError("Error building descriptor set layout with builder! (status={})", RxUtil::ResultToStr(status));
        return nullptr;
    }

    return mpDsLayout;
}
