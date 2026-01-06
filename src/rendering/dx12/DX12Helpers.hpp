/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

namespace Hyperion {

enum ResourceState : uint32;
enum TextureFormat : uint32;

class DX12GpuBuffer;
class DX12GpuImage;

DXGI_FORMAT ToDXGIFormat(TextureFormat);
D3D12_RESOURCE_STATES ToDX12ResourceStates(ResourceState state);

D3D12_CONSTANT_BUFFER_VIEW_DESC GetCBVDesc(DX12GpuBuffer* buffer);
D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuBuffer* buffer);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuBuffer* buffer);

D3D12_SHADER_RESOURCE_VIEW_DESC GetSRVDesc(DX12GpuImage* image);
D3D12_UNORDERED_ACCESS_VIEW_DESC GetUAVDesc(DX12GpuImage* image);

} // namespace Hyperion
