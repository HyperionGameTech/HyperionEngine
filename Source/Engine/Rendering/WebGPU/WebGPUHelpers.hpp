/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/WebGPU/WebGPUShared.hpp>

namespace Hyperion {

constexpr WGPUTextureFormat ToWGPUTextureFormat(TextureFormat format)
{
    switch (format)
    {
    case TextureFormat::R8:
    case TextureFormat::R8_SRGB:
    case TextureFormat::B8:
    case TextureFormat::B8_SRGB:
        return WGPUTextureFormat_R8Unorm;
    case TextureFormat::RG8:
    case TextureFormat::RG8_SRGB:
    case TextureFormat::BG8:
    case TextureFormat::BG8_SRGB:
        return WGPUTextureFormat_RG8Unorm;
    case TextureFormat::RGB8:
    case TextureFormat::RGBA8:
        return WGPUTextureFormat_RGBA8Unorm;
    case TextureFormat::RGB8_SRGB:
    case TextureFormat::RGBA8_SRGB:
        return WGPUTextureFormat_RGBA8UnormSrgb;
    case TextureFormat::BGR8:
    case TextureFormat::BGRA8:
        return WGPUTextureFormat_BGRA8Unorm;
    case TextureFormat::BGR8_SRGB:
    case TextureFormat::BGRA8_SRGB:
        return WGPUTextureFormat_BGRA8UnormSrgb;
    case TextureFormat::R16:
        return WGPUTextureFormat_R16Uint;
    case TextureFormat::RG16:
        return WGPUTextureFormat_RG16Uint;
    case TextureFormat::RGB16:
    case TextureFormat::RGBA16:
        return WGPUTextureFormat_RGBA16Uint;
    case TextureFormat::R32:
        return WGPUTextureFormat_R32Uint;
    case TextureFormat::RG32:
        return WGPUTextureFormat_RG32Uint;
    case TextureFormat::RGB32:
    case TextureFormat::RGBA32:
        return WGPUTextureFormat_RGBA32Uint;
    case TextureFormat::R11G11B10F:
        return WGPUTextureFormat_RG11B10Ufloat;
    case TextureFormat::R10G10B10A2:
        return WGPUTextureFormat_RGB10A2Unorm;
    case TextureFormat::R16F:
        return WGPUTextureFormat_R16Float;
    case TextureFormat::RG16F:
        return WGPUTextureFormat_RG16Float;
    case TextureFormat::RGB16F:
    case TextureFormat::RGBA16F:
        return WGPUTextureFormat_RGBA16Float;
    case TextureFormat::R32F:
        return WGPUTextureFormat_R32Float;
    case TextureFormat::RG32F:
        return WGPUTextureFormat_RG32Float;
    case TextureFormat::RGB32F:
    case TextureFormat::RGBA32F:
        return WGPUTextureFormat_RGBA32Float;
    case TextureFormat::D16:
        return WGPUTextureFormat_Depth16Unorm;
    case TextureFormat::D24_S8:
        return WGPUTextureFormat_Depth24PlusStencil8;
    case TextureFormat::D32F:
        return WGPUTextureFormat_Depth32Float;
    case TextureFormat::D32F_S8:
        return WGPUTextureFormat_Depth32FloatStencil8;
    default:
        return WGPUTextureFormat_Undefined;
    }
}

constexpr WGPUTextureDimension ToWGPUTextureDimension(TextureType textureType)
{
    return textureType == TextureType::Texture3D
        ? WGPUTextureDimension_3D
        : WGPUTextureDimension_2D;
}

constexpr WGPUTextureViewDimension ToWGPUTextureViewDimension(TextureType textureType)
{
    switch (textureType)
    {
    case TextureType::Texture2D:
        return WGPUTextureViewDimension_2D;
    case TextureType::Texture3D:
        return WGPUTextureViewDimension_3D;
    case TextureType::Cubemap:
        return WGPUTextureViewDimension_Cube;
    case TextureType::Texture2DArray:
        return WGPUTextureViewDimension_2DArray;
    case TextureType::CubemapArray:
        return WGPUTextureViewDimension_CubeArray;
    default:
        return WGPUTextureViewDimension_Undefined;
    }
}

constexpr WGPUTextureAspect GetSampledAspect(TextureFormat format)
{
    // a view bound for sampling may only cover one aspect of a depth-stencil texture
    return TextureUtils::IsDepthFormat(format) ? WGPUTextureAspect_DepthOnly : WGPUTextureAspect_All;
}

constexpr WGPUTextureSampleType GetDefaultSampleType(TextureFormat format, bool float32Filterable)
{
    if (TextureUtils::IsDepthFormat(format))
    {
        return WGPUTextureSampleType_UnfilterableFloat;
    }

    switch (format)
    {
    case TextureFormat::R16:
    case TextureFormat::RG16:
    case TextureFormat::RGB16:
    case TextureFormat::RGBA16:
    case TextureFormat::R32:
    case TextureFormat::RG32:
    case TextureFormat::RGB32:
    case TextureFormat::RGBA32:
        return WGPUTextureSampleType_Uint;
    case TextureFormat::R32F:
    case TextureFormat::RG32F:
    case TextureFormat::RGB32F:
    case TextureFormat::RGBA32F:
        return float32Filterable ? WGPUTextureSampleType_Float : WGPUTextureSampleType_UnfilterableFloat;
    default:
        return WGPUTextureSampleType_Float;
    }
}

constexpr WGPUCompareFunction ToWGPUCompareFunction(DepthCompareOp compareOp)
{
    switch (compareOp)
    {
    case DepthCompareOp::Less:
        return WGPUCompareFunction_Less;
    case DepthCompareOp::LessOrEqual:
        return WGPUCompareFunction_LessEqual;
    case DepthCompareOp::Greater:
        return WGPUCompareFunction_Greater;
    case DepthCompareOp::GreaterOrEqual:
        return WGPUCompareFunction_GreaterEqual;
    case DepthCompareOp::Equal:
        return WGPUCompareFunction_Equal;
    case DepthCompareOp::NotEqual:
        return WGPUCompareFunction_NotEqual;
    case DepthCompareOp::Never:
        return WGPUCompareFunction_Never;
    case DepthCompareOp::Always:
    default:
        return WGPUCompareFunction_Always;
    }
}

constexpr WGPUCompareFunction ToWGPUCompareFunction(StencilCompareOp compareOp)
{
    switch (compareOp)
    {
    case StencilCompareOp::Never:
        return WGPUCompareFunction_Never;
    case StencilCompareOp::Equal:
        return WGPUCompareFunction_Equal;
    case StencilCompareOp::NotEqual:
        return WGPUCompareFunction_NotEqual;
    case StencilCompareOp::Always:
    default:
        return WGPUCompareFunction_Always;
    }
}

constexpr WGPUCompareFunction ToWGPUCompareFunction(SamplerCompareOp compareOp)
{
    switch (compareOp)
    {
    case SamplerCompareOp::Less:
        return WGPUCompareFunction_Less;
    case SamplerCompareOp::LessEq:
        return WGPUCompareFunction_LessEqual;
    case SamplerCompareOp::Greater:
        return WGPUCompareFunction_Greater;
    case SamplerCompareOp::GreaterEq:
        return WGPUCompareFunction_GreaterEqual;
    case SamplerCompareOp::Equal:
        return WGPUCompareFunction_Equal;
    case SamplerCompareOp::NotEqual:
        return WGPUCompareFunction_NotEqual;
    case SamplerCompareOp::Always:
        return WGPUCompareFunction_Always;
    case SamplerCompareOp::Never:
        return WGPUCompareFunction_Never;
    case SamplerCompareOp::None:
    default:
        return WGPUCompareFunction_Undefined;
    }
}

constexpr WGPUStencilOperation ToWGPUStencilOperation(StencilOp stencilOp)
{
    switch (stencilOp)
    {
    case StencilOp::Zero:
        return WGPUStencilOperation_Zero;
    case StencilOp::Replace:
        return WGPUStencilOperation_Replace;
    case StencilOp::Increment:
        return WGPUStencilOperation_IncrementClamp;
    case StencilOp::Decrement:
        return WGPUStencilOperation_DecrementClamp;
    case StencilOp::Keep:
    default:
        return WGPUStencilOperation_Keep;
    }
}

constexpr WGPUBlendFactor ToWGPUBlendFactor(BlendModeFactor factor)
{
    switch (factor)
    {
    case BlendModeFactor::One:
        return WGPUBlendFactor_One;
    case BlendModeFactor::SrcColor:
        return WGPUBlendFactor_Src;
    case BlendModeFactor::SrcAlpha:
        return WGPUBlendFactor_SrcAlpha;
    case BlendModeFactor::DstColor:
        return WGPUBlendFactor_Dst;
    case BlendModeFactor::DstAlpha:
        return WGPUBlendFactor_DstAlpha;
    case BlendModeFactor::OneMinusSrcColor:
        return WGPUBlendFactor_OneMinusSrc;
    case BlendModeFactor::OneMinusSrcAlpha:
        return WGPUBlendFactor_OneMinusSrcAlpha;
    case BlendModeFactor::OneMinusDstColor:
        return WGPUBlendFactor_OneMinusDst;
    case BlendModeFactor::OneMinusDstAlpha:
        return WGPUBlendFactor_OneMinusDstAlpha;
    case BlendModeFactor::Zero:
    case BlendModeFactor::None:
    default:
        return WGPUBlendFactor_Zero;
    }
}

constexpr WGPUCullMode ToWGPUCullMode(FaceCullMode cullMode)
{
    switch (cullMode)
    {
    case FaceCullMode::Back:
        return WGPUCullMode_Back;
    case FaceCullMode::Front:
        return WGPUCullMode_Front;
    case FaceCullMode::None:
    default:
        return WGPUCullMode_None;
    }
}

constexpr WGPUPrimitiveTopology ToWGPUPrimitiveTopology(Topology topology)
{
    switch (topology)
    {
    case Topology::TriangleStrip:
        return WGPUPrimitiveTopology_TriangleStrip;
    case Topology::Lines:
        return WGPUPrimitiveTopology_LineList;
    case Topology::Points:
        return WGPUPrimitiveTopology_PointList;
    case Topology::Triangles:
    case Topology::TriangleFan: // not expressible; meshes are expected to arrive as lists
    default:
        return WGPUPrimitiveTopology_TriangleList;
    }
}

constexpr WGPUIndexFormat ToWGPUIndexFormat(GpuElemType elemType)
{
    switch (elemType)
    {
    case GpuElemType::UnsignedShort:
    case GpuElemType::SignedShort:
        return WGPUIndexFormat_Uint16;
    default:
        return WGPUIndexFormat_Uint32;
    }
}

constexpr WGPUAddressMode ToWGPUAddressMode(TextureWrapMode wrapMode)
{
    switch (wrapMode)
    {
    case TextureWrapMode::Repeat:
        return WGPUAddressMode_Repeat;
    case TextureWrapMode::ClampToBorder: // no border colors in WebGPU
    case TextureWrapMode::ClampToEdge:
    default:
        return WGPUAddressMode_ClampToEdge;
    }
}

constexpr WGPUBufferUsage GetWGPUBufferUsage(GpuBufferType bufferType)
{
    WGPUBufferUsage usage = WGPUBufferUsage_CopyDst | WGPUBufferUsage_CopySrc;

    switch (bufferType)
    {
    case GpuBufferType::IndexBuffer:
        usage |= WGPUBufferUsage_Index | WGPUBufferUsage_Storage;
        break;
    case GpuBufferType::VertexBuffer:
        usage |= WGPUBufferUsage_Vertex | WGPUBufferUsage_Storage;
        break;
    case GpuBufferType::ConstantBuffer:
        usage |= WGPUBufferUsage_Uniform;
        break;
    case GpuBufferType::StructuredBuffer:
    case GpuBufferType::RWStructuredBuffer:
        // a shader over the storage buffer limit binds the ones it reads a single element of as uniform buffers
        usage |= WGPUBufferUsage_Storage | WGPUBufferUsage_Uniform;
        break;
    case GpuBufferType::ByteAddressBuffer:
    case GpuBufferType::RWByteAddressBuffer:
    case GpuBufferType::ScratchBuffer:
    case GpuBufferType::RTMeshIndexBuffer:
    case GpuBufferType::RTMeshVertexBuffer:
        usage |= WGPUBufferUsage_Storage;
        break;
    case GpuBufferType::IndirectArgsBuffer:
        usage |= WGPUBufferUsage_Indirect | WGPUBufferUsage_Storage;
        break;
    case GpuBufferType::ReadbackBuffer:
        // a mappable buffer may carry no other usage than the opposite copy direction
        usage = WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst;
        break;
    case GpuBufferType::StagingBuffer:
    default:
        break;
    }

    return usage;
}

constexpr bool IsCpuWrittenBufferType(GpuBufferType bufferType)
{
    switch (bufferType)
    {
    case GpuBufferType::StagingBuffer:
    case GpuBufferType::ConstantBuffer:
    case GpuBufferType::ReadbackBuffer:
        return true;
    default:
        return false;
    }
}

} // namespace Hyperion
