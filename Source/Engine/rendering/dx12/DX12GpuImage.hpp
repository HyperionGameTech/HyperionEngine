/* Copyright (c) 2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#ifndef INCLUDE_FROM_RHI_BASE
#define INCLUDE_FROM_RHI
#include <rendering/GpuImage.hpp>
#endif

#undef INCLUDE_FROM_RHI
#undef INCLUDE_FROM_RHI_BASE

#include <rendering/dx12/DX12CommandBuffer.hpp>

// Fwd declaration
namespace D3D12MA { class Allocation; }

namespace Hyperion {

HYP_CLASS(NoScriptBindings)
class DX12GpuImage final : public GpuImageBase
{
    HYP_OBJECT_BODY(DX12GpuImage);

public:
    explicit DX12GpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags = GpuImageFlags::NONE);
    ~DX12GpuImage() override;

    HYP_FORCE_INLINE ID3D12Resource* GetResource() const
    {
        return m_resource.Get();
    }

    HYP_FORCE_INLINE D3D12MA::Allocation* GetAllocation() const
    {
        return m_allocation.Get();
    }

    bool IsCreated() const override;
    bool IsOwned() const override;

    RendererResult Create() override;
    RendererResult Create(ResourceState initialState) override;

    RendererResult Resize(const Vec3u& extent) override;

    HANDLE GetNativeHandle() const override;

    void SetResourceState(ResourceState newState) override;

    ResourceState GetSubResourceState(const ImageSubResource& subResource) const;
    void SetSubResourceState(const ImageSubResource& subResource, ResourceState newState);

    void InsertBarrier(
        DX12CommandBuffer* commandBuffer,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    void InsertBarrier(
        DX12CommandBuffer* commandBuffer,
        const ImageSubResource& subResource,
        ResourceState newState,
        ShaderModuleType shaderModuleType) override;

    RendererResult Blit(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuImage* srcImage) override;

    RendererResult Blit(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuImage* srcImage,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect) override;
        
    RendererResult Blit(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuImage* srcImage,
        Rect<uint32> srcRect,
        Rect<uint32> dstRect,
        const ImageSubResource& srcSubResource,
        const ImageSubResource& dstSubResource) override;

    RendererResult GenerateMipmaps(DX12CommandBuffer* commandBuffer) override;

    void CopyFromBuffer(
        DX12CommandBuffer* commandBuffer,
        const DX12GpuBuffer* srcBuffer,
        uint32 srcBufferOffset = 0,
        uint8 dstMipIndex = UINT8_MAX,
        uint16 dstArrayLayer = UINT16_MAX) const override;

    void CopyToBuffer(
        DX12CommandBuffer* commandBuffer,
        DX12GpuBuffer* dstBuffer) const override;

    DX12GpuImageViewRef MakeLayerImageView(uint32 layerIndex) const override;

#ifdef HYP_DEBUG_MODE
    void SetDebugName(Name name) override;
#endif

private:
    ComPtr<ID3D12Resource> m_resource;
    ComPtr<D3D12MA::Allocation> m_allocation;
};

} // namespace Hyperion
