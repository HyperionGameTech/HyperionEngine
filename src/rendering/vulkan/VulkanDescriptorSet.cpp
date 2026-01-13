/* Copyright (c) 2024 No Tomorrow Games. All rights reserved. */

#include <VulkanPch.hpp>

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
#include <rendering/vulkan/VulkanMemory.hpp>
#include <rendering/vulkan/rt/VulkanRaytracingPipeline.hpp>
#include <rendering/vulkan/rt/VulkanAccelerationStructure.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>

#include <core/math/MathUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <vulkan/vulkan.h>

#include <VulkanDescriptorSet.generated.inl>

namespace Hyperion {

extern VulkanRenderBackend* g_renderBackend;

#ifdef HYP_DEBUG_MODE
static inline void ValidateDynamicOffset(
    uint32 offset,
    const StringHash& dynamicElementName,
    const DescriptorSetLayoutElement* layoutElement,
    const DescriptorSetElement* element)
{
    AssertDebug(layoutElement != nullptr, "Invalid dynamic element: {}", Name(dynamicElementName));

    const VkPhysicalDeviceLimits& limits = g_renderBackend->GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits;

    // Validate alignment based on buffer type
    if (layoutElement->type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC)
    {
        AssertDebug(offset % limits.minUniformBufferOffsetAlignment == 0,
            "Dynamic uniform buffer offset {} for element %s is not aligned to minUniformBufferOffsetAlignment ({})",
            offset, Name(dynamicElementName), limits.minUniformBufferOffsetAlignment);
    }
    else if (layoutElement->type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC)
    {
        AssertDebug(offset % limits.minStorageBufferOffsetAlignment == 0,
            "Dynamic storage buffer offset {} for element %s is not aligned to minStorageBufferOffsetAlignment ({})",
            offset, Name(dynamicElementName), limits.minStorageBufferOffsetAlignment);
    }

    // Validate offset is within buffer bounds
    if (element != nullptr && !element->values.Empty())
    {
        const auto firstValueIt = element->values.Begin();
        if (firstValueIt != element->values.End())
        {
            const VulkanGpuBufferRef& buffer = ObjCast<GpuBuffer>(firstValueIt->second);

            if (buffer != nullptr)
            {
                const SizeType bufferSize = buffer->Size();
                const SizeType elementSize = layoutElement->size != ~0u ? layoutElement->size : bufferSize;

                AssertDebug(offset + elementSize <= bufferSize,
                    "Dynamic offset {} + element size {} for element {} exceeds buffer size {}",
                    offset, elementSize, Name(dynamicElementName), bufferSize);
            }
        }
    }
}
#endif

template <class AllocatorType>
static inline void PopulateDynamicOffsets(
    const DescriptorSetLayout& layout,
    const HashMap<Name, DescriptorSetElement>& elements,
    const DescriptorSetOffsetMap& offsets,
    Array<uint32, AllocatorType>& outDynamicOffsets)
{
    outDynamicOffsets.ResizeZeroed(layout.GetDynamicElements().Size());

    for (SizeType i = 0; i < layout.GetDynamicElements().Size(); i++)
    {
        const StringHash dynamicElementName = layout.GetDynamicElements()[i];

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
            const uint32 offset = offsets.values[idx];
            outDynamicOffsets[i] = offset;

#ifdef HYP_DEBUG_MODE
            const DescriptorSetLayoutElement* layoutElement = layout.GetElement(Name(dynamicElementName));
            const auto it = elements.Find(Name(dynamicElementName));
            const DescriptorSetElement* element = it != elements.End() ? &it->second : nullptr;

            ValidateDynamicOffset(offset, dynamicElementName, layoutElement, element);
#endif
        }
        else
        {
            outDynamicOffsets[i] = 0;

#if defined(HYP_DEBUG_MODE) && false
            HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", Name(dynamicElementName));
#endif
        }
    }
}

#pragma region VulkanDescriptorSet

static SharedMutex s_defaultsMutex;

static VulkanGpuImageViewRef GetDefaultVulkanImageView()
{
    TSharedLock lock(s_defaultsMutex);

    static VulkanGpuImageViewRef s_imageView;

    if (HYP_UNLIKELY(!s_imageView))
    {
        lock.Reset();

        TUniqueLock lock2(s_defaultsMutex);

        if (s_imageView)
        {
            return s_imageView;
        }

        TextureDesc textureDesc {};
        textureDesc.imageUsage = IU_SAMPLED | IU_STORAGE;

        VulkanGpuImageRef placeholderImage = CreateObject<VulkanGpuImage>(textureDesc);
        Assert(placeholderImage->Create());

        s_imageView = CreateObject<VulkanGpuImageView>(placeholderImage);
        Assert(s_imageView->Create());
    }

    return s_imageView;
}

static VulkanSamplerRef GetDefaultVulkanSampler()
{
    TSharedLock lock(s_defaultsMutex);

    static VulkanSamplerRef s_sampler;

    if (HYP_UNLIKELY(!s_sampler))
    {
        lock.Reset();

        TUniqueLock lock2(s_defaultsMutex);

        if (s_sampler)
        {
            return s_sampler;
        }

        s_sampler = CreateObject<VulkanSampler>(TFM_LINEAR, TFM_LINEAR, TWM_CLAMP_TO_EDGE);
        Assert(s_sampler->Create());
    }

    return s_sampler;
}

VulkanDescriptorSet::VulkanDescriptorSet(const DescriptorSetLayout& layout)
    : DescriptorSetBase(layout),
      m_handle(VK_NULL_HANDLE),
      m_vkDescriptorSetLayout(VK_NULL_HANDLE),
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
            PrefillElements<VulkanGpuBufferRef>(name, element.count);

            break;
        case DescriptorSetElementType::IMAGE:
            PrefillElements<VulkanGpuImageViewRef>(name, element.count, GetDefaultVulkanImageView());

            break;
        case DescriptorSetElementType::IMAGE_STORAGE:
            PrefillElements<VulkanGpuImageViewRef>(name, element.count, GetDefaultVulkanImageView());

            break;
        case DescriptorSetElementType::SAMPLER:
            PrefillElements<VulkanSamplerRef>(name, element.count, GetDefaultVulkanSampler());

            break;
        case DescriptorSetElementType::TLAS:
            PrefillElements<VulkanGpuTlasRef>(name, element.count);

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
        g_renderBackend->DestroyDescriptorSet(m_handle, m_vkDescriptorPool);

        m_handle = VK_NULL_HANDLE;
        m_vkDescriptorSetLayout = VK_NULL_HANDLE;
        m_vkDescriptorPool = VK_NULL_HANDLE;
    }
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
        AssertDebug(layoutElement != nullptr, "Invalid element: No item with name {} found", name);

        auto cachedIt = m_cachedElements.Find(name);
        AssertDebug(cachedIt != m_cachedElements.End());

        Array<VulkanDescriptorElementInfo>& cachedValues = cachedIt->second;
        AssertDebug(cachedValues.Size() == element.values.Size());

        Array<VulkanDescriptorElementInfo, VulkanAllocator> localDescriptorElementInfos;
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

                AssertDebug(!valuesIt.second || Hyperion::IsA<VulkanGpuBuffer>(*valuesIt.second));

                VulkanGpuBuffer* ref = static_cast<VulkanGpuBuffer*>(valuesIt.second.Get());
                AssertDebug(ref != nullptr);
                
                VulkanDescriptorElementInfo& descriptorElementInfo = localDescriptorElementInfos.EmplaceBack();
                descriptorElementInfo.binding = layoutElement->binding;
                descriptorElementInfo.index = index;
                descriptorElementInfo.descriptorType = ToVkDescriptorType(layoutElement->type);

                AssertDebug(ref->IsCreated(), "Buffer not initialized for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                descriptorElementInfo.bufferInfo = VkDescriptorBufferInfo {
                    .buffer = ref->GetVulkanHandle(),
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

                AssertDebug(!valuesIt.second || Hyperion::IsA<VulkanGpuImageView>(*valuesIt.second));

                VulkanGpuImageView* ref = static_cast<VulkanGpuImageView*>(valuesIt.second.Get());
                AssertDebug(ref != nullptr);

                VulkanDescriptorElementInfo& descriptorElementInfo = localDescriptorElementInfos.EmplaceBack();
                descriptorElementInfo.binding = layoutElement->binding;
                descriptorElementInfo.index = index;
                descriptorElementInfo.descriptorType = ToVkDescriptorType(layoutElement->type);
                
                AssertDebug(ref->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid image view for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                descriptorElementInfo.imageInfo = VkDescriptorImageInfo {
                    .sampler = VK_NULL_HANDLE,
                    .imageView = ref->GetVulkanHandle(),
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

                AssertDebug(!valuesIt.second || Hyperion::IsA<VulkanSampler>(*valuesIt.second));

                VulkanSampler* ref = static_cast<VulkanSampler*>(valuesIt.second.Get());
                AssertDebug(ref != nullptr);

                VulkanDescriptorElementInfo& descriptorElementInfo = localDescriptorElementInfos.EmplaceBack();
                descriptorElementInfo.binding = layoutElement->binding;
                descriptorElementInfo.index = index;
                descriptorElementInfo.descriptorType = ToVkDescriptorType(layoutElement->type);

                AssertDebug(ref->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid sampler for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                descriptorElementInfo.imageInfo = VkDescriptorImageInfo {
                    .sampler = ref->GetVulkanHandle(),
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

                const VulkanGpuTlasRef& ref = ObjCast<VulkanGpuTlas>(valuesIt.second);
                AssertDebug(ref.IsValid(), "Invalid TLAS reference for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);
                AssertDebug(ref->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid TLAS for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                VulkanDescriptorElementInfo& descriptorElementInfo = localDescriptorElementInfos.EmplaceBack();
                descriptorElementInfo.binding = layoutElement->binding;
                descriptorElementInfo.index = index;
                descriptorElementInfo.descriptorType = ToVkDescriptorType(layoutElement->type);

                descriptorElementInfo.accelerationStructureInfo = VkWriteDescriptorSetAccelerationStructureKHR {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
                    .pNext = nullptr,
                    .accelerationStructureCount = 1,
                    .pAccelerationStructures = &ref->GetVulkanHandle()
                };
            }

            break;
        }
        default:
            HYP_UNREACHABLE();
        }

        HYP_GFX_ASSERT(localDescriptorElementInfos.Size() <= cachedValues.Size(), "Index out of range for cached values");

        Range<uint32> localDirtyRange = Range<uint32>::Invalid();

        for (SizeType i = 0; i < localDescriptorElementInfos.Size(); i++)
        {
            if (localDescriptorElementInfos[i] != cachedValues[i])
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

            m_vkDescriptorElementInfos.Concat(localDescriptorElementInfos);
        }

        localDescriptorElementInfos.Clear();
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
        g_renderBackend->GetDevice()->GetDevice(),
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

    if (!m_layout.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Descriptor set layout is not valid: {}", 0, m_layout.GetName());
    }

    HYP_GFX_CHECK(g_renderBackend->GetOrCreateVkDescriptorSetLayout(m_layout, m_vkDescriptorSetLayout));

    if (m_layout.IsTemplate())
    {
        return RendererResult {};
    }

    RendererResult result;
    HYPERION_PASS_ERRORS(g_renderBackend->CreateDescriptorSet(m_vkDescriptorSetLayout, m_handle, m_vkDescriptorPool), result);

    if (!result)
    {
        return result;
    }

    AssertDebug(m_vkDescriptorPool != VK_NULL_HANDLE);

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

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanGraphicsPipeline* pipeline, uint32 bindIndex) const
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
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();
    cachedBinding.dynamicOffsets.Resize(m_layout.GetDynamicElements().Size());

    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

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
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanGraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();

    PopulateDynamicOffsets(m_layout, m_elements, offsets, cachedBinding.dynamicOffsets);

    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

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
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        pipeline->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanComputePipeline* pipeline, uint32 bindIndex) const
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
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();
    cachedBinding.dynamicOffsets.Resize(m_layout.GetDynamicElements().Size());

    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

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
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();

    PopulateDynamicOffsets(m_layout, m_elements, offsets, cachedBinding.dynamicOffsets);

    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

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
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_COMPUTE,
        pipeline->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanRaytracingPipeline* pipeline, uint32 bindIndex) const
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
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();
    cachedBinding.dynamicOffsets.ResizeZeroed(m_layout.GetDynamicElements().Size());

    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

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
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanRaytracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    HYP_GFX_ASSERT(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();

    PopulateDynamicOffsets(m_layout, m_elements, offsets, cachedBinding.dynamicOffsets);

    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

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
        commandBuffer->GetVulkanHandle(),
        VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR,
        pipeline->GetVulkanPipelineLayout(),
        bindIndex,
        1,
        &m_handle,
        uint32(cachedBinding.dynamicOffsets.Size()),
        cachedBinding.dynamicOffsets.Data());

    boundDescriptorSets[bindIndex] = cachedBinding;
}

VulkanDescriptorSetRef VulkanDescriptorSet::Clone() const
{
    VulkanDescriptorSetRef descriptorSet = CreateObject<VulkanDescriptorSet>(GetLayout());
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

    g_vulkanDynamicFunctions->vkSetDebugUtilsObjectNameEXT(g_renderBackend->GetDevice()->GetDevice(), &objectNameInfo);
}

#endif

#pragma endregion VulkanDescriptorSet

} // namespace Hyperion
