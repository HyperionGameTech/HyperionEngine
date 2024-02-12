#ifndef HYPERION_V2_BACKEND_RENDERER_VULKAN_DESCRIPTOR_SET2_HPP
#define HYPERION_V2_BACKEND_RENDERER_VULKAN_DESCRIPTOR_SET2_HPP

#include <core/Name.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/ArrayMap.hpp>
#include <core/lib/Mutex.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {
namespace renderer {
namespace platform {

template <>
class DescriptorSet2<Platform::VULKAN>
{
public:
    DescriptorSet2(const DescriptorSetLayout<Platform::VULKAN> &layout);
    DescriptorSet2(const DescriptorSet2 &other)                 = delete;
    DescriptorSet2 &operator=(const DescriptorSet2 &other)      = delete;
    DescriptorSet2(DescriptorSet2 &&other) noexcept             = delete;
    DescriptorSet2 &operator=(DescriptorSet2 &&other) noexcept  = delete;
    ~DescriptorSet2();
    
    void SetElement(const String &name, const GPUBufferRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, const GPUBufferRef<Platform::VULKAN> &ref);
    
    void SetElement(const String &name, const ImageViewRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, const ImageViewRef<Platform::VULKAN> &ref);
    
    void SetElement(const String &name, const SamplerRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, const SamplerRef<Platform::VULKAN> &ref);
    
    void SetElement(const String &name, const TLASRef<Platform::VULKAN> &ref);
    void SetElement(const String &name, uint index, const TLASRef<Platform::VULKAN> &ref);

private:
    DescriptorSetLayout<Platform::VULKAN>                       m_layout;
    ArrayMap<String, DescriptorSetElement<Platform::VULKAN>>    m_elements;
};

} // namespace renderer
} // namespace hyperion

#endif