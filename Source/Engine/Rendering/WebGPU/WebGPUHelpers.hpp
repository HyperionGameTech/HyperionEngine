/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
 */

#pragma once

#include <Rendering/WebGPU/WebGPUShared.hpp>

namespace Hyperion {

WGPUTextureFormat ToWGPUTextureFormat(TextureFormat format);
WGPUTextureDimension ToWGPUTextureDimension(TextureType textureType);
WGPUTextureViewDimension ToWGPUTextureViewDimension(TextureType textureType);
WGPUTextureAspect GetSampledAspect(TextureFormat format);

WGPUTextureSampleType GetDefaultSampleType(TextureFormat format, bool float32Filterable);

WGPUCompareFunction ToWGPUCompareFunction(DepthCompareOp compareOp);
WGPUCompareFunction ToWGPUCompareFunction(StencilCompareOp compareOp);
WGPUCompareFunction ToWGPUCompareFunction(SamplerCompareOp compareOp);
WGPUStencilOperation ToWGPUStencilOperation(StencilOp stencilOp);
WGPUBlendFactor ToWGPUBlendFactor(BlendModeFactor factor);
WGPUCullMode ToWGPUCullMode(FaceCullMode cullMode);
WGPUPrimitiveTopology ToWGPUPrimitiveTopology(Topology topology);
WGPUIndexFormat ToWGPUIndexFormat(GpuElemType elemType);
WGPUAddressMode ToWGPUAddressMode(TextureWrapMode wrapMode);
WGPUBufferUsage GetWGPUBufferUsage(GpuBufferType bufferType);

bool IsCpuWrittenBufferType(GpuBufferType bufferType);

} // namespace Hyperion
