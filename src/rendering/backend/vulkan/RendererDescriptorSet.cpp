/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/RendererDescriptorSet.hpp>
#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererCommandBuffer.hpp>
#include <rendering/backend/RendererGraphicsPipeline.hpp>
#include <rendering/backend/RendererComputePipeline.hpp>
#include <rendering/backend/rt/RendererRaytracingPipeline.hpp>

#include <math/MathUtil.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

#include <vulkan/vulkan.h>

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

struct VulkanDescriptorElementInfo
{
    uint                binding;
    uint                index;
    VkDescriptorType    descriptor_type;

    union {
        VkDescriptorBufferInfo                          buffer_info;
        VkDescriptorImageInfo                           image_info;
        VkWriteDescriptorSetAccelerationStructureKHR    acceleration_structure_info;
    };
};

struct VulkanDescriptorSetLayoutWrapper
{
    VkDescriptorSetLayout   vk_layout = VK_NULL_HANDLE;

    Result Create(Device<Platform::VULKAN> *device, const DescriptorSetLayout<Platform::VULKAN> &layout)
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

        return Result { };
    }

    Result Destroy(Device<Platform::VULKAN> *device)
    {
        AssertThrow(vk_layout != VK_NULL_HANDLE);

        vkDestroyDescriptorSetLayout(
            device->GetDevice(),
            vk_layout,
            nullptr
        );

        vk_layout = VK_NULL_HANDLE;

        return Result { };
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
        decl_ptr = g_static_descriptor_table_decl->FindDescriptorSetDeclaration(decl.name);

        AssertThrowMsg(decl_ptr != nullptr, "Invalid global descriptor set reference: %s", decl.name.LookupString());
    }

    for (const Array<DescriptorDeclaration> &slot : decl_ptr->slots) {
        for (const DescriptorDeclaration &descriptor : slot) {
            const uint descriptor_index = decl_ptr->CalculateFlatIndex(descriptor.slot, descriptor.name);
            AssertThrow(descriptor_index != uint(-1));
            
            if (descriptor.cond != nullptr && !descriptor.cond()) {
                // Skip this descriptor, condition not met
                continue;
            }

            HYP_LOG(RenderingBackend, LogLevel::DEBUG, "Set element {}.{}[{}] (slot: {}, count: {}, size: {}, is_dynamic: {})",
                decl_ptr->name, descriptor.name, descriptor_index, int(descriptor.slot),
                descriptor.count, descriptor.size, descriptor.is_dynamic);

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
    Array<Pair<Name, uint>> dynamic_elements_with_index;

    // Add to list of dynamic buffer names
    for (const auto &it : m_elements) {
        if (it.second.type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
            || it.second.type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC)
        {
            dynamic_elements_with_index.PushBack({ it.first, it.second.binding });
        }
    }

    std::sort(dynamic_elements_with_index.Begin(), dynamic_elements_with_index.End(), [](const Pair<Name, uint> &a, const Pair<Name, uint> &b)
    {
        return a.second < b.second;
    });

    m_dynamic_elements.Resize(dynamic_elements_with_index.Size());

    for (uint i = 0; i < dynamic_elements_with_index.Size(); i++) {
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
Result DescriptorSet<Platform::VULKAN>::Update(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_platform_impl.handle != VK_NULL_HANDLE);

    Array<VulkanDescriptorElementInfo> descriptor_element_infos;

    for (auto &it : m_elements) {
        const Name name = it.first;
        const DescriptorSetElement<Platform::VULKAN> &element = it.second;

        if (!element.IsDirty()) {
            continue;
        }

        const DescriptorSetLayoutElement *layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.LookupString());

        for (uint i = element.dirty_range.GetStart(); i < element.dirty_range.GetEnd(); i++) {
            VulkanDescriptorElementInfo descriptor_element_info { };
            descriptor_element_info.binding = layout_element->binding;
            descriptor_element_info.index = i;
            descriptor_element_info.descriptor_type = ToVkDescriptorType(layout_element->type);

            const auto value_it = element.values.Find(i);

            if (value_it == element.values.End()) {
                continue;
            }

            if (value_it->second.Is<GPUBufferRef<Platform::VULKAN>>()) {
                const bool layout_has_size = layout_element->size != 0 && layout_element->size != uint(-1);

                const bool is_dynamic = layout_element->type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
                    || layout_element->type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC;

                const GPUBufferRef<Platform::VULKAN> &ref = value_it->second.Get<GPUBufferRef<Platform::VULKAN>>();
                AssertThrowMsg(ref.IsValid(), "Invalid buffer reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);

                if (is_dynamic) {
                    AssertThrowMsg(layout_element->size != 0, "Buffer size not set for dynamic buffer element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);
                }

                AssertThrowMsg(ref->IsCreated(), "Buffer not initialized for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);
                
                descriptor_element_info.buffer_info = VkDescriptorBufferInfo {
                    .buffer = ref->GetPlatformImpl().handle,
                    .offset = 0,
                    .range  = layout_has_size ? layout_element->size : ref->Size()
                };
            } else if (value_it->second.Is<ImageViewRef<Platform::VULKAN>>()) {
                const bool is_storage_image = layout_element->type == DescriptorSetElementType::IMAGE_STORAGE;

                const ImageViewRef<Platform::VULKAN> &ref = value_it->second.Get<ImageViewRef<Platform::VULKAN>>();
                AssertThrowMsg(ref.IsValid(), "Invalid image view reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);

                AssertThrowMsg(ref->GetPlatformImpl().handle != VK_NULL_HANDLE, "Invalid image view for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);

                descriptor_element_info.image_info = VkDescriptorImageInfo {
                    .sampler        = VK_NULL_HANDLE,
                    .imageView      = ref->GetPlatformImpl().handle,
                    .imageLayout    = is_storage_image ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
            } else if (value_it->second.Is<SamplerRef<Platform::VULKAN>>()) {
                const SamplerRef<Platform::VULKAN> &ref = value_it->second.Get<SamplerRef<Platform::VULKAN>>();
                AssertThrowMsg(ref.IsValid(), "Invalid sampler reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);

                AssertThrowMsg(ref->GetPlatformImpl().handle != VK_NULL_HANDLE, "Invalid sampler for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);

                descriptor_element_info.image_info = VkDescriptorImageInfo {
                    .sampler        = ref->GetPlatformImpl().handle,
                    .imageView      = VK_NULL_HANDLE,
                    .imageLayout    = VK_IMAGE_LAYOUT_UNDEFINED
                };
            } else if (value_it->second.Is<TLASRef<Platform::VULKAN>>()) {
                const TLASRef<Platform::VULKAN> &ref = value_it->second.Get<TLASRef<Platform::VULKAN>>();
                AssertThrowMsg(ref.IsValid(), "Invalid TLAS reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);

                AssertThrowMsg(ref->GetAccelerationStructure() != VK_NULL_HANDLE, "Invalid TLAS for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), i);

                descriptor_element_info.acceleration_structure_info = VkWriteDescriptorSetAccelerationStructureKHR {
                    .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .pNext                      = nullptr,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures    = &ref->GetAccelerationStructure()
                };
            } else {
                AssertThrowMsg(false, "Unhandled descriptor set element type: %d", int(layout_element->type));
            }

            descriptor_element_infos.PushBack(descriptor_element_info);
        }
    }

    Array<VkWriteDescriptorSet> vk_write_descriptor_sets;
    vk_write_descriptor_sets.Resize(descriptor_element_infos.Size());

    for (SizeType i = 0; i < vk_write_descriptor_sets.Size(); i++) {
        const VulkanDescriptorElementInfo &descriptor_element_info = descriptor_element_infos[i];

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
    
    return Result { };
}

template <>
Result DescriptorSet<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    AssertThrow(m_platform_impl.handle == VK_NULL_HANDLE);

    m_platform_impl.vk_layout_wrapper = device->GetDescriptorSetManager()->GetOrCreateVkDescriptorSetLayout(device, m_layout);

    Result result = Result { };

    HYPERION_PASS_ERRORS(
        device->GetDescriptorSetManager()->CreateDescriptorSet(device, m_platform_impl.vk_layout_wrapper, m_platform_impl.handle),
        result
    );

    if (!result) {
        return result;
    }

    HYPERION_PASS_ERRORS(
        Update(device),
        result
    );

    return result;
}

template <>
Result DescriptorSet<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    if (m_platform_impl.handle != VK_NULL_HANDLE) {
        device->GetDescriptorSetManager()->DestroyDescriptorSet(device, m_platform_impl.handle);
        m_platform_impl.handle = VK_NULL_HANDLE;
    }

    // Release reference to layout
    m_platform_impl.vk_layout_wrapper.Reset();

    return Result { };
}

template <>
bool DescriptorSet<Platform::VULKAN>::HasElement(Name name) const
{
    return m_elements.Find(name) != m_elements.End();
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint index, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement<GPUBufferRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint index, uint buffer_size, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement<GPUBufferRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, const GPUBufferRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint index, const ImageViewRef<Platform::VULKAN> &ref)
{
    SetElement<ImageViewRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, const ImageViewRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint index, const SamplerRef<Platform::VULKAN> &ref)
{
    SetElement<SamplerRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, const SamplerRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, uint index, const TLASRef<Platform::VULKAN> &ref)
{
    SetElement<TLASRef<Platform::VULKAN>>(name, index, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::SetElement(Name name, const TLASRef<Platform::VULKAN> &ref)
{
    SetElement(name, 0, ref);
}

template <>
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const GraphicsPipeline<Platform::VULKAN> *pipeline, uint bind_index) const
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
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const GraphicsPipeline<Platform::VULKAN> *pipeline, const ArrayMap<Name, uint> &offsets, uint bind_index) const
{
    Array<uint> offsets_flat;
    offsets_flat.Resize(m_layout.GetDynamicElements().Size());

    for (uint i = 0; i < m_layout.GetDynamicElements().Size(); i++) {
        const Name dynamic_element_name = m_layout.GetDynamicElements()[i];

        const auto it = offsets.Find(dynamic_element_name);
        AssertThrowMsg(it != offsets.End(), "Dynamic element not found: %s", dynamic_element_name.LookupString());

        offsets_flat[i] = it->second;
    }

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
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const ComputePipeline<Platform::VULKAN> *pipeline, uint bind_index) const
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
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const ComputePipeline<Platform::VULKAN> *pipeline, const ArrayMap<Name, uint> &offsets, uint bind_index) const
{
    Array<uint> offsets_flat;
    offsets_flat.Resize(m_layout.GetDynamicElements().Size());

    for (uint i = 0; i < m_layout.GetDynamicElements().Size(); i++) {
        const Name dynamic_element_name = m_layout.GetDynamicElements()[i];

        const auto it = offsets.Find(dynamic_element_name);
        AssertThrowMsg(it != offsets.End(), "Dynamic element not found: %s", dynamic_element_name.LookupString());

        offsets_flat[i] = it->second;
    }

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
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const RaytracingPipeline<Platform::VULKAN> *pipeline, uint bind_index) const
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
void DescriptorSet<Platform::VULKAN>::Bind(const CommandBuffer<Platform::VULKAN> *command_buffer, const RaytracingPipeline<Platform::VULKAN> *pipeline, const ArrayMap<Name, uint> &offsets, uint bind_index) const
{
    Array<uint> offsets_flat;
    offsets_flat.Resize(m_layout.GetDynamicElements().Size());

    for (uint i = 0; i < m_layout.GetDynamicElements().Size(); i++) {
        const Name dynamic_element_name = m_layout.GetDynamicElements()[i];

        const auto it = offsets.Find(dynamic_element_name);
        AssertThrowMsg(it != offsets.End(), "Dynamic element not found: %s", dynamic_element_name.LookupString());

        offsets_flat[i] = it->second;
    }

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

Result DescriptorSetManager<Platform::VULKAN>::Create(Device<Platform::VULKAN> *device)
{
    static const Array<VkDescriptorPoolSize> pool_sizes = {
        { VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 },
        { VK_DESCRIPTOR_TYPE_SAMPLER,                    64 },
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

    return Result { };
}

Result DescriptorSetManager<Platform::VULKAN>::Destroy(Device<Platform::VULKAN> *device)
{
    Result result = Result { };

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

Result DescriptorSetManager<Platform::VULKAN>::CreateDescriptorSet(Device<Platform::VULKAN> *device, const RC<VulkanDescriptorSetLayoutWrapper> &layout, VkDescriptorSet &out_vk_descriptor_set)
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
        return { Result::RENDERER_ERR, "Failed to allocate descriptor set", int(vk_result) };
    }

    return Result { };
}

Result DescriptorSetManager<Platform::VULKAN>::DestroyDescriptorSet(Device<Platform::VULKAN> *device, VkDescriptorSet vk_descriptor_set)
{
    AssertThrow(m_vk_descriptor_pool != VK_NULL_HANDLE);

    AssertThrow(vk_descriptor_set != VK_NULL_HANDLE);

    vkFreeDescriptorSets(
        device->GetDevice(),
        m_vk_descriptor_pool,
        1,
        &vk_descriptor_set
    );

    return Result { };
}

RC<VulkanDescriptorSetLayoutWrapper> DescriptorSetManager<Platform::VULKAN>::GetOrCreateVkDescriptorSetLayout(Device<Platform::VULKAN> *device, const DescriptorSetLayout<Platform::VULKAN> &layout)
{
    const HashCode hash_code = layout.GetHashCode();

    auto it = m_vk_descriptor_set_layouts.Find(hash_code);

    if (it == m_vk_descriptor_set_layouts.End()) {
        RC<VulkanDescriptorSetLayoutWrapper> vk_descriptor_set_layout;
        vk_descriptor_set_layout.Reset(new VulkanDescriptorSetLayoutWrapper);
        
        HYPERION_ASSERT_RESULT(vk_descriptor_set_layout->Create(device, layout));

        m_vk_descriptor_set_layouts.Insert(hash_code, vk_descriptor_set_layout);

        return vk_descriptor_set_layout;
    }

    return it->second.Lock();
}

#pragma endregion DescriptorSetManager

#pragma region DescriptorTable

template <>
DescriptorTable<Platform::VULKAN>::DescriptorTable(const DescriptorTableDeclaration &decl)
    : m_decl(decl)
{
    for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_sets[frame_index].Reserve(decl.GetElements().Size());
    }

    for (const DescriptorSetDeclaration &set_decl : m_decl.GetElements()) {
        if (set_decl.is_reference) {
            const DescriptorSetDeclaration *decl_ptr = g_static_descriptor_table_decl->FindDescriptorSetDeclaration(set_decl.name);
            AssertThrowMsg(decl_ptr != nullptr, "Invalid global descriptor set reference: %s", set_decl.name.LookupString());

            for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                DescriptorSetRef<Platform::VULKAN> descriptor_set = g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(set_decl.name, frame_index);
                AssertThrowMsg(descriptor_set.IsValid(), "Invalid global descriptor set reference: %s", set_decl.name.LookupString());

                m_sets[frame_index].PushBack(std::move(descriptor_set));
            }

            continue;
        }
        
        DescriptorSetLayout<Platform::VULKAN> layout(set_decl);

        for (uint frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_sets[frame_index].PushBack(layout.CreateDescriptorSet());
        }
    }
}

#pragma endregion DescriptorTable

} // namespace platform
} // namespace renderer
} // namespace hyperion
