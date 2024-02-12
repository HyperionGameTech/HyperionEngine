#ifndef HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET2_HPP
#define HYPERION_V2_BACKEND_RENDERER_DESCRIPTOR_SET2_HPP

#include <core/Name.hpp>
#include <core/lib/Optional.hpp>
#include <core/lib/RefCountedPtr.hpp>
#include <core/lib/ArrayMap.hpp>
#include <core/lib/FixedArray.hpp>
#include <rendering/backend/Platform.hpp>
#include <rendering/backend/RenderObject.hpp>
#include <Types.hpp>
#include <util/Defines.hpp>


namespace hyperion {
namespace renderer {

namespace platform {

template <PlatformType PLATFORM>
class Device;
    
template <PlatformType PLATFORM>
class Instance;

enum class DescriptorSetElementType : uint32
{
    UNSET,
    UNIFORM_BUFFER,
    UNIFORM_BUFFER_DYNAMIC,
    STORAGE_BUFFER,
    STORAGE_BUFFER_DYNAMIC,
    IMAGE,
    IMAGE_STORAGE,
    SAMPLER,
    TLAS,
    MAX
};

template <PlatformType PLATFORM>
class DescriptorSet2;

template <DescriptorSetElementType ... Types>
static inline uint32 GetDescriptorSetElementTypeMask(Types ... types)
{
    return (1u << uint32(types)) | ...;
}

template <PlatformType PLATFORM>
class DescriptorSetLayout
{
public:
    DescriptorSetLayout()                                                   = default;
    DescriptorSetLayout(const DescriptorSetLayout &other)                   = default;
    DescriptorSetLayout &operator=(const DescriptorSetLayout &other)        = default;
    DescriptorSetLayout(DescriptorSetLayout &&other) noexcept               = default;
    DescriptorSetLayout &operator=(DescriptorSetLayout &&other) noexcept    = default;
    ~DescriptorSetLayout()                                                  = default;

    DescriptorSet2Ref<PLATFORM> CreateDescriptorSet() const;

    const ArrayMap<String, DescriptorSetElementType> &GetElements() const
        { return m_elements; }

    void AddElement(const String &name, DescriptorSetElementType type)
        { m_elements.Insert(name, type); }

    Optional<DescriptorSetElementType> GetElement(const String &name) const
    {
        const auto it = m_elements.Find(name);

        if (it == m_elements.End()) {
            return { };
        }

        return it->second;
    }

private:
    ArrayMap<String, DescriptorSetElementType>  m_elements;
};

template <PlatformType PLATFORM>
struct DescriptorSetElement
{
    using ValueType = Variant<GPUBufferRef<PLATFORM>, ImageViewRef<PLATFORM>, SamplerRef<PLATFORM>, TLASRef<PLATFORM>>;

    ValueType   value;
    bool        dirty = false;
};

template <PlatformType PLATFORM>
class DescriptorSet2
{
public:
    DescriptorSet2(const DescriptorSetLayout<PLATFORM> &layout);
    DescriptorSet2(const DescriptorSet2 &other)                 = delete;
    DescriptorSet2 &operator=(const DescriptorSet2 &other)      = delete;
    DescriptorSet2(DescriptorSet2 &&other) noexcept             = delete;
    DescriptorSet2 &operator=(DescriptorSet2 &&other) noexcept  = delete;
    ~DescriptorSet2();
    
    void SetElement(const String &name, const GPUBufferRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const GPUBufferRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const ImageViewRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const ImageViewRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const SamplerRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const SamplerRef<PLATFORM> &ref);
    
    void SetElement(const String &name, const TLASRef<PLATFORM> &ref);
    void SetElement(const String &name, uint index, const TLASRef<PLATFORM> &ref);

private:
    DescriptorSetLayout<PLATFORM>                       m_layout;
    ArrayMap<String, DescriptorSetElement<PLATFORM>>    m_elements;
};

} // namespace platform
} // namespace renderer
} // namespace hyperion


#if HYP_VULKAN
#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#else
#error Unsupported rendering backend
#endif

namespace hyperion {
namespace renderer {

using DescriptorSet2 = platform::DescriptorSet2<Platform::CURRENT>;

} // namespace renderer
} // namespace hyperion

#endif