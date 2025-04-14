/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <core/math/MathUtil.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

#include <vulkan/vulkan.h>

#define HYP_DESCRIPTOR_SET_AUTO_UPDATE

namespace hyperion {
namespace renderer {
namespace platform {

#pragma region Helper methods

static VkDescriptorType ToVkDescriptorType(DescriptorSetElementType type)
{
    switch (type) {
    case DescriptorSetElementType::UNIFORM_BUFFER:         return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
    case DescriptorSetElementType::STORAGE_BUFFER:         return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
    case DescriptorSetElementType::IMAGE:                  return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case DescriptorSetElementType::SAMPLER:                return VK_DESCRIPTOR_TYPE_SAMPLER;
    case DescriptorSetElementType::IMAGE_STORAGE:          return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case DescriptorSetElementType::TLAS:                   return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
    default:
        AssertThrowMsg(false, "Unsupported descriptor type for Vulkan");
    }
}

#pragma endregion Helper methods

#pragma region Vulkan struct wrappers

struct VulkanDescriptorSetLayoutWrapper
{
    VkDescriptorSetLayout   vk_layout = VK_NULL_HANDLE;

    RendererResult Create(Device<Platform::VULKAN> *device, const DescriptorSetLayout<Platform::VULKAN> &layout)
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
            binding.descriptorType = ToVkDescriptorType(element.type);
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

    RendererResult Destroy(Device<Platform::VULKAN> *device)
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

#pragma region DescriptorSetPlatformImpl

VkDescriptorSetLayout DescriptorSetPlatformImpl<Platform::VULKAN>::GetVkDescriptorSetLayout() const
{
    if (!vk_layout_wrapper) {
        return VK_NULL_HANDLE;
    }

    return vk_layout_wrapper->vk_layout;
}

#pragma endregion DescriptorSetPlatformImpl

#pragma region DescriptorSetLayout

template <>
DescriptorSetLayout<Platform::VULKAN>::DescriptorSetLayout(const DescriptorSetDeclaration &decl)
    : m_decl(decl)
{
    const DescriptorSetDeclaration *decl_ptr = &decl;

    if (decl.is_reference) {
        decl_ptr = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(decl.name);

        AssertThrowMsg(decl_ptr != nullptr, "Invalid global descriptor set reference: %s", decl.name.LookupString());
    }

    for (const Array<DescriptorDeclaration> &slot : decl_ptr->slots) {
        for (const DescriptorDeclaration &descriptor : slot) {
            const uint32 descriptor_index = decl_ptr->CalculateFlatIndex(descriptor.slot, descriptor.name);
            AssertThrow(descriptor_index != ~0u);
            
            if (descriptor.cond != nullptr && !descriptor.cond()) {
                // Skip this descriptor, condition not met
                continue;
            }

            // HYP_LOG(RenderingBackend, Debug, "Set element {}.{}[{}] (slot: {}, count: {}, size: {}, is_dynamic: {})",
            //     decl_ptr->name, descriptor.name, descriptor_index, int(descriptor.slot),
            //     descriptor.count, descriptor.size, descriptor.is_dynamic);

            switch (descriptor.slot) {
            case DescriptorSlot::DESCRIPTOR_SLOT_SRV:
                AddElement(descriptor.name, DescriptorSetElementType::IMAGE, descriptor_index, descriptor.count);
    
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_UAV:
                AddElement(descriptor.name, DescriptorSetElementType::IMAGE_STORAGE, descriptor_index, descriptor.count);
                    
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_CBUFF:
                if (descriptor.is_dynamic) {
                    AddElement(descriptor.name, DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC, descriptor_index, descriptor.count, descriptor.size);
                } else {
                    AddElement(descriptor.name, DescriptorSetElementType::UNIFORM_BUFFER, descriptor_index, descriptor.count, descriptor.size);
                }
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_SSBO:
                if (descriptor.is_dynamic) {
                    AddElement(descriptor.name, DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC, descriptor_index, descriptor.count, descriptor.size);
                } else {
                    AddElement(descriptor.name, DescriptorSetElementType::STORAGE_BUFFER, descriptor_index, descriptor.count, descriptor.size);
                }
                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_ACCELERATION_STRUCTURE:
                AddElement(descriptor.name, DescriptorSetElementType::TLAS, descriptor_index, descriptor.count);

                break;
            case DescriptorSlot::DESCRIPTOR_SLOT_SAMPLER:
                AddElement(descriptor.name, DescriptorSetElementType::SAMPLER, descriptor_index, descriptor.count);

                break;
            default:
                AssertThrowMsg(false, "Invalid descriptor slot");
                break;
            }
        }
    }

    // build a list of dynamic elements, paired by their element index so we can sort it after.
    Array<Pair<Name, uint32>> dynamic_elements_with_index;

    // Add to list of dynamic buffer names
    for (const auto &it : m_elements) {
        if (it.second.type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
            || it.second.type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC)
        {
            dynamic_elements_with_index.PushBack({ it.first, it.second.binding });
        }
    }

    std::sort(dynamic_elements_with_index.Begin(), dynamic_elements_with_index.End(), [](const Pair<Name, uint32> &a, const Pair<Name, uint32> &b)
    {
        return a.second < b.second;
    });

    m_dynamic_elements.Resize(dynamic_elements_with_index.Size());

    for (SizeType i = 0; i < dynamic_elements_with_index.Size(); i++) {
        m_dynamic_elements[i] = std::move(dynamic_elements_with_index[i].first);
    }
}

template <>
DescriptorSetRef<Platform::VULKAN> DescriptorSetLayout<Platform::VULKAN>::CreateDescriptorSet() const
{
#ifdef HYP_DEBUG_MODE
    DescriptorSetRef<Platform::VULKAN> descriptor_set_ref = MakeRenderObject<DescriptorSet<Platform::VULKAN>>(*this);
    
    // Set debug name
    descriptor_set_ref.SetName(m_decl.name);

    return descriptor_set_ref;
#else
    return MakeRenderObject<DescriptorSet<Platform::VULKAN>>(*this);
#endif
}

#pragma endregion DescriptorSetLayout

#pragma region DescriptorSet

template <>
DescriptorSet<Platform::VULKAN>::DescriptorSet(const DescriptorSetLayout<Platform::VULKAN> &layout)
    : m_platform_impl { this, VK_NULL_HANDLE },
      m_layout(layout)
{
    // Initial layout of elements
    for (auto &it : m_layout.GetElements()) {
        const Name name = it.first;
        const DescriptorSetLayoutElement &element = it.second;

        switch (element.type) {
        case DescriptorSetElementType::UNIFORM_BUFFER:          // fallthrough
        case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:  // fallthrough
        case DescriptorSetElementType::STORAGE_BUFFER:          // fallthrough
        case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:  // fallthrough
            PrefillElements<GPUBufferRef<Platform::VULKAN>>(name, element.count);

            break;
        case DescriptorSetElementType::IMAGE:                   // fallthrough
        case DescriptorSetElementType::IMAGE_STORAGE:           // fallthrough
            PrefillElements<ImageViewRef<Platform::VULKAN>>(name, element.count);

            break;
        case DescriptorSetElementType::SAMPLER:
            PrefillElements<SamplerRef<Platform::VULKAN>>(name, element.count);

            break;
        case DescriptorSetElementType::TLAS:
            PrefillElements<TLASRef<Platform::VULKAN>>(name, element.count);

            break;
        default:
            AssertThrowMsg(false, "Unhandled descriptor set element type in layout: %d", int(element.type));

            break;
        }
    }
}

template <>
DescriptorSet<Platform::VULKAN>::~DescriptorSet()
{
}

template <>
void DescriptorSet<Platform::VULKAN>::Update(Device<Platform::VULKAN> *device)
{
    static_assert(std::is_trivial_v<VulkanDescriptorElementInfo>, "VulkanDescriptorElementInfo should be a trivial type for fast copy and move operations");

    AssertThrow(m_platform_impl.handle != VK_NULL_HANDLE);

    Array<VulkanDescriptorElementInfo> dirty_descriptor_element_infos;

#ifdef HYP_DESCRIPTOR_SET_AUTO_UPDATE
    // Ensure all cached value containers are prepared
    for (auto &it : m_elements) {
        const Name name = it.first;
        const DescriptorSetElement<Platform::VULKAN> &element = it.second;

        auto cached_it = m_platform_impl.cached_elements.Find(name);

        if (cached_it == m_platform_impl.cached_elements.End()) {
            cached_it = m_platform_impl.cached_elements.Insert(name, Array<DescriptorSetElementCachedValue>()).first;
        }

        Array<DescriptorSetElementCachedValue> &cached_values = cached_it->second;

        if (cached_values.Size() != element.values.Size()) {
            cached_values.Resize(element.values.Size());
        }
    }
#endif

    // detect changes from cached_values
    for (auto &it : m_elements) {
        const Name name = it.first;
        DescriptorSetElement<Platform::VULKAN> &element = it.second;

        const DescriptorSetLayoutElement *layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.LookupString());

#ifdef HYP_DESCRIPTOR_SET_AUTO_UPDATE
        auto cached_it = m_platform_impl.cached_elements.Find(name);
        AssertDebug(cached_it != m_platform_impl.cached_elements.End());

        Array<DescriptorSetElementCachedValue> &cached_values = cached_it->second;
        AssertDebug(cached_values.Size() == element.values.Size());
#else
        if (!element.IsDirty()) {
            continue;
        }
#endif
        
#ifdef HYP_DESCRIPTOR_SET_AUTO_UPDATE
        for (auto &values_it : element.values) {
            const uint32 index = values_it.first;
            const DescriptorSetElement<Platform::VULKAN>::ValueType &value = values_it.second;
#else
        for (uint32 index = element.dirty_range.GetStart(); index < element.dirty_range.GetEnd(); index++) {
            const const DescriptorSetElement<Platform::VULKAN>::ValueType &value = element.values.At(index);
#endif

            VulkanDescriptorElementInfo descriptor_element_info { };
            descriptor_element_info.binding = layout_element->binding;
            descriptor_element_info.index = index;
            descriptor_element_info.descriptor_type = ToVkDescriptorType(layout_element->type);

            if (value.Is<GPUBufferRef<Platform::VULKAN>>()) {
                const bool layout_has_size = layout_element->size != 0 && layout_element->size != ~0u;

                const bool is_dynamic = layout_element->type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
                    || layout_element->type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC;

                const GPUBufferRef<Platform::VULKAN> &ref = value.Get<GPUBufferRef<Platform::VULKAN>>();
                AssertThrowMsg(ref.IsValid(), "Invalid buffer reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                if (is_dynamic) {
                    AssertThrowMsg(layout_element->size != 0, "Buffer size not set for dynamic buffer element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);
                }

                AssertThrowMsg(ref->IsCreated(), "Buffer not initialized for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);
                
                descriptor_element_info.buffer_info = VkDescriptorBufferInfo {
                    .buffer = ref->GetPlatformImpl().handle,
                    .offset = 0,
                    .range  = layout_has_size ? layout_element->size : ref->Size()
                };
            } else if (value.Is<ImageViewRef<Platform::VULKAN>>()) {
                const bool is_storage_image = layout_element->type == DescriptorSetElementType::IMAGE_STORAGE;

                const ImageViewRef<Platform::VULKAN> &ref = value.Get<ImageViewRef<Platform::VULKAN>>();
                AssertThrowMsg(ref.IsValid(), "Invalid image view reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                AssertThrowMsg(ref->GetPlatformImpl().handle != VK_NULL_HANDLE, "Invalid image view for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptor_element_info.image_info = VkDescriptorImageInfo {
                    .sampler        = VK_NULL_HANDLE,
                    .imageView      = ref->GetPlatformImpl().handle,
                    .imageLayout    = is_storage_image ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
            } else if (value.Is<SamplerRef<Platform::VULKAN>>()) {
                const SamplerRef<Platform::VULKAN> &ref = value.Get<SamplerRef<Platform::VULKAN>>();
                AssertThrowMsg(ref.IsValid(), "Invalid sampler reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                AssertThrowMsg(ref->GetPlatformImpl().handle != VK_NULL_HANDLE, "Invalid sampler for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptor_element_info.image_info = VkDescriptorImageInfo {
                    .sampler        = ref->GetPlatformImpl().handle,
                    .imageView      = VK_NULL_HANDLE,
                    .imageLayout    = VK_IMAGE_LAYOUT_UNDEFINED
                };
            } else if (value.Is<TLASRef<Platform::VULKAN>>()) {
                const TLASRef<Platform::VULKAN> &ref = value.Get<TLASRef<Platform::VULKAN>>();
                AssertThrowMsg(ref.IsValid(), "Invalid TLAS reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                AssertThrowMsg(ref->GetAccelerationStructure() != VK_NULL_HANDLE, "Invalid TLAS for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptor_element_info.acceleration_structure_info = VkWriteDescriptorSetAccelerationStructureKHR {
                    .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .pNext                      = nullptr,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures    = &ref->GetAccelerationStructure()
                };
            } else {
                HYP_UNREACHABLE();
            }
            
#ifdef HYP_DESCRIPTOR_SET_AUTO_UPDATE
            AssertThrowMsg(index < cached_values.Size(), "Index out of range for cached values");

            if (Memory::MemCmp(&cached_values[index].info, &descriptor_element_info, sizeof(VulkanDescriptorElementInfo)) != 0) {
                Memory::MemCpy(&cached_values[index].info, &descriptor_element_info, sizeof(VulkanDescriptorElementInfo));

                // mark the element as dirty
                element.dirty_range |= { index, index + 1 };

                dirty_descriptor_element_infos.PushBack(descriptor_element_info);

                // HYP_LOG(RenderingBackend, Debug, "Marked Dirty descriptor set element: {}.{}[{}]", m_layout.GetName().LookupString(), name.LookupString(), index);
            }
#else
            dirty_descriptor_element_infos.PushBack(descriptor_element_info);
#endif
        }
    }

    Array<VkWriteDescriptorSet> vk_write_descriptor_sets;
    vk_write_descriptor_sets.Resize(dirty_descriptor_element_infos.Size());

    for (SizeType i = 0; i < vk_write_descriptor_sets.Size(); i++) {
        const VulkanDescriptorElementInfo &descriptor_element_info = dirty_descriptor_element_infos[i];

        VkWriteDescriptorSet &write = vk_write_descriptor_sets[i];
        
        write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = m_platform_impl.handle;
        write.dstBinding = descriptor_element_info.binding;
        write.dstArrayElement = descriptor_element_info.index;
        write.descriptorCount = 1;
        write.descriptorType = descriptor_element_info.descriptor_type;

        if (descriptor_element_info.descriptor_type == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR) {
            write.pNext = &descriptor_element_info.acceleration_structure_info;
        }
        
        write.pImageInfo = &descriptor_element_info.image_info;
        write.pBufferInfo = &descriptor_element_info.buffer_info;
    }

    vkUpdateDescriptorSets(
        device->GetDevice(),
        uint32(vk_write_descriptor_sets.Size()),
        vk_write_descriptor_sets.Data(),
        0,
        nullptr
    );

    for (auto &it : m_elements) {
        DescriptorSetElement<Platform::VULKAN> &element = it.second;

        element.dirty_range = { };
    }
}

template <>
RendererResult DescriptorSet<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_platform_impl.handle == VK_NULL_HANDLE);

    m_platform_impl.vk_layout_wrapper = device->GetDescriptorSetManager()->GetOrCreateVkDescriptorSetLayout(device, m_layout);

    RendererResult result;

    HYPERION_PASS_ERRORS(
        device->GetDescriptorSetManager()->CreateDescriptorSet(device, m_platform_impl.vk_layout_wrapper, m_platform_impl.handle),
        result
    );

    if (!result) {
        return result;
    }

    for (const Pair<Name, DescriptorSetElement<Platform::VULKAN>> &it : m_elements) {
        const Name name = it.first;
        const DescriptorSetElement<Platform::VULKAN> &element = it.second;

        m_platform_impl.cached_elements.Insert({
            name,
            Array<DescriptorSetElementCachedValue>(element.values.Size())
        });
    }

    Update(device);

    return result;
}

template <>
RendererResult DescriptorSet<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (m_platform_impl.handle != VK_NULL_HANDLE) {
        device->GetDescriptorSetManager()->DestroyDescriptorSet(device, m_platform_impl.handle);
        m_platform_impl.handle = VK_NULL_HANDLE;
    }

    // Release reference to layout
    m_platform_impl.vk_layout_wrapper.Reset();

    return RendererResult { };
}

template <>
bool DescriptorSet<Platform::VULKAN>::IsCreated() const
{
    return m_platform_impl.handle != VK_NULL_HANDLE;
}

template <>
bool DescriptorSet<Platform::VULKAN>::HasElement(Name name) const
{
    return m_elements.Find(name) != m_elements.End();
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint32 index, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement<GPUBufferRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint32 index, uint32 buffer_size, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement<GPUBufferRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint32 index, const ImageViewRef<Platform::VULKAN> &ref)
{
    SetElement<ImageViewRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, const ImageViewRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint32 index, const SamplerRef<Platform::VULKAN> &ref)
{
    SetElement<SamplerRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, const SamplerRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint32 index, const TLASRef<Platform::VULKAN> &ref)
{
    SetElement<TLASRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, const TLASRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const GraphicsPipeline<Platform::VULKAN> *pipeline, uint32 bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetPlatformImpl().command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->Pipeline<Platform::VULKAN>::GetPlatformImpl().layout,
        bind_index,
        1,
        &m_platform_impl.handle,
        0,
        nullptr
    );
}

template <>
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const GraphicsPipeline<Platform::VULKAN> *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const
{
    HashSet<Name> used_offsets;

    Array<uint32> offsets_flat;
    offsets_flat.Resize(m_layout.GetDynamicElements().Size());

    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++) {
        const Name dynamic_element_name = m_layout.GetDynamicElements()[i];

        const auto it = offsets.Find(dynamic_element_name);

        if (it != offsets.End()) {
            offsets_flat[i] = it->second;

            used_offsets.Insert(dynamic_element_name);
        } else {
            offsets_flat[i] = 0;

#ifdef HYP_DEBUG_MODE
            // HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamic_element_name);
#endif
        }
    }

#ifdef HYP_DEBUG_MODE
    for (const auto &it : offsets) {
        if (!used_offsets.Contains(it.first)) {
            HYP_LOG(RenderingBackend, Warning, "Unused dynamic offset for descriptor set element: {}", it.first);
        }
    }
#endif

    vkCmdBindDescriptorSets(
        command_buffer->GetPlatformImpl().command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->Pipeline<Platform::VULKAN>::GetPlatformImpl().layout,
        bind_index,
        1,
        &m_platform_impl.handle,
        uint32(offsets_flat.Size()),
        offsets_flat.Data()
    );
}

template <>
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const ComputePipeline<Platform::VULKAN> *pipeline, uint32 bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetPlatformImpl().command_buffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->Pipeline<Platform::VULKAN>::GetPlatformImpl().layout,
        bind_index,
        1,
        &m_platform_impl.handle,
        0,
        nullptr
    );
}

template <>
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const ComputePipeline<Platform::VULKAN> *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const
{
    HashSet<Name> used_offsets;

    Array<uint32> offsets_flat;
    offsets_flat.Resize(m_layout.GetDynamicElements().Size());

    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++) {
        const Name dynamic_element_name = m_layout.GetDynamicElements()[i];

        const auto it = offsets.Find(dynamic_element_name);

        if (it != offsets.End()) {
            offsets_flat[i] = it->second;

            used_offsets.Insert(dynamic_element_name);
        } else {
            offsets_flat[i] = 0;

#ifdef HYP_DEBUG_MODE
            // HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamic_element_name);
#endif
        }
    }

#ifdef HYP_DEBUG_MODE
    for (const auto &it : offsets) {
        if (!used_offsets.Contains(it.first)) {
            HYP_LOG(RenderingBackend, Warning, "Unused dynamic offset for descriptor set element: {}", it.first);
        }
    }
#endif

    vkCmdBindDescriptorSets(
        command_buffer->GetPlatformImpl().command_buffer,
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->Pipeline<Platform::VULKAN>::GetPlatformImpl().layout,
        bind_index,
        1,
        &m_platform_impl.handle,
        uint32(offsets_flat.Size()),
        offsets_flat.Data()
    );
}

template <>
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const RaytracingPipeline<Platform::VULKAN> *pipeline, uint32 bind_index) const
{
    vkCmdBindDescriptorSets(
        command_buffer->GetPlatformImpl().command_buffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline->Pipeline<Platform::VULKAN>::GetPlatformImpl().layout,
        bind_index,
        1,
        &m_platform_impl.handle,
        0,
        nullptr
    );
}

template <>
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const RaytracingPipeline<Platform::VULKAN> *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const
{
    HashSet<Name> used_offsets;

    Array<uint32> offsets_flat;
    offsets_flat.Resize(m_layout.GetDynamicElements().Size());

    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++) {
        const Name dynamic_element_name = m_layout.GetDynamicElements()[i];

        const auto it = offsets.Find(dynamic_element_name);

        if (it != offsets.End()) {
            offsets_flat[i] = it->second;

            used_offsets.Insert(dynamic_element_name);
        } else {
            offsets_flat[i] = 0;

#ifdef HYP_DEBUG_MODE
            // HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamic_element_name);
#endif
        }
    }

#ifdef HYP_DEBUG_MODE
    for (const auto &it : offsets) {
        if (!used_offsets.Contains(it.first)) {
            HYP_LOG(RenderingBackend, Warning, "Unused dynamic offset for descriptor set element: {}", it.first);
        }
    }
#endif

    vkCmdBindDescriptorSets(
        command_buffer->GetPlatformImpl().command_buffer,
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline->Pipeline<Platform::VULKAN>::GetPlatformImpl().layout,
        bind_index,
        1,
        &m_platform_impl.handle,
        uint32(offsets_flat.Size()),
        offsets_flat.Data()
    );
}

template <>
DescriptorSetRef<Platform::VULKAN> DescriptorSet<Platform::VULKAN>::Clone() const
{
#ifdef HYP_DEBUG_MODE
    DescriptorSetRef<Platform::VULKAN> descriptor_set_ref = MakeRenderObject<DescriptorSet<Platform::VULKAN>>(GetLayout());
    
    // Set debug name
    descriptor_set_ref.SetName(m_layout.GetName());

    return descriptor_set_ref;
#else
    return MakeRenderObject<DescriptorSet<Platform::VULKAN>>(GetLayout());
#endif
}

#pragma endregion DescriptorSet

#pragma region DescriptorSetManager

DescriptorSetManager<Platform::VULKAN>::DescriptorSetManager()
    : m_vk_descriptor_pool(VK_NULL_HANDLE)
{
}

DescriptorSetManager<Platform::VULKAN>::~DescriptorSetManager() = default;

RendererResult DescriptorSetManager<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
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

RendererResult DescriptorSetManager<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
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

RendererResult DescriptorSetManager<Platform::VULKAN>::CreateDescriptorSet(Device<Platform::VULKAN> *device, const RC<VulkanDescriptorSetLayoutWrapper> &layout, VkDescriptorSet &out_vk_descriptor_set)
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

RendererResult DescriptorSetManager<Platform::VULKAN>::DestroyDescriptorSet(Device<Platform::VULKAN> *device, VkDescriptorSet vk_descriptor_set)
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

RC<VulkanDescriptorSetLayoutWrapper> DescriptorSetManager<Platform::VULKAN>::GetOrCreateVkDescriptorSetLayout(Device<Platform::VULKAN> *device, const DescriptorSetLayout<Platform::VULKAN> &layout)
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

#pragma endregion DescriptorSetManager

#pragma region DescriptorTable

template <>
DescriptorTable<Platform::VULKAN>::DescriptorTable(const DescriptorTableDeclaration &decl)
    : m_decl(decl)
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_sets[frame_index].Reserve(decl.GetElements().Size());
    }

    for (const DescriptorSetDeclaration &set_decl : m_decl.GetElements()) {
        if (set_decl.is_reference) {
            const DescriptorSetDeclaration *decl_ptr = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(set_decl.name);
            AssertThrowMsg(decl_ptr != nullptr, "Invalid global descriptor set reference: %s", set_decl.name.LookupString());

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                DescriptorSetRef<Platform::VULKAN> descriptor_set = g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(set_decl.name, frame_index);
                AssertThrowMsg(descriptor_set.IsValid(), "Invalid global descriptor set reference: %s", set_decl.name.LookupString());

                m_sets[frame_index].PushBack(std::move(descriptor_set));
            }

            continue;
        }
        
        DescriptorSetLayout<Platform::VULKAN> layout(set_decl);

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_sets[frame_index].PushBack(layout.CreateDescriptorSet());
        }
    }
}

#pragma endregion DescriptorTable

} // namespace platform
} // namespace renderer
} // namespace hyperion
