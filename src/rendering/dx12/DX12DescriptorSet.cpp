/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12DescriptorSet.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>
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

extern DX12RenderBackend* g_renderBackend;

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
        case DescriptorSetElementType::UNIFORM_BUFFER:
        case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:
        case DescriptorSetElementType::SSBO:
        case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:
            PrefillElements<DX12GpuBuffer>(name, element.count);
            break;
        case DescriptorSetElementType::IMAGE:
        case DescriptorSetElementType::IMAGE_STORAGE:
            PrefillElements<DX12GpuImageView>(name, element.count);
            break;
        case DescriptorSetElementType::SAMPLER:
            PrefillElements<DX12Sampler>(name, element.count);
            break;
        case DescriptorSetElementType::TLAS:
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
        g_renderBackend->descriptorHeapManager->Free(DX12DescriptorHeapType::CBV_SRV_UAV, std::move(m_viewDescriptorHandle));
    }

    if (m_samplerDescriptorHandle.IsValid())
    {
        g_renderBackend->descriptorHeapManager->Free(DX12DescriptorHeapType::SAMPLER, std::move(m_samplerDescriptorHandle));
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
        case DescriptorSetElementType::UNIFORM_BUFFER:
        case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:
        case DescriptorSetElementType::SSBO:
        case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:
        case DescriptorSetElementType::IMAGE:
        case DescriptorSetElementType::IMAGE_STORAGE:
        case DescriptorSetElementType::TLAS:
            m_viewBindingToHeapOffset.Set(element.binding, viewCount);
            viewCount += element.count;
            break;
        case DescriptorSetElementType::SAMPLER:
            m_samplerBindingToHeapOffset.Set(element.binding, samplerCount);
            samplerCount += element.count;
            break;
        default:
            HYP_UNREACHABLE();
        }
    }

    if (viewCount > 0)
    {
        m_viewDescriptorHandle = g_renderBackend->descriptorHeapManager->Allocate(DX12DescriptorHeapType::CBV_SRV_UAV, viewCount);
        
        if (!m_viewDescriptorHandle.IsValid())
        {
            return HYP_MAKE_ERROR(RendererError, "Failed to allocate {} view descriptors for descriptor set: {}", 0, viewCount, m_layout.GetName());
        }
    }

    if (samplerCount > 0)
    {
        m_samplerDescriptorHandle = g_renderBackend->descriptorHeapManager->Allocate(DX12DescriptorHeapType::SAMPLER, samplerCount);
        
        if (!m_samplerDescriptorHandle.IsValid())
        {
            // Free already allocated views
            if (m_viewDescriptorHandle.IsValid())
            {
                g_renderBackend->descriptorHeapManager->Free(DX12DescriptorHeapType::CBV_SRV_UAV, std::move(m_viewDescriptorHandle));
            }

            return HYP_MAKE_ERROR(RendererError, "Failed to allocate {} sampler descriptors for descriptor set: {}", 0, samplerCount, m_layout.GetName());
        }
    }

    for (const auto& it : m_elements)
    {
        const Name name = it.first;
        const DescriptorSetElement& element = it.second;

        auto cacheIt = m_elementCache.Find(name);
        if (cacheIt == m_elementCache.End())
        {
            cacheIt = m_elementCache.Emplace(name).first;
        }

        cacheIt->second.Resize(element.values.Size());
    }

    m_isCreated = true;

    UpdateDirtyState();
    Update();

    return {};
}

void DX12DescriptorSet::UpdateDirtyState(bool* outIsDirty)
{
    // Ensure all cached value containers are prepared
    for (auto& it : m_elements)
    {
        const Name name = it.first;
        const DescriptorSetElement& element = it.second;

        auto cachedIt = m_elementCache.Find(name);

        if (cachedIt == m_elementCache.End())
        {
            cachedIt = m_elementCache.Emplace(name).first;
        }

        Array<DX12DescriptorHandle>& cachedElementValues = cachedIt->second;

        if (cachedElementValues.Size() != element.values.Size())
        {
            cachedElementValues.Resize(element.values.Size());
        }
    }

    bool isDirty = false;

    for (auto& it : m_elements)
    {
        DescriptorSetElement& element = it.second;

        if (element.dirtyRange.Distance() > 0)
        {
            isDirty = true;
        }
    }

    if (outIsDirty)
    {
        *outIsDirty = isDirty;
    }
}

void DX12DescriptorSet::Update(bool force)
{
    //if (!m_isCreated)
    //{
    //    return;
    //}

    //ID3D12Device* device = g_renderBackend->GetDevice();

    //for (auto& it : m_elements)
    //{
    //    const Name name = it.first;
    //    DescriptorSetElement& element = it.second;

    //    const DescriptorSetLayoutElement* layoutElement = m_layout.GetElement(name);
    //    Assert(layoutElement != nullptr, "Invalid element: No item with name {} found", name);

    //    if (!force && element.dirtyRange.Distance() == 0)
    //    {
    //        continue;
    //    }

    //    const uint32 incrementSize = g_renderBackend->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    //    const uint32 samplerIncrementSize = g_renderBackend->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

    //    switch (layoutElement->type)
    //    {
    //    case DescriptorSetElementType::UNIFORM_BUFFER:          // fallthrough
    //    case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:  // fallthrough
    //    case DescriptorSetElementType::SSBO:                    // fallthrough
    //    case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:
    //    {
    //        const bool layoutHasSize = layoutElement->size != 0 && layoutElement->size != ~0u;
    //        const bool isDynamic = layoutElement->type == DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC
    //            || layoutElement->type == DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC;

    //        for (auto& valuesIt : element.values)
    //        {
    //            const uint32 index = valuesIt.first;

    //            if (!force && !element.dirtyRange.Includes(index))
    //            {
    //                continue;
    //            }

    //            const DX12GpuBufferRef& ref = ObjCast<DX12GpuBuffer>(valuesIt.second);
    //            if (!ref.IsValid() || !ref->IsCreated())
    //            {
    //                continue;
    //            }

    //            D3D12_CPU_DESCRIPTOR_HANDLE destHandle = GetViewCpuHandle(layoutElement->binding);
    //            destHandle.ptr += incrementSize * index;

    //            switch (layoutElement->type)
    //            { // CBuffers
    //            case DescriptorSetElementType::UNIFORM_BUFFER:
    //            case DescriptorSetElementType::UNIFORM_BUFFER_DYNAMIC:
    //            {
    //                D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = GetCBVDesc(ref.Get());
    //                device->CreateConstantBufferView(&cbvDesc, destHandle);

    //                break;
    //            }
    //            case DescriptorSetElementType::SSBO:
    //            case DescriptorSetElementType::STORAGE_BUFFER_DYNAMIC:
    //            {
    //                // Use UAV for storage buffers
    //                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = GetUAVDesc(
    //                    ref.Get(),
    //                    /* structureStride */ layoutHasSize ? layoutElement->size : ref->Size());

    //                device->CreateUnorderedAccessView(ref->GetResource(), nullptr, &uavDesc, destHandle);

    //                break;
    //            }
    //            default:
    //                HYP_UNREACHABLE();
    //            }
    //        }
    //        break;
    //    }
    //    case DescriptorSetElementType::IMAGE:
    //    case DescriptorSetElementType::IMAGE_STORAGE:
    //    {
    //        const bool isStorageImage = layoutElement->type == DescriptorSetElementType::IMAGE_STORAGE;

    //        for (auto& valuesIt : element.values)
    //        {
    //            const uint32 index = valuesIt.first;

    //            if (!force && !element.dirtyRange.Includes(index))
    //            {
    //                continue;
    //            }

    //            const DX12GpuImageViewRef& ref = ObjCast<DX12GpuImageView>(valuesIt.second);
    //            if (!ref.IsValid() || !ref->IsCreated())
    //            {
    //                continue;
    //            }

    //            D3D12_CPU_DESCRIPTOR_HANDLE destHandle = GetViewCpuHandle(layoutElement->binding);
    //            destHandle.ptr += incrementSize * index;

    //            if (layoutElement->type == DescriptorSetElementType::IMAGE_STORAGE)
    //            { // UAV
    //                const D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = GetUAVDesc(
    //                    ref->GetImage(),
    //                    ref->GetMipIndex(), ref->NumMips(),
    //                    ref->GetLayerIndex(), ref->NumArrayLayers());

    //                device->CreateUnorderedAccessView(ref->GetImage()->GetResource(), nullptr, &uavDesc, destHandle);
    //            }
    //            else
    //            { // SRV
    //                const D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = GetSRVDesc(
    //                    ref->GetImage(),
    //                    ref->GetMipIndex(), ref->NumMips(),
    //                    ref->GetLayerIndex(), ref->NumArrayLayers());

    //                device->CreateShaderResourceView(ref->GetImage()->GetResource(), &srvDesc, destHandle);
    //            }
    //        }

    //        break;
    //    }
    //    case DescriptorSetElementType::SAMPLER:
    //    {
    //        for (auto& valuesIt : element.values)
    //        {
    //            const uint32 index = valuesIt.first;

    //            if (!force && !element.dirtyRange.Includes(index))
    //            {
    //                continue;
    //            }

    //            const DX12SamplerRef& ref = ObjCast<DX12Sampler>(valuesIt.second);
    //            if (!ref.IsValid() || !ref->IsCreated())
    //            {
    //                continue;
    //            }

    //            D3D12_CPU_DESCRIPTOR_HANDLE destHandle = GetSamplerCpuHandle(layoutElement->binding);
    //            destHandle.ptr += samplerIncrementSize * index;

    //            // Create sampler descriptor
    //            D3D12_SAMPLER_DESC samplerDesc {};
    //            samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;  // @TODO 
    //            samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // @TODO
    //            samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // @TODO
    //            samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // @TODO
    //            samplerDesc.MipLODBias = 0.0f;  // @TODO
    //            samplerDesc.MaxAnisotropy = 1;
    //            samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    //            samplerDesc.MinLOD = 0.0f;
    //            samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;

    //            device->CreateSampler(&samplerDesc, destHandle);
    //        }
    //        break;
    //    }
    //    case DescriptorSetElementType::TLAS:
    //    {
    //        for (auto& valuesIt : element.values)
    //        {
    //            const uint32 index = valuesIt.first;

    //            if (!force && !element.dirtyRange.Includes(index))
    //            {
    //                continue;
    //            }

    //            const DX12GpuTlasRef& ref = ObjCast<DX12GpuTlas>(valuesIt.second);
    //            if (!ref.IsValid() || !ref->IsCreated())
    //            {
    //                continue;
    //            }

    //            // TLAS descriptors are created as SRVs in DX12
    //            // @TODO
    //        }
    //        break;
    //    }
    //    default:
    //        HYP_UNREACHABLE();
    //    }

    //    element.dirtyRange = Range<uint32>::Invalid();
    //}

    HYP_NOT_IMPLEMENTED();
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorSet::GetViewCpuHandle(uint32 binding) const
{
    auto it = m_viewBindingToHeapOffset.Find(binding);
    Assert(it != m_viewBindingToHeapOffset.End(), "Binding {} not found in view binding map", binding);

    const uint32 offset = it->second;
    const uint32 incrementSize = g_renderBackend->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    D3D12_CPU_DESCRIPTOR_HANDLE handle = m_viewDescriptorHandle.cpuHandle;
    handle.ptr += incrementSize * offset;

    return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE DX12DescriptorSet::GetSamplerCpuHandle(uint32 binding) const
{
    auto it = m_samplerBindingToHeapOffset.Find(binding);
    Assert(it != m_samplerBindingToHeapOffset.End(), "Binding {} not found in sampler binding map", binding);

    const uint32 offset = it->second;
    const uint32 incrementSize = g_renderBackend->GetDevice()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

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
