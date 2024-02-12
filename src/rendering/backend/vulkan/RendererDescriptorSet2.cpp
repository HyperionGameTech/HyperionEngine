#include <rendering/backend/RendererDescriptorSet2.hpp>

#include <math/MathUtil.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
DescriptorSet2Ref<Platform::VULKAN> DescriptorSetLayout<Platform::VULKAN>::CreateDescriptorSet() const
{
    auto ref = MakeRenderObject<DescriptorSet2<Platform::VULKAN>>(*this);

    return ref;
}

template <>
DescriptorSet2Ref<Platform::VULKAN>::DescriptorSet2Ref(const DescriptorSetLayout<Platform::VULKAN> &layout)
    : m_layout(layout)
{
    // Initial layout of elements
    for (auto &it : m_layout.GetElements()) {
        const String &name = it.first;
        const DescriptorSetElementType type = it.second;

        switch (type) {
        case DescriptorSetElementType::UNIFORM_BUFFER:          // fallthrough
        case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:  // fallthrough
        case DescriptorSetElementType::STORAGE_BUFFER:          // fallthrough
        case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:  // fallthrough
            SetElement(name, GPUBufferRef<Platform::VULKAN> { });

            break;
        case DescriptorSetElementType::IMAGE:                   // fallthrough
        case DescriptorSetElementType::IMAGE_STORAGE:           // fallthrough
            SetElement(name, ImageViewRef<Platform::VULKAN> { });

            break;
        case DescriptorSetElementType::SAMPLER:
            SetElement(name, SamplerRef<Platform::VULKAN> { });

            break;
        case DescriptorSetElementType::TLAS:
            SetElement(name, TLASRef<Platform::VULKAN> { });

            break;
        default:
            AssertThrowMsg(false, "Unhandled descriptor set element type in layout: %d", int(type));

            break;
        }
    }
}

template <>
DescriptorSet2Ref<Platform::VULKAN>::~DescriptorSet2Ref()
{
}

template <>
void DescriptorSet2Ref<Platform::VULKAN>::SetElement(const String &name, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet2Ref<Platform::VULKAN>::SetElement(const String &name, uint index, const GPUBufferRef<Platform::VULKAN> &ref)
{
    auto element = m_layout.GetElement(name);
    AssertThrowMsg(element.HasValue(), "Invalid element: No item with name %s found", name.Data());

    static const uint32 mask = GetDescriptorSetElementTypeMask(
        DescriptorSetElementType::UNIFORM_BUFFER,
        DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC,
        DescriptorSetElementType::STORAGE_BUFFER,
        DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC
    );

    AssertThrowMsg(mask & (1u << uint32(element.Get())), "Layout type for %s does not match given type", name.Data());

    auto it = m_elements.Find(name);

    if (it == m_elements.End()) {
        m_elements.Insert({
            name,
            DescriptorSetElement<Platform::VULKAN>::ValueType(ref)
        });
    } else {
        it->second = DescriptorSetElement<Platform::VULKAN>::ValueType(ref);
    }
}

template <>
void DescriptorSet2Ref<Platform::VULKAN>::SetElement(const String &name, const ImageViewRef<Platform::VULKAN> &ref)
{
    
}

template <>
void DescriptorSet2Ref<Platform::VULKAN>::SetElement(const String &name, uint index, const ImageViewRef<Platform::VULKAN> &ref)
{
    
}

template <>
void DescriptorSet2Ref<Platform::VULKAN>::SetElement(const String &name, const SamplerRef<Platform::VULKAN> &ref)
{
    
}

template <>
void DescriptorSet2Ref<Platform::VULKAN>::SetElement(const String &name, uint index, const SamplerRef<Platform::VULKAN> &ref)
{
    
}

template <>
void DescriptorSet2Ref<Platform::VULKAN>::SetElement(const String &name, const TLASRef<Platform::VULKAN> &ref)
{
    
}

template <>
void DescriptorSet2Ref<Platform::VULKAN>::SetElement(const String &name, uint index, const TLASRef<Platform::VULKAN> &ref)
{
    
}

} // namespace platform
} // namespace renderer
} // namespace hyperion
