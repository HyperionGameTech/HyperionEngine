/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#include <DX12Pch.hpp>

#include <Rendering/DX12/DX12Helpers.hpp>
#include <Rendering/DX12/DX12GpuBuffer.hpp>
#include <Rendering/DX12/DX12GpuImage.hpp>
#include <Rendering/DX12/DX12Sampler.hpp>

#include <Rendering/Shared.hpp>
#include <Rendering/Util/ShaderCompiler.hpp>

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
    case ResourceState::Common:
        return D3D12_RESOURCE_STATE_COMMON;

    case ResourceState::VertexBuffer:
    case ResourceState::ConstantBuffer:
        return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

    case ResourceState::IndexBuffer:
        return D3D12_RESOURCE_STATE_INDEX_BUFFER;

    case ResourceState::RenderTarget:
        return D3D12_RESOURCE_STATE_RENDER_TARGET;

    case ResourceState::UnorderedAccess:
        return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;

    case ResourceState::DepthStencil:
        return D3D12_RESOURCE_STATE_DEPTH_WRITE;

    case ResourceState::ShaderResource:
        return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

    case ResourceState::StreamOut:
        return D3D12_RESOURCE_STATE_STREAM_OUT;

    case ResourceState::IndirectArg:
        return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

    case ResourceState::CopyDst:
        return D3D12_RESOURCE_STATE_COPY_DEST;

    case ResourceState::CopySrc:
        return D3D12_RESOURCE_STATE_COPY_SOURCE;

    case ResourceState::ResolveDst:
        return D3D12_RESOURCE_STATE_RESOLVE_DEST;

    case ResourceState::ResolveSrc:
        return D3D12_RESOURCE_STATE_RESOLVE_SOURCE;

    case ResourceState::Present:
        return D3D12_RESOURCE_STATE_PRESENT;

    case ResourceState::ReadGeneric:
        return D3D12_RESOURCE_STATE_GENERIC_READ;

    case ResourceState::Predication:
        return D3D12_RESOURCE_STATE_PREDICATION;

    case ResourceState::Undefined:
    case ResourceState::PreInitialized:
        return D3D12_RESOURCE_STATE_COMMON;

    default:
        return D3D12_RESOURCE_STATE_COMMON;
    }
}

D3D12_BLEND ToDX12Blend(BlendModeFactor factor, bool isAlpha)
{
    // D3D12 disallows color-manipulating blend factors (SRC_COLOR, INV_SRC_COLOR, DEST_COLOR,
    // INV_DEST_COLOR) in the alpha slots (SrcBlendAlpha/DestBlendAlpha) - only ZERO, ONE, and the
    // *_ALPHA variants are valid there. Vulkan has no such restriction, so remap to the alpha
    // equivalent here rather than emitting an invalid CreateBlendState call.
    if (isAlpha)
    {
        switch (factor)
        {
        case BlendModeFactor::SrcColor:
            factor = BlendModeFactor::SrcAlpha;
            break;
        case BlendModeFactor::DstColor:
            factor = BlendModeFactor::DstAlpha;
            break;
        case BlendModeFactor::OneMinusSrcColor:
            factor = BlendModeFactor::OneMinusSrcAlpha;
            break;
        case BlendModeFactor::OneMinusDstColor:
            factor = BlendModeFactor::OneMinusDstAlpha;
            break;
        default:
            break;
        }
    }

    switch (factor)
    {
    case BlendModeFactor::One:
        return D3D12_BLEND_ONE;
    case BlendModeFactor::Zero:
        return D3D12_BLEND_ZERO;
    case BlendModeFactor::SrcColor:
        return D3D12_BLEND_SRC_COLOR;
    case BlendModeFactor::SrcAlpha:
        return D3D12_BLEND_SRC_ALPHA;
    case BlendModeFactor::DstColor:
        return D3D12_BLEND_DEST_COLOR;
    case BlendModeFactor::DstAlpha:
        return D3D12_BLEND_DEST_ALPHA;
    case BlendModeFactor::OneMinusSrcColor:
        return D3D12_BLEND_INV_SRC_COLOR;
    case BlendModeFactor::OneMinusSrcAlpha:
        return D3D12_BLEND_INV_SRC_ALPHA;
    case BlendModeFactor::OneMinusDstColor:
        return D3D12_BLEND_INV_DEST_COLOR;
    case BlendModeFactor::OneMinusDstAlpha:
        return D3D12_BLEND_INV_DEST_ALPHA;
    default:
        return D3D12_BLEND_ONE;
    }
}

D3D12_CULL_MODE ToDX12CullMode(FaceCullMode mode)
{
    switch (mode)
    {
    case FaceCullMode::Back:
        return D3D12_CULL_MODE_BACK;
    case FaceCullMode::Front:
        return D3D12_CULL_MODE_FRONT;
    case FaceCullMode::None:
        return D3D12_CULL_MODE_NONE;
    default:
        return D3D12_CULL_MODE_BACK;
    }
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDX12TopologyType(Topology topology)
{
    switch (topology)
    {
    case Topology::Triangles:
    case Topology::TriangleStrip:
    case Topology::TriangleFan:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    case Topology::Lines:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
    case Topology::Points:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    default:
        return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
    }
}

D3D12_PRIMITIVE_TOPOLOGY ToDX12PrimitiveTopology(Topology topology)
{
    switch (topology)
    {
    case Topology::Triangles:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    case Topology::TriangleStrip:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case Topology::TriangleFan:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
    case Topology::Lines:
        return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
    case Topology::Points:
        return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
    default:
        return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
}

DXGI_FORMAT ToDXGIFormat(GpuElemType elemType)
{
    switch (elemType)
    {
    case GpuElemType::UnsignedByte:
        return DXGI_FORMAT_R8_UINT;
    case GpuElemType::SignedByte:
        return DXGI_FORMAT_R8_SINT;
    case GpuElemType::UnsignedShort:
        return DXGI_FORMAT_R16_UINT;
    case GpuElemType::SignedShort:
        return DXGI_FORMAT_R16_SINT;
    case GpuElemType::UnsignedInt:
        return DXGI_FORMAT_R32_UINT;
    case GpuElemType::SignedInt:
        return DXGI_FORMAT_R32_SINT;
    case GpuElemType::Float:
        return DXGI_FORMAT_R32_FLOAT;
    default:
        return DXGI_FORMAT_R32_UINT;
    }
}

D3D12_STENCIL_OP ToDX12StencilOp(StencilOp op)
{
    switch (op)
    {
    case StencilOp::Keep:
        return D3D12_STENCIL_OP_KEEP;
    case StencilOp::Zero:
        return D3D12_STENCIL_OP_ZERO;
    case StencilOp::Replace:
        return D3D12_STENCIL_OP_REPLACE;
    case StencilOp::Increment:
        return D3D12_STENCIL_OP_INCR;
    case StencilOp::Decrement:
        return D3D12_STENCIL_OP_DECR;
    default:
        HYP_NOT_IMPLEMENTED();
    }
}

D3D12_COMPARISON_FUNC ToDX12ComparisonFunction(StencilCompareOp compareOp)
{
    switch (compareOp)
    {
    case StencilCompareOp::Always:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    case StencilCompareOp::Never:
        return D3D12_COMPARISON_FUNC_NEVER;
    case StencilCompareOp::Equal:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case StencilCompareOp::NotEqual:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    default:
        HYP_NOT_IMPLEMENTED();
    }
}

D3D12_COMPARISON_FUNC ToDX12DepthCompareOp(DepthCompareOp compareOp)
{
    switch (compareOp)
    {
    case DepthCompareOp::Less:
        return D3D12_COMPARISON_FUNC_LESS;
    case DepthCompareOp::LessOrEqual:
        return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case DepthCompareOp::Greater:
        return D3D12_COMPARISON_FUNC_GREATER;
    case DepthCompareOp::GreaterOrEqual:
        return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case DepthCompareOp::Equal:
        return D3D12_COMPARISON_FUNC_EQUAL;
    case DepthCompareOp::NotEqual:
        return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case DepthCompareOp::Always:
        return D3D12_COMPARISON_FUNC_ALWAYS;
    case DepthCompareOp::Never:
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

D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(GpuBufferType bufferType, size_t bufferSize, uint32 structureStride, uint32 firstElement, uint32 numElements)
{
    AssertDebug(bufferType != GpuBufferType::ConstantBuffer);

    D3D12_SHADER_RESOURCE_VIEW_DESC desc {};
    desc.Buffer.FirstElement = firstElement;
    desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    switch (bufferType)
    {
    case GpuBufferType::ByteAddressBuffer: // fallthrough
    case GpuBufferType::RWByteAddressBuffer:
        // For raw (byte address) buffers, elements are 4-byte R32_TYPELESS units
        desc.Buffer.NumElements = uint32(bufferSize / 4) - firstElement;
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        break;
    case GpuBufferType::StructuredBuffer: // fallthrough
    case GpuBufferType::RWStructuredBuffer:
    default:
        Assert(structureStride != 0);

        desc.Buffer.NumElements = (numElements == UINT32_MAX) ? uint32(bufferSize / structureStride) : numElements;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        desc.Buffer.StructureByteStride = structureStride;
        break;
    }

    AssertDebug(desc.Buffer.NumElements != 0);

    return desc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(GpuBufferType bufferType, size_t bufferSize, uint32 structureStride, uint32 firstElement, uint32 numElements)
{
    AssertDebug(bufferType != GpuBufferType::ConstantBuffer);

    D3D12_UNORDERED_ACCESS_VIEW_DESC desc {};
    desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    desc.Buffer.FirstElement = firstElement;
    desc.Buffer.CounterOffsetInBytes = 0;

    switch (bufferType)
    {
    case GpuBufferType::ByteAddressBuffer: // fallthrough
    case GpuBufferType::StructuredBuffer: // fallthrough
        HYP_FAIL("Cannot get UAV desc for non RW type, change the shader uniform decl or create an SRV instead.");
        break;
    case GpuBufferType::RWByteAddressBuffer:
        desc.Buffer.NumElements = uint32(bufferSize / 4) - firstElement;
        desc.Format = DXGI_FORMAT_R32_TYPELESS;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_RAW;
        break;
    case GpuBufferType::RWStructuredBuffer:
    default:
        Assert(structureStride != 0);

        desc.Buffer.NumElements = (numElements == UINT32_MAX) ? uint32(bufferSize / structureStride) : numElements;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
        desc.Buffer.StructureByteStride = structureStride;
        break;
    }

    AssertDebug(desc.Buffer.NumElements != 0);

    return desc;
}

D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuImage* image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers, TextureType viewType)
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

    const TextureType effectiveType = (viewType != TextureType::Max) ? viewType : textureDesc.type;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = ToDXGIFormat(textureDesc.format, DX12ViewType::SRV_UAV);
    srvDesc.ViewDimension = ToDX12SRVDimension(effectiveType);

    // When viewing a single face of a cubemap as a 2D texture, use TEXTURE2D dimension
    const bool isCubemap = effectiveType == TextureType::Cubemap || effectiveType == TextureType::CubemapArray;
    if (isCubemap && numLayers == 1)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    }
    // If we need to view a single cube starting at a nonzero array layer,
    // then we use the TEXTURECUBEARRAY dimension, with NumCubes = 1 instead, so First2DArrayFace can be honored.
    else if (effectiveType == TextureType::Cubemap && layerIndex != 0)
    {
        srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
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
        srvDesc.TextureCubeArray.NumCubes = (effectiveType == TextureType::Cubemap)
            ? 1
            : (numLayers / 6);
        srvDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
        break;
    default:
        HYP_UNREACHABLE();
        break;
    }

    return srvDesc;
}

D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuImage* image, uint32 mipIndex, uint32 /*numMips*/, uint32 layerIndex, uint32 numLayers, TextureType viewType)
{
    AssertDebug(image != nullptr);
    AssertDebug(image->GetTextureDesc().imageUsage & ImageUsage::Storage);

    const TextureDesc& textureDesc = image->GetTextureDesc();

    const uint32 descNumMips = textureDesc.NumMips();
    const uint32 descNumLayers = textureDesc.NumArrayLayers();

    AssertDebug(mipIndex < descNumMips);
    AssertDebug(layerIndex < descNumLayers && layerIndex + numLayers <= descNumLayers);

    mipIndex = MathUtil::Min(mipIndex, descNumMips - 1);
    layerIndex = MathUtil::Min(layerIndex, descNumLayers - 1);
    numLayers = MathUtil::Min(numLayers, descNumLayers);

    const TextureType effectiveType = (viewType != TextureType::Max) ? viewType : textureDesc.type;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc {};
    uavDesc.Format = ToDXGIFormat(textureDesc.format, DX12ViewType::SRV_UAV);
    uavDesc.ViewDimension = ToDX12UAVDimension(effectiveType);

    // When viewing a single face of a cubemap as a 2D texture, use TEXTURE2D dimension
    const bool isCubemapUav = effectiveType == TextureType::Cubemap || effectiveType == TextureType::CubemapArray;
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
    default:
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
    case TextureFilterMode::Nearest:
        switch (sampler->GetMagFilterMode())
        {
        case TextureFilterMode::Nearest:
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
            break;
        case TextureFilterMode::Linear:
            desc.Filter = D3D12_FILTER_MIN_MAG_POINT_MIP_LINEAR;
            break;
        default:
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        }
        break;
    case TextureFilterMode::Linear:
        switch (sampler->GetMagFilterMode())
        {
        case TextureFilterMode::Nearest:
            desc.Filter = D3D12_FILTER_MIN_POINT_MAG_LINEAR_MIP_POINT;
            break;
        case TextureFilterMode::Linear:
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            break;
        default:
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        }
        break;
    default:
        if (sampler->GetMinFilterMode() == TextureFilterMode::LinearMipmap)
        {
            desc.Filter = D3D12_FILTER_ANISOTROPIC;
        }
        else
        {
            desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        }
    }

    switch (sampler->GetWrapMode())
    {
    case TextureWrapMode::Repeat:
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        break;
    case TextureWrapMode::ClampToEdge:
        desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        break;
    case TextureWrapMode::ClampToBorder:
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
    desc.MaxAnisotropy = 8;

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

HYP_NODISCARD const char* CheckDeviceRemovedReason(ID3D12Device* device)
{
    if (!device)
    {
        return nullptr;
    }

    HRESULT hr = device->GetDeviceRemovedReason();
    if (SUCCEEDED(hr))
    {
        return nullptr;
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

    return reasonStr;
}

} // namespace Hyperion
