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
#include <rendering/dx12/DX12DescriptorHeaps.hpp>

#include <rendering/Shared.hpp>
#include <rendering/RenderHelpers.hpp>

#include <rendering/util/DeletionQueue.hpp>

#include <DX12GpuImage.generated.inl>

namespace Hyperion {

extern DX12RenderInterface RI;

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
        EnqueueDeletion(FunctionWrapper<Proc<void()>>([allocation = std::move(m_allocation), resource = std::move(m_resource)]() mutable
            {
                allocation.Reset();
                resource.Reset();
            }));

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

RendererResult DX12GpuImage::Create(ResourceState initialState)
{
    if (IsCreated())
    {
        return {};
    }

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

    // For depth textures that will be sampled, use TYPELESS format for the resource
    // Views will use the appropriate typed format (D16_UNORM for DSV, R16_UNORM for SRV)
    DX12ViewType resourceFormatType = DX12ViewType::SRV_UAV;
    if (isDepthStencil && (m_textureDesc.imageUsage & IU_SAMPLED))
    {
        resourceFormatType = DX12ViewType::None;  // Returns TYPELESS format
    }
    else if (isAttachmentTexture)
    {
        resourceFormatType = DX12ViewType::RTV_DSV;
    }

    resourceDesc.Format = ToDXGIFormat(m_textureDesc.format, resourceFormatType);

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

    if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        clearValue.DepthStencil.Depth = 1.0f;
        clearValue.DepthStencil.Stencil = 0;

        // Clear value format must match the typed format for depth/stencil views
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
    // NOTE: Render targets need a clear value to avoid D3D12 warning #820
    // during ClearRenderTargetView. Use transparent black as default.
    else if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        clearValue.Format = resourceDesc.Format;
        clearValue.Color[0] = 0.0f;
        clearValue.Color[1] = 0.0f;
        clearValue.Color[2] = 0.0f;
        clearValue.Color[3] = 0.0f;

        pClearValue = &clearValue;
    }

    D3D12MA::ALLOCATION_DESC allocDesc {};
    allocDesc.HeapType = D3D12_HEAP_TYPE_DEFAULT;

    HRESULT hr = RI.GetAllocator()->CreateResource(
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

    // Set the initial resource state to match the actual GPU state
    // RS_UNDEFINED/RS_PRE_INITIALIZED map to D3D12_RESOURCE_STATE_COMMON
    if (initialState != RS_UNDEFINED && initialState != RS_PRE_INITIALIZED)
    {
        SetResourceState(initialState);
    }
    else
    {
        // When created with COMMON state, track it as RS_COMMON
        SetResourceState(RS_COMMON);
    }

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

            DX12Frame* frame = RI.GetCurrentFrame();
            CommandRecorder& cr = frame->cr;
            cr << ::Hyperion::InsertBarrier(this, previousResourceState);
        }
    }

    return {};
}

ResourceState DX12GpuImage::GetSubResourceState(const ImageSubResource& subResource) const
{
    auto it = m_subResourceStates.Find(GetImageSubResourceKey(subResource));
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

    m_subResourceStates.Set(GetImageSubResourceKey(subResource), newState);
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

    Assert(m_subResourceStates.Empty());
}

void DX12GpuImage::InsertBarrier(
    DX12CommandBuffer* commandBuffer,
    const ImageSubResource& subResource,
    ResourceState newState,
    ShaderModuleType shaderModuleType,
    bool onlyDepth,
    bool onlyStencil)
{
    AssertDebug(newState != RS_UNDEFINED && newState != RS_PRE_INITIALIZED);
    AssertDebug(m_resource != nullptr);

    AssertDebug((subResource.baseArrayLayer + subResource.numLayers) <= NumArrayLayers()
        || (subResource.baseArrayLayer == 0 && subResource.numLayers == uint16(-1)));

    AssertDebug((subResource.baseMipLevel + subResource.numLevels) <= NumMips()
        || (subResource.baseMipLevel == 0 && subResource.numLevels == uint8(-1)));

    const uint16 maxArrayLayers = uint16(subResource.baseArrayLayer + MathUtil::Min(subResource.numLayers, NumArrayLayers()));
    const uint8 maxMipLevels = uint8(subResource.baseMipLevel + MathUtil::Min(subResource.numLevels, NumMips()));

    const bool isAttachmentTexture = m_textureDesc.imageUsage[IU_ATTACHMENT];

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();
    const bool hasStencil = TextureUtils::HasStencilComponent(m_textureDesc.format);

    // can only use these if we actually do have a stencil component,
    // otherwise use default/main path
    onlyDepth &= hasStencil;
    onlyStencil &= hasStencil;

    ResourceState currResourceState = m_resourceState;
    const ResourceState currStencilState = m_stencilState;

    if (HasSubResourceStates())
    {
        currResourceState = RS_UNDEFINED;

        bool firstSubResource = true;
        bool breakLoop = false;

        for (uint8 mipLevel = subResource.baseMipLevel; mipLevel < maxMipLevels; mipLevel++)
        {
            if (breakLoop)
                break;

            for (uint16 arrayLayer = subResource.baseArrayLayer; arrayLayer < maxArrayLayers; arrayLayer++)
            {
                ImageSubResource currSubResource {};
                currSubResource.baseMipLevel = uint8(mipLevel);
                currSubResource.numLevels = 1;
                currSubResource.baseArrayLayer = uint16(arrayLayer);
                currSubResource.numLayers = 1;

                const uint64 subResourceKey = GetImageSubResourceKey(currSubResource);

                ResourceState foundResourceState;

                auto it = m_subResourceStates.Find(subResourceKey);

                if (it != m_subResourceStates.End())
                {
                    foundResourceState = it->second;
                }
                else
                {
                    foundResourceState = GetResourceState();
                }

                // needs to match expected state we're transitioning from. (currResourceState)
                if (firstSubResource)
                {
                    currResourceState = foundResourceState;
                    firstSubResource = false;
                }
                else if (foundResourceState != currResourceState)
                {
                    currResourceState = RS_UNDEFINED;
                    breakLoop = true;

                    break;
                }
            }
        }
    }

#if HYP_DEBUG_MODE
    if (hasStencil && currResourceState != currStencilState)
    {
        // Depth/stencil separate states sanity checks.
        if (currResourceState == newState)
        {
            Assert(onlyStencil);

            if (newState == RS_SHADER_RESOURCE)
            {
                Assert(currStencilState == RS_RENDER_TARGET);
            }
            else if (newState == RS_RENDER_TARGET)
            {
                Assert(currStencilState == RS_SHADER_RESOURCE);
            }
        }
        else if (currStencilState == newState)
        {
            Assert(onlyDepth);

            if (newState == RS_SHADER_RESOURCE)
            {
                Assert(currResourceState == RS_RENDER_TARGET);
            }
            else if (newState == RS_RENDER_TARGET)
            {
                Assert(currResourceState == RS_SHADER_RESOURCE);
            }
        }
    }
#endif

    if (onlyDepth && currStencilState == newState)
    {
        onlyDepth = false;
    }

    if (onlyStencil && currResourceState == newState)
    {
        onlyStencil = false;
    }

    const bool adjustForDepthStencil = isDepthStencil && isAttachmentTexture;

    auto GetDX12State = [adjustForDepthStencil](ResourceState state) -> D3D12_RESOURCE_STATES
    {
        if (adjustForDepthStencil)
        {
            if (state == RS_RENDER_TARGET || state == RS_DEPTH_STENCIL)
            {
                return D3D12_RESOURCE_STATE_DEPTH_WRITE;
            }

            if (state == RS_SHADER_RESOURCE)
            {
                return D3D12_RESOURCE_STATE_DEPTH_READ;
            }
        }

        return ToDX12ResourceStates(state);
    };

    const ResourceState effectiveCurrState = onlyStencil ? currStencilState : currResourceState;
    D3D12_RESOURCE_STATES stateBefore = GetDX12State(effectiveCurrState);
    D3D12_RESOURCE_STATES stateAfter = GetDX12State(newState);

    // If we don't know the current state (UNDEFINED), we can't issue a single
    // ALL_SUBRESOURCES barrier because subresources have divergent states.
    // Instead, fall through to the per-subresource barrier loop below which
    // transitions each subresource individually using the correct tracked state.
    // Same applies when depth and stencil states have diverged — ALL_SUBRESOURCES
    // would use the wrong before-state for one of the planes.
    const bool stencilDiverged = (hasStencil && !onlyDepth && !onlyStencil
        && currResourceState != RS_UNDEFINED
        && currStencilState != currResourceState);

    if (effectiveCurrState == RS_UNDEFINED || stencilDiverged)
    {
        // Fall through to per-subresource barrier processing
    }
    else if (subResource.baseMipLevel == 0 && subResource.numLevels >= NumMips()
        && subResource.baseArrayLayer == 0 && subResource.numLayers >= NumArrayLayers()
        && !onlyDepth && !onlyStencil)
    {
        if (stateBefore == stateAfter)
        {
            return;
        }

        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Transition.pResource = m_resource.Get();
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = stateAfter;

        commandBuffer->GetCommandList()->ResourceBarrier(1, &barrier);

        if (onlyStencil)
        {
            m_stencilState = newState;
        }
        else
        {
            SetResourceState(newState);

            if (onlyDepth)
            {
                m_stencilState = currStencilState;
            }
        }

        return;
    }

    for (uint8 mipLevel = subResource.baseMipLevel; mipLevel < maxMipLevels; mipLevel++)
    {
        for (uint16 arrayLayer = subResource.baseArrayLayer; arrayLayer < maxArrayLayers; arrayLayer++)
        {
            ImageSubResource currSubResource {};
            currSubResource.baseMipLevel = uint8(mipLevel);
            currSubResource.numLevels = 1;
            currSubResource.baseArrayLayer = uint16(arrayLayer);
            currSubResource.numLayers = 1;

            const uint64 subResourceKey = GetImageSubResourceKey(currSubResource);

            // Determine current per-subresource state
            auto stateIt = m_subResourceStates.Find(subResourceKey);
            const ResourceState subResCurrentState = onlyStencil
                ? currStencilState
                : (stateIt != m_subResourceStates.End() ? stateIt->second : GetResourceState());

            const uint32 planeSlice = onlyStencil ? 1u : 0u;
            const uint32 subResourceIndex = D3D12CalcSubresource(
                mipLevel,
                arrayLayer,
                planeSlice,
                NumMips(),
                NumArrayLayers());

            const D3D12_RESOURCE_STATES subResStateBefore = GetDX12State(subResCurrentState);

            if (subResStateBefore != stateAfter)
            {
                D3D12_RESOURCE_BARRIER barrier {};
                barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                barrier.Transition.pResource = m_resource.Get();
                barrier.Transition.Subresource = subResourceIndex;
                barrier.Transition.StateBefore = subResStateBefore;
                barrier.Transition.StateAfter = stateAfter;

                commandBuffer->GetCommandList()->ResourceBarrier(1, &barrier);
            }

            // Update state tracking
            if (onlyStencil)
            {
                // Stencil-only transitions: don't update per-subresource state
                // (per-subresource tracking is for depth state only)
            }
            else if (stateIt != m_subResourceStates.End())
            {
                stateIt->second = newState;

                if (stateIt->second == m_resourceState)
                {
                    // same state as overall image, remove from set
                    m_subResourceStates.Erase(stateIt);
                }
            }
            else if (newState != m_resourceState)
            {
                m_subResourceStates.Set(subResourceKey, newState);
            }
        }
    }

    // If stencil and depth states diverged, we need to transition stencil
    // subresources individually since we couldn't use ALL_SUBRESOURCES above.
    if (stencilDiverged && !onlyStencil)
    {
        const D3D12_RESOURCE_STATES stencilStateBefore = GetDX12State(currStencilState);

        if (stencilStateBefore != stateAfter)
        {
            for (uint8 mipLevel = subResource.baseMipLevel; mipLevel < maxMipLevels; mipLevel++)
            {
                for (uint16 arrayLayer = subResource.baseArrayLayer; arrayLayer < maxArrayLayers; arrayLayer++)
                {
                    const uint32 subResourceIndex = D3D12CalcSubresource(
                        mipLevel,
                        arrayLayer,
                        1u,  // stencil plane
                        NumMips(),
                        NumArrayLayers());

                    D3D12_RESOURCE_BARRIER barrier {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
                    barrier.Transition.pResource = m_resource.Get();
                    barrier.Transition.Subresource = subResourceIndex;
                    barrier.Transition.StateBefore = stencilStateBefore;
                    barrier.Transition.StateAfter = stateAfter;

                    commandBuffer->GetCommandList()->ResourceBarrier(1, &barrier);
                }
            }
        }
    }

    // Update state tracking
    if (onlyStencil || stencilDiverged)
    {
        m_stencilState = newState;
    }
    else if (onlyDepth)
    {
        // Depth plane was transitioned -- update the main resource state
        // and clear per-subresource tracking to prevent stale per-subresource
        // states from blocking future transitions.
        SetResourceState(newState);
        m_stencilState = currStencilState;
        m_subResourceStates.Clear();
    }
    else if (m_subResourceStates.Empty())
    {
        SetResourceState(newState);

        if (onlyDepth)
        {
            m_stencilState = currStencilState;
        }
    }
    else if (subResource.baseMipLevel == 0 && subResource.numLevels >= NumMips()
        && subResource.baseArrayLayer == 0 && subResource.numLayers >= NumArrayLayers())
    {
        // Full resource was transitioned -- all subresources are now in the same state.
        // Clear per-subresource tracking and update global state.
        SetResourceState(newState);
        m_subResourceStates.Clear();
    }
}

void DX12GpuImage::InsertUAVBarrier(CommandBuffer* commandBuffer)
{
    AssertDebug(m_resource != nullptr);

    DX12CommandBuffer* dx12CommandBuffer = static_cast<DX12CommandBuffer*>(commandBuffer);
    Assert(dx12CommandBuffer != nullptr);

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource = m_resource.Get();

    dx12CommandBuffer->GetCommandList()->ResourceBarrier(1, &barrier);
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
    AssertDebug(srcImage != nullptr, "Source image is null");
    AssertDebug(srcImage->IsCreated(), "Source image is not created");
    AssertDebug(IsCreated(), "Destination image is not created");

    const TextureDesc& srcTextureDesc = srcImage->GetTextureDesc();
    const bool srcIsDepthStencil = srcTextureDesc.IsDepthStencil();
    const bool dstIsDepthStencil = m_textureDesc.IsDepthStencil();

    // numLevels == UINT8_MAX means "use all remaining mip levels"
    // numLayers == UINT16_MAX means "use all remaining array layers"
    const uint8 srcNumLevels = (srcSubResource.numLevels == UINT8_MAX)
        ? uint8(srcImage->NumMips() - srcSubResource.baseMipLevel)
        : srcSubResource.numLevels;
    const uint8 dstNumLevels = (dstSubResource.numLevels == UINT8_MAX)
        ? uint8(NumMips() - dstSubResource.baseMipLevel)
        : dstSubResource.numLevels;
    const uint16 srcNumLayers = (srcSubResource.numLayers == UINT16_MAX)
        ? uint16(srcImage->NumArrayLayers() - srcSubResource.baseArrayLayer)
        : srcSubResource.numLayers;
    const uint16 dstNumLayers = (dstSubResource.numLayers == UINT16_MAX)
        ? uint16(NumArrayLayers() - dstSubResource.baseArrayLayer)
        : dstSubResource.numLayers;

    const uint8 numLevelsToCopy = MathUtil::Min(srcNumLevels, dstNumLevels);
    const uint16 numLayersToCopy = MathUtil::Min(srcNumLayers, dstNumLayers);

    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();

    if (!HasSubResourceStates() && !srcImage->HasSubResourceStates())
    {
        const D3D12_RESOURCE_STATES srcState = srcIsDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COPY_SOURCE;
        const D3D12_RESOURCE_STATES dstState = dstIsDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COPY_DEST;

        AssertDebug(srcImage->GetResourceState() == RS_COPY_SRC);
        AssertDebug(GetResourceState() == RS_COPY_DST);

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

        commandList->CopyTextureRegion(
            &dstLocation,
            dstRect.x0, dstRect.y0, 0,
            &srcLocation,
            &srcBox);
    }
    else
    {
        for (uint16 layerIndex = 0; layerIndex < numLayersToCopy; layerIndex++)
        {
            for (uint8 mipLevel = 0; mipLevel < numLevelsToCopy; mipLevel++)
            {
                const uint8 actualSrcMip = srcSubResource.baseMipLevel + mipLevel;
                const uint8 actualDstMip = dstSubResource.baseMipLevel + mipLevel;
                const uint16 actualSrcLayer = srcSubResource.baseArrayLayer + layerIndex;
                const uint16 actualDstLayer = dstSubResource.baseArrayLayer + layerIndex;

                const ResourceState srcResourceState = srcImage->GetSubResourceState(ImageSubResource {
                    .baseMipLevel = actualSrcMip,
                    .numLevels = 1,
                    .baseArrayLayer = actualSrcLayer,
                    .numLayers = 1
                });

                const ResourceState dstResourceState = GetSubResourceState(ImageSubResource {
                    .baseMipLevel = actualDstMip,
                    .numLevels = 1,
                    .baseArrayLayer = actualDstLayer,
                    .numLayers = 1
                });

                AssertDebug(srcResourceState == RS_COPY_SRC);
                AssertDebug(dstResourceState == RS_COPY_DST);

                D3D12_TEXTURE_COPY_LOCATION srcLocation {};
                srcLocation.pResource = srcImage->GetResource();
                srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                srcLocation.SubresourceIndex = D3D12CalcSubresource(
                    actualSrcMip,
                    actualSrcLayer,
                    0,
                    srcImage->NumMips(),
                    srcImage->NumArrayLayers());

                D3D12_TEXTURE_COPY_LOCATION dstLocation {};
                dstLocation.pResource = m_resource.Get();
                dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dstLocation.SubresourceIndex = D3D12CalcSubresource(
                    actualDstMip,
                    actualDstLayer,
                    0,
                    NumMips(),
                    NumArrayLayers());

                // Clamp source rect to the source mip level dimensions
                const Vec3u srcMipExtent = srcTextureDesc.GetMipExtent(actualSrcMip);
                const uint32 srcX0 = MathUtil::Min(srcRect.x0, srcMipExtent.x);
                const uint32 srcY0 = MathUtil::Min(srcRect.y0, srcMipExtent.y);
                const uint32 srcX1 = MathUtil::Min(srcRect.x1, srcMipExtent.x);
                const uint32 srcY1 = MathUtil::Min(srcRect.y1, srcMipExtent.y);

                // Clamp destination rect to the destination mip level dimensions
                const Vec3u dstMipExtent = m_textureDesc.GetMipExtent(actualDstMip);
                const uint32 dstX0 = MathUtil::Min(dstRect.x0, dstMipExtent.x);
                const uint32 dstY0 = MathUtil::Min(dstRect.y0, dstMipExtent.y);

                D3D12_BOX srcBox {};
                srcBox.left = srcX0;
                srcBox.top = srcY0;
                srcBox.front = 0;
                srcBox.right = srcX1;
                srcBox.bottom = srcY1;
                srcBox.back = 1;

                commandList->CopyTextureRegion(
                    &dstLocation,
                    dstX0, dstY0, 0,
                    &srcLocation,
                    &srcBox);
            }
        }
    }
}

RendererResult DX12GpuImage::GenerateMipmaps(DX12CommandBuffer* commandBuffer)
{
    if (m_resource == nullptr)
    {
        return HYP_MAKE_ERROR(RendererError, "Cannot generate mipmaps on uninitialized image");
    }

    HYP_LOG(RenderingBackend, Error,
        "DX12 GenerateMipmaps not yet implemented. "
        "D3D12 has no built-in equivalent to Vulkan's vkCmdBlitImage; "
        "this requires a compute shader that samples from the larger mip "
        "with linear filtering and writes to the smaller mip.");

    return HYP_MAKE_ERROR(RendererError, "GenerateMipmaps not implemented for DX12 backend");
}

void DX12GpuImage::CopyFromBuffer(
    DX12CommandBuffer* commandBuffer,
    const DX12GpuBuffer* srcBuffer,
    uint32 srcBufferOffset,
    uint8 dstMipIndex,
    uint16 dstArrayLayer) const
{
    AssertDebug(srcBuffer != nullptr, "Source buffer is null");
    AssertDebug(srcBuffer->IsCreated(), "Source buffer is not created");
    AssertDebug(IsCreated(), "Destination image is not created");

    const uint8 mipIdx = dstMipIndex != UINT8_MAX ? dstMipIndex : 0;
    const uint16 arrayIdx = dstArrayLayer != UINT16_MAX ? dstArrayLayer : 0;

    // Validate subresource indices
    AssertDebug(mipIdx < NumMips(),
        "Destination mip level {} out of range (max: {})",
        mipIdx, NumMips() - 1);
    AssertDebug(arrayIdx < NumArrayLayers(),
        "Destination array layer {} out of range (max: {})",
        arrayIdx, NumArrayLayers() - 1);

    ResourceState subResourceState = GetSubResourceState(ImageSubResource {
        .baseMipLevel = mipIdx,
        .numLevels = 1,
        .baseArrayLayer = arrayIdx,
        .numLayers = 1
    });

    AssertDebug(subResourceState == RS_COPY_DST);
    AssertDebug(srcBuffer->GetResourceState() == RS_COPY_SRC);

    const Vec3u mipExtent = m_textureDesc.GetMipExtent(mipIdx);
    const uint32 bytesPerPixel = TextureUtils::BytesPerComponent(m_textureDesc.format) * TextureUtils::NumComponents(m_textureDesc.format);
    const size_t requiredBufferSize = static_cast<size_t>(srcBufferOffset) + (static_cast<size_t>(mipExtent.x) * mipExtent.y * mipExtent.z * bytesPerPixel);

    AssertDebug(srcBuffer->Size() >= requiredBufferSize,
        "Source buffer size ({}) is too small for copy operation. Required: {} bytes from offset {}",
        srcBuffer->Size(), requiredBufferSize, srcBufferOffset);

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();

    D3D12_RESOURCE_STATES state = isDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_WRITE : D3D12_RESOURCE_STATE_COPY_DEST;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedFootprint {};
    placedFootprint.Offset = srcBufferOffset;
    placedFootprint.Footprint.Depth = mipExtent.z;
    placedFootprint.Footprint.Height = mipExtent.y;
    placedFootprint.Footprint.Width = mipExtent.x;
    placedFootprint.Footprint.Format = ToDXGIFormat(m_textureDesc.format);
    placedFootprint.Footprint.RowPitch = mipExtent.x * TextureUtils::BytesPerComponent(m_textureDesc.format) * TextureUtils::NumComponents(m_textureDesc.format);

    D3D12_TEXTURE_COPY_LOCATION srcLocation {};
    srcLocation.pResource = srcBuffer->GetResource();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    srcLocation.PlacedFootprint = placedFootprint;

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
    AssertDebug(IsCreated(), "Source image is not created");
    AssertDebug(dstBuffer != nullptr, "Destination buffer is null");
    AssertDebug(dstBuffer->IsCreated(), "Destination buffer is not created");

    // Validate subresource ranges
    AssertDebug(subResource.baseMipLevel < NumMips(),
        "Source mip level {} out of range (max: {})",
        subResource.baseMipLevel, NumMips() - 1);
    AssertDebug(subResource.baseArrayLayer < NumArrayLayers(),
        "Source array layer {} out of range (max: {})",
        subResource.baseArrayLayer, NumArrayLayers() - 1);

    // numLevels == UINT8_MAX means "use all remaining mip levels"
    // numLayers == UINT16_MAX means "use all remaining array layers"
    const uint8 numLevels = (subResource.numLevels == UINT8_MAX)
        ? uint8(NumMips() - subResource.baseMipLevel)
        : subResource.numLevels;

    const uint16 numLayers = (subResource.numLayers == UINT16_MAX)
        ? uint16(NumArrayLayers() - subResource.baseArrayLayer)
        : subResource.numLayers;

    AssertDebug(numLevels > 0 && subResource.baseMipLevel + numLevels <= NumMips(),
        "Invalid mip level count: {} (base: {}, max: {})",
        numLevels, subResource.baseMipLevel, NumMips());
    AssertDebug(numLayers > 0 && subResource.baseArrayLayer + numLayers <= NumArrayLayers(),
        "Invalid array layer count: {} (base: {}, max: {})",
        numLayers, subResource.baseArrayLayer, NumArrayLayers());

    AssertDebug(GetSubResourceState(subResource) == RS_COPY_SRC && dstBuffer->GetResourceState() == RS_COPY_DST);

    // Calculate total size required for the specified subresources
    size_t totalSubResourceSize = 0;
    for (uint8 mipIndex = subResource.baseMipLevel; mipIndex < subResource.baseMipLevel + numLevels; mipIndex++)
    {
        for (uint16 layerIndex = subResource.baseArrayLayer; layerIndex < subResource.baseArrayLayer + numLayers; layerIndex++)
        {
            totalSubResourceSize += m_textureDesc.GetMipByteSize(mipIndex, /* includeArrayLayers */ false);
        }
    }

    AssertDebug(dstBuffer->Size() >= totalSubResourceSize,
        "Destination buffer size ({}) is too small to hold subresource data ({} bytes). "
        "Subresources: {} mip levels x {} array layers starting at mip {} layer {}",
        dstBuffer->Size(), totalSubResourceSize,
        numLevels, numLayers,
        subResource.baseMipLevel, subResource.baseArrayLayer);

    ImageSubResource newSubResource = subResource;
    newSubResource.numLayers = numLayers;
    newSubResource.numLevels = numLevels;

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();

    D3D12_RESOURCE_STATES state = isDepthStencil ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_COPY_SOURCE;

    D3D12_TEXTURE_COPY_LOCATION srcLocation {};
    srcLocation.pResource = m_resource.Get();
    srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    D3D12_PLACED_SUBRESOURCE_FOOTPRINT placedFootprint {};
    placedFootprint.Footprint.Format = ToDXGIFormat(m_textureDesc.format);
    placedFootprint.Offset = 0;

    D3D12_TEXTURE_COPY_LOCATION dstLocation {};
    dstLocation.pResource = dstBuffer->GetResource();
    dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    dstLocation.PlacedFootprint = placedFootprint;

    size_t bufferOffset = 0;

    for (uint8 mipIndex = newSubResource.baseMipLevel; mipIndex < newSubResource.baseMipLevel + newSubResource.numLevels; mipIndex++)
    {
        const Vec3u mipExtent = m_textureDesc.GetMipExtent(mipIndex);
        const size_t mipByteSize = m_textureDesc.GetMipByteSize(mipIndex, /* includeArrayLayers */ true);

        const size_t layerStep = mipByteSize / NumArrayLayers();

        for (uint16 layerIndex = newSubResource.baseArrayLayer; layerIndex < newSubResource.baseArrayLayer + newSubResource.numLayers; layerIndex++)
        {
            ResourceState subResourceState = GetSubResourceState(ImageSubResource {
                .baseMipLevel = mipIndex,
                .numLevels = 1,
                .baseArrayLayer = layerIndex,
                .numLayers = 1
            });
            AssertDebug(subResourceState == RS_COPY_SRC);

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

            dstLocation.PlacedFootprint = footprint;

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
    AssertDebug(srcImage != nullptr, "Source image is null");
    AssertDebug(srcImage->IsCreated(), "Source image is not created");
    AssertDebug(IsCreated(), "Destination image is not created");
    AssertDebug(srcImage->GetTextureFormat() == GetTextureFormat(),
        "Formats do not match: {} != {}",
        EnumToString(srcImage->GetTextureFormat()),
        EnumToString(GetTextureFormat()));

    // Validate subresource ranges
    AssertDebug(srcSubResource.baseMipLevel < srcImage->NumMips(),
        "Source mip level {} out of range (max: {})",
        srcSubResource.baseMipLevel, srcImage->NumMips() - 1);
    AssertDebug(dstSubResource.baseMipLevel < NumMips(),
        "Destination mip level {} out of range (max: {})",
        dstSubResource.baseMipLevel, NumMips() - 1);
    AssertDebug(srcSubResource.baseArrayLayer < srcImage->NumArrayLayers(),
        "Source array layer {} out of range (max: {})",
        srcSubResource.baseArrayLayer, srcImage->NumArrayLayers() - 1);
    AssertDebug(dstSubResource.baseArrayLayer < NumArrayLayers(),
        "Destination array layer {} out of range (max: {})",
        dstSubResource.baseArrayLayer, NumArrayLayers() - 1);

    // numLevels == UINT8_MAX means "use all remaining mip levels"
    // numLayers == UINT16_MAX means "use all remaining array layers"
    const uint8 srcNumLevels = (srcSubResource.numLevels == UINT8_MAX)
        ? uint8(srcImage->NumMips() - srcSubResource.baseMipLevel)
        : srcSubResource.numLevels;
    const uint8 dstNumLevels = (dstSubResource.numLevels == UINT8_MAX)
        ? uint8(NumMips() - dstSubResource.baseMipLevel)
        : dstSubResource.numLevels;
    const uint16 srcNumLayers = (srcSubResource.numLayers == UINT16_MAX)
        ? uint16(srcImage->NumArrayLayers() - srcSubResource.baseArrayLayer)
        : srcSubResource.numLayers;
    const uint16 dstNumLayers = (dstSubResource.numLayers == UINT16_MAX)
        ? uint16(NumArrayLayers() - dstSubResource.baseArrayLayer)
        : dstSubResource.numLayers;

    AssertDebug(srcNumLevels > 0 && srcSubResource.baseMipLevel + srcNumLevels <= srcImage->NumMips(),
        "Invalid source mip level count: {} (base: {}, max: {})",
        srcNumLevels, srcSubResource.baseMipLevel, srcImage->NumMips());
    AssertDebug(dstNumLevels > 0 && dstSubResource.baseMipLevel + dstNumLevels <= NumMips(),
        "Invalid destination mip level count: {} (base: {}, max: {})",
        dstNumLevels, dstSubResource.baseMipLevel, NumMips());
    AssertDebug(srcNumLayers > 0 && srcSubResource.baseArrayLayer + srcNumLayers <= srcImage->NumArrayLayers(),
        "Invalid source array layer count: {} (base: {}, max: {})",
        srcNumLayers, srcSubResource.baseArrayLayer, srcImage->NumArrayLayers());
    AssertDebug(dstNumLayers > 0 && dstSubResource.baseArrayLayer + dstNumLayers <= NumArrayLayers(),
        "Invalid destination array layer count: {} (base: {}, max: {})",
        dstNumLayers, dstSubResource.baseArrayLayer, NumArrayLayers());

    // Validate copy extent is non-zero
    AssertDebug(extent.x > 0 && extent.y > 0 && extent.z > 0,
        "Copy extent must be non-zero: ({}, {}, {})", extent.x, extent.y, extent.z);

    const bool srcIsDepthStencil = srcImage->GetTextureDesc().IsDepthStencil();
    const bool dstIsDepthStencil = m_textureDesc.IsDepthStencil();

    const bool srcIsAttachment = srcImage->GetTextureDesc().imageUsage[IU_ATTACHMENT];
    const bool dstIsAttachment = m_textureDesc.imageUsage[IU_ATTACHMENT];

    if (!HasSubResourceStates() && !srcImage->HasSubResourceStates())
    {
        AssertDebug(GetResourceState() == RS_COPY_DST && srcImage->GetResourceState() == RS_COPY_SRC);

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

        // Validate offset and extent are within bounds for the base mip level
        const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(srcSubResource.baseMipLevel);
        const Vec3u dstMipExtent = m_textureDesc.GetMipExtent(dstSubResource.baseMipLevel);

        AssertDebug(srcOffset.x + extent.x <= srcMipExtent.x,
            "Source copy region exceeds image bounds: offset.x({}) + extent.x({}) > mipWidth({})",
            srcOffset.x, extent.x, srcMipExtent.x);
        AssertDebug(srcOffset.y + extent.y <= srcMipExtent.y,
            "Source copy region exceeds image bounds: offset.y({}) + extent.y({}) > mipHeight({})",
            srcOffset.y, extent.y, srcMipExtent.y);
        AssertDebug(srcOffset.z + extent.z <= srcMipExtent.z,
            "Source copy region exceeds image bounds: offset.z({}) + extent.z({}) > mipDepth({})",
            srcOffset.z, extent.z, srcMipExtent.z);
        AssertDebug(dstOffset.x + extent.x <= dstMipExtent.x,
            "Destination copy region exceeds image bounds: offset.x({}) + extent.x({}) > mipWidth({})",
            dstOffset.x, extent.x, dstMipExtent.x);
        AssertDebug(dstOffset.y + extent.y <= dstMipExtent.y,
            "Destination copy region exceeds image bounds: offset.y({}) + extent.y({}) > mipHeight({})",
            dstOffset.y, extent.y, dstMipExtent.y);
        AssertDebug(dstOffset.z + extent.z <= dstMipExtent.z,
            "Destination copy region exceeds image bounds: offset.z({}) + extent.z({}) > mipDepth({})",
            dstOffset.z, extent.z, dstMipExtent.z);

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
    else
    {
        // Use resolved counts for iteration (handles UINT8_MAX/UINT16_MAX sentinel values)
        const uint8 numLevelsToCopy = MathUtil::Min(srcNumLevels, dstNumLevels);
        const uint16 numLayersToCopy = MathUtil::Min(srcNumLayers, dstNumLayers);

        for (uint16 layerIndex = 0; layerIndex < numLayersToCopy; layerIndex++)
        {
            for (uint8 mipLevel = 0; mipLevel < numLevelsToCopy; mipLevel++)
            {
                const uint8 actualSrcMip = srcSubResource.baseMipLevel + mipLevel;
                const uint8 actualDstMip = dstSubResource.baseMipLevel + mipLevel;
                const uint16 actualSrcLayer = srcSubResource.baseArrayLayer + layerIndex;
                const uint16 actualDstLayer = dstSubResource.baseArrayLayer + layerIndex;

                const ResourceState srcResourceState = srcImage->GetSubResourceState(ImageSubResource {
                    .baseMipLevel = actualSrcMip,
                    .numLevels = 1,
                    .baseArrayLayer = actualSrcLayer,
                    .numLayers = 1
                });

                const ResourceState dstResourceState = GetSubResourceState(ImageSubResource {
                    .baseMipLevel = actualDstMip,
                    .numLevels = 1,
                    .baseArrayLayer = actualDstLayer,
                    .numLayers = 1
                });

                AssertDebug(srcResourceState == RS_COPY_SRC);
                AssertDebug(dstResourceState == RS_COPY_DST);

                D3D12_TEXTURE_COPY_LOCATION srcLocation {};
                srcLocation.pResource = srcImage->GetResource();
                srcLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                srcLocation.SubresourceIndex = D3D12CalcSubresource(
                    actualSrcMip,
                    actualSrcLayer,
                    0,
                    srcImage->NumMips(),
                    srcImage->NumArrayLayers());

                D3D12_TEXTURE_COPY_LOCATION dstLocation {};
                dstLocation.pResource = m_resource.Get();
                dstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
                dstLocation.SubresourceIndex = D3D12CalcSubresource(
                    actualDstMip,
                    actualDstLayer,
                    0,
                    NumMips(),
                    NumArrayLayers());

                // Calculate the proper extent for this mip level
                const Vec3u srcMipExtent = srcImage->GetTextureDesc().GetMipExtent(actualSrcMip);
                const Vec3u dstMipExtent = m_textureDesc.GetMipExtent(actualDstMip);

                // Validate offsets are within mip level bounds
                AssertDebug(srcOffset.x < srcMipExtent.x,
                    "Source offset.x({}) exceeds mip level {} width({})",
                    srcOffset.x, actualSrcMip, srcMipExtent.x);
                AssertDebug(srcOffset.y < srcMipExtent.y,
                    "Source offset.y({}) exceeds mip level {} height({})",
                    srcOffset.y, actualSrcMip, srcMipExtent.y);
                AssertDebug(srcOffset.z < srcMipExtent.z,
                    "Source offset.z({}) exceeds mip level {} depth({})",
                    srcOffset.z, actualSrcMip, srcMipExtent.z);
                AssertDebug(dstOffset.x < dstMipExtent.x,
                    "Destination offset.x({}) exceeds mip level {} width({})",
                    dstOffset.x, actualDstMip, dstMipExtent.x);
                AssertDebug(dstOffset.y < dstMipExtent.y,
                    "Destination offset.y({}) exceeds mip level {} height({})",
                    dstOffset.y, actualDstMip, dstMipExtent.y);
                AssertDebug(dstOffset.z < dstMipExtent.z,
                    "Destination offset.z({}) exceeds mip level {} depth({})",
                    dstOffset.z, actualDstMip, dstMipExtent.z);

                // Clamp the copy extent to the mip level dimensions
                const uint32 copyWidth = MathUtil::Min(extent.x, MathUtil::Min(srcMipExtent.x - srcOffset.x, dstMipExtent.x - dstOffset.x));
                const uint32 copyHeight = MathUtil::Min(extent.y, MathUtil::Min(srcMipExtent.y - srcOffset.y, dstMipExtent.y - dstOffset.y));
                const uint32 copyDepth = MathUtil::Min(extent.z, MathUtil::Min(srcMipExtent.z - srcOffset.z, dstMipExtent.z - dstOffset.z));

                // Warn if extent was clamped (may indicate incorrect copy parameters)
                AssertDebug(copyWidth == extent.x && copyHeight == extent.y && copyDepth == extent.z,
                    "Copy extent was clamped from ({}, {}, {}) to ({}, {}, {}) for mip {}. "
                    "Source mip extent: ({}, {}, {}), Dest mip extent: ({}, {}, {})",
                    extent.x, extent.y, extent.z, copyWidth, copyHeight, copyDepth, actualSrcMip,
                    srcMipExtent.x, srcMipExtent.y, srcMipExtent.z,
                    dstMipExtent.x, dstMipExtent.y, dstMipExtent.z);

                D3D12_BOX srcBox {};
                srcBox.left = srcOffset.x;
                srcBox.top = srcOffset.y;
                srcBox.front = srcOffset.z;
                srcBox.right = srcOffset.x + copyWidth;
                srcBox.bottom = srcOffset.y + copyHeight;
                srcBox.back = srcOffset.z + copyDepth;

                commandBuffer->GetCommandList()->CopyTextureRegion(
                    &dstLocation,
                    dstOffset.x, dstOffset.y, dstOffset.z,
                    &srcLocation,
                    &srcBox);
            }
        }
    }
}

void DX12GpuImage::Fill(
    DX12CommandBuffer* commandBuffer,
    float value,
    const ImageSubResource& subResource,
    const Vec3u& offset,
    const Vec3u& extent)
{
    AssertDebug(IsCreated(), "Image is not created");
    AssertDebug(commandBuffer != nullptr, "Command buffer is null");

    const bool isDepthStencil = m_textureDesc.IsDepthStencil();

    // Transition to appropriate state for clearing
    if (isDepthStencil)
    {
        InsertBarrier(commandBuffer, subResource, RS_DEPTH_STENCIL, ShaderModuleType::None);
    }
    else
    {
        InsertBarrier(commandBuffer, subResource, RS_RENDER_TARGET, ShaderModuleType::None);
    }

    // Get the device and command list
    ID3D12Device* device = RI.GetDevice();
    ID3D12GraphicsCommandList* commandList = commandBuffer->GetCommandList();

    // Determine the number of mip levels and array layers to clear
    const uint8 numLevels = (subResource.numLevels == UINT8_MAX)
        ? uint8(NumMips() - subResource.baseMipLevel)
        : subResource.numLevels;

    const uint16 numLayers = (subResource.numLayers == UINT16_MAX)
        ? uint16(NumArrayLayers() - subResource.baseArrayLayer)
        : subResource.numLayers;

    if (isDepthStencil)
    {
        // For depth/stencil, create a temporary DSV and clear it
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc {};
        dsvDesc.Format = ToDXGIFormat(m_textureDesc.format, DX12ViewType::RTV_DSV);

        switch (m_textureDesc.type)
        {
        case TextureType::Texture2D:
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Texture2D.MipSlice = subResource.baseMipLevel;
            break;
        case TextureType::Texture2DArray:
        case TextureType::Cubemap:
        case TextureType::CubemapArray:
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            dsvDesc.Texture2DArray.ArraySize = numLayers;
            dsvDesc.Texture2DArray.FirstArraySlice = subResource.baseArrayLayer;
            dsvDesc.Texture2DArray.MipSlice = subResource.baseMipLevel;
            break;
        default:
            HYP_UNREACHABLE();
        }

        // Get a temporary descriptor from the render interface's DSV heap
        DX12DescriptorHandle dsvHandle = RI.descriptorHeapManager->Allocate(DX12DescriptorHeapType::DSV, 1);
        AssertDebug(dsvHandle.IsValid(), "Failed to allocate DSV descriptor");

        device->CreateDepthStencilView(
            m_resource.Get(),
            &dsvDesc,
            dsvHandle.cpuHandle);

        D3D12_CLEAR_FLAGS clearFlags = D3D12_CLEAR_FLAG_DEPTH;
        if (TextureUtils::HasStencilComponent(m_textureDesc.format))
        {
            clearFlags |= D3D12_CLEAR_FLAG_STENCIL;
        }

        commandList->ClearDepthStencilView(
            dsvHandle.cpuHandle,
            clearFlags,
            value,
            0,  // stencil
            0,
            nullptr);

        EnqueueDeletion(FunctionWrapper<Proc<void()>>([dsvHandle = std::move(dsvHandle)]() mutable
            {
                // Release the descriptor back to the heap
                RI.descriptorHeapManager->Free(DX12DescriptorHeapType::DSV, std::move(dsvHandle));
            }));
    }
    else
    {
        // For color, create a temporary RTV and clear it
        D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
        rtvDesc.Format = ToDXGIFormat(m_textureDesc.format, DX12ViewType::RTV_DSV);

        switch (m_textureDesc.type)
        {
        case TextureType::Texture2D:
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
            rtvDesc.Texture2D.MipSlice = subResource.baseMipLevel;
            break;
        case TextureType::Texture2DArray:
        case TextureType::Cubemap:
        case TextureType::CubemapArray:
            rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            rtvDesc.Texture2DArray.ArraySize = numLayers;
            rtvDesc.Texture2DArray.FirstArraySlice = subResource.baseArrayLayer;
            rtvDesc.Texture2DArray.MipSlice = subResource.baseMipLevel;
            break;
        default:
            HYP_UNREACHABLE();
        }

        // Get a temporary descriptor from the render interface's RTV heap
        DX12DescriptorHandle rtvHandle = RI.descriptorHeapManager->Allocate(DX12DescriptorHeapType::RTV, 1);
        AssertDebug(rtvHandle.IsValid(), "Failed to allocate RTV descriptor");

        device->CreateRenderTargetView(
            m_resource.Get(),
            &rtvDesc,
            rtvHandle.cpuHandle);

        float clearColor[4] = { value, value, value, value };

        commandList->ClearRenderTargetView(
            rtvHandle.cpuHandle,
            clearColor,
            0,
            nullptr);

        EnqueueDeletion(FunctionWrapper<Proc<void()>>([rtvHandle = std::move(rtvHandle)]() mutable
            {
                // Release the descriptor back to the heap
                RI.descriptorHeapManager->Free(DX12DescriptorHeapType::RTV, std::move(rtvHandle));
            }));
    }
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

    return RI.MakeImageView(
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

    if (!name.IsValid())
    {
        return;
    }

    WideString ws = *name;

    if (m_resource)
    {
        m_resource->SetName(ws.Data());
    }

    if (m_allocation)
    {
        m_allocation->SetName(ws.Data());
    }
}
#endif

#pragma endregion DX12GpuImage

} // namespace Hyperion
