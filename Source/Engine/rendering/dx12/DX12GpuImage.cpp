/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Core/functional/Proc.hpp>

#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12GpuImageView.hpp>
#include <rendering/dx12/DX12RenderInterface.hpp>
#include <rendering/dx12/DX12Frame.hpp>
#include <rendering/dx12/DX12CommandBuffer.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12Helpers.hpp>
#include <rendering/Shared.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <DX12GpuImage.generated.inl>

namespace Hyperion {

extern DX12RenderInterface* g_renderInterface;

#pragma region DX12GpuImage

DX12GpuImage::DX12GpuImage(const TextureDesc& textureDesc, EnumFlags<GpuImageFlags> flags)
    : GpuImageBase(textureDesc, flags)
{
    m_size = textureDesc.GetByteSize();
}

DX12GpuImage::~DX12GpuImage()
{
    if (IsCreated())
    {
        if (m_allocation != nullptr)
        {
            Assert(m_isHandleOwned, "If allocation is not null, m_isHandleOwned should be true");

            EnqueueDeletion(FunctionWrapper<Proc<void()>>([allocation = m_allocation]() -> void
                {
                    allocation->Release();
                }));
        }

        m_resource.Reset();

        m_isHandleOwned = true;

        m_resourceState = RS_UNDEFINED;
        m_stencilState = RS_UNDEFINED;
        m_subResourceStates.Clear();
    }
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
    if (extent == m_textureDesc.extent)
    {
        return {};
    }

    if (extent.Volume() == 0)
    {
        return HYP_MAKE_ERROR(RendererError, "Invalid image extent - width*height*depth cannot equal zero");
    }

    m_textureDesc.extent = extent;

    const uint32 newSize = m_textureDesc.GetByteSize();

    if (newSize != m_size)
    {
        m_size = newSize;
    }

    const ResourceState previousResourceState = m_resourceState;

    if (IsCreated())
    {
        if (!m_isHandleOwned)
        {
            return HYP_MAKE_ERROR(RendererError, "Cannot resize non-owned image");
        }

        m_resource.Reset();
        m_allocation.Reset();

        m_resourceState = RS_UNDEFINED;
        m_stencilState = RS_UNDEFINED;
        m_subResourceStates.Clear();

        CheckResultOrReturn(Create());

        if (previousResourceState != RS_UNDEFINED)
        {
            SetResourceState(RS_UNDEFINED);

            DX12Frame* frame = g_renderInterface->GetCurrentFrame();
            CommandRecorder& cr = frame->cr;
            cr << ::Hyperion::InsertBarrier(this, previousResourceState);
        }
    }

    return {};
}

HANDLE DX12GpuImage::GetNativeHandle() const
{
    return nullptr;
}

ResourceState DX12GpuImage::GetSubResourceState(const ImageSubResource& subResource) const
{
    const uint32 subResourceIndex = D3D12CalcSubresource(
        subResource.baseMipLevel,
        subResource.baseArrayLayer,
        0,
        NumMips(),
        NumArrayLayers());

    auto it = m_subResourceStates.Find(subResourceIndex);
    if (it != m_subResourceStates.End())
    {
        return it->second;
    }

    return m_resourceState;
}

void DX12GpuImage::SetSubResourceState(const ImageSubResource& subResource, ResourceState newState)
{
    if (subResource.baseMipLevel == 0 && subResource.numLevels >= NumMips()
        && subResource.baseArrayLayer == 0 && subResource.numLayers >= NumArrayLayers())
    {
        SetResourceState(newState);
        m_subResourceStates.Clear();
        return;
    }

    const uint32 subResourceIndex = D3D12CalcSubresource(
        subResource.baseMipLevel,
        subResource.baseArrayLayer,
        0,
        NumMips(),
        NumArrayLayers());

    m_subResourceStates.Set(subResourceIndex, newState);
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
    ShaderModuleType shaderModuleType,
    bool onlyDepth,
    bool onlyStencil)
{
    ImageSubResource subResource {};
    subResource.baseMipLevel = 0;
    subResource.numLevels = NumMips();
    subResource.baseArrayLayer = 0;
    subResource.numLayers = NumArrayLayers();

    InsertBarrier(
        commandBuffer,
        subResource,
        newState,
        shaderModuleType,
        onlyDepth,
        onlyStencil);
}

void DX12GpuImage::InsertBarrier(
    DX12CommandBuffer* commandBuffer,
    const ImageSubResource& subResource,
    ResourceState newState,
    ShaderModuleType shaderModuleType,
    bool onlyDepth,
    bool onlyStencil)
{
    if (m_resource == nullptr)
    {
        HYP_LOG(
            RenderingBackend,
            Warning,
            "Attempt to insert a resource barrier but image was not defined");

        return;
    }

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool hasStencil = TextureUtils::HasStencilComponent(m_textureDesc.format);

    onlyDepth &= hasStencil;
    onlyStencil &= hasStencil;

    D3D12_RESOURCE_STATES stateBefore = ToDX12ResourceStates(m_resourceState);
    D3D12_RESOURCE_STATES stateAfter = ToDX12ResourceStates(newState);

    uint32 subResourceIndex = D3D12CalcSubresource(
        subResource.baseMipLevel,
        subResource.baseArrayLayer,
        0,
        NumMips(),
        NumArrayLayers());

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = m_resource.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    if (subResource.baseMipLevel == 0 && subResource.numLevels >= NumMips()
        && subResource.baseArrayLayer == 0 && subResource.numLayers >= NumArrayLayers())
    {
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = stateAfter;
    }
    else
    {
        barrier.Transition.Subresource = subResourceIndex;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = stateAfter;
    }

    commandBuffer->GetCommandList()->ResourceBarrier(1, &barrier);

    if (subResource.baseMipLevel == 0 && subResource.numLevels >= NumMips()
        && subResource.baseArrayLayer == 0 && subResource.numLayers >= NumArrayLayers())
    {
        if (onlyStencil)
        {
            m_stencilState = newState;
        }
        else
        {
            SetResourceState(newState);

            if (onlyDepth)
            {
                m_stencilState = newState;
            }
        }
    }
    else
    {
        m_subResourceStates.Set(subResourceIndex, newState);
    }
}

void DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage)
{
    Blit(
        commandBuffer,
        srcImage,
        Rect<uint32> { 0, 0, srcImage->GetExtent().x, srcImage->GetExtent().y },
        Rect<uint32> { 0, 0, m_textureDesc.extent.x, m_textureDesc.extent.y },
        ImageSubResource {
            .numLevels = srcImage->m_textureDesc.NumMips(),
            .numLayers = srcImage->m_textureDesc.NumArrayLayers()
        },
        ImageSubResource {
            .numLevels = m_textureDesc.NumMips(),
            .numLayers = m_textureDesc.NumArrayLayers() 
    });
}

void DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage,
    const Rect<uint32>& srcRect,
    const Rect<uint32>& dstRect)
{
    Blit(
        commandBuffer,
        srcImage,
        srcRect,
        dstRect,
        ImageSubResource {
            .numLevels = srcImage->m_textureDesc.NumMips(),
            .numLayers = srcImage->m_textureDesc.NumArrayLayers()
        },
        ImageSubResource {
            .numLevels = m_textureDesc.NumMips(),
            .numLayers = m_textureDesc.NumArrayLayers()
        });
}
        
void DX12GpuImage::Blit(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage,
    const Rect<uint32>& srcRect,
    const Rect<uint32>& dstRect,
    const ImageSubResource& srcSubResource,
    const ImageSubResource& dstSubResource)
{
    const bool srcIsDepthStencil = srcImage->GetTextureDesc().IsDepthStencil();
    const bool dstIsDepthStencil = m_textureDesc.IsDepthStencil();

    D3D12_RESOURCE_STATES srcState = srcIsDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COPY_SOURCE;
    D3D12_RESOURCE_STATES dstState = dstIsDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COPY_DEST;

    D3D12_TEXTURE_COPY_LOCATION srcLocation {};
    srcLocation.pResource = srcImage->GetResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLocation.SubresourceIndex = D3D12CalcSubresource(
        srcSubResource.baseMipLevel,
        srcSubResource.baseArrayLayer,
        0,
        srcImage->NumMips(),
        srcImage->NumArrayLayers());

    D3D12_TEXTURE_COPY_LOCATION dstLocation {};
    dstLocation.pResource = m_resource.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = D3D12CalcSubresource(
        dstSubResource.baseMipLevel,
        dstSubResource.baseArrayLayer,
        0,
        NumMips(),
        NumArrayLayers());

    D3D12_BOX srcBox {};
    srcBox.left = srcRect.x0;
    srcBox.top = srcRect.y0;
    srcBox.front = 0;
    srcBox.right = srcRect.x1;
    srcBox.bottom = srcRect.y1;
    srcBox.back = 1;

    commandBuffer->GetCommandList()->CopyTextureRegion(
        &dstLocation,
        dstRect.x0, dstRect.y0, 0,
        &srcLocation,
        &srcBox);
}

RendererResult DX12GpuImage::GenerateMipmaps(DX12CommandBuffer* commandBuffer)
{
    if (m_resource == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot generate mipmaps on uninitialized image");
    }

    const uint16 numLayers = NumArrayLayers();
    const uint8 numMipmaps = NumMips();

    for (uint16 face = 0; face < numLayers; face++)
    {
        for (int32 i = 1; i < int32(numMipmaps + 1); i++)
        {
            const int mipWidth = int(helpers::MipmapSize(m_textureDesc.extent.x, i)),
                      mipHeight = int(helpers::MipmapSize(m_textureDesc.extent.y, i)),
                      mipDepth = int(helpers::MipmapSize(m_textureDesc.extent.z, i));

            const ImageSubResource src {
                .baseMipLevel = uint8(i - 1),
                .numLevels = 1,
                .baseArrayLayer = face,
                .numLayers = 1
            };

            const ImageSubResource dst {
                .baseMipLevel = uint8(i),
                .numLevels = 1,
                .baseArrayLayer = src.baseArrayLayer,
                .numLayers = 1
            };

            InsertBarrier(
                commandBuffer,
                src,
                RS_COPY_SRC,
                ShaderModuleType::None);

            if (i == int32(numMipmaps))
            {
                if (face == numLayers - 1)
                {
                    SetResourceState(RS_COPY_SRC);
                }

                break;
            }

            D3D12_TEXTURE_COPY_LOCATION srcLocation {};
            srcLocation.pResource = m_resource.Get();
            srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            srcLocation.SubresourceIndex = D3D12CalcSubresource(
                src.baseMipLevel,
                src.baseArrayLayer,
                0,
                NumMips(),
                NumArrayLayers());

            D3D12_TEXTURE_COPY_LOCATION dstLocation {};
            dstLocation.pResource = m_resource.Get();
            dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dstLocation.SubresourceIndex = D3D12CalcSubresource(
                dst.baseMipLevel,
                dst.baseArrayLayer,
                0,
                NumMips(),
                NumArrayLayers());

            D3D12_BOX srcBox {};
            srcBox.left = 0;
            srcBox.top = 0;
            srcBox.front = 0;
            srcBox.right = uint32(helpers::MipmapSize(m_textureDesc.extent.x, i - 1));
            srcBox.bottom = uint32(helpers::MipmapSize(m_textureDesc.extent.y, i - 1));
            srcBox.back = uint32(helpers::MipmapSize(m_textureDesc.extent.z, i - 1));

            commandBuffer->GetCommandList()->CopyTextureRegion(
                &dstLocation,
                0, 0, 0,
                &srcLocation,
                &srcBox);
        }
    }

    return {};
}

void DX12GpuImage::CopyFromBuffer(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuBuffer* srcBuffer,
    uint32 srcBufferOffset,
    uint8 dstMipIndex,
    uint16 dstArrayLayer) const
{
    const bool isDepthStencil = m_textureDesc.IsDepthStencil();

    D3D12_RESOURCE_STATES state = isDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COPY_DEST;

    D3D12_TEXTURE_COPY_LOCATION srcLocation {};
    srcLocation.pResource = srcBuffer->GetResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_BUFFER;
    srcLocation.Buffer.Location = srcBuffer->GetResource()->GetGPUVirtualAddress() + srcBufferOffset;
    srcLocation.Buffer.RowPitch = 0;
    srcLocation.Buffer.PlacedFootprint.Footprint.Depth = 1;
    srcLocation.Buffer.PlacedFootprint.Footprint.Height = 1;
    srcLocation.Buffer.PlacedFootprint.Footprint.Width = 1;
    srcLocation.Buffer.PlacedFootprint.Offset = 0;

    const uint8 mipIdx = dstMipIndex != UINT8_MAX ? dstMipIndex : 0;
    const Vec3u mipExtent = m_textureDesc.GetMipExtent(mipIdx);

    D3D12_TEXTURE_COPY_LOCATION dstLocation {};
    dstLocation.pResource = m_resource.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = D3D12CalcSubresource(
        mipIdx,
        dstArrayLayer == UINT16_MAX ? 0 : dstArrayLayer,
        0,
        NumMips(),
        NumArrayLayers());

    D3D12_BOX srcBox {};
    srcBox.left = 0;
    srcBox.top = 0;
    srcBox.front = 0;
    srcBox.right = mipExtent.x;
    srcBox.bottom = mipExtent.y;
    srcBox.back = mipExtent.z;

    commandBuffer->GetCommandList()->CopyTextureRegion(
        &dstLocation,
        0, 0, 0,
        &srcLocation,
        &srcBox);
}

void DX12GpuImage::CopyToBuffer(
    DX12CommandBuffer* commandBuffer,
    DX12GpuBuffer* dstBuffer,
    const ImageSubResource& subResource) const
{
    Assert(dstBuffer != nullptr && dstBuffer->IsCreated(), "Destination buffer is null or invalid!");
    Assert(dstBuffer->Size() >= m_size, "Destination buffer is too small to hold image data!");

    ImageSubResource newSubResource = subResource;
    newSubResource.numLayers = MathUtil::Min(subResource.numLayers, NumArrayLayers() - subResource.baseArrayLayer);
    newSubResource.numLevels = MathUtil::Min(subResource.numLevels, NumMips() - subResource.baseMipLevel);

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();

    D3D12_RESOURCE_STATES state = isDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COPY_SOURCE;

    D3D12_TEXTURE_COPY_LOCATION srcLocation {};
    srcLocation.pResource = m_resource.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    D3D12_TEXTURE_COPY_LOCATION dstLocation {};
    dstLocation.pResource = dstBuffer->GetResource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_BUFFER;
    dstLocation.Buffer.Location = dstBuffer->GetResource()->GetGPUVirtualAddress();

    size_t bufferOffset = 0;

    for (uint8 mipIndex = newSubResource.baseMipLevel; mipIndex < newSubResource.baseMipLevel + newSubResource.numLevels; mipIndex++)
    {
        const Vec3u mipExtent = m_textureDesc.GetMipExtent(mipIndex);
        const size_t mipByteSize = m_textureDesc.GetMipByteSize(mipIndex, /* includeArrayLayers */ true);

        const size_t layerStep = mipByteSize / NumArrayLayers();

        for (uint16 layerIndex = newSubResource.baseArrayLayer; layerIndex < newSubResource.baseArrayLayer + newSubResource.numLayers; layerIndex++)
        {
            srcLocation.SubresourceIndex = D3D12CalcSubresource(
                mipIndex,
                layerIndex,
                0,
                NumMips(),
                NumArrayLayers());

            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint {};
            footprint.Footprint.Width = mipExtent.x;
            footprint.Footprint.Height = mipExtent.y;
            footprint.Footprint.Depth = mipExtent.z;
            footprint.Footprint.Format = ToDXGIFormat(m_textureDesc.format);
            footprint.Offset = bufferOffset + (layerIndex * layerStep);

            dstLocation.Buffer.PlacedFootprint = footprint;

            D3D12_BOX dstBox {};
            dstBox.left = 0;
            dstBox.top = 0;
            dstBox.front = 0;
            dstBox.right = mipExtent.x;
            dstBox.bottom = mipExtent.y;
            dstBox.back = mipExtent.z;

            commandBuffer->GetCommandList()->CopyTextureRegion(
                &dstLocation,
                0, 0, 0,
                &srcLocation,
                &dstBox);
        }

        bufferOffset += mipByteSize;
    }
}

void DX12GpuImage::CopyFrom(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuImage* srcImage,
    const Vec3u& srcOffset,
    const Vec3u& dstOffset,
    const Vec3u& extent,
    const ImageSubResource& srcSubResource,
    const ImageSubResource& dstSubResource)
{
    const bool srcIsDepthStencil = srcImage->GetTextureDesc().IsDepthStencil();
    const bool dstIsDepthStencil = m_textureDesc.IsDepthStencil();

    D3D12_RESOURCE_STATES srcState = srcIsDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COPY_SOURCE;
    D3D12_RESOURCE_STATES dstState = dstIsDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COPY_DEST;

    D3D12_TEXTURE_COPY_LOCATION srcLocation {};
    srcLocation.pResource = srcImage->GetResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    srcLocation.SubresourceIndex = D3D12CalcSubresource(
        srcSubResource.baseMipLevel,
        srcSubResource.baseArrayLayer,
        0,
        srcImage->NumMips(),
        srcImage->NumArrayLayers());

    D3D12_TEXTURE_COPY_LOCATION dstLocation {};
    dstLocation.pResource = m_resource.Get();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    dstLocation.SubresourceIndex = D3D12CalcSubresource(
        dstSubResource.baseMipLevel,
        dstSubResource.baseArrayLayer,
        0,
        NumMips(),
        NumArrayLayers());

    D3D12_BOX srcBox {};
    srcBox.left = srcOffset.x;
    srcBox.top = srcOffset.y;
    srcBox.front = srcOffset.z;
    srcBox.right = srcOffset.x + extent.x;
    srcBox.bottom = srcOffset.y + extent.y;
    srcBox.back = srcOffset.z + extent.z;

    commandBuffer->GetCommandList()->CopyTextureRegion(
        &dstLocation,
        dstOffset.x, dstOffset.y, dstOffset.z,
        &srcLocation,
        &srcBox);
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
