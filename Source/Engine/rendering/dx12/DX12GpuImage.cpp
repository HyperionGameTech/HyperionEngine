/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <DX12GpuImage.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12GpuImage

DX12GpuImage::DX12GpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags)
    : GpuImageBase(textureDesc, flags)
{
}

DX12GpuImage::~DX12GpuImage()
{
}

bool DX12GpuImage::IsCreated() const
{
    return m_resource != nullptr;
}

bool DX12GpuImage::IsOwned() const
{
    return true;
}

RendererResult DX12GpuImage::Create()
{
    return Create(RS_UNDEFINED);
}

HYP_DISABLE_OPTIMIZATION;
RendererResult DX12GpuImage::Create(ResourceState initialState)
{
    D3D12_RESOURCE_STATES resourceStates = ToDX12ResourceStates(initialState);

    const Vec3u extent = GetExtent();

    const TextureFormat format = GetTextureFormat();
    const TextureType type = GetType();

    const bool isAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];
    const bool isRWTexture = m_textureDesc.imageUsage[IU_STORAGE];
    const bool isExternalMemory = m_textureDesc.imageUsage[IU_EXTERNAL];

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool isBlended = m_textureDesc.IsBlended();
    const bool isSrgb = m_textureDesc.IsSrgb();

    const bool hasMipmaps = m_textureDesc.HasMipMaps();
    const uint32 numMipmaps = m_textureDesc.NumMips();
    const uint32 numLayers = m_textureDesc.NumArrayLayers();
    
    if (extent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Alignment = 0;
    resourceDesc.Width = extent.x;
    resourceDesc.Height = extent.y;
    resourceDesc.DepthOrArraySize = extent.z;
    resourceDesc.MipLevels = m_textureDesc.HasMipMaps() ? m_textureDesc.NumMips() : 1;
    resourceDesc.Format = ToDXGIFormat(m_textureDesc.format);
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    switch (m_textureDesc.type)
    {
    case TextureType::Texture3D:
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        break;
    case TextureType::Texture2D: // fallthrough
    case TextureType::Cubemap:
    case TextureType::Texture2DArray:
    case TextureType::CubemapArray:
        resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        break;
    default:
        HYP_UNREACHABLE();
    }

    if (isDepthStencil)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    }
    else if (isAttachmentTexture)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    }
    else if (isRWTexture)
    {
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    }

    if (m_textureDesc.IsTexture2DArray()
        || m_textureDesc.IsTextureCube()
        || m_textureDesc.IsTextureCubeArray())
    {
        resourceDesc.DepthOrArraySize = m_textureDesc.NumArrayLayers();
    }

    D3D12_CLEAR_VALUE clearValue {};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;

    if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        clearValue.Format = resourceDesc.Format;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 1.0f;
        pClearValue = &clearValue;
    }
    else if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        switch (m_textureDesc.format)
        {
        case TextureFormat::D16:
            clearValue.Format = DXGI_FORMAT_D16_UNORM;
            break;
        case TextureFormat::D24_S8:
            clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
            break;
        case TextureFormat::D32F:
            clearValue.Format = DXGI_FORMAT_D32_FLOAT;
            break;
        case TextureFormat::D32F_S8:
            clearValue.Format = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            break;
        default:
            HYP_UNREACHABLE();
        }

        pClearValue = &clearValue;
    }

    D3D12MA::ALLOCATION_DESC allocDesc {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = g_renderInterface->GetAllocator()->CreateResource(
        &allocDesc,
        &resourceDesc,
        resourceStates,
        pClearValue,
        &m_allocation,
        __uuidof(ID3D12Resource),
        &m_resource
    );

    if (!SUCCEEDED(hr))
        return HYP_MAKE_ERROR(RendererError, "Failed to create image resource!", hr);

    return {};
}

RendererResult DX12GpuImage::Resize(const Vec3u& extent)
{
    // @TODO
    return {};
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
    return {};
}

RendererResult DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage,
    Rect<uint32> srcRect,
    Rect<uint32> dstRect)
{
    // @TODO
    return {};
}
        
RendererResult DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage,
    Rect<uint32> srcRect,
    Rect<uint32> dstRect,
    const ImageSubResource& srcSubResource,
    const ImageSubResource& dstSubResource)
{
    // @TODO
    return {};
}

RendererResult DX12GpuImage::GenerateMipmaps(DX12CommandBuffer* commandBuffer)
{
    // @TODO
    return {};
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
    DX12CommandBuffer* commandBuffer,
    DX12GpuBuffer* dstBuffer) const
{
    // @TODO
}

DX12GpuImageViewRef DX12GpuImage::MakeLayerImageView(uint32 layerIndex) const
{
    if (!IsCreated())
    {
        HYP_LOG(
            RenderingBackend,
            Warning,
            "Attempt to create image view on uninitialized image");

        return DX12GpuImageViewRef::Null();
    }

    return g_renderInterface->MakeImageView(
        MakeStrongRef(this),
        0,
        m_textureDesc.NumMips(),
        layerIndex,
        1);
}

#ifdef HYP_DEBUG_MODE
void DX12GpuImage::SetDebugName(Name name)
{
    GpuImageBase::SetDebugName(name);
}
#endif

#pragma endregion DX12GpuImage

} // namespace Hyperion
