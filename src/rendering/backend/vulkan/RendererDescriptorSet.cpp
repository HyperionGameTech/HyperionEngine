/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/backend/vulkan/RendererDescriptorSet.hpp>
#include <rendering/backend/vulkan/RendererCommandBuffer.hpp>
#include <rendering/backend/vulkan/RendererImageView.hpp>
#include <rendering/backend/vulkan/RendererSampler.hpp>
#include <rendering/backend/vulkan/RendererGraphicsPipeline.hpp>
#include <rendering/backend/vulkan/RendererComputePipeline.hpp>
#include <rendering/backend/vulkan/rt/RendererRaytracingPipeline.hpp>
#include <rendering/backend/vulkan/rt/RendererAccelerationStructure.hpp>
#include <rendering/backend/vulkan/VulkanRenderingAPI.hpp>

#include <rendering/backend/RendererDevice.hpp>
#include <rendering/backend/RendererHelpers.hpp>

#include <core/math/MathUtil.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <Engine.hpp>

#include <vulkan/vulkan.h>

#define HYP_DESCRIPTOR_SET_AUTO_UPDATE

namespace hyperion {

extern IRenderingAPI *g_rendering_api;

namespace renderer {

static inline VulkanRenderingAPI *GetRenderingAPI()
{
    return static_cast<VulkanRenderingAPI *>(g_rendering_api);
}

#pragma region DescriptorSet

VulkanDescriptorSet::VulkanDescriptorSet(const DescriptorSetLayout &layout)
    : DescriptorSetBase(layout),
      m_handle(VK_NULL_HANDLE)
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
            PrefillElements<GPUBufferRef>(name, element.count);

            break;
        case DescriptorSetElementType::IMAGE:                   // fallthrough
        case DescriptorSetElementType::IMAGE_STORAGE:           // fallthrough
            PrefillElements<ImageViewRef>(name, element.count);

            break;
        case DescriptorSetElementType::SAMPLER:
            PrefillElements<SamplerRef>(name, element.count);

            break;
        case DescriptorSetElementType::TLAS:
            PrefillElements<TLASRef>(name, element.count);

            break;
        default:
            AssertThrowMsg(false, "Unhandled descriptor set element type in layout: %d", int(element.type));

            break;
        }
    }
}

VulkanDescriptorSet::~VulkanDescriptorSet()
{
}

void VulkanDescriptorSet::Update()
{
    static_assert(std::is_trivial_v<VulkanDescriptorElementInfo>, "VulkanDescriptorElementInfo should be a trivial type for fast copy and move operations");

    AssertThrow(m_handle != VK_NULL_HANDLE);

    Array<VulkanDescriptorElementInfo> dirty_descriptor_element_infos;

#ifdef HYP_DESCRIPTOR_SET_AUTO_UPDATE
    // Ensure all cached value containers are prepared
    for (auto &it : m_elements) {
        const Name name = it.first;
        const DescriptorSetElement &element = it.second;

        auto cached_it = m_cached_elements.Find(name);

        if (cached_it == m_cached_elements.End()) {
            cached_it = m_cached_elements.Insert(name, Array<DescriptorSetElementCachedValue>()).first;
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
        DescriptorSetElement &element = it.second;

        const DescriptorSetLayoutElement *layout_element = m_layout.GetElement(name);
        AssertThrowMsg(layout_element != nullptr, "Invalid element: No item with name %s found", name.LookupString());

#ifdef HYP_DESCRIPTOR_SET_AUTO_UPDATE
        auto cached_it = m_cached_elements.Find(name);
        AssertDebug(cached_it != m_cached_elements.End());

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
            const DescriptorSetElement::ValueType &value = values_it.second;
#else
        for (uint32 index = element.dirty_range.GetStart(); index < element.dirty_range.GetEnd(); index++) {
            const const DescriptorSetElement::ValueType &value = element.values.At(index);
#endif

            VulkanDescriptorElementInfo descriptor_element_info { };
            descriptor_element_info.binding = layout_element->binding;
            descriptor_element_info.index = index;
            descriptor_element_info.descriptor_type = helpers::ToVkDescriptorType(layout_element->type);

            if (value.Is<GPUBufferRef>()) {
                const bool layout_has_size = layout_element->size != 0 && layout_element->size != ~0u;

                const bool is_dynamic = layout_element->type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
                    || layout_element->type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC;

                const GPUBufferRef &ref = value.Get<GPUBufferRef>();
                AssertThrowMsg(ref.IsValid(), "Invalid buffer reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                if (is_dynamic) {
                    AssertThrowMsg(layout_element->size != 0, "Buffer size not set for dynamic buffer element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);
                }

                AssertThrowMsg(ref->IsCreated(), "Buffer not initialized for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);
                
                descriptor_element_info.buffer_info = VkDescriptorBufferInfo {
                    .buffer = static_cast<VulkanGPUBuffer *>(ref.Get())->GetVulkanHandle(),
                    .offset = 0,
                    .range  = layout_has_size ? layout_element->size : ref->Size()
                };
            } else if (value.Is<ImageViewRef>()) {
                const bool is_storage_image = layout_element->type == DescriptorSetElementType::IMAGE_STORAGE;

                const ImageViewRef &ref = value.Get<ImageViewRef>();
                AssertThrowMsg(ref.IsValid(), "Invalid image view reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                AssertThrowMsg(static_cast<VulkanImageView *>(ref.Get())->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid image view for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptor_element_info.image_info = VkDescriptorImageInfo {
                    .sampler        = VK_NULL_HANDLE,
                    .imageView      = static_cast<VulkanImageView *>(ref.Get())->GetVulkanHandle(),
                    .imageLayout    = is_storage_image ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
            } else if (value.Is<SamplerRef>()) {
                const SamplerRef &ref = value.Get<SamplerRef>();
                AssertThrowMsg(ref.IsValid(), "Invalid sampler reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                AssertThrowMsg(static_cast<VulkanSampler *>(ref.Get())->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid sampler for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptor_element_info.image_info = VkDescriptorImageInfo {
                    .sampler        = static_cast<VulkanSampler *>(ref.Get())->GetVulkanHandle(),
                    .imageView      = VK_NULL_HANDLE,
                    .imageLayout    = VK_IMAGE_LAYOUT_UNDEFINED
                };
            } else if (value.Is<TLASRef>()) {
                const TLASRef &ref = value.Get<TLASRef>();
                AssertThrowMsg(ref.IsValid(), "Invalid TLAS reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                AssertThrowMsg(static_cast<VulkanTLAS *>(ref.Get())->GetVulkanAccelerationStructure() != VK_NULL_HANDLE, "Invalid TLAS for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptor_element_info.acceleration_structure_info = VkWriteDescriptorSetAccelerationStructureKHR {
                    .sType                      = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .pNext                      = nullptr,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures    = &static_cast<VulkanTLAS *>(ref.Get())->GetVulkanAccelerationStructure()
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
        write.dstSet = m_handle;
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
        GetRenderingAPI()->GetDevice()->GetDevice(),
        uint32(vk_write_descriptor_sets.Size()),
        vk_write_descriptor_sets.Data(),
        0,
        nullptr
    );

    for (auto &it : m_elements) {
        DescriptorSetElement &element = it.second;

        element.dirty_range = { };
    }
}

RendererResult VulkanDescriptorSet::Create()
{
    AssertThrow(m_handle == VK_NULL_HANDLE);

    m_vk_layout_wrapper = GetRenderingAPI()->GetOrCreateVkDescriptorSetLayout(m_layout);

    RendererResult result;

    HYPERION_PASS_ERRORS(
        GetRenderingAPI()->CreateDescriptorSet(m_vk_layout_wrapper, m_handle),
        result
    );

    if (!result) {
        return result;
    }

    for (const Pair<Name, DescriptorSetElement> &it : m_elements) {
        const Name name = it.first;
        const DescriptorSetElement &element = it.second;

        m_cached_elements.Insert({
            name,
            Array<DescriptorSetElementCachedValue>(element.values.Size())
        });
    }

    Update();

    return result;
}

RendererResult VulkanDescriptorSet::Destroy()
{
    if (m_handle != VK_NULL_HANDLE) {
        GetRenderingAPI()->DestroyDescriptorSet(m_handle);
        m_handle = VK_NULL_HANDLE;
    }

    // Release reference to layout
    m_vk_layout_wrapper.Reset();

    return RendererResult { };
}

bool VulkanDescriptorSet::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

void VulkanDescriptorSet::Bind(const CommandBufferBase *command_buffer, const GraphicsPipelineBase *pipeline, uint32 bind_index) const
{
    vkCmdBindDescriptorSets(
        static_cast<const VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        static_cast<const VulkanGraphicsPipeline *>(pipeline)->GetVulkanPipelineLayout(),
        bind_index,
        1,
        &m_handle,
        0,
        nullptr
    );
}

void VulkanDescriptorSet::Bind(const CommandBufferBase *command_buffer, const GraphicsPipelineBase *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const
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
        static_cast<const VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        static_cast<const VulkanGraphicsPipeline *>(pipeline)->GetVulkanPipelineLayout(),
        bind_index,
        1,
        &m_handle,
        uint32(offsets_flat.Size()),
        offsets_flat.Data()
    );
}

void VulkanDescriptorSet::Bind(const CommandBufferBase *command_buffer, const ComputePipelineBase *pipeline, uint32 bind_index) const
{
    vkCmdBindDescriptorSets(
        static_cast<const VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        static_cast<const VulkanComputePipeline *>(pipeline)->GetVulkanPipelineLayout(),
        bind_index,
        1,
        &m_handle,
        0,
        nullptr
    );
}

void VulkanDescriptorSet::Bind(const CommandBufferBase *command_buffer, const ComputePipelineBase *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const
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
        static_cast<const VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        static_cast<const VulkanComputePipeline *>(pipeline)->GetVulkanPipelineLayout(),
        bind_index,
        1,
        &m_handle,
        uint32(offsets_flat.Size()),
        offsets_flat.Data()
    );
}

void VulkanDescriptorSet::Bind(const CommandBufferBase *command_buffer, const RaytracingPipelineBase *pipeline, uint32 bind_index) const
{
    vkCmdBindDescriptorSets(
        static_cast<const VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        static_cast<const VulkanRaytracingPipeline *>(pipeline)->GetVulkanPipelineLayout(),
        bind_index,
        1,
        &m_handle,
        0,
        nullptr
    );
}

void VulkanDescriptorSet::Bind(const CommandBufferBase *command_buffer, const RaytracingPipelineBase *pipeline, const ArrayMap<Name, uint32> &offsets, uint32 bind_index) const
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
        static_cast<const VulkanCommandBuffer *>(command_buffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        static_cast<const VulkanRaytracingPipeline *>(pipeline)->GetVulkanPipelineLayout(),
        bind_index,
        1,
        &m_handle,
        uint32(offsets_flat.Size()),
        offsets_flat.Data()
    );
}

DescriptorSetRef VulkanDescriptorSet::Clone() const
{
    return MakeRenderObject<VulkanDescriptorSet>(GetLayout());
}

#pragma endregion VulkanDescriptorSet

#pragma region VulkanDescriptorTable

VulkanDescriptorTable::VulkanDescriptorTable(const DescriptorTableDeclaration &decl)
    : DescriptorTableBase(decl)
{
    for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
        m_sets[frame_index].Reserve(decl.GetElements().Size());
    }

    for (const DescriptorSetDeclaration &set_decl : m_decl.GetElements()) {
        if (set_decl.is_reference) {
            const DescriptorSetDeclaration *decl_ptr = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(set_decl.name);
            AssertThrowMsg(decl_ptr != nullptr, "Invalid global descriptor set reference: %s", set_decl.name.LookupString());

            for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
                DescriptorSetRef descriptor_set = g_engine->GetGlobalDescriptorTable()->GetDescriptorSet(set_decl.name, frame_index);
                AssertThrowMsg(descriptor_set.IsValid(), "Invalid global descriptor set reference: %s", set_decl.name.LookupString());

                m_sets[frame_index].PushBack(std::move(descriptor_set));
            }

            continue;
        }
        
        DescriptorSetLayout layout { set_decl };

        for (uint32 frame_index = 0; frame_index < max_frames_in_flight; frame_index++) {
            m_sets[frame_index].PushBack(MakeRenderObject<VulkanDescriptorSet>(layout));
        }
    }
}

#pragma endregion VulkanDescriptorTable

} // namespace renderer
} // namespace hyperion
