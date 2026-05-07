/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12DescriptorHeaps.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12Sampler.hpp>
#include <rendering/dx12/DX12AccelerationStructure.hpp>
#include <rendering/dx12/DX12Helpers.hpp>
#include <rendering/dx12/DX12GraphicsPipeline.hpp>
#include <rendering/dx12/DX12ComputePipeline.hpp>
#include <rendering/dx12/DX12RayTracingPipeline.hpp>

#include <rendering/Bindless.hpp>
#include <rendering/PlaceholderData.hpp>

#include <algorithm>

#include <DX12DescriptorSet.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

#pragma region DX12DescriptorSet

DX12DescriptorSet::DX12DescriptorSet(const DescriptorSetLayout& layout)
    : DescriptorSetBase(layout),
      m_updateVersion(0),
      m_isCreated(false)
{
    for (auto& it : m_layout.GetElements())
    {
        const Name name = it.first;
        const DescriptorSetLayoutElement& element = it.second;

        switch (element.type)
        {
        case ShaderInputType::CBV:
        case ShaderInputType::CBV_Dynamic:
            PrefillElements<DX12GpuBuffer>(name, element.count);
            break;
        case ShaderInputType::SRV:
        case ShaderInputType::SRV_Dynamic:
        case ShaderInputType::UAV:
        case ShaderInputType::UAV_Dynamic:
            if (element.category == ShaderResourceCategory::Buffer)
            {
                PrefillElements<DX12GpuBuffer>(name, element.count);
            }
            else if (element.category == ShaderResourceCategory::Image)
            {
                if (element.type == ShaderInputType::SRV || element.type == ShaderInputType::SRV_Dynamic)
                {
                    PrefillElements<DX12GpuImageView>(name, element.count, RI.placeholderData->GetImageView2D1x1R8());
                }
                else
                {
                    // leave empty to avoid overwriting default image view for UAV
                    PrefillElements<DX12GpuImageView>(name, element.count);
                }
            }
            else if (element.category == ShaderResourceCategory::AccelerationStructure)
            {
                PrefillElements<DX12GpuTlas>(name, element.count);
            }
            else
            {
                HYP_UNREACHABLE();
            }
            break;
        case ShaderInputType::Sampler:
            PrefillElements<DX12Sampler>(name, element.count);
            break;
        }
    }
}

DX12DescriptorSet::~DX12DescriptorSet()
{
    if (m_viewDescriptorHandle.IsValid())
    {
        RI.descriptorHeapManager->Free(DX12DescriptorHeapType::CBV_SRV_UAV, std::move(m_viewDescriptorHandle));
    }

    if (m_samplerDescriptorHandle.IsValid())
    {
        RI.descriptorHeapManager->Free(DX12DescriptorHeapType::SAMPLER, std::move(m_samplerDescriptorHandle));
    }
}

bool DX12DescriptorSet::IsCreated() const
{
    return m_isCreated;
}

RendererResult DX12DescriptorSet::Create()
{
    if (!m_layout.IsValid())
    {
        return HYP_MAKE_ERROR(RendererError, "Descriptor set layout is not valid: {}", 0, m_layout.GetName());
    }

    if (m_layout.IsTemplate())
    {
        return {};
    }

    const ShaderInputSet* decl = m_layout.GetDeclaration();

    // Build sorted entries that match D3D12 descriptor range ordering from BuildRootSignature.
    // View ranges are sorted by (RangeType, BaseShaderRegister).
    // Sampler ranges are sorted by (BaseShaderRegister).
    // This ordering must match the D3D12 descriptor table layout so that
    // D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND resolves each range to the correct heap offset.
    struct ViewEntry
    {
        uint32 binding;
        uint32 count;
        D3D12_DESCRIPTOR_RANGE_TYPE rangeType;
        uint32 baseShaderRegister;
    };

    struct SamplerEntry
    {
        uint32 binding;
        uint32 count;
        uint32 baseShaderRegister;
    };

    Array<ViewEntry> viewEntries;
    Array<SamplerEntry> samplerEntries;

    for (uint8 slotIndex = 0; slotIndex < NumDescriptorSlots; slotIndex++)
    {
        for (const ShaderInput& desc : decl->slots[slotIndex])
        {
            // Skip descriptors excluded by compile-time conditions (matches DescriptorSetLayout constructor behavior)
            if (desc.cond != nullptr && !desc.cond())
            {
                continue;
            }

            const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(desc.name);
            if (layoutElement == nullptr)
            {
                continue;
            }

            // Dynamic buffer entries are handled via root descriptors and do not participate in the descriptor table
            if (desc.isDynamic && desc.category == ShaderResourceCategory::Buffer && desc.slot != ShaderRegister::SAMPLER)
            {
                continue;
            }

            // For bindless elements, use the MaxBindlessResources limit instead of ~0u
            const uint32 elementCount = layoutElement->IsBindless()
                ? (layoutElement->IsBuffer()
                    ? MaxBindlessResources[BindlessStorage_Buffers]
                    : MaxBindlessResources[BindlessStorage_Textures])
                : layoutElement->count;

            if (desc.slot == ShaderRegister::SAMPLER)
            {
                samplerEntries.PushBack({ layoutElement->binding, elementCount, desc.index });
            }
            else
            {
                viewEntries.PushBack({ layoutElement->binding, elementCount, ToDX12DescriptorRangeType(desc.slot), desc.index });
            }
        }
    }

    // Sort view entries to match D3D12 descriptor range ordering used in BuildRootSignature
    std::sort(viewEntries.Begin(), viewEntries.End(),
        [](const ViewEntry& a, const ViewEntry& b) {
            if (a.rangeType != b.rangeType)
            {
                return a.rangeType < b.rangeType;
            }
            return a.baseShaderRegister < b.baseShaderRegister;
        });

    // Sort sampler entries to match D3D12 descriptor range ordering
    std::sort(samplerEntries.Begin(), samplerEntries.End(),
        [](const SamplerEntry& a, const SamplerEntry& b) {
            return a.baseShaderRegister < b.baseShaderRegister;
        });

    // Assign heap offsets in sorted order matching the D3D12 descriptor table layout
    uint32 viewCount = 0;
    for (const ViewEntry& entry : viewEntries)
    {
        m_viewBindingToHeapOffset.Set(entry.binding, viewCount);
        viewCount += entry.count;
    }

    uint32 samplerCount = 0;
    for (const SamplerEntry& entry : samplerEntries)
    {
        m_samplerBindingToHeapOffset.Set(entry.binding, samplerCount);
        samplerCount += entry.count;
    }

    if (viewCount > 0)
    {
        m_viewDescriptorHandle = RI.descriptorHeapManager->Allocate(DX12DescriptorHeapType::CBV_SRV_UAV, viewCount);

        if (!m_viewDescriptorHandle.IsValid())
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to allocate {} view descriptors for descriptor set: {}", 0, viewCount, m_layout.GetName());
        }
    }

    if (samplerCount > 0)
    {
        m_samplerDescriptorHandle = RI.descriptorHeapManager->Allocate(DX12DescriptorHeapType::SAMPLER, samplerCount);

        if (!m_samplerDescriptorHandle.IsValid())
        {
            // Free already allocated views
            if (m_viewDescriptorHandle.IsValid())
            {
                RI.descriptorHeapManager->Free(DX12DescriptorHeapType::CBV_SRV_UAV, std::move(m_viewDescriptorHandle));
            }

            return HYP_MAKE_ERROR(RendererError, "Failed to allocate {} sampler descriptors for descriptor set: {}", 0, samplerCount, m_layout.GetName());
        }
    }

    for (const auto& it : m_elements)
    {
        const Name name = it.first;
        const DescriptorSetElement& element = it.second;

        m_cachedElements.Emplace(name, Array<DX12CachedDescriptor, RHIAllocator>(element.values.Size()));
    }

    m_isCreated = true;

    UpdateDirtyState();
    Update();

    return {};
}

void DX12DescriptorSet::UpdateDirtyState(bool* outIsDirty)
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

        Array<DX12CachedDescriptor, RHIAllocator>& cachedElementValues = cachedIt->second;

        if (cachedElementValues.Size() != element.values.Size())
        {
            cachedElementValues.ResizeZeroed(element.values.Size());
        }
    }

    Array<DX12CachedDescriptor, RHIAllocator> localDescriptors;

    // detect changes from cachedValues
    for (auto& it : m_elements)
    {
        const Name name = it.first;
        DescriptorSetElement& element = it.second;

        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
        AssertDebug(layoutElement != nullptr, "Invalid element: No item with name {} found", name);

        auto cachedIt = m_cachedElements.Find(name);
        AssertDebug(cachedIt != m_cachedElements.End());

        Array<DX12CachedDescriptor, RHIAllocator>& cachedValues = cachedIt->second;
        localDescriptors.Clear();
        localDescriptors.Reserve(element.values.Size());

        switch (layoutElement->type)
        {
        case ShaderInputType::CBV:
        case ShaderInputType::CBV_Dynamic:
        case ShaderInputType::SRV:
        case ShaderInputType::SRV_Dynamic:
        case ShaderInputType::UAV:
        case ShaderInputType::UAV_Dynamic:
        {
            for (uint32 index : element.occupiedArrayElems)
            {
                ObjectBase* ptr = element.values[index];

                if (layoutElement->category == ShaderResourceCategory::Buffer)
                {
                    AssertDebug(ptr && ptr->IsA<DX12GpuBuffer>(), "Invalid buffer descriptor: {}", name);

                    DX12GpuBuffer* ref = StaticCast<DX12GpuBuffer>(ptr);
                    AssertDebug(ref != nullptr);

                    DX12CachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                    Memory::Fill(&descriptor, 0, sizeof(DX12CachedDescriptor));
                    descriptor.binding = layoutElement->binding;
                    descriptor.index = index;
                    descriptor.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

                    AssertDebug(ref->IsCreated(), "Buffer not initialized for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                    descriptor.objectPtr = ref;
                }
                else if (layoutElement->category == ShaderResourceCategory::Image)
                {
                    AssertDebug(ptr && ptr->IsA<DX12GpuImageView>(), "Invalid image descriptor: {}", name);

                    DX12GpuImageView* ref = StaticCast<DX12GpuImageView>(ptr);
                    AssertDebug(ref != nullptr);

                    DX12CachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                    Memory::Fill(&descriptor, 0, sizeof(DX12CachedDescriptor));
                    descriptor.binding = layoutElement->binding;
                    descriptor.index = index;
                    descriptor.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

                    AssertDebug(ref->IsCreated(), "Image view not initialized for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                    descriptor.objectPtr = ref;
                }
                else if (layoutElement->category == ShaderResourceCategory::AccelerationStructure)
                {
                    AssertDebug(ptr && ptr->IsA<DX12GpuTlas>(), "Invalid TLAS descriptor: {}", name);

                    DX12GpuTlas* ref = StaticCast<DX12GpuTlas>(ptr);
                    AssertDebug(ref != nullptr);

                    DX12CachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                    Memory::Fill(&descriptor, 0, sizeof(DX12CachedDescriptor));
                    descriptor.binding = layoutElement->binding;
                    descriptor.index = index;
                    descriptor.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

                    descriptor.objectPtr = ref;
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

                AssertDebug(ptr && ptr->IsA<DX12Sampler>(), "Invalid sampler descriptor: {}", name);

                DX12Sampler* ref = StaticCast<DX12Sampler>(ptr);
                AssertDebug(ref != nullptr);

                DX12CachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                Memory::Fill(&descriptor, 0, sizeof(DX12CachedDescriptor));
                descriptor.binding = layoutElement->binding;
                descriptor.index = index;
                descriptor.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

                AssertDebug(ref->IsCreated(), "Invalid sampler for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                descriptor.objectPtr = ref;
            }

            break;
        }
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

            Memory::Copy(cachedValues.Data() + localDirtyRange.GetStart(), localDescriptors.Data() + localDirtyRange.GetStart(), sizeof(DX12CachedDescriptor) * size_t(localDirtyRange.Distance()));

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

void DX12DescriptorSet::Update(bool force)
{
    if (!m_isCreated)
    {
        return;
    }

    if (force)
    {
        m_cachedElements.Clear();
        UpdateDirtyState();
    }

    if (m_pendingDescriptors.Empty())
    {
        return;
    }

    ID3D12Device* device = RI.GetDevice();

    const uint32 incrementSize = RI.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const uint32 samplerIncrementSize = RI.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    for (size_t i = 0; i < m_pendingDescriptors.Size(); i++)
    {
        const DX12CachedDescriptor& descriptor = m_pendingDescriptors[i];

        DescriptorSetElement* element = nullptr;
        const DescriptorSetLayoutElement* layoutElement = nullptr;

        for (auto& it : m_elements)
        {
            const Name name = it.first;
            const DescriptorSetLayoutElement* layoutEl = m_layout.GetElement(name);
            if (layoutEl && layoutEl->binding == descriptor.binding)
            {
                element = &it.second;
                layoutElement = layoutEl;
                break;
            }
        }

        if (element == nullptr || layoutElement == nullptr)
        {
            continue;
        }

        ObjectBase* ptr = element->values[descriptor.index];
        if (ptr == nullptr)
        {
            continue;
        }

        // Dynamic buffer entries are handled via root descriptors at bind time,
        // they do not participate in the descriptor table
        if (layoutElement->type == ShaderInputType::CBV_Dynamic
            || layoutElement->type == ShaderInputType::SRV_Dynamic
            || layoutElement->type == ShaderInputType::UAV_Dynamic)
        {
            continue;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE destHandle {};
        destHandle.ptr = 0;

        switch (descriptor.heapType)
        {
        case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
            destHandle = GetViewCpuHandle(descriptor.binding);
            destHandle.ptr += incrementSize * descriptor.index;
            break;
        case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
            destHandle = GetSamplerCpuHandle(descriptor.binding);
            destHandle.ptr += samplerIncrementSize * descriptor.index;
            break;
        default:
            continue;
        }

        switch (layoutElement->type)
        {
        case ShaderInputType::CBV:
        case ShaderInputType::CBV_Dynamic:
        {
            AssertDebug(ptr && ptr->IsA<DX12GpuBuffer>());

            DX12GpuBuffer* buffer = StaticCast<DX12GpuBuffer>(ptr);
            if (!buffer->IsCreated())
            {
                continue;
            }

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = GetCBVDesc(buffer);
            device->CreateConstantBufferView(&cbvDesc, destHandle);
            break;
        }
        case ShaderInputType::SRV:
        case ShaderInputType::SRV_Dynamic:
        {
            if (layoutElement->category == ShaderResourceCategory::Buffer)
            {
                AssertDebug(ptr && ptr->IsA<DX12GpuBuffer>());

                DX12GpuBuffer* buffer = StaticCast<DX12GpuBuffer>(ptr);
                if (!buffer->IsCreated())
                {
                    continue;
                }

                if (buffer->GetBufferType() == GpuBufferType::StructuredBuffer
                    || buffer->GetBufferType() == GpuBufferType::RWStructuredBuffer)
                {
                    // @NOTE: we allow zero so we can pass a StructuredBuffer where shaders expect a ByteAddressBuffer.
                    AssertDebug(element->bufferStride != ~0u, "(RW)StructuredBuffer must have stride passed in!");
                }

                const uint32 structureStride = element->bufferStride != ~0u ? element->bufferStride : uint32(buffer->Size());
                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = GetSRVDesc(buffer, structureStride);
                device->CreateShaderResourceView(buffer->GetResource(), &srvDesc, destHandle);
            }
            else if (layoutElement->category == ShaderResourceCategory::Image)
            {
                AssertDebug(ptr && ptr->IsA<DX12GpuImageView>());

                DX12GpuImageView* imageView = StaticCast<DX12GpuImageView>(ptr);
                if (!imageView->IsCreated())
                {
                    continue;
                }

                const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = GetSRVDesc(
                    imageView->GetImage(),
                    imageView->GetMipIndex(), imageView->NumMips(),
                    imageView->GetLayerIndex(), imageView->NumArrayLayers());

                device->CreateShaderResourceView(imageView->GetImage()->GetResource(), &srvDesc, destHandle);
            }
            else if (layoutElement->category == ShaderResourceCategory::AccelerationStructure)
            {
                AssertDebug(ptr && ptr->IsA<DX12GpuTlas>());

                DX12GpuTlas* tlas = StaticCast<DX12GpuTlas>(ptr);
                if (!tlas || !tlas->IsCreated())
                {
                    continue;
                }

                D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
                srvDesc.RaytracingAccelerationStructure.Location = tlas->GetBuffer()->GetBufferDeviceAddress();

                device->CreateShaderResourceView(tlas->GetBuffer()->GetResource(), &srvDesc, destHandle);
            }
            else
            {
                HYP_UNREACHABLE();
            }

            break;
        }
        case ShaderInputType::UAV:
        case ShaderInputType::UAV_Dynamic:
        {
            if (layoutElement->category == ShaderResourceCategory::Buffer)
            {
                DX12GpuBuffer* buffer = static_cast<DX12GpuBuffer*>(ptr);
                if (!buffer->IsCreated())
                {
                    continue;
                }

                if (buffer->GetBufferType() == GpuBufferType::StructuredBuffer
                    || buffer->GetBufferType() == GpuBufferType::RWStructuredBuffer)
                {
                    // @NOTE: we allow zero so we can pass a StructuredBuffer where shaders expect a ByteAddressBuffer.
                    AssertDebug(element->bufferStride != ~0u, "(RW)StructuredBuffer must have stride passed in!");
                }

                const uint32 structureStride = element->bufferStride != ~0u ? element->bufferStride : uint32(buffer->Size());
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = GetUAVDesc(buffer, structureStride);
                device->CreateUnorderedAccessView(buffer->GetResource(), nullptr, &uavDesc, destHandle);
            }
            else if (layoutElement->category == ShaderResourceCategory::Image)
            {
                DX12GpuImageView* imageView = static_cast<DX12GpuImageView*>(ptr);
                if (!imageView->IsCreated())
                {
                    continue;
                }

                const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = GetUAVDesc(
                    imageView->GetImage(),
                    imageView->GetMipIndex(), imageView->NumMips(),
                    imageView->GetLayerIndex(), imageView->NumArrayLayers());

                // @TODO needs atomic count resource
                device->CreateUnorderedAccessView(imageView->GetImage()->GetResource(), nullptr, &uavDesc, destHandle);
            }
            else
            {
                HYP_UNREACHABLE();
            }

            break;
        }
        case ShaderInputType::Sampler:
        {
            DX12Sampler* sampler = static_cast<DX12Sampler*>(ptr);
            if (!sampler->IsCreated())
            {
                continue;
            }

            const D3D12_SAMPLER_DESC& samplerDesc = sampler->GetD3D12SamplerDesc();

            device->CreateSampler(&samplerDesc, destHandle);
            break;
        }
        }
    }

    for (auto& it : m_elements)
    {
        DescriptorSetElement& element = it.second;
        element.dirtyRange = Range<uint32>::Invalid();
    }

    ++m_updateVersion;

    m_pendingDescriptors.Clear();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorSet::GetViewCpuHandle(uint32 binding) const
{
    auto it = m_viewBindingToHeapOffset.Find(binding);
    Assert(it != m_viewBindingToHeapOffset.End(), "Binding {} not found in view binding map", binding);

    const uint32 offset = it->second;
    const uint32 incrementSize = RI.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_viewDescriptorHandle.cpuHandle;
    handle.ptr += incrementSize * offset;

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorSet::GetSamplerCpuHandle(uint32 binding) const
{
    auto it = m_samplerBindingToHeapOffset.Find(binding);
    Assert(it != m_samplerBindingToHeapOffset.End(), "Binding {} not found in sampler binding map", binding);

    const uint32 offset = it->second;
    const uint32 incrementSize = RI.GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_samplerDescriptorHandle.cpuHandle;
    handle.ptr += incrementSize * offset;

    return handle;
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12GraphicsPipeline* pipeline, uint32 bindIndex) const
{
    DescriptorSetOffsetMap offsets {};
    Bind(commandBuffer, pipeline, offsets, bindIndex);
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    Assert(m_isCreated);

    const DescriptorSetRootIndices& rootIndices = pipeline->GetDescriptorSetRootIndices(bindIndex);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();

    // Compute dynamic buffer addresses and apply via root descriptors
    UINT64 dynamicEntryAddresses[DescriptorSetRootIndices::MaxDynamicEntries];
    uint32 dynamicEntryCount = 0;

    for (const Name& elementName : m_layout.GetDynamicElements())
    {
        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(elementName);
        Assert(layoutElement != nullptr);

        if (layoutElement->type != ShaderInputType::CBV_Dynamic
            && layoutElement->type != ShaderInputType::SRV_Dynamic
            && layoutElement->type != ShaderInputType::UAV_Dynamic)
        {
            continue;
        }

        auto elementIt = m_elements.Find(elementName);
        if (elementIt == m_elements.End())
        {
            continue;
        }

        const DescriptorSetElement& element = elementIt->second;
        if (element.values.Empty())
        {
            continue;
        }

        DX12GpuBuffer* buffer = StaticCast<DX12GpuBuffer>(element.values[0]);
        if (buffer == nullptr || !buffer->IsCreated())
        {
            continue;
        }

        uint32 offset = 0;
        for (uint32 j = 0; j < offsets.count; j++)
        {
            if (offsets.keys[j] == StringHash(elementName))
            {
                offset = offsets.values[j];
                break;
            }
        }

        if (dynamicEntryCount < rootIndices.dynamicEntryCount)
        {
            const uint32 rootParamIndex = rootIndices.dynamicEntryRootParamIndices[dynamicEntryCount];
            D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = buffer->GetResource()->GetGPUVirtualAddress() + offset;
            dynamicEntryAddresses[dynamicEntryCount] = gpuAddress;

            switch (layoutElement->type)
            {
            case ShaderInputType::CBV_Dynamic:
                commandList->SetGraphicsRootConstantBufferView(rootParamIndex, gpuAddress);
                break;
            case ShaderInputType::SRV_Dynamic:
                commandList->SetGraphicsRootShaderResourceView(rootParamIndex, gpuAddress);
                break;
            case ShaderInputType::UAV_Dynamic:
                commandList->SetGraphicsRootUnorderedAccessView(rootParamIndex, gpuAddress);
                break;
            }
            dynamicEntryCount++;
        }
    }

    // Check if the same descriptor set + offsets are already bound
    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex].descriptorSet == this
        && boundDescriptorSets[bindIndex].dynamicEntryCount == dynamicEntryCount
        && boundDescriptorSets[bindIndex].updateVersion == m_updateVersion
        && Memory::Compare(boundDescriptorSets[bindIndex].dynamicEntryAddresses, dynamicEntryAddresses, dynamicEntryCount * sizeof(UINT64)) == 0)
    {
        return;
    }

    // Bind descriptor tables (view + sampler)
    if (m_viewDescriptorHandle.IsValid() && rootIndices.viewRootIndex != ~0u)
    {
        commandList->SetGraphicsRootDescriptorTable(rootIndices.viewRootIndex, m_viewDescriptorHandle.gpuHandle);
    }

    if (m_samplerDescriptorHandle.IsValid() && rootIndices.samplerRootIndex != ~0u)
    {
        commandList->SetGraphicsRootDescriptorTable(rootIndices.samplerRootIndex, m_samplerDescriptorHandle.gpuHandle);
    }

    // Update cache
    boundDescriptorSets[bindIndex].descriptorSet = this;
    boundDescriptorSets[bindIndex].updateVersion = m_updateVersion;
    boundDescriptorSets[bindIndex].dynamicEntryCount = dynamicEntryCount;
    Memory::Copy(boundDescriptorSets[bindIndex].dynamicEntryAddresses, dynamicEntryAddresses, dynamicEntryCount * sizeof(UINT64));
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12ComputePipeline* pipeline, uint32 bindIndex) const
{
    DescriptorSetOffsetMap offsets {};
    Bind(commandBuffer, pipeline, offsets, bindIndex);
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    Assert(m_isCreated);

    const DescriptorSetRootIndices& rootIndices = pipeline->GetDescriptorSetRootIndices(bindIndex);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();

    // Compute dynamic buffer addresses and apply via root descriptors
    UINT64 dynamicEntryAddresses[DescriptorSetRootIndices::MaxDynamicEntries];
    uint32 dynamicEntryCount = 0;

    for (const Name& elementName : m_layout.GetDynamicElements())
    {
        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(elementName);
        Assert(layoutElement != nullptr);

        if (layoutElement->type != ShaderInputType::CBV_Dynamic
            && layoutElement->type != ShaderInputType::SRV_Dynamic
            && layoutElement->type != ShaderInputType::UAV_Dynamic)
        {
            continue;
        }

        auto elementIt = m_elements.Find(elementName);
        if (elementIt == m_elements.End())
        {
            continue;
        }

        const DescriptorSetElement& element = elementIt->second;
        if (element.values.Empty())
        {
            continue;
        }

        DX12GpuBuffer* buffer = StaticCast<DX12GpuBuffer>(element.values[0]);
        if (buffer == nullptr || !buffer->IsCreated())
        {
            continue;
        }

        uint32 offset = 0;
        for (uint32 j = 0; j < offsets.count; j++)
        {
            if (offsets.keys[j] == StringHash(elementName))
            {
                offset = offsets.values[j];
                break;
            }
        }

        if (dynamicEntryCount < rootIndices.dynamicEntryCount)
        {
            const uint32 rootParamIndex = rootIndices.dynamicEntryRootParamIndices[dynamicEntryCount];
            D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = buffer->GetResource()->GetGPUVirtualAddress() + offset;
            dynamicEntryAddresses[dynamicEntryCount] = gpuAddress;

            switch (layoutElement->type)
            {
            case ShaderInputType::CBV_Dynamic:
                commandList->SetComputeRootConstantBufferView(rootParamIndex, gpuAddress);
                break;
            case ShaderInputType::SRV_Dynamic:
                commandList->SetComputeRootShaderResourceView(rootParamIndex, gpuAddress);
                break;
            case ShaderInputType::UAV_Dynamic:
                commandList->SetComputeRootUnorderedAccessView(rootParamIndex, gpuAddress);
                break;
            }
            dynamicEntryCount++;
        }
    }

    // Check if the same descriptor set + offsets are already bound
    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex].descriptorSet == this
        && boundDescriptorSets[bindIndex].dynamicEntryCount == dynamicEntryCount
        && boundDescriptorSets[bindIndex].updateVersion == m_updateVersion
        && Memory::Compare(boundDescriptorSets[bindIndex].dynamicEntryAddresses, dynamicEntryAddresses, dynamicEntryCount * sizeof(UINT64)) == 0)
    {
        return;
    }

    // Bind descriptor tables (view + sampler) for compute
    if (m_viewDescriptorHandle.IsValid() && rootIndices.viewRootIndex != ~0u)
    {
        commandList->SetComputeRootDescriptorTable(rootIndices.viewRootIndex, m_viewDescriptorHandle.gpuHandle);
    }

    if (m_samplerDescriptorHandle.IsValid() && rootIndices.samplerRootIndex != ~0u)
    {
        commandList->SetComputeRootDescriptorTable(rootIndices.samplerRootIndex, m_samplerDescriptorHandle.gpuHandle);
    }

    // Update cache
    boundDescriptorSets[bindIndex].descriptorSet = this;
    boundDescriptorSets[bindIndex].updateVersion = m_updateVersion;
    boundDescriptorSets[bindIndex].dynamicEntryCount = dynamicEntryCount;
    Memory::Copy(boundDescriptorSets[bindIndex].dynamicEntryAddresses, dynamicEntryAddresses, dynamicEntryCount * sizeof(UINT64));
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12RayTracingPipeline* pipeline, uint32 bindIndex) const
{
    DescriptorSetOffsetMap offsets {};
    Bind(commandBuffer, pipeline, offsets, bindIndex);
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    Assert(m_isCreated);

    const DescriptorSetRootIndices& rootIndices = pipeline->GetDescriptorSetRootIndices(bindIndex);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();

    // Compute dynamic buffer addresses and apply via root descriptors
    UINT64 dynamicEntryAddresses[DescriptorSetRootIndices::MaxDynamicEntries];
    uint32 dynamicEntryCount = 0;

    for (const Name& elementName : m_layout.GetDynamicElements())
    {
        const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(elementName);
        Assert(layoutElement != nullptr);

        if (layoutElement->type != ShaderInputType::CBV_Dynamic
            && layoutElement->type != ShaderInputType::SRV_Dynamic
            && layoutElement->type != ShaderInputType::UAV_Dynamic)
        {
            continue;
        }

        auto elementIt = m_elements.Find(elementName);
        if (elementIt == m_elements.End())
        {
            continue;
        }

        const DescriptorSetElement& element = elementIt->second;
        if (element.values.Empty())
        {
            continue;
        }

        DX12GpuBuffer* buffer = StaticCast<DX12GpuBuffer>(element.values[0]);
        if (buffer == nullptr || !buffer->IsCreated())
        {
            continue;
        }

        uint32 offset = 0;
        for (uint32 j = 0; j < offsets.count; j++)
        {
            if (offsets.keys[j] == StringHash(elementName))
            {
                offset = offsets.values[j];
                break;
            }
        }

        if (dynamicEntryCount < rootIndices.dynamicEntryCount)
        {
            const uint32 rootParamIndex = rootIndices.dynamicEntryRootParamIndices[dynamicEntryCount];
            D3D12_GPU_VIRTUAL_ADDRESS gpuAddress = buffer->GetResource()->GetGPUVirtualAddress() + offset;
            dynamicEntryAddresses[dynamicEntryCount] = gpuAddress;

            switch (layoutElement->type)
            {
            case ShaderInputType::CBV_Dynamic:
                commandList->SetComputeRootConstantBufferView(rootParamIndex, gpuAddress);
                break;
            case ShaderInputType::SRV_Dynamic:
                commandList->SetComputeRootShaderResourceView(rootParamIndex, gpuAddress);
                break;
            case ShaderInputType::UAV_Dynamic:
                commandList->SetComputeRootUnorderedAccessView(rootParamIndex, gpuAddress);
                break;
            }
            dynamicEntryCount++;
        }
    }

    // Check if the same descriptor set + offsets are already bound
    auto& boundDescriptorSets = commandBuffer->m_boundDescriptorSets;

    if (boundDescriptorSets.Size() <= bindIndex)
    {
        boundDescriptorSets.Resize(bindIndex + 1);
    }
    else if (boundDescriptorSets[bindIndex].descriptorSet == this
        && boundDescriptorSets[bindIndex].dynamicEntryCount == dynamicEntryCount
        && boundDescriptorSets[bindIndex].updateVersion == m_updateVersion
        && Memory::Compare(boundDescriptorSets[bindIndex].dynamicEntryAddresses, dynamicEntryAddresses, dynamicEntryCount * sizeof(UINT64)) == 0)
    {
        return;
    }

    // Bind descriptor tables (view + sampler) for compute (ray tracing uses compute queues)
    if (m_viewDescriptorHandle.IsValid() && rootIndices.viewRootIndex != ~0u)
    {
        commandList->SetComputeRootDescriptorTable(rootIndices.viewRootIndex, m_viewDescriptorHandle.gpuHandle);
    }

    if (m_samplerDescriptorHandle.IsValid() && rootIndices.samplerRootIndex != ~0u)
    {
        commandList->SetComputeRootDescriptorTable(rootIndices.samplerRootIndex, m_samplerDescriptorHandle.gpuHandle);
    }

    // Update cache
    boundDescriptorSets[bindIndex].descriptorSet = this;
    boundDescriptorSets[bindIndex].updateVersion = m_updateVersion;
    boundDescriptorSets[bindIndex].dynamicEntryCount = dynamicEntryCount;
    Memory::Copy(boundDescriptorSets[bindIndex].dynamicEntryAddresses, dynamicEntryAddresses, dynamicEntryCount * sizeof(UINT64));
}

DescriptorSetRef DX12DescriptorSet::Clone() const
{
    DescriptorSetRef descriptorSet = MakeHandle<DX12DescriptorSet>(GetLayout());
    descriptorSet->SetDebugName(GetDebugName());

    return descriptorSet;
}

#ifdef HYP_DEBUG_MODE
void DX12DescriptorSet::SetDebugName(Name name)
{
    DescriptorSetBase::SetDebugName(name);
}
#endif

#pragma endregion DX12DescriptorSet

} // namespace Hyperion
