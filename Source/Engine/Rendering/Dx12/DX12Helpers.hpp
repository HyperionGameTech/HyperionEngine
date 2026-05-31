/*!
 *  @author: The Hyperion Contributors
 *  @date 2016-2026
 *  @licence MIT
*/

#pragma once

#include <Core/Types.hpp>

#include <Rendering/Shared.hpp>

#include <Rendering/DX12/DX12Shared.hpp>

namespace Hyperion {

enum ResourceState : uint8;
enum FaceCullMode : uint8;
enum Topology : uint8;
enum BlendModeFactor : uint8;
enum StencilOp : uint8;
enum StencilCompareOp : uint8;
enum DepthCompareOp : uint8;
enum GpuElemType : uint8;

enum class TextureFormat : uint8;
enum class TextureType : uint8;
enum class ShaderRegister : uint8;
enum class GpuBufferType : uint8;

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
D3D12_PRIMITIVE_TOPOLOGY ToDX12PrimitiveTopology(Topology topology);
DXGI_FORMAT ToDXGIFormat(GpuElemType elemType);
D3D12_STENCIL_OP ToDX12StencilOp(StencilOp op);
D3D12_COMPARISON_FUNC ToDX12ComparisonFunction(StencilCompareOp compareOp);
D3D12_COMPARISON_FUNC ToDX12DepthCompareOp(DepthCompareOp compareOp);
D3D12_SRV_DIMENSION ToDX12SRVDimension(TextureType textureType);
D3D12_UAV_DIMENSION ToDX12UAVDimension(TextureType textureType);

D3D12_CONSTANT_BUFFER_VIEW_DESC GetCBVDesc(DX12GpuBuffer* buffer);
D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(GpuBufferType bufferType, size_t bufferSize, uint32 structureStride, uint32 firstElement = 0, uint32 numElements = UINT32_MAX);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(GpuBufferType bufferType, size_t bufferSize, uint32 structureStride, uint32 firstElement = 0, uint32 numElements = UINT32_MAX);

D3D12_SAMPLER_DESC GetSamplerDesc(const class DX12Sampler* sampler);

D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuImage* image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers, TextureType viewType = TextureType::Max);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuImage* image, uint32 mipIndex, uint32 numMips, uint32 layerIndex, uint32 numLayers, TextureType viewType = TextureType::Max);

D3D12_DESCRIPTOR_RANGE_TYPE ToDX12DescriptorRangeType(ShaderRegister reg);

/*! \brief Checks if the device has been removed and returns the reason as a c-string
 *  \param device The D3D12 device to check.
 *  \return String (const char*) representation of the error, nullptr if no error */
HYP_NODISCARD const char* CheckDeviceRemovedReason(ID3D12Device* device);

} // namespace Hyperion
