/* Copyright (c) 2016-2026 Andrew J. MacDonald. All rights reserved. */

#pragma once

#include <Core/Types.hpp>

namespace Hyperion {

enum ResourceState : uint8;
enum class TextureFormat : uint8;
enum class TextureType : uint8;
enum FaceCullMode : uint8;
enum Topology : uint8;
enum BlendModeFactor : uint8;
enum StencilOp : uint8;
enum StencilCompareOp : uint8;

class DX12GpuBuffer;
class DX12GpuImage;

enum class DX12ViewType
{
    None,
    SRV_UAV,
    RTV_DSV
};

DXGI_FORMAT ToDXGIFormat(TextureFormat format, DX12ViewType getForViewType = DX12ViewType::None);
D3D12_RESOURCE_STATES ToDX12ResourceStates(ResourceState state);
D3D12_BLEND ToDX12Blend(BlendModeFactor factor);
D3D12_CULL_MODE ToDX12CullMode(FaceCullMode mode);
D3D12_PRIMITIVE_TOPOLOGY_TYPE ToDX12TopologyType(Topology topology);
D3D12_STENCIL_OP ToDX12StencilOp(StencilOp op);
D3D12_COMPARISON_FUNC ToDX12ComparisonFunction(StencilCompareOp compareOp);
D3D12_SRV_DIMENSION ToDX12SRVDimension(TextureType textureType);
D3D12_UAV_DIMENSION ToDX12UAVDimension(TextureType textureType);

D3D12_CONSTANT_BUFFER_VIEW_DESC GetCBVDesc(DX12GpuBuffer* buffer);
D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuBuffer* buffer, uint32 structureStride, uint32 firstElement = 0, uint32 numElements = UINT32_MAX);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuBuffer* buffer, uint32 structureStride, uint32 firstElement = 0, uint32 numElements = UINT32_MAX);

D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuImage* image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuImage* image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers);

} // namespace Hyperion
