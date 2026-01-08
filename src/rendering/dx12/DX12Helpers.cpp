/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Helpers.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>

#include <rendering/Shared.hpp>

namespace Hyperion {

DXGI_FORMAT ToDXGIFormat(TextureFormat format, DX12ViewType getForViewType)
{
    switch (format)
    {
    case TF_R8:
        return DXGI_FORMAT_R8_UNORM;
    case TF_RG8:
        return DXGI_FORMAT_R8G8_UNORM;
    case TF_RGB8:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TF_RGBA8:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TF_R8_SRGB:
        return DXGI_FORMAT_R8_UNORM;
    case TF_RG8_SRGB:
        return DXGI_FORMAT_R8G8_UNORM;
    case TF_RGB8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TF_RGBA8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TF_R11G11B10F:
        return DXGI_FORMAT_R11G11B10_FLOAT;
    case TF_R10G10B10A2:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case TF_R16:
        return DXGI_FORMAT_R16_UINT;
    case TF_RG16_: // fallthrough
    case TF_RG16:
        return DXGI_FORMAT_R16G16_UINT;
    case TF_RGB16:
        return DXGI_FORMAT_R16G16B16A16_UINT;
    case TF_RGBA16:
        return DXGI_FORMAT_R16G16B16A16_UINT;
    case TF_R32_: // fallthrough
    case TF_R32:
        return DXGI_FORMAT_R32_UINT;
    case TF_RG32:
        return DXGI_FORMAT_R32G32_UINT;
    case TF_RGB32:
        return DXGI_FORMAT_R32G32B32A32_UINT;
    case TF_RGBA32:
        return DXGI_FORMAT_R32G32B32A32_UINT;
    case TF_R16F:
        return DXGI_FORMAT_R16_FLOAT;
    case TF_RG16F:
        return DXGI_FORMAT_R16G16_FLOAT;
    case TF_RGB16F:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TF_RGBA16F:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TF_R32F:
        return DXGI_FORMAT_R32_FLOAT;
    case TF_RG32F:
        return DXGI_FORMAT_R32G32_FLOAT;
    case TF_RGB32F:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TF_RGBA32F:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case TF_BGRA8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case TF_BGR8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case TF_BGRA8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case TF_DEPTH_16:
        if (getForViewType == DX12ViewType::RTV_DSV)
            return DXGI_FORMAT_D16_UNORM;

        return DXGI_FORMAT_R16_TYPELESS;
    case TF_DEPTH_24:
        if (getForViewType == DX12ViewType::RTV_DSV)
            return DXGI_FORMAT_D24_UNORM_S8_UINT;

        return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    case TF_DEPTH_32F:
        if (getForViewType == DX12ViewType::SRV_UAV)
            return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

        if (getForViewType == DX12ViewType::RTV_DSV)
            return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

        return DXGI_FORMAT_R32G8X24_TYPELESS;
    default:
        break;
    }
    return DXGI_FORMAT_UNKNOWN;
}

D3D12_RESOURCE_STATES ToDX12ResourceStates(ResourceState state)
{
    switch (state)
    {
    case RS_COMMON:
        return D3D12_RESOURCE_STATE_COMMON;

    case RS_VERTEX_BUFFER:
    case RS_CONSTANT_BUFFER:
        return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

    case RS_INDEX_BUFFER:
        return D3D12_RESOURCE_STATE_INDEX_BUFFER;

    case RS_RENDER_TARGET:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;

    case RS_UNORDERED_ACCESS:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    case RS_DEPTH_STENCIL:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;

    case RS_SHADER_RESOURCE:
        return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

    case RS_STREAM_OUT:
        return D3D12_RESOURCE_STATE_STREAM_OUT;

    case RS_INDIRECT_ARG:
        return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

    case RS_COPY_DST:
        return D3D12_RESOURCE_STATE_COPY_DEST;

    case RS_COPY_SRC:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;

    case RS_RESOLVE_DST:
        return D3D12_RESOURCE_STATE_RESOLVE_DEST;

    case RS_RESOLVE_SRC:
        return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

    case RS_PRESENT:
        return D3D12_RESOURCE_STATE_PRESENT;

    case RS_READ_GENERIC:
        return D3D12_RESOURCE_STATE_GENERIC_READ;

    case RS_PREDICATION:
        return D3D12_RESOURCE_STATE_PREDICATION;

    case RS_UNDEFINED:
    case RS_PRE_INITIALIZED:
        return D3D12_RESOURCE_STATE_COMMON;

    default:
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

D3D12_BLEND ToDX12Blend(BlendModeFactor factor)
{
    switch (factor)
    {
    case BMF_ONE:
        return D3D12_BLEND_ONE;
    case BMF_ZERO:
        return D3D12_BLEND_ZERO;
    case BMF_SRC_COLOR:
        return D3D12_BLEND_SRC_COLOR;
    case BMF_SRC_ALPHA:
        return D3D12_BLEND_SRC_ALPHA;
    case BMF_DST_COLOR:
        return D3D12_BLEND_DEST_COLOR;
    case BMF_DST_ALPHA:
        return D3D12_BLEND_DEST_ALPHA;
    case BMF_ONE_MINUS_SRC_COLOR:
        return D3D12_BLEND_INV_SRC_COLOR;
    case BMF_ONE_MINUS_SRC_ALPHA:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case BMF_ONE_MINUS_DST_COLOR:
        return D3D12_BLEND_INV_DEST_COLOR;
    case BMF_ONE_MINUS_DST_ALPHA:
        return D3D12_BLEND_INV_DEST_ALPHA;
    default:
        return D3D12_BLEND_ONE;
    }
}

D3D12_CULL_MODE ToDX12CullMode(FaceCullMode mode)
{
    switch (mode)
    {
    case FCM_BACK:
        return D3D12_CULL_MODE_BACK;
    case FCM_FRONT:
        return D3D12_CULL_MODE_FRONT;
    case FCM_NONE:
        return D3D12_CULL_MODE_NONE;
    default:
        return D3D12_CULL_MODE_BACK;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDX12TopologyType(Topology topology)
{
    switch (topology)
    {
    case TOP_TRIANGLES: 
    case TOP_TRIANGLE_STRIP: 
    case TOP_TRIANGLE_FAN: 
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case TOP_LINES: 
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case TOP_POINTS: 
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

D3D12_COMPARISON_FUNC ToDX12ComparisonFunction(StencilCompareOp compareOp)
{
    switch (compareOp) {
    case SCO_ALWAYS:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    case SCO_NEVER:
        return D3D12_COMPARISON_FUNC_NEVER;
    case SCO_EQUAL:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case SCO_NOT_EQUAL:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    default:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

D3D12_SRV_DIMENSION ToDX12SRVDimension(TextureType textureType)
{
    switch (textureType)
    {
    case TextureType::TT_TEX2D:
        return D3D12_SRV_DIMENSION_TEXTURE2D;
    case TextureType::TT_TEX3D:
        return D3D12_SRV_DIMENSION_TEXTURE3D;
    case TextureType::TT_TEX2D_ARRAY:
        return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    case TextureType::TT_CUBEMAP:
        return D3D12_SRV_DIMENSION_TEXTURECUBE;
    case TextureType::TT_CUBEMAP_ARRAY:
        return D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    default:
        return D3D12_SRV_DIMENSION_UNKNOWN;
    }
}

D3D12_UAV_DIMENSION ToDX12UAVDimension(TextureType textureType)
{
    switch (textureType)
    {
    case TextureType::TT_TEX2D:
        return D3D12_UAV_DIMENSION_TEXTURE2D;
    case TextureType::TT_TEX3D:
        return D3D12_UAV_DIMENSION_TEXTURE3D;
    case TextureType::TT_TEX2D_ARRAY:   // fallthrough
    case TextureType::TT_CUBEMAP:
    case TextureType::TT_CUBEMAP_ARRAY:
        return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    default:
        return D3D12_UAV_DIMENSION_UNKNOWN;
    }
}

D3D12_CONSTANT_BUFFER_VIEW_DESC GetCBVDesc(DX12GpuBuffer* buffer)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() == GpuBufferType::CBUFF);

    D3D12_CONSTANT_BUFFER_VIEW_DESC desc {};
    desc.BufferLocation = buffer->GetResource()->GetGPUVirtualAddress();

    // DX12 requires cbuffers sizes to be 256-byte aligned
    // In DX12GpuBuffer, we ensure CBUFF sizes are 256-byte aligned internally so this will be valid
    desc.SizeInBytes = ByteUtil::AlignAs(buffer->Size(), 256);

    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuBuffer* buffer, uint32 structureStride, uint32 firstElement, uint32 numElements)
{
    AssertDebug(buffer != nullptr);

    const bool useByteAddressBuffer = (structureStride == 0);

    if (!useByteAddressBuffer)
    {
        numElements = (numElements == UINT32_MAX) ? uint32(buffer->Size() / structureStride) : numElements;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC desc {};
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    desc.Format = (useByteAddressBuffer) ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
    desc.Buffer.FirstElement = firstElement;
    desc.Buffer.NumElements = numElements;
    desc.Buffer.StructureByteStride = structureStride;
    desc.Buffer.Flags = (useByteAddressBuffer) ? D3D12_BUFFER_SRV_FLAG_RAW : D3D12_BUFFER_SRV_FLAG_NONE;

    return desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuBuffer* buffer, uint32 structureStride, uint32 firstElement, uint32 numElements)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() != GpuBufferType::CBUFF);

    const bool useByteAddressBuffer = (structureStride == 0);

    if (!useByteAddressBuffer)
    {
        numElements = (numElements == UINT32_MAX) ? uint32(buffer->Size() / structureStride) : numElements;
    }

    D3D12_UNORDERED_ACCESS_VIEW_DESC desc {};
    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    desc.Format = (useByteAddressBuffer) ? DXGI_FORMAT_R32_TYPELESS : DXGI_FORMAT_UNKNOWN;
    desc.Buffer.FirstElement = firstElement;
    desc.Buffer.NumElements = numElements;
    desc.Buffer.StructureByteStride = structureStride;
    desc.Buffer.CounterOffsetInBytes = 0;
    desc.Buffer.Flags = (useByteAddressBuffer) ? D3D12_BUFFER_UAV_FLAG_RAW : D3D12_BUFFER_UAV_FLAG_NONE;

    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuImage* image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers)
{
    AssertDebug(image != nullptr);

    const TextureDesc& textureDesc = image->GetTextureDesc();

    const uint32 descNumMips = textureDesc.NumMips();
    const uint32 descNumLayers = textureDesc.NumArrayLayers();

    AssertDebug(mipIndex < descNumMips && mipIndex + numMips <= descNumMips);
    AssertDebug(layerIndex < descNumLayers && layerIndex + numLayers <= descNumLayers);

    mipIndex = MathUtil::Min(mipIndex, descNumMips - 1);
    numMips = MathUtil::Min(numMips, descNumMips);
    layerIndex = MathUtil::Min(layerIndex, descNumLayers - 1);
    numLayers = MathUtil::Min(numLayers, descNumLayers);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = ToDXGIFormat(textureDesc.format, DX12ViewType::SRV_UAV);
    srvDesc.ViewDimension = ToDX12SRVDimension(textureDesc.type);

    switch (srvDesc.ViewDimension)
    {
    case D3D12_SRV_DIMENSION_TEXTURE2D:
        srvDesc.Texture2D.MostDetailedMip = mipIndex;
        srvDesc.Texture2D.MipLevels = numMips;
        srvDesc.Texture2D.PlaneSlice = 0;
        srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
        break;
    case D3D12_SRV_DIMENSION_TEXTURE3D:
        srvDesc.Texture3D.MostDetailedMip = mipIndex;
        srvDesc.Texture3D.MipLevels = numMips;
        srvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
        break;
    case D3D12_SRV_DIMENSION_TEXTURE2DARRAY:
        srvDesc.Texture2DArray.MostDetailedMip = mipIndex;
        srvDesc.Texture2DArray.MipLevels = numMips;
        srvDesc.Texture2DArray.FirstArraySlice = layerIndex;
        srvDesc.Texture2DArray.ArraySize = numLayers;
        srvDesc.Texture2DArray.PlaneSlice = 0;
        srvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
        break;
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
        srvDesc.TextureCube.MostDetailedMip = mipIndex;
        srvDesc.TextureCube.MipLevels = numMips;
        srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
        break;
    case D3D12_SRV_DIMENSION_TEXTURECUBEARRAY:
        srvDesc.TextureCubeArray.MostDetailedMip = mipIndex;
        srvDesc.TextureCubeArray.MipLevels = numMips;
        srvDesc.TextureCubeArray.First2DArrayFace = layerIndex;
        srvDesc.TextureCubeArray.NumCubes = textureDesc.NumArrayLayers() / 6;
        srvDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
        break;
    case D3D12_SRV_DIMENSION_UNKNOWN:
        HYP_UNREACHABLE();
        break;
    }

    return srvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuImage* image, uint32 mipIndex, uint32 /*numMips*/, uint32 layerIndex, uint32 numLayers)
{
    AssertDebug(image != nullptr);
    AssertDebug(image->GetTextureDesc().imageUsage & ImageUsage::IU_STORAGE);

    const TextureDesc& textureDesc = image->GetTextureDesc();

    const uint32 descNumMips = textureDesc.NumMips();
    const uint32 descNumLayers = textureDesc.NumArrayLayers();

    AssertDebug(mipIndex < descNumMips);
    AssertDebug(layerIndex < descNumLayers && layerIndex + numLayers <= descNumLayers);

    mipIndex = MathUtil::Min(mipIndex, descNumMips - 1);
    layerIndex = MathUtil::Min(layerIndex, descNumLayers - 1);
    numLayers = MathUtil::Min(numLayers, descNumLayers);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
    uavDesc.Format = ToDXGIFormat(textureDesc.format, DX12ViewType::SRV_UAV);
    uavDesc.ViewDimension = ToDX12UAVDimension(textureDesc.type);

    switch (uavDesc.ViewDimension)
    {
    case D3D12_UAV_DIMENSION_TEXTURE2D:
        uavDesc.Texture2D.MipSlice = mipIndex;
        uavDesc.Texture2D.PlaneSlice = 0;
        break;
    case D3D12_UAV_DIMENSION_TEXTURE3D:
        uavDesc.Texture3D.MipSlice = mipIndex;
        uavDesc.Texture3D.FirstWSlice = 0;
        uavDesc.Texture3D.WSize = UINT(-1);
        break;
    case D3D12_UAV_DIMENSION_TEXTURE2DARRAY:
        uavDesc.Texture2DArray.MipSlice = mipIndex;
        uavDesc.Texture2DArray.FirstArraySlice = layerIndex;
        uavDesc.Texture2DArray.ArraySize = numLayers;
        uavDesc.Texture2DArray.PlaneSlice = 0;
        break;
    case D3D12_UAV_DIMENSION_UNKNOWN:
        HYP_UNREACHABLE();
        break;
    }

    return uavDesc;
}

} // namespace Hyperion
