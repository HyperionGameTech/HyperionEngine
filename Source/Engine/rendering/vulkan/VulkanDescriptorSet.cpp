/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <VulkanPch.hpp>

#include <rendering/vulkan/VulkanDescriptorSet.hpp>
#include <rendering/vulkan/VulkanRenderInterface.hpp>
#include <rendering/vulkan/VulkanDevice.hpp>
#include <rendering/vulkan/VulkanHelpers.hpp>
#include <rendering/vulkan/VulkanCommandBuffer.hpp>
#include <rendering/vulkan/VulkanGpuImageView.hpp>
#include <rendering/vulkan/VulkanSampler.hpp>
#include <rendering/vulkan/VulkanGraphicsPipeline.hpp>
#include <rendering/vulkan/VulkanComputePipeline.hpp>
#include <rendering/vulkan/VulkanFeatures.hpp>
#include <rendering/vulkan/VulkanMemory.hpp>
#include <rendering/vulkan/VulkanRayTracingPipeline.hpp>
#include <rendering/vulkan/VulkanAccelerationStructure.hpp>

#include <rendering/RenderInterface.hpp>
#include <rendering/PlaceholderData.hpp>

#include <Core/math/MathUtil.hpp>

#include <engine/EngineDriver.hpp>

#include <vulkan/vulkan.h>

#include <VulkanDescriptorSet.generated.inl>

namespace Hyperion {

extern VulkanRenderInterface RI;

#if HYP_DEBUG_MODE

static inline void ValidateDynamicOffset(
    uint32 offset,
    const StringHash& dynamicElementName,
    const ShaderInput& shaderInput,
    const DescriptorSetElement* element)
{
    if (shaderInput.type != ShaderInputType::CBV_Dynamic
        && shaderInput.type != ShaderInputType::SRV_Dynamic
        && shaderInput.type != ShaderInputType::UAV_Dynamic)
    {
        return;
    }

    AssertDebug(offset != ~0u, "Invalid offset set for dynamic element: {}", Name(dynamicElementName));

    const VkPhysicalDeviceLimits& limits = RI.GetDevice()->GetFeatures().GetPhysicalDeviceProperties().limits;

    // Validate alignment based on buffer type
    if (shaderInput.type == ShaderInputType::CBV_Dynamic)
    {
        AssertDebug(offset % limits.minUniformBufferOffsetAlignment == 0,
            "Dynamic uniform buffer offset {} for element {} is not aligned to minUniformBufferOffsetAlignment ({})",
            offset, Name(dynamicElementName), limits.minUniformBufferOffsetAlignment);
    }
    else if (shaderInput.type == ShaderInputType::SRV_Dynamic
        || shaderInput.type == ShaderInputType::UAV_Dynamic)
    {
        AssertDebug(offset % limits.minStorageBufferOffsetAlignment == 0,
            "Dynamic storage buffer offset {} for element {} is not aligned to minStorageBufferOffsetAlignment ({})",
            offset, Name(dynamicElementName), limits.minStorageBufferOffsetAlignment);
    }

    /*// Validate offset is within buffer bounds
    if (element != nullptr && !element->values.Empty())
    {
        const auto firstValueIt = element->values.Begin();
        if (firstValueIt != element->values.End())
        {
            VulkanGpuBuffer* buffer = DynamicCast<VulkanGpuBuffer>(*firstValueIt);

            if (buffer != nullptr)
            {
                const size_t bufferSize = buffer->Size();

                AssertDebug(offset + layoutElement->size <= bufferSize,
                    "Dynamic offset {} + element size {} for element {} exceeds buffer size {}",
                    offset, layoutElement->size, Name(dynamicElementName), bufferSize);
            }
        }
    }*/
}

#endif

static inline void PopulateDynamicOffsets(
    const DescriptorSetLayout& layout,
    const DescriptorSet::ElementsMap& elements,
    const DescriptorSetOffsetMap& offsets,
    uint32* outDynamicOffsets,
    uint32& outNumDynamicOffsets)
{
    outNumDynamicOffsets = uint32(layout.GetDynamicElements().Size());

    for (size_t i = 0; i < layout.GetDynamicElements().Size(); i++)
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

#if HYP_DEBUG_MODE
            const ShaderInput* shaderInput = layout.GetElement(Name(dynamicElementName));
            AssertDebug(shaderInput != nullptr);

            const auto it = elements.Find(Name(dynamicElementName));
            const DescriptorSetElement* element = it != elements.End() ? &it->second : nullptr;

            ValidateDynamicOffset(offset, dynamicElementName, *shaderInput, element);
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

VulkanDescriptorSet::VulkanDescriptorSet(const DescriptorSetLayout& layout)
    : DescriptorSetBase(layout),
      m_handle(VK_NULL_HANDLE),
      m_vkDescriptorSetLayout(VK_NULL_HANDLE),
      m_vkDescriptorPool(VK_NULL_HANDLE)
{
    // Initial layout of elements
    for (const ShaderInput& shaderInput : m_layout.GetElements())
    {
        switch (shaderInput.type)
        {
        case ShaderInputType::CBV:         // fallthrough
        case ShaderInputType::CBV_Dynamic: // fallthrough
            PrefillElements<VulkanGpuBuffer>(shaderInput.name, shaderInput.count);

            break;
        case ShaderInputType::SRV:         // fallthrough
        case ShaderInputType::SRV_Dynamic: // fallthrough
        case ShaderInputType::UAV:         // fallthrough
        case ShaderInputType::UAV_Dynamic:
            if (shaderInput.category == ShaderResourceCategory::Buffer)
            {
                PrefillElements<VulkanGpuBuffer>(shaderInput.name, shaderInput.count);
            }
            else if (shaderInput.category == ShaderResourceCategory::Image)
            {
                if (shaderInput.type == ShaderInputType::SRV || shaderInput.type == ShaderInputType::SRV_Dynamic)
                {
                    PrefillElements<VulkanGpuImageView>(shaderInput.name, shaderInput.count, RI.placeholderData->GetImageView2D1x1R8());
                }
                else
                {
                    // leave empty to avoid overwriting default image view for UAV
                    PrefillElements<VulkanGpuImageView>(shaderInput.name, shaderInput.count);
                }
            }
            else if (shaderInput.category == ShaderResourceCategory::AccelerationStructure)
            {
                PrefillElements<VulkanGpuTlas>(shaderInput.name, shaderInput.count);
            }
            else
            {
                HYP_UNREACHABLE();
            }

            break;
        case ShaderInputType::Sampler:
            PrefillElements<VulkanSampler>(shaderInput.name, shaderInput.count, RI.placeholderData->GetSamplerLinear());

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
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([handle = m_handle, pool = m_vkDescriptorPool]() -> void
            {
                RI.DestroyDescriptorSet(handle, pool);
            }));

        m_handle = VK_NULL_HANDLE;
        m_vkDescriptorSetLayout = VK_NULL_HANDLE;
        m_vkDescriptorPool = VK_NULL_HANDLE;
    }
}

void VulkanDescriptorSet::UpdateDirtyState(bool* outIsDirty)
{
    m_pendingDescriptors.Clear();

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

        Array<VulkanCachedDescriptor, VulkanAllocator>& cachedElementValues = cachedIt->second;

        if (cachedElementValues.Size() != element.values.Size())
        {
            cachedElementValues.ResizeZeroed(element.values.Size());
        }
    }

    Array<VulkanCachedDescriptor, VulkanTempAllocator> localDescriptors;

    // detect changes from cachedValues
    for (auto& it : m_elements)
    {
        const Name name = it.first;
        DescriptorSetElement& element = it.second;

        const ShaderInputWithBinding* shaderInput = m_layout.GetElement(name);
        AssertDebug(shaderInput != nullptr, "Invalid element: No item with name {} found", name);

        auto cachedIt = m_cachedElements.Find(name);
        AssertDebug(cachedIt != m_cachedElements.End());

        Array<VulkanCachedDescriptor, VulkanAllocator>& cachedValues = cachedIt->second;
        localDescriptors.Clear();
        localDescriptors.Reserve(element.values.Size());

        switch (shaderInput->type)
        {
        case ShaderInputType::CBV:
        case ShaderInputType::CBV_Dynamic:
        case ShaderInputType::SRV:
        case ShaderInputType::SRV_Dynamic:
        case ShaderInputType::UAV:
        case ShaderInputType::UAV_Dynamic:
        {
            const bool isBufferBinding = (shaderInput->category == ShaderResourceCategory::Buffer);

            for (uint32 index : element.occupiedArrayElems) // @TODO use dirtyRange to skip bits / end loop early?
            {
                ObjectBase* ptr = element.values[index];

                AssertDebug(ptr, "Invalid buffer descriptor: {}", name);

                if (isBufferBinding)
                {
                    AssertDebug(Hyperion::IsA<VulkanGpuBuffer>(ptr), "Invalid buffer descriptor: {}", name);

                    VulkanGpuBuffer* ref = static_cast<VulkanGpuBuffer*>(ptr);
                    AssertDebug(ref != nullptr);

                    // Default to VK_WHOLE_SIZE -- ByteAddressBuffers will use this.
                    VkDeviceSize bufferRange = VK_WHOLE_SIZE;

                    const bool isDynamic = shaderInput->type == ShaderInputType::CBV_Dynamic
                        || shaderInput->type == ShaderInputType::SRV_Dynamic
                        || shaderInput->type == ShaderInputType::UAV_Dynamic;

                    const bool isByteAddressBuffer = ref->GetBufferType() == GpuBufferType::ByteAddressBuffer
                        || ref->GetBufferType() == GpuBufferType::RWByteAddressBuffer;

                    if (ref->GetBufferType() == GpuBufferType::ConstantBuffer)
                    {
                        AssertDebug(element.bufferStride != ByteAddressBufferStride,
                            "Constant buffer for {} may not have zero for buffer stride!", name);
                    }

                    if (isDynamic)
                    {
                        AssertDebug(element.bufferStride != ~0u && element.bufferStride != ByteAddressBufferStride,
                            "Buffer {} is marked as having a dynamic offset, so it must have stride passed in, and must not be zero (ByteAddressBufferStride)", name);

                        bufferRange = element.bufferStride;
                    }
                    else
                    {
                        // use entire buffer size for range, if not dynamic.
                        // This differs from DX12 where we have to pass the structure stride for StructuredBuffers.
                        bufferRange = VK_WHOLE_SIZE;
                    }

                    VulkanCachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                    Memory::Zero(&descriptor, sizeof(VulkanCachedDescriptor));
                    descriptor.binding = shaderInput->binding;
                    descriptor.index = index;
                    descriptor.descriptorType = ToVkDescriptorType(shaderInput->type, shaderInput->category);

                    AssertDebug(ref->IsCreated(), "Buffer not initialized for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                    descriptor.bufferInfo = VkDescriptorBufferInfo {
                        .buffer = ref->GetVulkanHandle(),
                        .offset = 0,
                        .range = bufferRange
                    };
                }
                else if (shaderInput->category == ShaderResourceCategory::Image)
                {
                    AssertDebug(Hyperion::IsA<VulkanGpuImageView>(ptr), "Invalid image descriptor: {}", name);

                    VulkanGpuImageView* ref = static_cast<VulkanGpuImageView*>(ptr);
                    AssertDebug(ref != nullptr);

                    const bool isStorageImage = (shaderInput->type == ShaderInputType::UAV || shaderInput->type == ShaderInputType::UAV_Dynamic);

                    VulkanCachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                    Memory::Zero(&descriptor, sizeof(VulkanCachedDescriptor));
                    descriptor.binding = shaderInput->binding;
                    descriptor.index = index;
                    descriptor.descriptorType = ToVkDescriptorType(shaderInput->type, shaderInput->category);

                    AssertDebug(ref->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid image view for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                    descriptor.imageInfo = VkDescriptorImageInfo {
                        .sampler = VK_NULL_HANDLE,
                        .imageView = ref->GetVulkanHandle(),
                        .imageLayout =  GetVkImageLayout(
                            isStorageImage ? RS_UNORDERED_ACCESS : RS_SHADER_RESOURCE,
                            ref->GetImage()->GetTextureDesc().IsDepthStencil())
                    };
                }
                else if (shaderInput->category == ShaderResourceCategory::AccelerationStructure)
                {
                    AssertDebug(Hyperion::IsA<VulkanGpuTlas>(ptr), "Invalid TLAS descriptor: {}", name);

                    VulkanGpuTlas* ref = DynamicCast<VulkanGpuTlas>(ptr);
                    AssertDebug(ref != nullptr, "Invalid TLAS reference for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);
                    AssertDebug(ref->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid TLAS for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                    VulkanCachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                    Memory::Zero(&descriptor, sizeof(VulkanCachedDescriptor));
                    descriptor.binding = shaderInput->binding;
                    descriptor.index = index;
                    descriptor.descriptorType = ToVkDescriptorType(shaderInput->type, shaderInput->category);

                    descriptor.accelerationStructure = ref->GetVulkanHandle();
                }
                else
                {
                    HYP_UNREACHABLE();
                }
            }

            break;
        }
        case ShaderInputType::Sampler:
        {
            for (uint32 index : element.occupiedArrayElems)
            {
                ObjectBase* ptr = element.values[index];

                AssertDebug(ptr && Hyperion::IsA<VulkanSampler>(ptr), "Invalid sampler descriptor: {}", name);

                VulkanSampler* ref = static_cast<VulkanSampler*>(ptr);
                AssertDebug(ref != nullptr);

                VulkanCachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                Memory::Zero(&descriptor, sizeof(VulkanCachedDescriptor));
                descriptor.binding = shaderInput->binding;
                descriptor.index = index;
                descriptor.descriptorType = ToVkDescriptorType(shaderInput->type, shaderInput->category);

                AssertDebug(ref->GetVulkanHandle() != VK_NULL_HANDLE, "Invalid sampler for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                descriptor.imageInfo = VkDescriptorImageInfo {
                    .sampler = ref->GetVulkanHandle(),
                    .imageView = VK_NULL_HANDLE,
                    .imageLayout = VK_IMAGE_LAYOUT_UNDEFINED
                };
            }

            break;
        }
        default:
            HYP_UNREACHABLE();
        }

        Assert(localDescriptors.Size() <= cachedValues.Size(), "Index out of range for cached values");

        Range<uint32> localDirtyRange = Range<uint32>::Invalid();

        for (size_t i = 0; i < localDescriptors.Size(); i++)
        {
            if (localDescriptors[i] != cachedValues[i])
            {
                localDirtyRange |= { uint32(i), uint32(i + 1) };
            }
        }

        if (localDirtyRange.Distance() > 0)
        {
            AssertDebug(localDirtyRange.GetEnd() <= cachedValues.Size());
            AssertDebug(localDirtyRange.GetEnd() <= localDescriptors.Size());

            Memory::Copy(cachedValues.Data() + localDirtyRange.GetStart(), localDescriptors.Data() + localDirtyRange.GetStart(), sizeof(VulkanCachedDescriptor) * size_t(localDirtyRange.Distance()));
            // std::copy_n(
            //     std::begin(cachedValues) + localDirtyRange.GetStart(),
            //     localDirtyRange.Distance(),
            //     std::begin(localDescriptors) + localDirtyRange.GetStart());

            // mark the element as dirty
            element.dirtyRange |= localDirtyRange;

            m_pendingDescriptors.Concat(localDescriptors);
        }

        localDescriptors.Clear();
    }

    if (outIsDirty)
    {
        *outIsDirty = m_pendingDescriptors.Any();
    }
}

void VulkanDescriptorSet::Update(bool force)
{
    static_assert(std::is_trivial_v<VulkanCachedDescriptor>, "VulkanCachedDescriptor should be a trivial type for fast copy and move operations");

    Assert(m_handle != VK_NULL_HANDLE);

    if (force)
    {
        m_cachedElements.Clear();
        UpdateDirtyState();
    }

    if (m_pendingDescriptors.Empty())
    {
        return;
    }

    Array<VkWriteDescriptorSet, VulkanTempAllocator> vkWriteDescriptorSets;
    vkWriteDescriptorSets.Resize(m_pendingDescriptors.Size());

    Array<VkWriteDescriptorSetAccelerationStructureKHR, VulkanTempAllocator> vkWriteDescriptorSetAccelerationStructures;

    for (size_t i = 0; i < vkWriteDescriptorSets.Size(); i++)
    {
        const VulkanCachedDescriptor& descriptor = m_pendingDescriptors[i];

        VkWriteDescriptorSet& write = vkWriteDescriptorSets[i];

        write = VkWriteDescriptorSet { VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
        write.dstSet = m_handle;
        write.dstBinding = descriptor.binding;
        write.dstArrayElement = descriptor.index;
        write.descriptorCount = 1;
        write.descriptorType = descriptor.descriptorType;

        if (descriptor.descriptorType == VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR)
        {
            if (vkWriteDescriptorSetAccelerationStructures.Capacity() < vkWriteDescriptorSets.Size())
            {
                // on first encounter of an AS, we reserve the max size we could possibly need so as to not
                // potentially invalidate any ptrs of prior writes.
                // --
                // note that this is extremely unlikely given we currently only support 1 TLAS per descriptor pool,
                // but this sort of future proofing helps me sleep at night.
                vkWriteDescriptorSetAccelerationStructures.Reserve(vkWriteDescriptorSets.Size());
            }

            VkWriteDescriptorSetAccelerationStructureKHR& writeAS = vkWriteDescriptorSetAccelerationStructures.EmplaceBack();
            writeAS.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
            writeAS.pNext = nullptr;
            writeAS.accelerationStructureCount = 1;
            writeAS.pAccelerationStructures = &descriptor.accelerationStructure;

            write.pNext = &writeAS;
        }
        else
        {
            write.pImageInfo = &descriptor.imageInfo;
            write.pBufferInfo = &descriptor.bufferInfo;
        }
    }

    vkUpdateDescriptorSets(
        RI.GetDevice()->GetDevice(),
        uint32(vkWriteDescriptorSets.Size()),
        vkWriteDescriptorSets.Data(),
        0,
        nullptr);

    for (auto& it : m_elements)
    {
        DescriptorSetElement& element = it.second;

        element.dirtyRange = Range<uint32>::Invalid();
    }

    m_pendingDescriptors.Clear();
}

RendererResult VulkanDescriptorSet::Create()
{
    Assert(m_handle == VK_NULL_HANDLE && m_vkDescriptorPool == VK_NULL_HANDLE);

    if (!m_layout.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Descriptor set layout is not valid: {}", 0, m_layout.GetName());
    }

    CheckResultOrReturn(RI.GetOrCreateVkDescriptorSetLayout(m_layout, m_vkDescriptorSetLayout));

    if (m_layout.IsTemplate())
    {
        return {};
    }

    bool isBindlessTextures = false;
    bool isBindlessBuffers = false;
    bool isRayTracing = false;

    for (const ShaderInput& shaderInput : m_layout.GetElements())
    {
        const bool isBindless = (shaderInput.count == ~0u);

        if (isBindless)
        {
            isBindlessTextures |= (shaderInput.category == ShaderResourceCategory::Image && shaderInput.type == ShaderInputType::SRV);
            isBindlessBuffers |= (shaderInput.category == ShaderResourceCategory::Buffer);
        }

        isRayTracing |= (shaderInput.type == ShaderInputType::SRV && shaderInput.category == ShaderResourceCategory::AccelerationStructure);
    }

    CheckResultOrReturn(RI.CreateDescriptorSet(
        m_vkDescriptorSetLayout,
        isBindlessTextures, isBindlessBuffers, isRayTracing,
        m_handle,
        m_vkDescriptorPool));

    AssertDebug(m_vkDescriptorPool != VK_NULL_HANDLE);

#if HYP_DEBUG_MODE
    if (Name debugName = GetDebugName())
    {
        SetDebugName(debugName);
    }
#endif

    for (const auto& it : m_elements)
    {
        const Name name = it.first;
        const DescriptorSetElement& element = it.second;

        m_cachedElements.Emplace(name, Array<VulkanCachedDescriptor, VulkanAllocator>(element.values.Size()));
    }

    UpdateDirtyState();
    Update();

    return {};
}

bool VulkanDescriptorSet::IsCreated() const
{
    return m_handle != VK_NULL_HANDLE;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanGraphicsPipeline* pipeline, uint32 bindIndex) const
{
    Assert(m_handle != VK_NULL_HANDLE);

#if defined(HYP_DEBUG_MODE) && false
    for (size_t i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const Name dynamicElementName = m_layout.GetDynamicElements()[i];

        HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamicElementName);
    }
#endif

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();
    cachedBinding.numDynamicOffsets = uint32(m_layout.GetDynamicElements().Size());
    Memory::Zero(cachedBinding.dynamicOffsets, cachedBinding.numDynamicOffsets * sizeof(uint32));

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
        cachedBinding.numDynamicOffsets,
        cachedBinding.dynamicOffsets);

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanGraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    Assert(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();

    PopulateDynamicOffsets(m_layout, m_elements, offsets, cachedBinding.dynamicOffsets, cachedBinding.numDynamicOffsets);

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
        cachedBinding.numDynamicOffsets,
        cachedBinding.dynamicOffsets);

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanComputePipeline* pipeline, uint32 bindIndex) const
{
    Assert(m_handle != VK_NULL_HANDLE);

#if defined(HYP_DEBUG_MODE) && false
    for (size_t i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const Name dynamicElementName = m_layout.GetDynamicElements()[i];

        HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamicElementName);
    }
#endif

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();
    cachedBinding.numDynamicOffsets = uint32(m_layout.GetDynamicElements().Size());
    Memory::Zero(cachedBinding.dynamicOffsets, cachedBinding.numDynamicOffsets * sizeof(uint32));

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
        cachedBinding.numDynamicOffsets,
        cachedBinding.dynamicOffsets);

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    Assert(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();

    PopulateDynamicOffsets(m_layout, m_elements, offsets, cachedBinding.dynamicOffsets, cachedBinding.numDynamicOffsets);

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
        cachedBinding.numDynamicOffsets,
        cachedBinding.dynamicOffsets);

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanRayTracingPipeline* pipeline, uint32 bindIndex) const
{
    Assert(m_handle != VK_NULL_HANDLE);

#if defined(HYP_DEBUG_MODE) && false
    for (size_t i = 0; i < m_layout.GetDynamicElements().Size(); i++)
    {
        const Name dynamicElementName = m_layout.GetDynamicElements()[i];

        HYP_LOG(RenderingBackend, Warning, "Missing dynamic offset for descriptor set element: {}", dynamicElementName);
    }
#endif

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();
    cachedBinding.numDynamicOffsets = uint32(m_layout.GetDynamicElements().Size());
    Memory::Zero(cachedBinding.dynamicOffsets, cachedBinding.numDynamicOffsets * sizeof(uint32));

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
        cachedBinding.numDynamicOffsets,
        cachedBinding.dynamicOffsets);

    boundDescriptorSets[bindIndex] = cachedBinding;
}

void VulkanDescriptorSet::Bind(VulkanCommandBuffer* commandBuffer, const VulkanRayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    Assert(m_handle != VK_NULL_HANDLE);

    VulkanCachedDescriptorSetBinding cachedBinding {};
    cachedBinding.descriptorSet = m_handle;
    cachedBinding.pipeline = pipeline->GetVulkanHandle();
    cachedBinding.pipelineLayout = pipeline->GetVulkanPipelineLayout();

    PopulateDynamicOffsets(m_layout, m_elements, offsets, cachedBinding.dynamicOffsets, cachedBinding.numDynamicOffsets);

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
        cachedBinding.numDynamicOffsets,
        cachedBinding.dynamicOffsets);

    boundDescriptorSets[bindIndex] = cachedBinding;
}

VulkanDescriptorSetRef VulkanDescriptorSet::Clone() const
{
    VulkanDescriptorSetRef descriptorSet = MakeHandle<VulkanDescriptorSet>(GetLayout());

#if HYP_DEBUG_MODE
    descriptorSet->SetDebugName(GetDebugName());
#endif

    return descriptorSet;
}

#if HYP_DEBUG_MODE

void VulkanDescriptorSet::SetDebugName(Name name)
{
    DescriptorSetBase::SetDebugName(name);

    if (!IsCreated())
    {
        return;
    }

    const char* strName = *name;

    if (RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT)
    {
        VkDebugUtilsObjectNameInfoEXT objectNameInfo { VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
        objectNameInfo.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
        objectNameInfo.objectHandle = (uint64)m_handle;
        objectNameInfo.pObjectName = strName;

        RI.dynamicFunctions.vkSetDebugUtilsObjectNameEXT(RI.GetDevice()->GetDevice(), &objectNameInfo);
    }
}

#endif

#pragma endregion VulkanDescriptorSet

} // namespace Hyperion
