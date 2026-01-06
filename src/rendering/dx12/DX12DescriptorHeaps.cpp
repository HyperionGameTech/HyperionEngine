/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12DescriptorHeaps.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

DX12DescriptorHeapManager::DX12DescriptorHeapManager()
    : m_descriptorSizes {},
      m_descriptorHeaps {}
{
}

void DX12DescriptorHeapManager::Initialize()
{
    ID3D12Device* device = g_renderBackend->GetDevice();

    // CBV_SRV_UAV
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.NumDescriptors = 1000; // @TODO
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeaps[uint32(DX12DescriptorHeapType::CBV_SRV_UAV)]));
        Assert(hr, "Failed to create CBV_SRV_UAV descriptor heap");

        m_descriptorSizes[uint32(DX12DescriptorHeapType::CBV_SRV_UAV)] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }

    // SAMPLER
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        heapDesc.NumDescriptors = 1000; // @TODO
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeaps[uint32(DX12DescriptorHeapType::SAMPLER)]));
        Assert(hr, "Failed to create SAMPLER descriptor heap");

        m_descriptorSizes[uint32(DX12DescriptorHeapType::SAMPLER)] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    }

    // RTV
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.NumDescriptors = 100; // @TODO
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeaps[uint32(DX12DescriptorHeapType::RTV)]));
        Assert(hr, "Failed to create RTV descriptor heap");

        m_descriptorSizes[uint32(DX12DescriptorHeapType::RTV)] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    }

    // DSV
    {
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc {};
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        heapDesc.NumDescriptors = 100; // @TODO
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeaps[uint32(DX12DescriptorHeapType::DSV)]));
        Assert(hr, "Failed to create DSV descriptor heap");

        m_descriptorSizes[uint32(DX12DescriptorHeapType::DSV)] = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    }
}

void DX12DescriptorHeapManager::Shutdown()
{
    for (uint32 i = 0; i < uint32(DX12DescriptorHeapType::MAX); ++i)
    {
        m_descriptorHeaps[i].Reset();
        m_descriptorSizes[i] = 0;
    }
}

uint32 DX12DescriptorHeapManager::GetDescriptorSize(DX12DescriptorHeapType heapType) const
{
    return m_descriptorSizes[uint32(heapType)];
}

ID3D12DescriptorHeap* DX12DescriptorHeapManager::GetDescriptorHeap(DX12DescriptorHeapType heapType) const
{
    return m_descriptorHeaps[uint32(heapType)].Get();
}

} // namespace Hyperion
