/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Helpers.hpp>
#include <rendering/dx12/DX12GpuBuffer.hpp>
#include <rendering/dx12/DX12GpuImage.hpp>
#include <rendering/dx12/DX12Sampler.hpp>

#include <rendering/Shared.hpp>
#include <rendering/util/ShaderCompiler.hpp> // For ShaderCompiler

namespace Hyperion {

DXGI_FORMAT ToDXGIFormat(TextureFormat format, DX12ViewType getForViewType)
{
    switch (format)
    {
    case TextureFormat::R8:
        return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::RG8:
        return DXGI_FORMAT_R8G8_UNORM;
    case TextureFormat::RGB8:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::RGBA8:
        return DXGI_FORMAT_R8G8B8A8_UNORM;
    case TextureFormat::R8_SRGB:
        return DXGI_FORMAT_R8_UNORM;
    case TextureFormat::RG8_SRGB:
        return DXGI_FORMAT_R8G8_UNORM;
    case TextureFormat::RGB8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::RGBA8_SRGB:
        return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    case TextureFormat::R11G11B10F:
        return DXGI_FORMAT_R11G11B10_FLOAT;
    case TextureFormat::R10G10B10A2:
        return DXGI_FORMAT_R10G10B10A2_UNORM;
    case TextureFormat::R16:
        return DXGI_FORMAT_R16_UINT;
    case TextureFormat::RG16:
        return DXGI_FORMAT_R16G16_UINT;
    case TextureFormat::RGB16:
        return DXGI_FORMAT_R16G16B16A16_UINT;
    case TextureFormat::RGBA16:
        return DXGI_FORMAT_R16G16B16A16_UINT;
    case TextureFormat::R32:
        return DXGI_FORMAT_R32_UINT;
    case TextureFormat::RG32:
        return DXGI_FORMAT_R32G32_UINT;
    case TextureFormat::RGB32:
        return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::RGBA32:
        return DXGI_FORMAT_R32G32B32A32_UINT;
    case TextureFormat::R16F:
        return DXGI_FORMAT_R16_FLOAT;
    case TextureFormat::RG16F:
        return DXGI_FORMAT_R16G16_FLOAT;
    case TextureFormat::RGB16F:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::RGBA16F:
        return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case TextureFormat::R32F:
        return DXGI_FORMAT_R32_FLOAT;
    case TextureFormat::RG32F:
        return DXGI_FORMAT_R32G32_FLOAT;
    case TextureFormat::RGB32F:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case TextureFormat::RGBA32F:
        return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case TextureFormat::BGRA8:
        return DXGI_FORMAT_B8G8R8A8_UNORM;
    case TextureFormat::BGR8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case TextureFormat::BGRA8_SRGB:
        return DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
    case TextureFormat::D16:
        if (getForViewType == DX12ViewType::RTV_DSV)
            return DXGI_FORMAT_D16_UNORM;

        if (getForViewType == DX12ViewType::SRV_UAV)
            return DXGI_FORMAT_R16_UNORM;

        return DXGI_FORMAT_R16_TYPELESS;
    case TextureFormat::D24_S8:
        if (getForViewType == DX12ViewType::SRV_UAV)
            return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

        if (getForViewType == DX12ViewType::RTV_DSV)
            return DXGI_FORMAT_D24_UNORM_S8_UINT;
        
        return DXGI_FORMAT_R24G8_TYPELESS;
    case TextureFormat::D32F:
        if (getForViewType == DX12ViewType::SRV_UAV)
            return DXGI_FORMAT_R32_FLOAT;

        if (getForViewType == DX12ViewType::RTV_DSV)
            return DXGI_FORMAT_D32_FLOAT;

        return DXGI_FORMAT_R32_TYPELESS;
    case TextureFormat::D32F_S8:
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
        return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

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

D3D12_PRIMITIVE_TOPOLOGY ToDX12PrimitiveTopology(Topology topology)
{
    switch (topology)
    {
    case TOP_TRIANGLES:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case TOP_TRIANGLE_STRIP:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case TOP_TRIANGLE_FAN:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case TOP_LINES:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case TOP_POINTS:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    default:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

DXGI_FORMAT ToDXGIFormat(GpuElemType elemType)
{
    switch (elemType)
    {
    case GET_UNSIGNED_BYTE:
        return DXGI_FORMAT_R8_UINT;
    case GET_SIGNED_BYTE:
        return DXGI_FORMAT_R8_SINT;
    case GET_UNSIGNED_SHORT:
        return DXGI_FORMAT_R16_UINT;
    case GET_SIGNED_SHORT:
        return DXGI_FORMAT_R16_SINT;
    case GET_UNSIGNED_INT:
        return DXGI_FORMAT_R32_UINT;
    case GET_SIGNED_INT:
        return DXGI_FORMAT_R32_SINT;
    case GET_FLOAT:
        return DXGI_FORMAT_R32_FLOAT;
    default:
        return DXGI_FORMAT_R32_UINT;
    }
}

D3D12_STENCIL_OP ToDX12StencilOp(StencilOp op)
{
    switch (op)
    {
    case StencilOp::SO_KEEP:
        return D3D12_STENCIL_OP_KEEP;
    case StencilOp::SO_ZERO:
        return D3D12_STENCIL_OP_ZERO;
    case StencilOp::SO_REPLACE:
        return D3D12_STENCIL_OP_REPLACE;
    case StencilOp::SO_INCREMENT:
        return D3D12_STENCIL_OP_INCR;
    case StencilOp::SO_DECREMENT:
        return D3D12_STENCIL_OP_DECR;
    default:
        HYP_NOT_IMPLEMENTED();
    }
}

D3D12_COMPARISON_FUNC ToDX12ComparisonFunction(StencilCompareOp compareOp)
{
    switch (compareOp)
    {
    case SCO_ALWAYS:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    case SCO_NEVER:
        return D3D12_COMPARISON_FUNC_NEVER;
    case SCO_EQUAL:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case SCO_NOT_EQUAL:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    default:
        HYP_NOT_IMPLEMENTED();
    }
}

D3D12_COMPARISON_FUNC ToDX12DepthCompareOp(DepthCompareOp compareOp)
{
    switch (compareOp)
    {
    case DCO_LESS:
        return D3D12_COMPARISON_FUNC_LESS;
    case DCO_LESS_OR_EQUAL:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case DCO_GREATER:
        return D3D12_COMPARISON_FUNC_GREATER;
    case DCO_GREATER_OR_EQUAL:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case DCO_EQUAL:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case DCO_NOT_EQUAL:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case DCO_ALWAYS:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    case DCO_NEVER:
        return D3D12_COMPARISON_FUNC_NEVER;
    default:
        return D3D12_COMPARISON_FUNC_LESS;
    }
}

static inline D3D12_COMPARISON_FUNC ToDX12SamplerCompareOp(SamplerCompareOp compareOp)
{
    switch (compareOp)
    {
    case SamplerCompareOp::None:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    case SamplerCompareOp::Less:
        return D3D12_COMPARISON_FUNC_LESS;
    case SamplerCompareOp::LessEq:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case SamplerCompareOp::Greater:
        return D3D12_COMPARISON_FUNC_GREATER;
    case SamplerCompareOp::GreaterEq:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case SamplerCompareOp::Equal:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case SamplerCompareOp::NotEqual:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case SamplerCompareOp::Always:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    case SamplerCompareOp::Never:
        return D3D12_COMPARISON_FUNC_NEVER;
    default:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    }
}

D3D12_SRV_DIMENSION ToDX12SRVDimension(TextureType textureType)
{
    switch (textureType)
    {
    case TextureType::Texture2D:
        return D3D12_SRV_DIMENSION_TEXTURE2D;
    case TextureType::Texture3D:
        return D3D12_SRV_DIMENSION_TEXTURE3D;
    case TextureType::Texture2DArray:
        return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    case TextureType::Cubemap:
        return D3D12_SRV_DIMENSION_TEXTURECUBE;
    case TextureType::CubemapArray:
        return D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
    default:
        return D3D12_SRV_DIMENSION_UNKNOWN;
    }
}

D3D12_UAV_DIMENSION ToDX12UAVDimension(TextureType textureType)
{
    switch (textureType)
    {
    case TextureType::Texture2D:
        return D3D12_UAV_DIMENSION_TEXTURE2D;
    case TextureType::Texture3D:
        return D3D12_UAV_DIMENSION_TEXTURE3D;
    case TextureType::Texture2DArray:   // fallthrough
    case TextureType::Cubemap:
    case TextureType::CubemapArray:
        return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
    default:
        return D3D12_UAV_DIMENSION_UNKNOWN;
    }
}

D3D12_CONSTANT_BUFFER_VIEW_DESC GetCBVDesc(DX12GpuBuffer* buffer)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() == GpuBufferType::ConstantBuffer);

    D3D12_CONSTANT_BUFFER_VIEW_DESC desc {};
    desc.BufferLocation = buffer->GetResource()->GetGPUVirtualAddress();

    // DX12 requires cbuffers sizes to be 256-byte aligned
    // In DX12GpuBuffer, we ensure CBUFF sizes are 256-byte aligned internally so this will be valid
    desc.SizeInBytes = ByteUtil::AlignAs(buffer->Size(), 256);

    return desc;
}

static constexpr DXGI_FORMAT SizeToBufferFormat[] = {
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R32_TYPELESS,
    DXGI_FORMAT_R32G32_TYPELESS,
    DXGI_FORMAT_R32G32B32_TYPELESS,
    DXGI_FORMAT_R32G32B32A32_TYPELESS
};

D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuBuffer* buffer, uint32 structureStride, uint32 firstElement, uint32 numElements)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() != GpuBufferType::ConstantBuffer);
    
    D3D12_SHADER_RESOURCE_VIEW_DESC desc {};
    desc.Buffer.FirstElement = firstElement;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    switch (buffer->GetBufferType())
    {
    case GpuBufferType::ByteAddressBuffer: // fallthrough
    case GpuBufferType::RWByteAddressBuffer:
        // For raw (byte address) buffers, elements are 4-byte R32_TYPELESS units
        desc.Buffer.NumElements = uint32(buffer->Size() / 4) - firstElement;
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        break;
    case GpuBufferType::StructuredBuffer: // fallthrough
    case GpuBufferType::RWStructuredBuffer:
    default:
        Assert(structureStride != 0);

        desc.Buffer.NumElements = (numElements == UINT32_MAX) ? uint32(buffer->Size() / structureStride) : numElements;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        desc.Buffer.StructureByteStride = structureStride;
        break;
    }

    AssertDebug(desc.Buffer.NumElements != 0);
    
    return desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuBuffer* buffer, uint32 structureStride, uint32 firstElement, uint32 numElements)
{
    AssertDebug(buffer != nullptr);
    AssertDebug(buffer->GetBufferType() != GpuBufferType::ConstantBuffer);

    D3D12_UNORDERED_ACCESS_VIEW_DESC desc {};
    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = firstElement;
    desc.Buffer.CounterOffsetInBytes = 0;

    switch (buffer->GetBufferType())
    {
    case GpuBufferType::ByteAddressBuffer: // fallthrough
    case GpuBufferType::RWByteAddressBuffer:
        desc.Buffer.NumElements = uint32(buffer->Size() / 4) - firstElement;
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        break;
    case GpuBufferType::StructuredBuffer: // fallthrough
    case GpuBufferType::RWStructuredBuffer:
    default:
        Assert(structureStride != 0);

        desc.Buffer.NumElements = (numElements == UINT32_MAX) ? uint32(buffer->Size() / structureStride) : numElements;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        desc.Buffer.StructureByteStride = structureStride;
        break;
    }
    
    AssertDebug(desc.Buffer.NumElements != 0);

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

    // When viewing a single face of a cubemap as a 2D texture, use TEXTURE2D dimension
    const bool isCubemap = textureDesc.IsTextureCube() || textureDesc.IsTextureCubeArray();
    if (isCubemap && numLayers == 1)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    }

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

    // When viewing a single face of a cubemap as a 2D texture, use TEXTURE2D dimension
    const bool isCubemapUav = textureDesc.IsTextureCube() || textureDesc.IsTextureCubeArray();
    if (isCubemapUav && numLayers == 1)
    {
        uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    }

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

D3D12_SAMPLER_DESC GetSamplerDesc(const DX12Sampler* sampler)
{
    AssertDebug(sampler != nullptr);

    D3D12_SAMPLER_DESC desc {};

    switch (sampler->GetMinFilterMode())
    {
    case TFM_NEAREST:
        switch (sampler->GetMagFilterMode())
        {
        case TFM_NEAREST:
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            break;
        case TFM_LINEAR:
            desc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            break;
        default:
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        }
        break;
    case TFM_LINEAR:
        switch (sampler->GetMagFilterMode())
        {
        case TFM_NEAREST:
            desc.Filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            break;
        case TFM_LINEAR:
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            break;
        default:
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        }
        break;
    default:
        desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    }

    switch (sampler->GetWrapMode())
    {
    case TWM_REPEAT:
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        break;
    case TWM_CLAMP_TO_EDGE:
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        break;
    case TWM_CLAMP_TO_BORDER:
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        break;
    default:
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    }

    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 1;

    const SamplerCompareOp compareOp = sampler->GetCompareOp();
    if (compareOp == SamplerCompareOp::None)
    {
        // Non-comparison sampler: leave filter as-is and skip ComparisonFunc.
        // ComparisonFunc stays at 0 from zero-initialization, which D3D12
        // validation ignores for non-comparison filters.
    }
    else
    {
        // Upgrade the filter to a comparison type
        desc.Filter = D3D12_FILTER(static_cast<UINT>(desc.Filter) | 0x80);
        desc.ComparisonFunc = ToDX12SamplerCompareOp(compareOp);
    }

    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D12_FLOAT32_MAX;

    return desc;
}

D3D12_DESCRIPTOR_RANGE_TYPE ToDX12DescriptorRangeType(ShaderRegister reg)
{
    switch (reg)
    {
    case ShaderRegister::SRV:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    case ShaderRegister::UAV:
        return D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    case ShaderRegister::BUFFER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
    case ShaderRegister::SAMPLER:
        return D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER;
    default:
        HYP_UNREACHABLE();
    }
}

bool CheckDeviceRemovedReason(ID3D12Device* device)
{
    if (device == nullptr)
    {
        return false;
    }

    HRESULT hr = device->GetDeviceRemovedReason();
    if (SUCCEEDED(hr))
    {
        return false;
    }

    const char* reasonStr = "Unknown";

    switch (hr)
    {
    case DXGI_ERROR_DEVICE_HUNG:
        reasonStr = "DEVICE_HUNG - The device took too long to execute commands (GPU timeout/TDR)";
        break;
    case DXGI_ERROR_DEVICE_REMOVED:
        reasonStr = "DEVICE_REMOVED - The device was physically removed or driver was upgraded";
        break;
    case DXGI_ERROR_DEVICE_RESET:
        reasonStr = "DEVICE_RESET - The device was reset due to a driver error or GPU hang";
        break;
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
        reasonStr = "DRIVER_INTERNAL_ERROR - The driver encountered an internal error";
        break;
    case DXGI_ERROR_INVALID_CALL:
        reasonStr = "INVALID_CALL - An invalid API call was made";
        break;
    case E_OUTOFMEMORY:
        reasonStr = "OUT_OF_MEMORY - The GPU or system ran out of memory";
        break;
    default:
        reasonStr = "Unknown device removal reason";
        break;
    }

    HYP_LOG(RenderingBackend, Error, "D3D12 Device Removed! Reason: {} (HRESULT: {})", reasonStr, hr);

    return true;
}

} // namespace Hyperion
