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

#include <DX12DescriptorSet.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12DescriptorSet

DX12DescriptorSet::DX12DescriptorSet(const DescriptorSetLayout& layout)
    : DescriptorSetBase(layout),
      m_isCreated(false)
{
    for (auto& it : m_layout.GetElements())
    {
        const Name name = it.first;
        const DescriptorSetLayoutElement& element = it.second;

        switch (element.type)
        {
        case ShaderInputType::UNIFORM_BUFFER:
        case ShaderInputType::UNIFORM_BUFFER_DYNAMIC:
        case ShaderInputType::STORAGE_BUFFER:
        case ShaderInputType::STORAGE_BUFFER_DYNAMIC:
            PrefillElements<DX12GpuBuffer>(name, element.count);
            break;
        case ShaderInputType::IMAGE:
        case ShaderInputType::IMAGE_STORAGE:
            PrefillElements<DX12GpuImageView>(name, element.count);
            break;
        case ShaderInputType::SAMPLER:
            PrefillElements<DX12Sampler>(name, element.count);
            break;
        case ShaderInputType::TLAS:
            PrefillElements<DX12GpuTlas>(name, element.count);
            break;
        default:
            HYP_UNREACHABLE();
        }
    }
}

DX12DescriptorSet::~DX12DescriptorSet()
{
    if (m_viewDescriptorHandle.IsValid())
    {
        g_renderInterface->descriptorHeapManager->Free(DX12DescriptorHeapType::CBV_SRV_UAV, std::move(m_viewDescriptorHandle));
    }

    if (m_samplerDescriptorHandle.IsValid())
    {
        g_renderInterface->descriptorHeapManager->Free(DX12DescriptorHeapType::SAMPLER, std::move(m_samplerDescriptorHandle));
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

    uint32 viewCount = 0;      // CBV/SRV/UAV or acceleration structures.
    uint32 samplerCount = 0;

    for (const auto& it : m_layout.GetElements())
    {
        const DescriptorSetLayoutElement& element = it.second;

        switch (element.type)
        {
        case ShaderInputType::UNIFORM_BUFFER:
        case ShaderInputType::UNIFORM_BUFFER_DYNAMIC:
        case ShaderInputType::STORAGE_BUFFER:
        case ShaderInputType::STORAGE_BUFFER_DYNAMIC:
        case ShaderInputType::IMAGE:
        case ShaderInputType::IMAGE_STORAGE:
        case ShaderInputType::TLAS:
            m_viewBindingToHeapOffset.Set(element.binding, viewCount);
            viewCount += element.count;
            break;
        case ShaderInputType::SAMPLER:
            m_samplerBindingToHeapOffset.Set(element.binding, samplerCount);
            samplerCount += element.count;
            break;
        default:
            HYP_UNREACHABLE();
        }
    }

    if (viewCount > 0)
    {
        m_viewDescriptorHandle = g_renderInterface->descriptorHeapManager->Allocate(DX12DescriptorHeapType::CBV_SRV_UAV, viewCount);

        if (!m_viewDescriptorHandle.IsValid())
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to allocate {} view descriptors for descriptor set: {}", 0, viewCount, m_layout.GetName());
        }

        m_viewDescriptorHeap = g_renderInterface->descriptorHeapManager->GetDescriptorHeap(DX12DescriptorHeapType::CBV_SRV_UAV);
    }

    if (samplerCount > 0)
    {
        m_samplerDescriptorHandle = g_renderInterface->descriptorHeapManager->Allocate(DX12DescriptorHeapType::SAMPLER, samplerCount);

        if (!m_samplerDescriptorHandle.IsValid())
        {
            // Free already allocated views
            if (m_viewDescriptorHandle.IsValid())
            {
                g_renderInterface->descriptorHeapManager->Free(DX12DescriptorHeapType::CBV_SRV_UAV, std::move(m_viewDescriptorHandle));
            }

            return HYP_MAKE_ERROR(RendererError, "Failed to allocate {} sampler descriptors for descriptor set: {}", 0, samplerCount, m_layout.GetName());
        }

        m_samplerDescriptorHeap = g_renderInterface->descriptorHeapManager->GetDescriptorHeap(DX12DescriptorHeapType::SAMPLER);
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
        case ShaderInputType::UNIFORM_BUFFER:
        case ShaderInputType::UNIFORM_BUFFER_DYNAMIC:
        case ShaderInputType::STORAGE_BUFFER:
        case ShaderInputType::STORAGE_BUFFER_DYNAMIC:
        {
            for (uint32 index : element.occupiedArrayElems)
            {
                ObjectBase* ptr = element.values[index];

                AssertDebug(ptr && Hyperion::IsA<DX12GpuBuffer>(ptr), "Invalid buffer descriptor: {}", name);

                DX12GpuBuffer* ref = static_cast<DX12GpuBuffer*>(ptr);
                AssertDebug(ref != nullptr);

                DX12CachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                Memory::Fill(&descriptor, 0, sizeof(DX12CachedDescriptor));
                descriptor.binding = layoutElement->binding;
                descriptor.index = index;
                descriptor.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

                AssertDebug(ref->IsCreated(), "Buffer not initialized for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                descriptor.object_ptr = ref;
            }

            break;
        }
        case ShaderInputType::IMAGE:
        case ShaderInputType::IMAGE_STORAGE:
        {
            for (uint32 index : element.occupiedArrayElems)
            {
                ObjectBase* ptr = element.values[index];

                AssertDebug(ptr && Hyperion::IsA<DX12GpuImageView>(ptr), "Invalid image descriptor: {}", name);

                DX12GpuImageView* ref = static_cast<DX12GpuImageView*>(ptr);
                AssertDebug(ref != nullptr);

                DX12CachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                Memory::Fill(&descriptor, 0, sizeof(DX12CachedDescriptor));
                descriptor.binding = layoutElement->binding;
                descriptor.index = index;
                descriptor.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

                AssertDebug(ref->GetImage() != nullptr && ref->GetImage()->GetResource() != nullptr, "Invalid image view for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                descriptor.object_ptr = ref;
            }

            break;
        }
        case ShaderInputType::SAMPLER:
        {
            for (uint32 index : element.occupiedArrayElems)
            {
                ObjectBase* ptr = element.values[index];

                AssertDebug(ptr && Hyperion::IsA<DX12Sampler>(ptr), "Invalid sampler descriptor: {}", name);

                DX12Sampler* ref = static_cast<DX12Sampler*>(ptr);
                AssertDebug(ref != nullptr);

                DX12CachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                Memory::Fill(&descriptor, 0, sizeof(DX12CachedDescriptor));
                descriptor.binding = layoutElement->binding;
                descriptor.index = index;
                descriptor.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;

                AssertDebug(ref->IsCreated(), "Invalid sampler for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                descriptor.object_ptr = ref;
            }

            break;
        }
        case ShaderInputType::TLAS:
        {
            for (uint32 index : element.occupiedArrayElems)
            {
                ObjectBase* ptr = element.values[index];

                DX12GpuTlas* ref = DynamicCast<DX12GpuTlas>(ptr);
                AssertDebug(ref != nullptr, "Invalid TLAS reference for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);
                AssertDebug(ref->IsCreated(), "Invalid TLAS for descriptor set element: {}.{}[{}]", m_layout.GetName(), name, index);

                DX12CachedDescriptor& descriptor = localDescriptors.EmplaceBack();
                Memory::Fill(&descriptor, 0, sizeof(DX12CachedDescriptor));
                descriptor.binding = layoutElement->binding;
                descriptor.index = index;
                descriptor.heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

                descriptor.object_ptr = ref;
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

    ID3D12Device* device = g_renderInterface->GetDevice();

    const uint32 incrementSize = g_renderInterface->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    const uint32 samplerIncrementSize = g_renderInterface->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

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
        case ShaderInputType::UNIFORM_BUFFER:
        case ShaderInputType::UNIFORM_BUFFER_DYNAMIC:
        {
            DX12GpuBuffer* buffer = static_cast<DX12GpuBuffer*>(ptr);
            if (!buffer->IsCreated())
            {
                continue;
            }

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = GetCBVDesc(buffer);
            device->CreateConstantBufferView(&cbvDesc, destHandle);
            break;
        }
        case ShaderInputType::STORAGE_BUFFER:
        case ShaderInputType::STORAGE_BUFFER_DYNAMIC:
        {
            DX12GpuBuffer* buffer = static_cast<DX12GpuBuffer*>(ptr);
            if (!buffer->IsCreated())
            {
                continue;
            }

            const uint32 structureStride = element->bufferStride != ~0u ? element->bufferStride : uint32(buffer->Size());
            D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = GetUAVDesc(buffer, structureStride);
            device->CreateUnorderedAccessView(buffer->GetResource(), nullptr, &uavDesc, destHandle);
            break;
        }
        case ShaderInputType::IMAGE:
        {
            DX12GpuImageView* imageView = static_cast<DX12GpuImageView*>(ptr);
            if (!imageView->IsCreated())
            {
                continue;
            }

            const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = GetSRVDesc(
                imageView->GetImage(),
                imageView->GetMipIndex(), imageView->NumMips(),
                imageView->GetLayerIndex(), imageView->NumArrayLayers());

            device->CreateShaderResourceView(imageView->GetImage()->GetResource(), &srvDesc, destHandle);
            break;
        }
        case ShaderInputType::IMAGE_STORAGE:
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

            device->CreateUnorderedAccessView(imageView->GetImage()->GetResource(), nullptr, &uavDesc, destHandle);
            break;
        }
        case ShaderInputType::SAMPLER:
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
        case ShaderInputType::TLAS:
        {
            DX12GpuTlas* tlas = DynamicCast<DX12GpuTlas>(ptr);
            if (!tlas || !tlas->IsCreated())
            {
                continue;
            }

            D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
            srvDesc.RaytracingAccelerationStructure.Location = tlas->GetBuffer()->GetBufferDeviceAddress();

            device->CreateShaderResourceView(tlas->GetBuffer()->GetResource(), &srvDesc, destHandle);
            break;
        }
        default:
            HYP_UNREACHABLE();
        }
    }

    for (auto& it : m_elements)
    {
        DescriptorSetElement& element = it.second;
        element.dirtyRange = Range<uint32>::Invalid();
    }

    m_pendingDescriptors.Clear();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorSet::GetViewCpuHandle(uint32 binding) const
{
    auto it = m_viewBindingToHeapOffset.Find(binding);
    Assert(it != m_viewBindingToHeapOffset.End(), "Binding {} not found in view binding map", binding);

    const uint32 offset = it->second;
    const uint32 incrementSize = g_renderInterface->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_viewDescriptorHandle.cpuHandle;
    handle.ptr += incrementSize * offset;

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorSet::GetSamplerCpuHandle(uint32 binding) const
{
    auto it = m_samplerBindingToHeapOffset.Find(binding);
    Assert(it != m_samplerBindingToHeapOffset.End(), "Binding {} not found in sampler binding map", binding);

    const uint32 offset = it->second;
    const uint32 incrementSize = g_renderInterface->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_samplerDescriptorHandle.cpuHandle;
    handle.ptr += incrementSize * offset;

    return handle;
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12GraphicsPipeline* pipeline, uint32 bindIndex) const
{
    Assert(m_isCreated);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();
    
    if (m_viewDescriptorHandle.IsValid())
    {
        commandList->SetGraphicsRootDescriptorTable(bindIndex * 2, m_viewDescriptorHandle.gpuHandle);
    }
    
    if (m_samplerDescriptorHandle.IsValid())
    {
        commandList->SetGraphicsRootDescriptorTable(bindIndex * 2 + 1, m_samplerDescriptorHandle.gpuHandle);
    }
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12GraphicsPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    // @TODO: Support dynamic offsets
    Bind(commandBuffer, pipeline, bindIndex);
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12ComputePipeline* pipeline, uint32 bindIndex) const
{
    Assert(m_isCreated);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();
    
    if (m_viewDescriptorHandle.IsValid())
    {
        commandList->SetComputeRootDescriptorTable(bindIndex * 2, m_viewDescriptorHandle.gpuHandle);
    }
    
    if (m_samplerDescriptorHandle.IsValid())
    {
        commandList->SetComputeRootDescriptorTable(bindIndex * 2 + 1, m_samplerDescriptorHandle.gpuHandle);
    }
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12ComputePipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    // @TODO: Support dynamic offsets
    Bind(commandBuffer, pipeline, bindIndex);
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12RayTracingPipeline* pipeline, uint32 bindIndex) const
{
    Assert(m_isCreated);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();
    
    if (m_viewDescriptorHandle.IsValid())
    {
        commandList->SetComputeRootDescriptorTable(bindIndex * 2, m_viewDescriptorHandle.gpuHandle);
    }
    
    if (m_samplerDescriptorHandle.IsValid())
    {
        commandList->SetComputeRootDescriptorTable(bindIndex * 2 + 1, m_samplerDescriptorHandle.gpuHandle);
    }
}

void DX12DescriptorSet::Bind(DX12CommandBuffer* commandBuffer, const DX12RayTracingPipeline* pipeline, const DescriptorSetOffsetMap& offsets, uint32 bindIndex) const
{
    // @TODO: Support dynamic offsets
    Bind(commandBuffer, pipeline, bindIndex);
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
