/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#pragma once

#include <core/Types.hpp>

namespace Hyperion {

enum ResourceState : uint32;
enum TextureFormat : uint32;

DXGI_FORMAT ToDXGIFormat(TextureFormat);
D3D12_RESOURCE_STATES ToDX12ResourceStates(ResourceState state);

} // namespace Hyperion
