/* Copyright (c) 2026 No Tomorrow Games. All rights reserved. */

#include <DX12Pch.hpp>

#include <rendering/dx12/DX12Helpers.hpp>

#include <rendering/Shared.hpp>

namespace Hyperion {

DXGI_FORMAT ToDXGIFormat(TextureFormat format)
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
        return DXGI_FORMAT_D16_UNORM;
    case TF_DEPTH_24:
        return DXGI_FORMAT_D24_UNORM_S8_UINT;
    case TF_DEPTH_32F:
        return DXGI_FORMAT_D32_FLOAT;
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

} // namespace Hyperion
