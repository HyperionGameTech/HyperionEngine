/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanRenderBackend.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanSampler.hpp>
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#include <rendering/vulkan/VulkanComputePipeline.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/rt/VulkanRaytracingPipeline.hpp>
#include <rendering/vulkan/rt/VulkanAccelerationStructure.hpp>

#include <rendering/RenderGlobalState.hpp>

#include <core/math/MathUtil.hpp>

#include <core/containers/HashSet.hpp>

#include <core/logging/Logger.hpp>
#include <core/logging/LogChannels.hpp>

#include <engine/EngineGlobals.hpp>
#include <engine/EngineDriver.hpp>

#include <vulkan/vulkan.h>

namespace hyperion {

extern IRenderBackend* g_renderBackend;

static inline VulkanRenderBackend* GetRenderBackend()
{
    return static_cast<VulkanRenderBackend*>(g_renderBackend);
}

#pragma region VulkanDescriptorSet

VulkanDescriptorSet::VulkanDescriptorSet(const DescriptorSetLayout& layout)
    : DescriptorSetBase(layout),
      m_handle(VK_NULL_HANDLE),
      m_vkDescriptorPool(VK_NULL_HANDLE)
{
    // Initial layout of elements
    for (auto& it : m_layout.GetElements())
    {
        const Name name = it.first;
        const DescriptorSetLayoutElement& element = it.second;

        switch (element.type)
        {
        case DescriptorSetElementType::UNIFORM_BUFFER:         // fallthrough
        case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC: // fallthrough
        case DescriptorSetElementType::SSBO:                   // fallthrough
        case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC: // fallthrough
            PrefillElements<GpuBufferRef>(name, element.count);

            break;
        case DescriptorSetElementType::IMAGE:         // fallthrough
        case DescriptorSetElementType::IMAGE_STORAGE: // fallthrough
            PrefillElements<GpuImageViewRef>(name, element.count);

            break;
        case DescriptorSetElementType::SAMPLER:
            PrefillElements<SamplerRef>(name, element.count);

            break;
        case DescriptorSetElementType::TLAS:
            PrefillElements<TLASRef>(name, element.count);

            break;
        default:
            HYP_UNREACHABLE();
        }
    }
}

VulkanDescriptorSet::~VulkanDescriptorSet()
{
    if (m_handle != VK_NULL_HANDLE)
    {
        HYP_LOG(RenderingBackend, Debug, "Destroying descriptor set with layout: {}\tDebug name: {}", m_layout.GetName().LookupString(), GetDebugName().LookupString());

        GetRenderBackend()->DestroyDescriptorSet(m_handle, m_vkDescriptorPool);

        m_handle = VK_NULL_HANDLE;
        m_vkDescriptorPool = VK_NULL_HANDLE;
    }

    // Release reference to layout
    m_vkLayoutWrapper.Reset();
}

void VulkanDescriptorSet::UpdateDirtyState(bool* outIsDirty)
{
    m_vkDescriptorElementInfos.Clear();

    // Ensure all cached value containers are prepared
    for (auto& it : m_elements)
    {
        const Name name = it.first;
        const DescriptorSetElement& element = it.second;

        auto cachedIt = m_cachedElements.Find(name);

        if (cachedIt == m_cachedElements.End())
        {
            cachedIt = m_cachedElements.Emplace(name).first;
        }

        Array<VulkanDescriptorElementInfo>& cachedElementValues = cachedIt->second;

        if (cachedElementValues.Size() != element.values.Size())
        {
            cachedElementValues.ResizeZeroed(element.values.Size());
        }
    }

    // detect changes from cachedValues
    for (auto& it : m_elements)
    {
        const Name name = it.first;
        DescriptorSetElement& element = it.second;

        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
        HYP_GFX_ASSERT(layoutElement != nullptr, "Invalid element: No item with name %s found", name.LookupString());

        auto cachedIt = m_cachedElements.Find(name);
        HYP_GFX_ASSERT(cachedIt != m_cachedElements.End());

        Array<VulkanDescriptorElementInfo>& cachedValues = cachedIt->second;
        HYP_GFX_ASSERT(cachedValues.Size() == element.values.Size());

        Array<VulkanDescriptorElementInfo> localDescriptorElementInfos;
        localDescriptorElementInfos.Reserve(element.values.Size());

        switch (layoutElement->type)
        {
        case DescriptorSetElementType::UNIFORM_BUFFER:
        case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:
        case DescriptorSetElementType::SSBO:
        case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:
        {
            const bool layoutHasSize = layoutElement->size != 0 && layoutElement->size != ~0u;
            const bool isDynamic = layoutElement->type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
                || layoutElement->type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC;

            //            if (isDynamic)
            //            {
            //                HYP_GFX_ASSERT(layoutElement->size != 0, "Buffer size not set for dynamic buffer element: %s.%s", m_layout.GetName().LookupString(), name.LookupString());
            //            }

            for (auto& valuesIt : element.values)
            {
                const uint32 index = valuesIt.first;
                const DescriptorSetElement::ValueType& value = valuesIt.second;

                const GpuBufferRef& ref = value.Get<GpuBufferRef>();
                HYP_GFX_ASSERT(ref.IsValid(), "Invalid buffer reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);
                HYP_GFX_ASSERT(ref->IsCreated(), "Buffer not initialized for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                VulkanDescriptorElementInfo& descriptorElementInfo = localDescriptorElementInfos.EmplaceBack();
                descriptorElementInfo.binding = layoutElement->binding;
                descriptorElementInfo.index = index;
                descriptorElementInfo.descriptorType = ToVkDescriptorType(layoutElement->type);

                descriptorElementInfo.bufferInfo = VkDescriptorBufferInfo {
                    .buffer = VULKAN_CAST(ref.Get())->GetVulkanHandle(),
                    .offset = 0,
                    .range = layoutHasSize ? layoutElement->size : ref->Size()
                };
            }

            break;
        }
        case DescriptorSetElementType::IMAGE:
        case DescriptorSetElementType::IMAGE_STORAGE:
        {
            const bool isStorageImage = layoutElement->type == DescriptorSetElementType::IMAGE_STORAGE;

            for (auto& valuesIt : element.values)
            {
                const uint32 index = valuesIt.first;
                const DescriptorSetElement::ValueType& value = valuesIt.second;

                VulkanDescriptorElementInfo& descriptorElementInfo = localDescriptorElementInfos.EmplaceBack();
                descriptorElementInfo.binding = layoutElement->binding;
                descriptorElementInfo.index = index;
                descriptorElementInfo.descriptorType = ToVkDescriptorType(layoutElement->type);

                const GpuImageViewRef& ref = value.Get<GpuImageViewRef>();
                HYP_GFX_ASSERT(ref.IsValid(), "Invalid image view reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);
                HYP_GFX_ASSERT(VULKAN_CAST(ref.Get())->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid image view for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptorElementInfo.imageInfo = VkDescriptorImageInfo {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = VULKAN_CAST(ref.Get())->GetVulkanHandle(),
                    .imageLayout = isStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };
            }

            break;
        }
        case DescriptorSetElementType::SAMPLER:
        {
            for (auto& valuesIt : element.values)
            {
                const uint32 index = valuesIt.first;
                const DescriptorSetElement::ValueType& value = valuesIt.second;

                VulkanDescriptorElementInfo& descriptorElementInfo = localDescriptorElementInfos.EmplaceBack();
                descriptorElementInfo.binding = layoutElement->binding;
                descriptorElementInfo.index = index;
                descriptorElementInfo.descriptorType = ToVkDescriptorType(layoutElement->type);

                const SamplerRef& ref = value.Get<SamplerRef>();
                HYP_GFX_ASSERT(ref.IsValid(), "Invalid sampler reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                HYP_GFX_ASSERT(VULKAN_CAST(ref.Get())->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid sampler for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptorElementInfo.imageInfo = VkDescriptorImageInfo {
                    .sampler = VULKAN_CAST(ref.Get())->GetVulkanHandle(),
                    .imageView = VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
                };
            }

            break;
        }
        case DescriptorSetElementType::TLAS:
        {
            for (auto& valuesIt : element.values)
            {
                const uint32 index = valuesIt.first;
                const DescriptorSetElement::ValueType& value = valuesIt.second;

                VulkanDescriptorElementInfo& descriptorElementInfo = localDescriptorElementInfos.EmplaceBack();
                descriptorElementInfo.binding = layoutElement->binding;
                descriptorElementInfo.index = index;
                descriptorElementInfo.descriptorType = ToVkDescriptorType(layoutElement->type);

                const TLASRef& ref = value.Get<TLASRef>();
                HYP_GFX_ASSERT(ref.IsValid(), "Invalid TLAS reference for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                HYP_GFX_ASSERT(VULKAN_CAST(ref.Get())->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid TLAS for descriptor set element: %s.%s[%u]", m_layout.GetName().LookupString(), name.LookupString(), index);

                descriptorElementInfo.accelerationStructureInfo = VkWriteDescriptorSetAccelerationStructureKHR {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .pNext = nullptr,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures = &VULKAN_CAST(ref.Get())->GetVulkanHandle()
                };
            }

            break;
        }
        default:
            HYP_UNREACHABLE();
        }

        HYP_GFX_ASSERT(localDescriptorElementInfos.Size() <= cachedValues.Size(), "Index out of range for cached values");

        auto localDirtyRange = Range<uint32>::Invalid();

        for (SizeType i = 0; i < localDescriptorElementInfos.Size(); i++)
        {
            if (Memory::MemCmp(localDescriptorElementInfos.Data() + i, cachedValues.Data() + i, sizeof(VulkanDescriptorElementInfo)) != 0)
            {
                localDirtyRange |= { uint32(i), uint32(i + 1) };
            }
        }

        if (localDirtyRange.Distance() > 0)
        {
            HYP_GFX_ASSERT(localDirtyRange.GetStart() < cachedValues.Size());
            HYP_GFX_ASSERT(localDirtyRange.GetEnd() <= cachedValues.Size());
            HYP_GFX_ASSERT(localDirtyRange.GetStart() < localDescriptorElementInfos.Size());
            HYP_GFX_ASSERT(localDirtyRange.GetEnd() <= localDescriptorElementInfos.Size());

            Memory::MemCpy(cachedValues.Data() + localDirtyRange.GetStart(), localDescriptorElementInfos.Data() + localDirtyRange.GetStart(), sizeof(VulkanDescriptorElementInfo) * SizeType(localDirtyRange.Distance()));

            // mark the element as dirty
            element.dirtyRange |= localDirtyRange;

            m_vkDescriptorElementInfos.Concat(std::move(localDescriptorElementInfos));
        }
    }

    if (outIsDirty)
    {
        *outIsDirty = m_vkDescriptorElementInfos.Any();
    }
}

void VulkanDescriptorSet::Update(bool force)
{
    static_assert(std::is_trivial_v<VulkanDescriptorElementInfo>, "VulkanDescriptorElementInfo should be a trivial type for fast copy and move operations");

    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    if (force)
    {
        m_cachedElements.Clear();
        UpdateDirtyState();
    }

    if (m_vkDescriptorElementInfos.Empty())
    {
        return;
    }

    Array<VkWriteDescriptorSet> vkWriteDescriptorSets;
    vkWriteDescriptorSets.Resize(m_vkDescriptorElementInfos.Size());

    for (SizeType i = 0; i < vkWriteDescriptorSets.Size(); i++)
    {
        const VulkanDescriptorElementInfo& descriptorElementInfo = m_vkDescriptorElementInfos[i];

        VkWriteDescriptorSet& write = vkWriteDescriptorSets[i];

        write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = m_handle;
        write.dstBinding = descriptorElementInfo.binding;
        write.dstArrayElement = descriptorElementInfo.index;
        write.descriptorCount = 1;
        write.descriptorType = descriptorElementInfo.descriptorType;

        if (descriptorElementInfo.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            write.pNext = &descriptorElementInfo.accelerationStructureInfo;
        }

        write.pImageInfo = &descriptorElementInfo.imageInfo;
        write.pBufferInfo = &descriptorElementInfo.bufferInfo;
    }

    vkUpdateDescriptorSets(
        GetRenderBackend()->GetDevice()->GetDevice(),
        uint32(vkWriteDescriptorSets.Size()),
        vkWriteDescriptorSets.Data(),
        0,
        nullptr);

    for (auto& it : m_elements)
    {
        DescriptorSetElement& element = it.second;

        element.dirtyRange = Range<uint32>::Invalid();
    }

    m_vkDescriptorElementInfos.Clear();
}

RendererResult VulkanDescriptorSet::Create()
{
    HYP_GFX_ASSERT(m_handle == VK_NULL_HANDLE && m_vkDescriptorPool == VK_NULL_HANDLE);

    HYP_LOG(RenderingBackend, Debug, "Created descriptor set with layout: {}\tDebug name: {}", m_layout.GetName().LookupString(), GetDebugName().LookupString());

    if (!m_layout.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Descriptor set layout is not valid: {}", 0, m_layout.GetName().LookupString());
    }

    HYP_GFX_CHECK(GetRenderBackend()->GetOrCreateVkDescriptorSetLayout(m_layout, m_vkLayoutWrapper));

    if (m_layout.IsTemplate())
    {
        return RendererResult {};
    }

    RendererResult result;

    HYPERION_PASS_ERRORS(GetRenderBackend()->CreateDescriptorSet(m_vkLayoutWrapper, m_handle, m_vkDescriptorPool), result);

    if (!result)
    {
        return result;
    }
    
    HYP_GFX_ASSERT(m_vkDescriptorPool != VK_NULL_HANDLE);

#ifdef HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    for (const Pair<Name, DescriptorSetElement>& it : m_elements)
    {
        const Name name = it.first;
        const DescriptorSetElement& element = it.second;

        m_cachedElements.Emplace(name, Array<VulkanDescriptorElementInfo>(element.values.Size()));
    }

    UpdateDirtyState();
    Update();

    return result;
}

bool VulkanDescriptorSet::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

void VulkanDescriptorSet::Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

#if defined(HYP_DEBUG_MODE) && false
    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const Name dynamicElementName = m_layout.GetDynamicElements()[i];

        HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamicElementName);
    }
#endif

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = VULKAN_CAST(pipeline)->GetVulkanHandle();
    cachedBinding.pipelineLayout = VULKAN_CAST(pipeline)->GetVulkanPipelineLayout();
    cachedBinding.dynamicOffsets.Resize(m_layout.GetDynamicElements().Size());

    auto& boundDescriptorSets = VULKAN_CAST(commandBuffer)->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex] == cachedBinding)
    {
        // no sense in binding it again if nothing has changed.
        return;
    }

    vkCmdBindDescriptorSets(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        VULKAN_CAST(pipeline)->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(CommandBufferBase* commandBuffer, const GraphicsPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = VULKAN_CAST(pipeline)->GetVulkanHandle();
    cachedBinding.pipelineLayout = VULKAN_CAST(pipeline)->GetVulkanPipelineLayout();

    HashSet<WeakName> usedOffsets;

    cachedBinding.dynamicOffsets.ResizeZeroed(m_layout.GetDynamicElements().Size());

    // TODO: optimize
    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const WeakName dynamicElementName = m_layout.GetDynamicElements()[i];

        int idx = -1;

        for (uint32 j = 0; j < offsets.count; j++)
        {
            if (offsets.keys[j] == dynamicElementName)
            {
                idx = int(j);
                break;
            }
        }

        if (idx != -1)
        {
            cachedBinding.dynamicOffsets[i] = offsets.values[idx];

            usedOffsets.Insert(dynamicElementName);
        }
        else
        {
            cachedBinding.dynamicOffsets[i] = 0;

#if defined(HYP_DEBUG_MODE) && false
            HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", Name(dynamicElementName));
#endif
        }
    }

    auto& boundDescriptorSets = VULKAN_CAST(commandBuffer)->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex] == cachedBinding)
    {
        // no sense in binding it again if nothing has changed.
        return;
    }

    vkCmdBindDescriptorSets(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        VULKAN_CAST(pipeline)->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

#if defined(HYP_DEBUG_MODE) && false
    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const Name dynamicElementName = m_layout.GetDynamicElements()[i];

        HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamicElementName);
    }
#endif

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = VULKAN_CAST(pipeline)->GetVulkanHandle();
    cachedBinding.pipelineLayout = VULKAN_CAST(pipeline)->GetVulkanPipelineLayout();
    cachedBinding.dynamicOffsets.Resize(m_layout.GetDynamicElements().Size());

    auto& boundDescriptorSets = VULKAN_CAST(commandBuffer)->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex] == cachedBinding)
    {
        // no sense in binding it again if nothing has changed.
        return;
    }

    vkCmdBindDescriptorSets(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        VULKAN_CAST(pipeline)->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(CommandBufferBase* commandBuffer, const ComputePipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = VULKAN_CAST(pipeline)->GetVulkanHandle();
    cachedBinding.pipelineLayout = VULKAN_CAST(pipeline)->GetVulkanPipelineLayout();

    HashSet<WeakName> usedOffsets;

    cachedBinding.dynamicOffsets.ResizeZeroed(m_layout.GetDynamicElements().Size());

    // TODO: optimize
    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const WeakName dynamicElementName = m_layout.GetDynamicElements()[i];

        int idx = -1;

        for (uint32 j = 0; j < offsets.count; j++)
        {
            if (offsets.keys[j] == dynamicElementName)
            {
                idx = int(j);
                break;
            }
        }

        if (idx != -1)
        {
            cachedBinding.dynamicOffsets[i] = offsets.values[idx];

            usedOffsets.Insert(dynamicElementName);
        }
        else
        {
            cachedBinding.dynamicOffsets[i] = 0;

#if defined(HYP_DEBUG_MODE) && false
            HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", Name(dynamicElementName));
#endif
        }
    }

    auto& boundDescriptorSets = VULKAN_CAST(commandBuffer)->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex] == cachedBinding)
    {
        // no sense in binding it again if nothing has changed.
        return;
    }

    vkCmdBindDescriptorSets(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        VULKAN_CAST(pipeline)->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

#if defined(HYP_DEBUG_MODE) && false
    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const Name dynamicElementName = m_layout.GetDynamicElements()[i];

        HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamicElementName);
    }
#endif

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = VULKAN_CAST(pipeline)->GetVulkanHandle();
    cachedBinding.pipelineLayout = VULKAN_CAST(pipeline)->GetVulkanPipelineLayout();
    cachedBinding.dynamicOffsets.ResizeZeroed(m_layout.GetDynamicElements().Size());

    auto& boundDescriptorSets = VULKAN_CAST(commandBuffer)->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex] == cachedBinding)
    {
        // no sense in binding it again if nothing has changed.
        return;
    }

    vkCmdBindDescriptorSets(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        VULKAN_CAST(pipeline)->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(CommandBufferBase* commandBuffer, const RaytracingPipelineBase* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = VULKAN_CAST(pipeline)->GetVulkanHandle();
    cachedBinding.pipelineLayout = VULKAN_CAST(pipeline)->GetVulkanPipelineLayout();

    HashSet<WeakName> usedOffsets;

    cachedBinding.dynamicOffsets.ResizeZeroed(m_layout.GetDynamicElements().Size());

    // TODO: optimize
    for (SizeType i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const WeakName dynamicElementName = m_layout.GetDynamicElements()[i];

        int idx = -1;

        for (uint32 j = 0; j < offsets.count; j++)
        {
            if (offsets.keys[j] == dynamicElementName)
            {
                idx = int(j);
                break;
            }
        }

        if (idx != -1)
        {
            cachedBinding.dynamicOffsets[i] = offsets.values[idx];

            usedOffsets.Insert(dynamicElementName);
        }
        else
        {
            cachedBinding.dynamicOffsets[i] = 0;

#if defined(HYP_DEBUG_MODE) && false
            HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", Name(dynamicElementName));
#endif
        }
    }

    auto& boundDescriptorSets = VULKAN_CAST(commandBuffer)->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex] == cachedBinding)
    {
        // no sense in binding it again if nothing has changed.
        return;
    }

    vkCmdBindDescriptorSets(
        VULKAN_CAST(commandBuffer)->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        VULKAN_CAST(pipeline)->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

DescriptorSetRef VulkanDescriptorSet::Clone() const
{
    DescriptorSetRef descriptorSet = CreateObject<VulkanDescriptorSet>(GetLayout());
    descriptorSet->SetDebugName(GetDebugName());

    return descriptorSet;
}

#ifdef HYP_DEBUG_MODE

void VulkanDescriptorSet::SetDebugName(Name name)
{
    DescriptorSetBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = name.LookupString();

    VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
    objectNameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
    objectNameInfo.objectHandle = (uint64)m_handle;
    objectNameInfo.pObjectName = strName;

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(GetRenderBackend()->GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanDescriptorSet

#pragma region VulkanDescriptorTable

VulkanDescriptorTable::VulkanDescriptorTable(const DescriptorTableDeclaration* decl)
    : DescriptorTableBase(decl)
{
    if (!IsValid())
    {
        HYP_LOG(RenderingBackend, Error, "Invalid descriptor table declaration");
        return;
    }

    for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
    {
        m_sets[frameIndex].Reserve(m_decl->elements.Size());
    }

    for (const DescriptorSetDeclaration& descriptorSetDeclaration : m_decl->elements)
    {
        if (descriptorSetDeclaration.flags[DescriptorSetDeclarationFlags::REFERENCE])
        {
            const DescriptorSetDeclaration* referencedDescriptorSetDeclaration = GetStaticDescriptorTableDeclaration().FindDescriptorSetDeclaration(descriptorSetDeclaration.name);
            HYP_GFX_ASSERT(referencedDescriptorSetDeclaration != nullptr, "Invalid global descriptor set reference: %s", descriptorSetDeclaration.name.LookupString());

            for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
            {
                DescriptorSetRef descriptorSet = g_renderGlobalState->globalDescriptorTable->GetDescriptorSet(referencedDescriptorSetDeclaration->name, frameIndex);
                HYP_GFX_ASSERT(descriptorSet.IsValid(), "Invalid global descriptor set reference: %s", referencedDescriptorSetDeclaration->name.LookupString());

                m_sets[frameIndex].PushBack(std::move(descriptorSet));
            }

            continue;
        }

        DescriptorSetLayout layout { &descriptorSetDeclaration };

        for (uint32 frameIndex = 0; frameIndex < g_framesInFlight; frameIndex++)
        {
            DescriptorSetRef descriptorSet = CreateObject<VulkanDescriptorSet>(layout);
            descriptorSet->SetDebugName(layout.GetName());

            m_sets[frameIndex].PushBack(std::move(descriptorSet));
        }
    }
}

#pragma endregion VulkanDescriptorTable

} // namespace hyperion
