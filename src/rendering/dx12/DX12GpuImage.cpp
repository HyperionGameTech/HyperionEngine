/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12RenderBackend.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>

#include <rendering/util/SafeDeleter.hpp>

#include <DX12GpuImage.generated.inl>

namespace Hyperion {

extern DX12RenderBackend* g_renderBackend;

#pragma region DX12GpuImage

DX12GpuImage::DX12GpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags)
    : GpuImageBase(textureDesc, flags)
{
}

DX12GpuImage::~DX12GpuImage()
{
    // @TODO
}

bool DX12GpuImage::IsCreated() const
{
    return false;
}

bool DX12GpuImage::IsOwned() const
{
    return true;
}

RendererResult DX12GpuImage::Create()
{
    return Create(RS_UNDEFINED);
}

RendererResult DX12GpuImage::Create(ResourceState initialState)
{
    // @TODO
    HYPERION_RETURN_OK;
}

RendererResult DX12GpuImage::Resize(const Vec3u& extent)
{
    // @TODO
    HYPERION_RETURN_OK;
}

HANDLE DX12GpuImage::GetNativeHandle() const
{
    return nullptr;
}

void DX12GpuImage::SetResourceState(ResourceState newState)
{
    GpuImageBase::SetResourceState(newState);
}

ResourceState DX12GpuImage::GetSubResourceState(const ImageSubResource& subResource) const
{
    return m_resourceState;
}

void DX12GpuImage::SetSubResourceState(const ImageSubResource& subResource, ResourceState newState)
{
}

void DX12GpuImage::InsertBarrier(
    DX12CommandBuffer* commandBuffer,
    ResourceState newState,
    ShaderModuleType shaderModuleType)
{
    // @TODO
}

void DX12GpuImage::InsertBarrier(
    DX12CommandBuffer* commandBuffer,
    const ImageSubResource& subResource,
    ResourceState newState,
    ShaderModuleType shaderModuleType)
{
    // @TODO
}

RendererResult DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage)
{
    // @TODO
    HYPERION_RETURN_OK;
}

RendererResult DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage,
    uint32 srcMip,
    uint32 dstMip,
    uint32 srcFace,
    uint32 dstFace)
{
    // @TODO
    HYPERION_RETURN_OK;
}

RendererResult DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage,
    Rect<uint32> srcRect,
    Rect<uint32> dstRect)
{
    // @TODO
    HYPERION_RETURN_OK;
}

RendererResult DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage,
    Rect<uint32> srcRect,
    Rect<uint32> dstRect,
    uint32 srcMip,
    uint32 dstMip,
    uint32 srcFace,
    uint32 dstFace)
{
    // @TODO
    HYPERION_RETURN_OK;
}

RendererResult DX12GpuImage::GenerateMipmaps(DX12CommandBuffer* commandBuffer)
{
    // @TODO
    HYPERION_RETURN_OK;
}

void DX12GpuImage::CopyFromBuffer(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuBuffer* srcBuffer,
    uint32 srcBufferOffset,
    uint8 dstMipIndex,
    uint16 dstArrayLayer) const
{
    // @TODO
}

void DX12GpuImage::CopyToBuffer(
    CommandBuffer* commandBuffer,
    GpuBuffer* dstBuffer) const
{
    // @TODO
}

DX12GpuImageViewRef DX12GpuImage::MakeLayerImageView(uint32 layerIndex) const
{
    // @TODO
    return DX12GpuImageViewRef();
}

#ifdef HYP_DEBUG_MODE
void DX12GpuImage::SetDebugName(Name name)
{
    GpuImageBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuImage

} // namespace Hyperion
